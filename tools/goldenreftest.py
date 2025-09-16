#!/usr/bin/env python3
import goldencar20 as goldencar

goldencar.st_pr(DX = 0.05, K1 = 653.0, K2 = 63.3, MU = 0.15, C = 6.0)
goldencar.reset_iri()
prev_x_m = 0
prev_mov_avg_y_mm = 0
mov_avg=[0.0,0.0,0.0,0.0,0.0] # 5x50mm for 250mm moving average
mov_avg_i=0 # 0..4
report_every_x_m = 20
report_prev_x_m = report_every_x_m-0.001
with open("refiri/high_dx_50mm.txt","r") as file:
  for line in file:
    lstrip = line.strip()
    lsplit = lstrip.split("\t")
    x_m = float(lsplit[0]) # [m]
    y_mm = float(lsplit[1]) # [mm]
    mov_avg[mov_avg_i]=y_mm
    mov_avg_i+=1
    if mov_avg_i>=len(mov_avg):
      mov_avg_i=0
    mov_avg_y_mm=sum(mov_avg)/len(mov_avg)
    slope=1.0E-3*(mov_avg_y_mm-prev_mov_avg_y_mm)/(x_m-prev_x_m)
    goldencar.enter_slope(slope,slope)
    if x_m >= report_prev_x_m:
      print("%7.2fm %14.6fmm s=%7.3fmm/m iri20=%6.2fmm/m" % (x_m,y_mm,1000*slope,goldencar.iri()[0]))
      report_prev_x_m += report_every_x_m
    prev_x_m,prev_mov_avg_y_mm = x_m,mov_avg_y_mm
