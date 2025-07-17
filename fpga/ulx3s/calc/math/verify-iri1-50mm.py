#!/usr/bin/env python3

# verification:
# generating profile from WTP-46
# and getting identical results

# calculates z-acceleration over the profile
# calculates slope from z-acceleration and
# verifies that IRI can be calculated from
# the slope with very high precision.

from matplotlib.pylab import *  # pylab is the easiest approach to any plotting
import numpy as np

# import local vehicle response analyzer package
import goldencar

from sympy import *

# set some vars that take part in following symbolc function and differentiation
x,y,z = symbols("x y z", real=True)

# profile x [m] -> z [m]
def fpath_sympy(x):
  # this makes profile with IRI=1 m/km
  return \
    +1.08e-3*sin(2*pi*0.2*x)

#    +0.921e-3*sin(2*pi*x)
# clean sine 1 m wavelength

#    +1.08e-3*sin(2*pi*0.2*x)
# clean sine 5 m wavelength

#    +2.15e-3*sin(2*pi*0.09*x)
# clean sine 11.111 m wavelength

#    +2.13e-3*sin(2*pi*0.09*x+0.02/0.0111111*sin(2*pi*0.0111111*x))
# wobbling sine around 11.111 m wavelength

#    +2.12e-3*sin(2*pi*0.09*x+0.02/0.0111111*sin(2*pi*0.0111111*x))
#    +0.50e-3*sin(2*pi*4.50*x+0.10/0.0526315*sin(2*pi*0.0526315*x))  # negligible contribution
# superposition of two wobbling sines:
# first wobbles around 11.111 m wavelength and second wobbles around 0.222222 m wavelength


d1fpath_sympy = diff(  fpath_sympy(x),x) # 1st derivative
d2fpath_sympy = diff(d1fpath_sympy   ,x) # 2nd derivative

# lambdify: convert symbolic function to global function suitable for numerical evaluation
fpath   = lambdify(x,   fpath_sympy(x))
# 1st derivative, done symbolic and converted similar as above
d1fpath = lambdify(x, d1fpath_sympy)

# ----------- generate test profile from d1fpath(x)
#points = 401
#dx = 0.25 # [m]
points = 2001
dx = 0.05 # [m]
x = np.zeros(points).astype(np.float32)
for i in range(0,points):
  x[i] = i * dx
y = np.zeros(points).astype(np.int32)
# fill slopes from fpath 1st derivative
slope_int_scale = 1.0E6 # convert [m/m] -> [um/m]
for i in range(0,points):
  y[i] = int(d1fpath(i * dx) * slope_int_scale)

iri = np.zeros(points).astype(np.float32)

# ----------- instantiate goldencar model
roughness = goldencar.response(sampling_interval = dx, iri_length = 100.0, buf_size = points)

# ---- enter profile to goldencar model and print results
#for i in range(1,points):
print("Z")
print(roughness.Z);
print("Z1")
print(roughness.Z);
print("")
for i in range(1,points):
  yp = y[i]
  print(yp)
  #yp = -0x1000
  roughness.enter_slope(yp)
  iri[i] = roughness.IRI() * 1000 # [mm/m], roughness.IRI() is in [m/m]
  print("%3d %6.2f YP=0x%08X IRI=%9.5f mm/m VZ=0x%08X" % (i,x[i],yp,iri[i], np.uint32(roughness.Z[0]-roughness.Z[2]) ))
  print("Z=[ 0x%08X 0x%08X 0x%08X 0x%08X ]" % (np.uint32(roughness.Z[0]), np.uint32(roughness.Z[1]), np.uint32(roughness.Z[2]), np.uint32(roughness.Z[3])))
  print(roughness.Z);
  print("")
  #print(i,"z",z[i],"iri",iri[i])
  