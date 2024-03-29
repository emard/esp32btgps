#include <stdio.h>
#include <stdint.h> // uint32_t etc
#include <string.h>
#include <stdlib.h> // abs

/*
longitude -180..180 (x-direction, EW)
latitude   -90..90  (y-direction, NS)
heading      0..360
*/

FILE *fkml;
char  kmlbuf[8192]; // 4-8K for write efficiency
int kmlbuf_len = 0; // max write position
int kmlbuf_pos = 0; // current write position
uint8_t kmlbuf_has_arrow = 0;
float red_iri = 2.5;

struct s_kml_arrow
{
  float lon, lat, value, heading;
  char *timestamp, *description;
};
struct s_kml_arrow x_kml_arrow[1];

struct s_kml_line
{
  float lon1, lat1, lon2, lat2, value;
  char *timestamp, *description;
};
struct s_kml_line x_kml_line[1];

void kml_open(void)
{
  fkml = fopen("/tmp/demo.kml", "wb");
}

const char *str_kml_header = "\
<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n\
  <Document id=\"docid\">\n\
    <name>doc name</name>\n\
    <description>description</description>\n\
    <visibility>1</visibility>\n\
    <Folder id=\"id1\">\n\
      <name>name1</name>\n\
      <description>description1</description>\n\
      <visibility>1</visibility>\n\
";
int str_kml_header_len;

const char *str_kml_line = "\
      <Placemark id=\"id\">\n\
        <name>NAME5</name>\n\
        <description>DESCRIPTION1234567890123456789012345678901234567890\n\
        </description>\n\
        <visibility>1</visibility>\n\
        <Style>\n\
          <LineStyle>\n\
            <color>COLOR678</color>\n\
            <width>6</width>\n\
          </LineStyle>\n\
        </Style>\n\
        <TimeStamp>\n\
          <when>TIMESTAMP0123456789012</when>\n\
        </TimeStamp>\n\
        <LineString>\n\
          <coordinates>LONLAT789012345678901234567890123456789012345</coordinates>\n\
        </LineString>\n\
      </Placemark>\n\
";
int str_kml_line_pos_lonlat;
int str_kml_line_pos_time;
int str_kml_line_pos_name;
int str_kml_line_pos_color;
int str_kml_line_len;

const char *str_kml_arrow = "\
      <Placemark id=\"id\">\n\
        <name>NAME5</name>\n\
        <description>DESCRIPTION1234567890123456789012345678901234567890\n\
        </description>\n\
        <visibility>1</visibility>\n\
        <Style>\n\
          <IconStyle id=\"id\">\n\
            <color>COLOR678</color>\n\
            <scale>1.0</scale>\n\
            <heading>HEAD5</heading>\n\
            <Icon>\n\
              <href>http://maps.google.com/mapfiles/kml/shapes/arrow.png</href>\n\
            </Icon>\n\
          </IconStyle>\n\
        </Style>\n\
        <TimeStamp>\n\
          <when>TIMESTAMP0123456789012</when>\n\
        </TimeStamp>\n\
        <Point>\n\
          <coordinates>LONLAT7890123456789012</coordinates>\n\
        </Point>\n\
      </Placemark>\n\
";
int str_kml_arrow_pos_lonlat;
int str_kml_arrow_pos_heading;
int str_kml_arrow_pos_time;
int str_kml_arrow_pos_name;
int str_kml_arrow_pos_color;
int str_kml_arrow_len;

const char *str_kml_footer = "\
    </Folder>\n\
    <LookAt>\n\
      <longitude>LONGITUDE01</longitude>\n\
      <latitude>LATITUDE90</latitude>\n\
      <heading>HEAD5</heading>\n\
      <tilt>0</tilt>\n\
      <range>2000</range>\n\
      <altitudeMode>relativeToGround</altitudeMode>\n\
      <TimeSpan>\n\
        <begin>TIMEBEGIN9012345678901</begin>\n\
        <end>TIMEEND789012345678901</end>\n\
      </TimeSpan>\n\
    </LookAt>\n\
  </Document>\n\
</kml>\n\
";
int str_kml_footer_pos_longitude;
int str_kml_footer_pos_latitude;
int str_kml_footer_pos_heading;
int str_kml_footer_pos_begin;
int str_kml_footer_pos_end;
int str_kml_footer_len;

void kml_init(void)
{
  str_kml_header_len           = strlen(str_kml_header);

  str_kml_line_pos_lonlat      = strstr(str_kml_line, "LONLAT"    ) - str_kml_line;
  str_kml_line_pos_name        = strstr(str_kml_line, "NAME"      ) - str_kml_line;
  str_kml_line_pos_time        = strstr(str_kml_line, "TIMESTAMP" ) - str_kml_line;
  str_kml_line_pos_color       = strstr(str_kml_line, "COLOR"     ) - str_kml_line;
  str_kml_line_len             = strlen(str_kml_line);

  str_kml_arrow_pos_lonlat     = strstr(str_kml_arrow, "LONLAT"    ) - str_kml_arrow;
  str_kml_arrow_pos_heading    = strstr(str_kml_arrow, "HEAD"      ) - str_kml_arrow;
  str_kml_arrow_pos_name       = strstr(str_kml_arrow, "NAME"      ) - str_kml_arrow;
  str_kml_arrow_pos_time       = strstr(str_kml_arrow, "TIMESTAMP" ) - str_kml_arrow;
  str_kml_arrow_pos_color      = strstr(str_kml_arrow, "COLOR"     ) - str_kml_arrow;
  str_kml_arrow_len            = strlen(str_kml_arrow);

  str_kml_footer_pos_longitude = strstr(str_kml_footer, "LONGITUDE") - str_kml_footer;
  str_kml_footer_pos_latitude  = strstr(str_kml_footer, "LATITUDE" ) - str_kml_footer;
  str_kml_footer_pos_heading   = strstr(str_kml_footer, "HEAD"     ) - str_kml_footer;
  str_kml_footer_len           = strlen(str_kml_footer);

  str_kml_footer_pos_begin     = strstr(str_kml_footer, "TIMEBEGIN") - str_kml_footer;
  str_kml_footer_pos_end       = strstr(str_kml_footer, "TIMEEND"  ) - str_kml_footer;
  str_kml_footer_len           = strlen(str_kml_footer);

}

// init buffer for subsequent adding lines and arrows
// first entry is arrow (not always used)
// subsequent entries are lines until end of buffer
void kml_buf_init(void)
{
  strcpy(kmlbuf, str_kml_arrow);
  
  kmlbuf_len = str_kml_arrow_len; // bytes in the buffer
  char *a = kmlbuf+str_kml_arrow_len; // pointer to the boffer

  for(; kmlbuf_len < sizeof(kmlbuf)-str_kml_line_len-1; a += str_kml_line_len, kmlbuf_len += str_kml_line_len)
    strcpy(a, str_kml_line);
  
  kmlbuf_pos = str_kml_arrow_len; // write position past arrow, default to first line
}

void kml_header(void)
{
  strcpy(kmlbuf, str_kml_header);

  fwrite(kmlbuf, str_kml_header_len, 1, fkml);

}

// color 0-1023
uint32_t color32(uint16_t color)
{
/*
HSV to RGB conversion formula
When 0 ≤ H < 360, 0 ≤ S ≤ 1 and 0 ≤ V ≤ 1:
C = V × S
X = C × (1 - |(H / 60°) mod 2 - 1|)
m = V - C
(R',G',B') = (C,X,0)   0<=H<60
(R',G',B') = (X,C,0)  60<=H<120
(R',G',B') = (0,C,X) 120<=H<180
(R',G',B') = (0,X,C) 180<=H<240
(R',G',B') = (X,0,C) 240<=H<300
(R',G',B') = (C,0,X) 300<=H<360
(R,G,B) = ((R'+m)×255, (G'+m)×255, (B'+m)×255)
H=(1.0001-color)*0.8332
S=1
V=1
-----
C=1
X=1 - |(H / 60°) mod 2 - 1|
m=0
(R,G,B) = ((R'+m)×255, (G'+m)×255, (B'+m)×255)
*/
  if(color <    0) color = 0;
  if(color > 1023) color = 1023;
  uint16_t H = ((1023 - color)*1280)>>10; // 1280=0.8332*6*256 -> 0<H<6*256
  uint16_t X1 =   256 - abs(H % 512 - 256);
  uint8_t  X = X1 > 255 ? 255 : X1;
  uint32_t c32; // 0xF0BBGGRR
  //                         R      G          B
  if     (H<1*256)   c32 = 255 | (  X<<8)            ;
  else if(H<2*256)   c32 =   X | (255<<8)            ;
  else if(H<3*256)   c32 =       (255<<8) | (  X<<16);
  else if(H<4*256)   c32 =       (  X<<8) | (255<<16);
  else if(H<5*256)   c32 =   X            | (255<<16);
  else /* H<6*256 */ c32 = 255            | (  X<<16);
  return c32 | 0xF0000000;
}

void kml_line(struct s_kml_line *kl)
{
  if(kmlbuf_pos+str_kml_line_len > kmlbuf_len) // rather loose data than crash
    return;
  char *a = kmlbuf+kmlbuf_pos;

  sprintf(a+str_kml_line_pos_lonlat, "%+011.6f,%+010.6f %+011.6f,%+010.6f",
    kl->lon1, kl->lat1, kl->lon2, kl->lat2);
  kmlbuf[kmlbuf_pos+str_kml_line_pos_lonlat+45] = '<'; // replace null

  sprintf(a+str_kml_line_pos_color, "%08X", color32(1024*kl->value/red_iri));
  kmlbuf[kmlbuf_pos+str_kml_line_pos_color+8] = '<'; // replace null

  sprintf(a+str_kml_line_pos_name, "%5.2f", kl->value < 99.99 ? kl->value : 99.99);
  kmlbuf[kmlbuf_pos+str_kml_line_pos_name+5] = '<'; // replace null

  kmlbuf_pos += str_kml_line_len;
}

// overwrite arrow data. Arrow is the first entry in kmlbuf.
// kmlbuf holds 1 arrow and approx 12 lines
// arrow can't be written more than 1 every 12 lines
void kml_arrow(struct s_kml_arrow *ka)
{
  sprintf(kmlbuf+str_kml_arrow_pos_lonlat, "%+011.6f,%+010.6f",
    ka->lon, ka->lat);
  kmlbuf[str_kml_arrow_pos_lonlat+22] = '<'; // replace null

  sprintf(kmlbuf+str_kml_arrow_pos_heading, "%05.1f", (float)(((int)(10*ka->heading)+1800)%3600)/10.0);
  kmlbuf[str_kml_arrow_pos_heading+5] = '<'; // replace null

  sprintf(kmlbuf+str_kml_arrow_pos_color, "%08X", color32(1024*ka->value/red_iri));
  kmlbuf[str_kml_arrow_pos_color+8] = '<'; // replace null

  sprintf(kmlbuf+str_kml_arrow_pos_name, "%5.2f", ka->value < 99.99 ? ka->value : 99.99);
  kmlbuf[str_kml_arrow_pos_name+5] = '<'; // replace null

  kmlbuf_has_arrow = 1;
}

// if buffer is full then write
void kml_write(uint8_t force)
{
  //printf("kmlbuf_pos %d\n", kmlbuf_pos);
  if(force == 0 && kmlbuf_pos < kmlbuf_len-str_kml_line_len-1)
    return;  // there is space for another line, don't write yet
  if(kmlbuf_has_arrow)
  { // write lines including the arrow
    fwrite(kmlbuf, kmlbuf_pos, 1, fkml);
    kmlbuf_has_arrow = 0; // consumed
  }
  else
  { // skip arrow, write only lines
    fwrite(kmlbuf+str_kml_arrow_len, kmlbuf_pos-str_kml_arrow_len, 1, fkml);
  }
  kmlbuf_pos = str_kml_arrow_len; // consumed, default start next write past arrow
}

void kml_content(void)
{
  int i;
  const int step = 8;

  x_kml_arrow->lon       = 16.000000;
  x_kml_arrow->lat       = 46.000000;
  x_kml_arrow->heading   =  0.0;
  x_kml_arrow->value     =  1.0;
  x_kml_arrow->timestamp = "2021-07-24T11:54:19.0Z";
  kml_arrow(x_kml_arrow);
  kml_write(0);

  for(i = 0; i < 3000; i += step)
  {
    x_kml_line->lon1      = 16.000000;
    x_kml_line->lat1      = 46.000000 +  i       * 0.00001;
    x_kml_line->lon2      = 16.000000;
    x_kml_line->lat2      = 46.000500 + (i+step) * 0.00001;
    x_kml_line->value     = i/1024.0;
    x_kml_line->timestamp = "2021-07-24T11:54:19.0Z";
    kml_line(x_kml_line);
    kml_write(0);
  }
}


void kml_footer(char *begin, char *end)
{
  strcpy(kmlbuf, str_kml_footer);

  sprintf(kmlbuf+str_kml_footer_pos_longitude, "%+011.6f", 16.0);
  kmlbuf[str_kml_footer_pos_longitude+11] = '<'; // replace null

  sprintf(kmlbuf+str_kml_footer_pos_latitude, "%+010.6f", 46.0);
  kmlbuf[str_kml_footer_pos_latitude+10] = '<'; // replace null

  sprintf(kmlbuf+str_kml_footer_pos_heading, "%05.1f", 0.0);
  kmlbuf[str_kml_footer_pos_heading+5] = '<'; // replace null

  memcpy(kmlbuf+str_kml_footer_pos_begin, begin, 22);
  memcpy(kmlbuf+str_kml_footer_pos_end, end, 22);

  fwrite(kmlbuf, str_kml_footer_len, 1, fkml);
}

void kml_close(void)
{
  fclose(fkml);
}

int main(int argc, char *argv[])
{
  kml_init();
  kml_open();
  kml_header();
  kml_buf_init();
  kml_content();
  kml_footer("2021-07-24T00:00:32.6Z", "2021-07-24T14:59:26.4Z");
  kml_close();
  printf("line %d bytes\narrow %d bytes\nkml_buf_len %d bytes\n",
    str_kml_line_len, str_kml_arrow_len, kmlbuf_len);
}
