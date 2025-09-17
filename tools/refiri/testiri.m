#!/usr/bin/octave

% reads profile as complex vector (x,y*i)
p=csvread("low_dx_50mm.txt");
x_m=real(p); % real part is x-vector in m
y_mm=imag(p); % complex part is y-vector in mm
dx=x_m(2)-x_m(1); % diff of first 2 elements is dx, assuming all the rest are the same
L=20; % [m]
N=round(L/dx);
M=5; % internal iri()'s running average of 5 points
% dx=x_m(1234)-x_m(1233); % should be the same
% invoking iri() loads file iri.m and uses function iri(prof,dx)
iri_value=iri(y_mm,dx);
disp("x[m]   IRI20[mm/m]");
disp("------------------");
for i = N:N:length(x_m)
  % disp([x_m(i),iri_value(i-5)]);
  % -M because there is moving sum of M values each 5 cm spaced - 5 makes avg of 25 cm
  N1=N; % N1 is similar to N but avoids first negative index
  if i == N N1=N-M; end
  printf('%7.2f %10.6f\n', x_m(i), sum(iri_value(i-(N1+M-1):i-M))/N1);
  % disp([i-(N1+M-1) i-M N1]);
end
%disp(length(x_m))
%disp(length(iri_value))
