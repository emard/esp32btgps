#!/usr/bin/octave

dx=0.05; % [m] sampling step
% dx=x_m(1234)-x_m(1233); % should be the same
%L=20; % [m] for IRI20
L=100; % [m] for IRI100
%L=500; % [m] for IRI500
N=round(L/dx); % number of 5-cm points to average for IRI
% evaluate fpath function to generate input data
for i = 1:N
  x_m(i)=i*dx;
  y_mm(i)=1000*fpath(x_m(i)); % [m] -> [mm]
end

% invoking iri() loads file iri.m and uses function iri(prof,dx)
iri_value=iri(y_mm,dx);
% internal iri()'s running average of 5 points for 25-cm average
% output vector "iri_value" is M shorter than input vector "y_mm":
% M=5 actually
M=length(y_mm)-length(iri_value)
printf('x[m]   IRI%d[mm/m]\n', L);
printf('-------------------\n');
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
