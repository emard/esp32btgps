#!/usr/bin/env python3
import goldencar
# import math
from sympy import *
# set some vars that take part in following symbolc function and differentiation
x,y,z = symbols("x y z", real=True)

# profile x [m] -> z [m]
def fpath_sympy(x):
  # this makes profile with IRI=1 m/km
  return \
    +1.07e-3*sin(2*pi*0.2*x)

d1fpath_sympy = diff(  fpath_sympy(x),x) # 1st derivative
# lambdify: convert symbolic function to global function suitable for numerical evaluation
fpath   = lambdify(x,   fpath_sympy(x))
# 1st derivative, done symbolic and converted similar as above
d1fpath = lambdify(x, d1fpath_sympy)

pathlen_m = 1000 # [m] calculate IRI after this length
average_last_m = 100 # [m]

goldencar.st_pr(DX = 0.05, K1 = 653.0, K2 = 63.3, MU = 0.15, C = 6.0)
goldencar.reset_iri()
n_points = int(pathlen_m/goldencar.sampling_length)
print("sampling points:",n_points,"every:",goldencar.sampling_length*100,"cm")
for i in range(n_points):
  slope=d1fpath(i*goldencar.sampling_length)
  goldencar.enter_slope(slope,slope)
print("IRI",goldencar.iri())
