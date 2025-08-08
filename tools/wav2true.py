#!/usr/bin/env python3

# ./wav2true.py 20210701.wav > 20210701-true.wav

from sys import argv
from functools import reduce
from operator import xor
import numpy as np

# calculate
# 0:IRI from wav tags,
# 1:IRI calculated from z-accel wav data (adxl355)
# 3:IRI calculated from y-height wav data (laser)
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
if calculate == 3: # laser vertical distance
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

# slope reconstructed from z-accel and x-speed
slope = np.zeros(2).astype(np.float32)
# for slope DC remove
slope_prev = np.zeros(2).astype(np.float32)
# slope calculated from laser height
slope_h = np.zeros(2).astype(np.float32)

# buffer to read wav
b=bytearray(12)
mvb=memoryview(b)

# equal-distance slope sampling length (m)
sampling_length = 0.05

# equal-time accelerometer sample time
a_sample_dt = 1/1000 # s (1kHz accelerometer sample rate)    

# enter_height() needs inverse sampling length
inv_sampling_length = 1/sampling_length

# raw values reading (accelerometer or gyro) ac =
ac = np.zeros(6).astype(np.int16) # current integer accelerations vector

# accelerometer DC remove
azl0 = 0.0
azr0 = 0.0
dc_azl = 0
dc_azr = 0
if calculate == 1:
  azl0 = 9.81 # average azl (to remove slope DC offset)
  azr0 = 9.81 # average azr (to remove slope DC offset)
  dc_azl = int(9.81/aint2float)
  dc_azr = int(9.81/aint2float)
if calculate == 3:
  azl0 = 9.81 # average azl (to remove slope DC offset)
  azr0 = 9.81 # average azr (to remove slope DC offset)
  dc_azl = int(9.81/aint2float)
  dc_azr = int(9.81/aint2float)
slope_dc_remove_count = 0

def slope_dc_remove():
  global azl0, azr0, slope_prev, slope_dc_remove_count
  global dc_azl, dc_azr
  #slope_dc_remove_count += 1
  #if slope_dc_remove_count < 129:
  #  return
  #slope_dc_remove_count = 0
  if slope[0] > 0 and slope[0] > slope_prev[0]:
    azl0 += dc_remove_step
    dc_azl += 1
  if slope[0] < 0 and slope[0] < slope_prev[0]:
    azl0 -= dc_remove_step
    dc_azl -= 1
  if slope[1] > 0 and slope[1] > slope_prev[1]:
    azr0 += dc_remove_step
    dc_azr += 1
  if slope[1] < 0 and slope[1] < slope_prev[1]:
    azr0 -= dc_remove_step
    dc_azr -= 1
  slope_prev[0] = slope[0]
  slope_prev[1] = slope[1]

# initialization before first data entry
# usually called at stops because slope is difficult to keep after the stop
# TODO rename reset_iri -> reset_stop
def reset_iri():
  global slope, slope_prev, azl0, azr0, dc_azl, dc_azr
  global prev_hyl, prev_hyr
  global slope_h
  # multiply all matrix elements with 0 resets them to 0
  slope *= 0
  slope_prev *= 0
  slope_h *= 0
  if calculate == 1:
    # reset DC compensation to current accelerometer reading
    azl0 = ac[wav_ch_l]*aint2float
    azr0 = ac[wav_ch_r]*aint2float
    dc_azl = ac[wav_ch_l]
    dc_azr = ac[wav_ch_r]
  if calculate == 3:
    # reset DC compensation to current accelerometer reading
    azl0 = ac[wav_ch_l]*aint2float
    azr0 = ac[wav_ch_r]*aint2float
    dc_azl = ac[wav_ch_l]
    dc_azr = ac[wav_ch_r]
    # reset laser height
    prev_hyl = prev_hyr = 0.0

# slope reconstruction from equal-time sampled z-accel and vehicle x-speed
# updates global slope[0] = left, slope[1] = right
def az2slope(azl:float, azr:float, c:float):
  global slope
  slope[0] += azl * c
  slope[1] += azr * c

# integrate z-acceleration in time domain 
# updates slope in z/x space domain
# needs x-speed as input 
# (vx = vehicle speed at the time when azl,azr accel are measured)
# for small vx model is inaccurate. at vx=0 division by zero
# returns 1 when slope is ready (each sampling_interval), otherwise 0
def enter_accel(azl:float, azr:float, vx:float)->int:
  global travel_sampling
  az2slope(azl, azr, a_sample_dt / vx)
  travel_sampling += vx * a_sample_dt
  if travel_sampling > sampling_length:
    travel_sampling -= sampling_length
    return 1
  return 0

# hyl, hyr: Y-distance [m]
# c = 1/dx [1/m] inverse sampling_length
def hy2slope(hyl:float, hyr:float, c:float):
  global prev_hyl, prev_hyr
  slope_h[0] = (hyl-prev_hyl)*c
  slope_h[1] = (hyr-prev_hyr)*c
  prev_hyl = hyl
  prev_hyr = hyr

# enter direct profile Z-height in time domain
# Z-height reading comes from Y channel (laser)
# updates slope in z/x space domain
# needs x-speed as input 
# (vx = vehicle speed at the time when hyl,hyr distances are measured)
# returns 1 when slope is ready (each sampling_interval), otherwise 0
# for small vx model is inaccurate. at vx=0 division by zero
def enter_height(hyl:float, hyr:float, vx:float)->int:
  global travel_sampling
  dx = vx * a_sample_dt
  travel_sampling += dx
  if travel_sampling > sampling_length:
    travel_sampling -= sampling_length
    hy2slope(hyl, hyr, inv_sampling_length)
    return 1
  return 0

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

for wavfile in argv[1:]:
  i = 0
  f = open(wavfile, "rb")
  seek = 44+0*12
  f.seek(seek)
  speed_kt    = 0.0
  speed_kmh   = 0.0
  should_reset_iri = 1
  nmea=bytearray(0)
  while f.readinto(mvb):
    seek += 12 # bytes per sample
    a=(b[0]&1) | ((b[2]&1)<<1) | ((b[4]&1)<<2) | ((b[6]&1)<<3) | ((b[8]&1)<<4) | ((b[10]&1)<<5)
    if calculate:
      for j in range(0,6):
        ac[j] = int.from_bytes(b[j*2:j*2+2],byteorder="little",signed=True)//2*2 # //2*2 removes LSB bit (nmea tag)
      if should_reset_iri:
        reset_iri()
        should_reset_iri = 0 # consumed
      if speed_kmh > 1: # TODO unhardcode
        if calculate == 1: # accelerometer
          if enter_accel(ac[wav_ch_l]*aint2float - azl0,
                         ac[wav_ch_r]*aint2float - azr0,
                         speed_kmh/3.6):
          #  enter_slope(slope[0],slope[1])
            slope_dc_remove()
        if calculate == 3: # laser height measurement
          # accelerometer still needs slope DC removal
          # use accelerometer to calculate slope compensation
          flag_dc_remove = enter_accel(ac[wav_ch_l]*aint2float - azl0,
                                       ac[wav_ch_r]*aint2float - azr0,
                                       speed_kmh/3.6)
          # direct height doesn't need DC removal
          # generates slope with already removed DC
          if enter_height(ac[wav_ch_hl]*hint2float,
                          ac[wav_ch_hr]*hint2float,
                          speed_kmh/3.6):
            xyz = 0
            # enter compensated slope (difference)
            #enter_slope(slope_h[0]-slope[0],slope_h[1]-slope[1])
          # after slope is entered, apply DC removal
          if flag_dc_remove:
            slope_dc_remove()

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
            should_reset_iri = 1
        elif (len(nmea)==79 or len(nmea)==68) and nmea[0:6]==b"$GPRMC" and nmea[-3]==42: # 68 is lost signal, tunnel mode
         # nmea[-3]="*" checks for asterisk on the right place, simple 8-bit crc follows
         crc = reduce(xor, map(int, nmea[1:-3]))
         hexcrc = bytearray(b"%02X" % crc)
         if nmea[-2:] == hexcrc:
           print(nmea.decode("utf-8"))
      # delete, consumed
      nmea=bytearray(0)
    i += 1
  f.close()
