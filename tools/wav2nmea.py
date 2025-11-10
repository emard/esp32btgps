#!/usr/bin/env python3

# ./wav2nmea.py 20210701.wav > 20210701.nmea

from sys import argv
from subprocess import Popen, PIPE
from functools import reduce
from operator import xor

# buffer to read wav
b=bytearray(12)
mvb=memoryview(b)

for wavfile in argv[1:]:
  if wavfile.lower().endswith(".wav"):
    f = open(wavfile, "rb")
  if wavfile.lower().endswith(".flac"):
    f = Popen(["flac", "--silent", "--decode", wavfile, "--stdout"], stdout=PIPE, shell=False, text=False).stdout
  i = 0
  seek = 44+0*12
  f.read(seek)
  nmea=bytearray(0)
  while f.readinto(mvb):
    seek += 12 # bytes per sample
    a=(b[0]&1) | ((b[2]&1)<<1) | ((b[4]&1)<<2) | ((b[6]&1)<<3) | ((b[8]&1)<<4) | ((b[10]&1)<<5)
    if a != 32:
      c = a
      # convert control chars<32 to uppercase letters >=64
      if((a & 0x20) == 0):
        c ^= 0x40
      if a != 33:
        nmea.append(c)
    else: # a == 32
      if(len(nmea)>4):
        crc = reduce(xor, map(int, nmea[1:-3]))
        hexcrc = bytearray(b"%02X" % crc)
        if nmea[-2:] == hexcrc:
          print(nmea.decode("utf-8"))
      # delete, consumed
      nmea=bytearray(0)
    i += 1
  f.close()
