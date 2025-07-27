# simple kml generator
red_iri=2.5

def header(name:str,version:str)->str:
  # float colors to colorize IRI values
  red      =         red_iri
  orange   = 2.0/2.5*red_iri
  green    = 1.5/2.5*red_iri
  cyan     = 1.0/2.5*red_iri
  blue     = 0.5/2.5*red_iri
  violet   = 0.3/2.5*red_iri
  magenta  = 0.0
  return "\
<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n\
  <Document id=\"docid\">\n\
    <name>%22s</name>\n\
    <description>\
<![CDATA[\n\
Speed-time 100 m segment cuts with statistics.<br/>\n\
Click any point on the track to display mm/m value of a 100 m\n\
segment measured before the point. Value represents average\n\
rectified speed in the shock absorber over 100 m segment\n\
and divided by standard speed of 80 km/h. Value comes from the\n\
numeric model that calculates response at standard speed,\n\
removing dependency on actual speed at which measurement has been done.<br/>\n\
<br/>\n\
Color codes: \
<font color=\"red\">%.1f</font>, <font color=\"orange\">%.1f</font>, <font color=\"green\">%.1f</font>, <font color=\"cyan\">%.1f</font>, \
<font color=\"blue\">%.1f</font>, <font color=\"violet\">%.1f</font>, <font color=\"magenta\">%.1f</font><br/>\n\
<br/>\n\
Click arrow to display statistics as<br/>\n\
average ± uncertainty<br/>\n\
where \"uncertainty\" represents 2σ = 95%% coverage.\n\
<br/>\n\
<font color=\"white\">%s</font>\n\
]]>\n\
    </description>\n\
    <visibility>1</visibility>\n\
    <Folder id=\"lines\">\n\
      <name>Lines</name>\n\
      <description>Lines with measurement details</description>\n\
      <visibility>1</visibility>\n\
" % (name, red, orange, green, cyan, blue, violet, magenta, version)

def arrow(value:float,left:float,left_stdev:float,right:float,right_stdev:float,n:int,speed_min:int,speed_max:int,lon:float,lat:float,heading:float,timestamp:str)->str:
  color_int=color32(int(1024*value/red_iri))
  icon_heading=((int(10*heading)+1800)%3600)/10.0
  return "\
      <Placemark id=\"id\">\n\
        <name>%5.2f</name>\n\
        <description>\
L100=%5.2f ± %5.2f mm/m\n\
R100=%5.2f ± %5.2f mm/m\n\
n=%2d \n\
v=%3d-%3d km/h\n\
        </description>\n\
        <visibility>1</visibility>\n\
        <Style>\n\
          <IconStyle id=\"id\">\n\
            <color>%08X</color>\n\
            <scale>1.0</scale>\n\
            <heading>%05.1f</heading>\n\
            <Icon>\n\
              <href>http://maps.google.com/mapfiles/kml/shapes/arrow.png</href>\n\
            </Icon>\n\
          </IconStyle>\n\
        </Style>\n\
        <TimeStamp>\n\
          <when>%22s</when>\n\
        </TimeStamp>\n\
        <Point>\n\
          <coordinates>%+011.6f,%+010.6f</coordinates>\n\
        </Point>\n\
      </Placemark>\n\
" % (value,left,left_stdev,right,right_stdev,n,speed_min,speed_max,color_int,icon_heading,timestamp,lon,lat)

def line(value:float,left20:float,right20:float,left100:float,right100:float,speed:float,lon1:float,lat1:float,lon2:float,lat2:float,timestamp)->str:
  color_int=color32(int(1024*value/red_iri))
  return "\
      <Placemark id=\"id\">\n\
        <name>%5.2f</name>\n\
        <description>\
L20=%5.2f mm/m\n\
R20=%5.2f mm/m\n\
L100=%5.2f mm/m\n\
R100=%5.2f mm/m\n\
v=%5.1f km/h\n\
%22s\n\
        </description>\n\
        <visibility>1</visibility>\n\
        <Style>\n\
          <LineStyle>\n\
            <color>%08X</color>\n\
            <width>6</width>\n\
          </LineStyle>\n\
        </Style>\n\
        <TimeStamp>\n\
          <when>%22s</when>\n\
        </TimeStamp>\n\
        <LineString>\n\
          <coordinates>%+011.6f,%+010.6f %+011.6f,%+010.6f</coordinates>\n\
        </LineString>\n\
      </Placemark>\n\
" % (value,left20,right20,left100,right100,speed,timestamp,color_int,timestamp,lon1,lat1,lon2,lat2)


def footer(lon:float, lat:float, heading:float, begin:str, end:str)->str:
  return "\
    </Folder>\n\
    <LookAt>\n\
      <longitude>%+011.6f</longitude>\n\
      <latitude>%+010.6f</latitude>\n\
      <heading>%05.1f</heading>\n\
      <tilt>0</tilt>\n\
      <range>2000</range>\n\
      <altitudeMode>relativeToGround</altitudeMode>\n\
      <TimeSpan>\n\
        <begin>%22s</begin>\n\
        <end>%22s</end>\n\
      </TimeSpan>\n\
    </LookAt>\n\
  </Document>\n\
</kml>\n\
" % (lon, lat, heading, begin, end)

# color 0-1023 magenta-red
def color32(color:int)->int:
# HSV to RGB conversion formula
# When 0 ≤ H < 360, 0 ≤ S ≤ 1 and 0 ≤ V ≤ 1:
# C = V × S
# X = C × (1 - |(H / 60°) mod 2 - 1|)
# m = V - C
# (R',G',B') = (C,X,0)   0<=H<60
# (R',G',B') = (X,C,0)  60<=H<120
# (R',G',B') = (0,C,X) 120<=H<180
# (R',G',B') = (0,X,C) 180<=H<240
# (R',G',B') = (X,0,C) 240<=H<300
# (R',G',B') = (C,0,X) 300<=H<360
# (R,G,B) = ((R'+m)×255, (G'+m)×255, (B'+m)×255)
# H=(1.0001-color)*0.8332
# S=1
# V=1
# -----
# C=1
# X=1 - |(H / 60°) mod 2 - 1|
# m=0
# (R,G,B) = ((R'+m)×255, (G'+m)×255, (B'+m)×255)
  if color <    0: color = 0
  if color > 1023: color = 1023
  H = ((1023 - color)*1280)>>10 # 1280=0.8332*6*256 -> 0<H<6*256
  X =   256 - abs(H % 512 - 256)
  if X > 255: X = 255
  c32 = 0 # 0xF0BBGGRR
  #                       R      G          B
  if   H<1*256:   c32 = 255 | (  X<<8)
  elif H<2*256:   c32 =   X | (255<<8)
  elif H<3*256:   c32 =       (255<<8) | (  X<<16)
  elif H<4*256:   c32 =       (  X<<8) | (255<<16)
  elif H<5*256:   c32 =   X            | (255<<16)
  else        :   c32 = 255            | (  X<<16) # H<6*256
  return c32 | 0xF0000000
