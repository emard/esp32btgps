function y = fpath(x)
% wave function: x = fpath(y)
% x,y are in meters [m]

% each example makes a profile with IRI=1 mm/m:

% y = +0.92e-3*sin(2*pi*x);
% clean sine 1 m wavelength
% without 5-point average makes IRI = 1
% with 5-point average makes IRI = 0.9

y = +1.07e-3*sin(2*pi*0.2*x);
% clean sine 5 m wavelength

% y = +2.116e-3*sin(2*pi*0.09*x);
% clean sine 11.111 m wavelength

% y = +2.1e-3*sin(2*pi*0.09*x+0.01/0.0111111*sin(2*pi*0.0111111*x));
% wobbling sine around 11.111 m wavelength
% along pathlen_m IRI varies around 1.00+-0.05 with period 90 m

% y = +2.1e-3*sin(2*pi*0.09*x+0.01/0.0111111*sin(2*pi*0.0111111*x)) +0.1e-3*sin(2*pi*4.50*x+0.00/0.0526315*sin(2*pi*0.0526315*x));
% 0.1e-3*sin() has negligible contribution
% superposition of two wobbling sines:
% first wobbles around 11.111 m wavelength and second wobbles around 0.222222 m wavelength
% along pathlen_m IRI varies around 1.00+-0.05 with period 90 m

end % function
