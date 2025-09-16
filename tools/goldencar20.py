import numpy as np

# IRI averaging length (m)
iri_length = 20

# equal-distance slope sampling length (m)
sampling_length = 0.05

# number of buffered vz speed points for IRI averaging
n_buf_points = int(iri_length/sampling_length + 0.5)
n_buf_points20 = n_buf_points//5 # 20 m buf points
rvz = np.zeros(2).astype(np.uint32)# vz speed contains values scaled as unsigned integer (um/s)
# dimension 2 is for 0=left 1=right index
rvz_buf = np.zeros((n_buf_points,2)).astype(np.uint32)
rvz_buf_ptr = 0 # buffer pointer, runs 0 .. n_buf_points-1, next wraparound to 0
# sum of rectified speeds in um/s, dimension 2 is for 0=left, 1=right
srvz = np.zeros(2).astype(np.uint32)
srvz20 = np.zeros(2).astype(np.uint32)
# slope reconstructed from z-accel and x-speed
slope = np.zeros(2).astype(np.float32)
# for slope DC remove
slope_prev = np.zeros(2).astype(np.float32)

# Z state matrix left,right (used for iterative slope entry at each sampling interval)
ZL = np.zeros(4).astype(np.float32)
ZR = np.zeros(4).astype(np.float32)

# ready calculated HSRI-type golden car response coefficients for DX = 0.05 m
ST = np.array([
 [ 9.99847591e-01,  2.23521003e-03,  1.03813734e-04,  1.47621931e-05 ],
 [-1.33122370e-01,  9.87026870e-01,  6.88576102e-02,  1.29245874e-02 ],
 [ 1.01390202e-03,  9.84146318e-05,  9.88310337e-01,  2.14351248e-03 ],
 [ 8.84141088e-01,  8.61639306e-02, -1.02155657e+01,  9.03160334e-01 ]
]).astype(np.float32)
PR = np.array(
 [ 4.85082633e-05,  6.42647498e-02,  1.06757468e-02,  9.33142468e+00 ]
).astype(np.float32)


# calculate HSRI-type golden car response coefficients matrix ST, PR
def st_pr(DX = 0.05, K1 = 653.0, K2 = 62.3, MU = 0.15, C = 6.0):
  global ST, PR
  # DX =   0.05  # m sampling_length (for 0.05 m it takes 2000 points per 100 m)
  # HSRI-type golden car:
  # K1 = 653.0   # tyre stiffness per vehicle body mass
  # K2 =  62.3   # shock absorber spring stiffness per vehicle body mass (wtp-46 uses 63.3)
  # MU =   0.15  # tyre mass per vehicle body mass
  # C  =   6.0   # shock absorber damping coefficient per vehicle body mass
  # BPR-type golden car:
  # K1 = 643.0   # tyre stiffness per vehicle body mass
  # K2 = 128.7   # shock absorber spring stiffness per vehicle body mass
  # MU =   0.162 # tyre mass per vehicle body mass
  # C  =   3.86  # shock absorber damping coefficient per vehicle body mass
  V  = 80/3.6 # m/s standard speed 80 km/h (don't touch)
  T  = DX/V   # seconds time step per sample interval at std speed
  A = np.array(
    [[   0    ,  1    ,       0     ,  0    ],
     [ -K2    , -C    ,      K2     ,  C    ],
     [   0    ,  0    ,       0     ,  1    ],
     [  K2/MU ,  C/MU , -(K1+K2)/MU , -C/MU ]]).astype(np.float32)
  B = np.array(
     [   0    ,  0    ,       0     , K1/MU ]).astype(np.float32)
  # Calculate the state transition matrix 'ST' using a Taylor
  # Series expansion until required precision is reached.
  # ST = e^(A*dt) -> taylor expansion:
  # ST = I + A*dt + A^2*dt^2/2! + A^3*dt^3/3! + ...
  # start from identity matrix, 1 at diagonal, rest 0
  ST = np.identity(4).astype(np.float32)
  An = np.identity(4).astype(np.float32)
  for n in range(1,36): # An should converge to 0 when divided by 35! = 1e40
    An = np.matmul(An, A) * T / n # A^n * dt^n / n!
    ST += An
  # Calculate A matrix inversion A^-1
  Ainv = np.linalg.inv(A)
  #print("Ainv")
  #print(Ainv)
  # Calculate the Partial Response matrix.
  # PR = A^-1 * (ST - I) * B
  PR = np.matmul(np.matmul(Ainv, (ST - np.identity(4))), B)
  #print("ST (State Transition matrix):")
  #print(ST)
  #print("PR (Partial Response matrix):")
  #print(PR)

def new_sampling_length():
  # matrix is already calculated for sampling_length = 0.05 m
  # for different sampling_lenghth, calculate new matrix:
  st_pr(DX=sampling_length)

# matrix is already calculated for sampling_length = 0.05 m
# for different sampling_lenghth, calculate new matrix:
if sampling_length != 0.05:
  st_pr(DX=sampling_length)

# enter slope (dimensionless, m vertical per m horizontal) at sampling interval DX
# into a model that simulates vehicle response
# ST 4x4 state transition matrix
# ZL, ZR 4-dim state space vectors
# PR 4-dim partial response vector
def slope2model(slope_l:float, slope_r:float):
  global ZL, ZR
  ZL = np.matmul(ST, ZL) + PR * slope_l
  ZR = np.matmul(ST, ZR) + PR * slope_r
  # return (ZL[0] - ZL[2], ZR[0] - ZR[2]) # shock absorber speed

# initialization before first data entry
# usually called at stops because slope is difficult to keep after the stop
def reset_iri():
  global ZL, ZR, rvz_buf, rvz_buf_ptr, srvz, srvz20, slope, slope_prev, azl0, azr0
  global phi, theta, psi, prev_phi, prev_theta, prev_psi, dc_p, dc_q, dc_r
  # multiply all matrix elements with 0 resets them to 0
  ZL *= 0
  ZR *= 0
  srvz *= 0
  srvz20 *= 0
  rvz_buf *= 0
  rvz_buf_ptr = 0
  slope *= 0
  slope_prev *= 0

# enter slope, calculate running average
# slope = dz/dx (tangent)
def enter_slope(slope_l:float, slope_r:float):
  global rvz, rvz_buf, rvz_buf_ptr, srvz, srvz20
  slope2model(slope_l, slope_r) # updates ZL, ZR
  # scale shock absorber speed to integer um/s
  rvz[0] = int(abs(1.0e6 * (ZL[0]-ZL[2])))
  rvz[1] = int(abs(1.0e6 * (ZR[0]-ZR[2])))
  # running average
  srvz += rvz - rvz_buf[rvz_buf_ptr] # subtract from sum old data 100 m before
  srvz20 += rvz - rvz_buf[(rvz_buf_ptr+n_buf_points-n_buf_points20)%n_buf_points]
  rvz_buf[rvz_buf_ptr] = rvz # new data
  # next pointer with wraparound
  rvz_buf_ptr += 1
  if rvz_buf_ptr >= n_buf_points:
    rvz_buf_ptr = 0

# returns IRI in [m/m] calculated for iri_length (default 100 [m])
def iri():
  # srvz is in micro-units, 1000 scales to milli-units
  return srvz/(n_buf_points*1000)
