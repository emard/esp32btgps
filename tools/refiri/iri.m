function irivec = iri(prof, dx)
%IRI Calculate the IRI from a height input profile.
% irivec = iri(prof, dx)
% 'prof' is given in millimeters and 'dx' in meters.

% Set parameters.
base = 0.25;
xinit = 11;

% Physical Constants Of Quarter Car.
K1 = 653.0;  % kt/ms
K2 = 63.3;   % ks/ms
U  = 0.15;   % mu/ms
C  = 6.0;    % cs/ms
V  = 80/3.6; % m/s   speed

% The wheel-road contact moving average distance.
ibase = max(round(base/dx), 1);

% Initialize simulation variables.
ilead = min(round(xinit/dx) + 1, length(prof));
X = [(prof(ilead) - prof(1))/(dx*ilead) ;
                                      0 ;
     (prof(ilead) - prof(1))/(dx*ilead) ;
                                      0];

% The slope profile.
sprof = (prof((ibase + 1):(length(prof))) - prof(1:(length(prof) - ibase)))./(dx*ibase);

% State Space Matrix 'A'.
A = [ -0,   1,            0,    0  ;
     -K2,  -C,           K2,    C  ;
      -0,  -0,            0,    1  ;
   +K2/U, C/U, -(K2 + K1)/U, -C/U ];

% State Space Matrix 'B'.
B = [0; 0; 0; K1/U];

% Calculate the State Transition matrix 'ST' and the Partial Response vector 'PR'.
ST = expm(A*dx/V);
PR = inv(A)*(ST - eye(4))*B;

% Initialize the IRI vector.
irivec = zeros(length(sprof), 1);

% Calculate the IRI.
for i = 1:length(sprof)
  % Calculate the new state vector.
  X = ST*X + PR*sprof(i);
  irivec(i) = abs(X(3) - X(1));
end % for
% TODO running average of irivec to get IRI20
end % function
