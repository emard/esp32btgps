#!/usr/bin/env python3

# ./wav2true.py /tmp/circle.wav
# output /tmp/true.wav
# then wav2kml.py with calculate = 3
# ./wav2kml.py /tmp/true.wav > /tmp/true.kml

# TODO output file cmdline option
# TODO better DC removal, smart/smooth
# TODO csv output x/z values

from sys import argv
from functools import reduce
from operator import xor
import numpy as np
import struct

# calculate
# 0:just a copy to check I/O
# 1:integrate z-accel wav data (adxl355)
# 3:y-height with intgrared compensation TODO
calculate = 1
# accel/gyro, select constant and data wav channel
# to remap channels change wav_ch_*
if calculate == 1: # accel adxl355
  g_scale    = 8 # 2/4/8 g is 32000 integer reading
  aint2float = 9.81 * g_scale / 32000 # int -> a [m/s^2]
  # Z-channel of accelerometer
  wav_ch_l = 2
  wav_ch_r = 5
  # slope DC remove by inc/dec of accel offset at each sampling length
  dc_remove_step = 1.0e-4
  # output
  # Y-channel true profile
  out_wav_ch_hl = 1
  out_wav_ch_hr = 4
  float2hint = 1.0E5 # convert float to integer in wav 1 = 0.01 mm = 1E-5 m
  # X-channel of accelerometer is zero
  out_wav_ch_l = 0
  out_wav_ch_r = 3
  
if calculate == 3: # laser vertical distance
  # FIXME this option is currently not supported
  # laser height int->float conversion factor
  hint2float = 1.0e-5 # int -> h [m] int 1 = 0.01 mm = 1E-5 [m]
  # Y-channel contains laser distance reading
  wav_ch_hl = 1
  wav_ch_hr = 4
  # accelerometer int->float conversion factor
  g_scale    = 8 # 2/4/8 g is 32000 integer reading
  aint2float = 9.81 * g_scale / 32000 # int -> a [m/s^2]
  # X-channel contains laser accelerometer reading
  wav_ch_l = 0
  wav_ch_r = 3
  # slope DC remove by inc/dec of accel offset at each sampling length
  dc_remove_step = 1.0e-4

# buffer to read wav
b=bytearray(12)
mvb=memoryview(b)

# buffer to write wav
sample=bytearray(12)

# equal-distance slope sampling length (m)
sampling_length = 0.05

# equal-time accelerometer sample time
a_sample_dt = 1/1000 # s (1kHz accelerometer sample rate)    

# raw values reading (accelerometer or gyro) ac =
ac = np.zeros(6).astype(np.int16) # current integer accelerations vector

class integrator:
  def __init__(self):
    # slope reconstructed from z-accel and x-speed
    self.sum = np.zeros(2).astype(np.float64)
    # for slope DC remove
    self.sum_prev = np.zeros(2).astype(np.float64)
    # DC removal
    self.dc = np.zeros(2).astype(np.float64)
    self.dc_remove_step = 0.0 # 0 disables DC remove
    self.dc_remove_factor = 0.0 # 0 disables proportional sum steps
    self.travel_sampling = 0.0 # [m] for sampling_interval triggering
    self.reset(0,0)

  def reset(self, azl0:float, azr0:float):
    self.sum *= 0
    self.sum_prev *= 0
    self.dc[0] = azl0
    self.dc[1] = azr0

  def sum_dc_remove(self):
    for i in range(0,2):
      if self.sum[i] > 0 and self.sum[i] > self.sum_prev[i]:
        self.dc[i] += self.dc_remove_step # + self.dc_remove_factor*(self.sum[i]-self.dc[i])
      if self.sum[i] < 0 and self.sum[i] < self.sum_prev[i]:
        self.dc[i] -= self.dc_remove_step # - self.dc_remove_factor*(self.sum[i]-self.dc[i])
      self.sum_prev[i] = self.sum[i]

  # slope reconstruction from equal-time sampled z-accel and vehicle x-speed
  # updates slope[0] = left, slope[1] = right
  def az2sum(self, azl:float, azr:float, c:float):
    self.sum[0] += azl * c
    self.sum[1] += azr * c

  # integrate z-acceleration in time domain 
  # updates slope in z/x space domain
  # for slope : c = a_sample_dt/vx
  # for height: c = vx/a_sample_dt
  # needs x-speed as input 
  # (vx = vehicle speed at the time when azl,azr accel are measured)
  # for small vx model is inaccurate. at vx=0 division by zero
  # returns 1 when slope is ready (each sampling_interval), otherwise 0
  def enter_sum(self, azl:float, azr:float, c:float, vx:float)->int:
    self.az2sum(azl, azr, c)
    self.travel_sampling += vx * a_sample_dt
    if self.travel_sampling > sampling_length:
      self.travel_sampling -= sampling_length
      return 1
    return 0

# adjust aci and hci dc_remove_step
# for waveform to "look better" and have
# insignificant contribution to result
# from wav2kml
aci = integrator() # accel->slope
aci.reset(9.81,9.81)
aci.dc_remove_step=1E-4

hci = integrator() # slope->height
hci.reset(0,0)
hci.dc_remove_step=1E-6
# 1E-5 looks better but contributes +5% to makecircle

gps_list = list()
# ((1234, "2021T15Z", (16,44), 80.0, 90.0), ...)
# named constant indexes in gps list
gps_seek      = 0
gps_datetime  = 1
gps_lonlat    = 2
gps_speed_kmh = 3
gps_heading   = 4
gps_iril      = 5
gps_irir      = 6

outfile = "/tmp/true.wav"
print("output", outfile)
o = open(outfile, "wb")
hdr  = b"RIFF" + bytearray([0x00, 0x00, 0x00, 0x00]) # chunk size bytes (len, including hdr), file growing, not yet known
hdr += b"WAVE"
# subchunk1: fmt
hdr += b"fmt " + bytearray([
    0x10, 0x00, 0x00, 0x00, # subchunk 1 size 16 bytes
    0x01, 0x00, # audio format = 1 (PCM)
    0x06, 0x00, # num channels = 6
    0xE8, 0x03, 0x00, 0x00, # sample rate = 1000 Hz
    0xE0, 0x2E, 0x00, 0x00, # byte rate = 12*1000 = 12000 byte/s
    0x0C, 0x00, # block align = 12 bytes
    0x10, 0x00]) # bits per sample = 16 bits
# subchunk2: data
hdr += b"data" + bytearray([0x00, 0x00, 0x00, 0x00]) # chunk size bytes (len), file growing, not yet known
if len(hdr) != 44:
  print("wrong wav header length=%d, should be 44" % len(hdr))
o.write(hdr)

for wavfile in argv[1:]:
  i = 0
  f = open(wavfile, "rb")
  seek = 44+0*12
  f.seek(seek)
  speed_kt    = 0.0
  speed_kmh   = 0.0
  should_reset_sum = 1
  nmea=bytearray(0)
  while f.readinto(mvb):
    seek += 12 # bytes per sample
    a=(b[0]&1) | ((b[2]&1)<<1) | ((b[4]&1)<<2) | ((b[6]&1)<<3) | ((b[8]&1)<<4) | ((b[10]&1)<<5)
    for j in range(0,6):
      ac[j] = int.from_bytes(b[j*2:j*2+2],byteorder="little",signed=True)//2*2 # //2*2 removes LSB bit (nmea tag)
    if calculate:
      if should_reset_sum:
        aci.reset(ac[wav_ch_l]*aint2float, ac[wav_ch_r]*aint2float)
        hci.reset(aci.sum[0],aci.sum[1])
        should_reset_sum = 0 # consumed
      if speed_kmh > 1: # TODO unhardcode
        if calculate == 1: # accelerometer
          #print(ac[wav_ch_l],ac[wav_ch_r])
          # NOTE *3.6/speed_kmh and *speed_kmh/3.6 may
          # cancel out as approximation
          # but if canceled, math is not exactly the same
          # because speed slowly changes. Before canceling
          # speed_kmh/3.6 do testing with real measurements
          if aci.enter_sum(ac[wav_ch_l]*aint2float-aci.dc[0],
                           ac[wav_ch_r]*aint2float-aci.dc[1],
                           a_sample_dt*3.6/speed_kmh,
                           speed_kmh/3.6):
            aci.sum_dc_remove()
          # with DC remove, should we subtract hci.acl0
          if hci.enter_sum(aci.sum[0]-hci.dc[0],
                           aci.sum[1]-hci.dc[1],
                           a_sample_dt*speed_kmh/3.6,
                           speed_kmh/3.6):
            hci.sum_dc_remove()
          #print("aci",aci.sum,"hci",hci.sum)
        if calculate == 3: # laser height measurement
          # FIXME write new code for height with complensation
          # use slope from wav2kml, integrate, make DC removal
          # laser accelerometer is for slope compensation
          if aci.enter_sum(ac[wav_ch_l]*aint2float - aci.dc[0],
                           ac[wav_ch_r]*aint2float - aci.dc[1],
                           a_sample_dt*3.6/speed_kmh,
                           speed_kmh/3.6):
            aci.sum_dc_remove()
          # direct height doesn't need DC removal
          # generates slope with already removed DC
          # see usage of this code in wav2kml.py
          if enter_height(ac[wav_ch_hl]*hint2float,
                          ac[wav_ch_hr]*hint2float,
                          speed_kmh/3.6):
            pass
            # enter compensated slope (difference)
            #enter_slope(slope_h[0]-slope[0],slope_h[1]-slope[1])

    if a != 32:
      c = a
      # convert control chars<32 to uppercase letters >=64
      if((a & 0x20) == 0):
        c ^= 0x40
      if a != 33:
        nmea.append(c)
    else: # a == 32
      if(len(nmea)):
        if nmea.find(b'#') >= 0: # discontinuety, reset travel
          speed_kt = 0.0
          speed_kmh = 0.0
          if calculate:
            should_reset_sum = 1
        elif (len(nmea)==79 or len(nmea)==68) and nmea[0:6]==b"$GPRMC" and nmea[-3]==42: # 68 is lost signal, tunnel mode
         # nmea[-3]="*" checks for asterisk on the right place, simple 8-bit crc follows
         crc = reduce(xor, map(int, nmea[1:-3]))
         hexcrc = bytearray(b"%02X" % crc)
         if nmea[-2:] == hexcrc:
          #print(nmea.decode("utf-8"))
          if len(nmea)==79: # normal mode with signal
            tunel = 0
          elif len(nmea)==68: # tunnel mode without signal, keep heading
            tunel = 11 # number of less chars in shorter nmea sentence for tunnel mode
          if tunel == 0:
            heading=float(nmea[54:59])
            speed_kt=float(nmea[47:53])
          speed_kmh=speed_kt*1.852
          #print(speed_kmh,"kmh")
      # delete, consumed
      nmea=bytearray(0)
    if calculate:
      # generate new sample and write to output
      # replace Y with true profile
      # left  Y: ac[1]
      # right Y: ac[4]
      # 0xFFFF casts to uint16_t (16-bit unsigned)
      hl=int(float2hint*hci.sum[0]) & 0xFFFF
      hr=int(float2hint*hci.sum[1]) & 0xFFFF
      # if casts to int16_t (16-bit signed)
      if(hl>=0x8000): hl-=0x10000
      if(hr>=0x8000): hr-=0x10000
      # ac[] accepts only 16-bit signed
      ac[out_wav_ch_hl]=hl
      ac[out_wav_ch_hr]=hr
      # reset X to 0
      # fictional "laser" has 0 acceleration
      # with accel 0 it will be "fixed" to some
      # point relative to sea level
      # left  X: ac[0]
      # right Y: ac[3]
      ac[out_wav_ch_l]=0
      ac[out_wav_ch_r]=0
    # copy text tags from old sample to new
    sample = bytearray(struct.pack("<hhhhhh", 
      ac[0], ac[1], ac[2],
      ac[3], ac[4], ac[5],
    ))
    # mix old text tags into new sample
    # print(ac)
    for j in range(6):
      sample[2*j]=(sample[2*j]&0xFE)|(mvb[2*j]&1)
    o.write(sample)
    i += 1
  f.close()
