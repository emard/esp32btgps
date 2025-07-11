#!/usr/bin/env python3

import kml

kml.red_iri=2.5
print(kml.header(name="name"),end="")
print(kml.line(value=0.7,left20=1.0,right20=1.1,left100=1.1,right100=1.2,speed=50,lon1=16.000000,lat1=46.000000,lon2=16.000100,lat2=46.000100,timestamp="2025-01-01T10:00:01.0Z"),end="")
print(kml.arrow(value=0.8,left=1.0,left_stdev=0.1,right=2.0,right_stdev=0.2,n=5,speed_min=50,speed_max=60,lon=16.000100,lat=46.000100,heading=30,timestamp="2025-01-01T10:00:02.0Z"),end="")
print(kml.line(value=1.6,left20=1.0,right20=1.1,left100=1.1,right100=1.2,speed=50,lon1=16.000100,lat1=46.000100,lon2=16.000100,lat2=46.000200,timestamp="2025-01-01T10:00:03.0Z"),end="")
print(kml.arrow(value=1.7,left=1.0,left_stdev=0.1,right=2.0,right_stdev=0.2,n=5,speed_min=50,speed_max=60,lon=16.000100,lat=46.000200,heading=0,timestamp="2025-01-01T10:00:04.0Z"),end="")
print(kml.footer(lon=16,lat=46,heading=0,begin="2025-01-01T10:00:00.0Z",end="2025-01-01T11:00:00.0Z"),end="")
