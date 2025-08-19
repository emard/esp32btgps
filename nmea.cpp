// used to handle $GPRMC NMEA sentence
#include "nmea.h"
#include <string.h> // only for NULL pointer
#include <stdlib.h> // strtol
#include <stdio.h> // snprintf

// NMEA timestamp string (day time) from conversion factors to 10x seconds
uint32_t nmea2sx[8] = { 360000,36000,6000,600,100,10,0,1 };

// convert nmea daytime HHMMSS.S to seconds since midnight x10 (0.1 s resolution)
int nmea2s(char *nmea)
{
  int s = 0;
  for(int i = 0; i < 8; i++)
  s += (nmea[i]-'0')*nmea2sx[i];
  return s;
}

// parse NMEA ascii string -> write to struct tm
// int seconds, doesn't handle 1/10 seconds
int nmea2tm(char *a, struct tm *t)
{
  char *b = nthchar(a, 9, ',');
  if (b == NULL)
    return 0;
  t->tm_year  = (b[ 5] - '0') * 10 + (b[ 6] - '0') + 100;
  t->tm_mon   = (b[ 3] - '0') * 10 + (b[ 4] - '0') - 1;
  t->tm_mday  = (b[ 1] - '0') * 10 + (b[ 2] - '0');
  t->tm_hour  = (a[ 7] - '0') * 10 + (a[ 8] - '0');
  t->tm_min   = (a[ 9] - '0') * 10 + (a[10] - '0');
  t->tm_sec   = (a[11] - '0') * 10 + (a[12] - '0');
  return 1;
}

// from GPRMC nmea timedate
// to 22-byte len kml timestamp (23 bytes including null termination)
int nmea2kmltime(char *nmea, char *kml)
{
  char *b = nthchar(nmea, 9, ',');
  if (b == NULL)
    return 0;
  strcpy(kml, "2000-01-01T00:00:00.0Z");
  // position to date, nmea[7] is first char of time
  kml[ 2] = b[5]; // year/10
  kml[ 3] = b[6]; // year%10
  kml[ 5] = b[3]; // month/10
  kml[ 6] = b[4]; // month%10
  kml[ 8] = b[1]; // day/10
  kml[ 9] = b[2]; // day%10
  kml[11] = nmea[ 7]; // hour/10
  kml[12] = nmea[ 8]; // hour%10
  kml[14] = nmea[ 9]; // minute/10
  kml[15] = nmea[10]; // minute%10
  kml[17] = nmea[11]; // int(second)/10
  kml[18] = nmea[12]; // int(second)%10
  kml[20] = nmea[14]; // second*10%10 (1/10 seconds)
  return 1;
}

inline uint8_t hex2int(char a)
{
  return a <= '9' ? a-'0' : a-'A'+10;
}

inline void int2hex(uint8_t i, char *a)
{
  uint8_t nib = i >> 4;
  a[0] = nib < 10 ? '0'+nib : 'A'+nib-10;
  nib = i & 15;
  a[1] = nib < 10 ? '0'+nib : 'A'+nib-10;
}

// write crc as 2 hex digits after '*'
// return crc
uint8_t write_nmea_crc(char *a)
{
  uint8_t crc = 0;
  for(a++; a[0] != '\0' && a[0] != '*'; a++)
    crc ^= a[0];
  if(a[0] != '*')
    return crc;
  int2hex(crc, a+1);
  return crc;
}

int check_nmea_crc(char *a)
{
  if(a[0] != '$')
    return 0;
  uint8_t crc = 0;
  for(a++; a[0] != '\0' && a[0] != '*'; a++)
    crc ^= a[0];
  if(a[0] != '*')
    return 0;
  return crc == ( (hex2int(a[1])<<4) | hex2int(a[2]) );
}

// find position of nth char (used as CSV parser)
char *nthchar(char *a, int n, char c)
{
  int i;
  for(i=0; *a; a++)
  {
    if(*a == c)
      i++;
    if(i == n)
      return a;
  }
  return NULL;
}

// parsing this will write null-delimiters into a
// so "a" will be temporarily broken and restored afterwards
// negative microminutes mean S or W
#if 0
void nmea2latlon_defunct(char *a, struct int_latlon *latlon)
{
  int umin;
  char a0,a1,a2; // preserve original values

  // read fractional minutes from back to top, null-terminating the string
  a2 = a[29];
  a[29] = 0; // replace ','->0
  umin  = strtol(a+23, NULL, 10);          // fractional minutes to microminutes
  a1 = a[22];
  a[22] = 0; // replace '.'->0
  umin += strtol(a+20, NULL, 10)*1000000;  // add integer minutes to microminutes
  a0 = a[20];
  a[20] = 0;
  latlon->lat_deg  = strtol(a+18, NULL, 10);
  if(a[30]=='S')
    umin = -umin;
  latlon->lat_umin = umin;
  // return original values
  a[29] = a2;
  a[22] = a1;
  a[20] = a0;

  a2 = a[44];
  a[44] = 0; // replace ','->0
  umin  = strtol(a+38, NULL, 10);          // fractional minutes to microminutes
  a1 = a[37];
  a[37] = 0; // replace '.'->0
  umin += strtol(a+35, NULL, 10)*1000000;  // add integer minutes to microminutes
  a0 = a[35];
  a[35] = 0;
  latlon->lon_deg  = strtol(a+32, NULL, 10);
  if(a[45]=='W')
    umin = -umin;
  latlon->lon_umin = umin;
  // return original values
  a[44] = a2;
  a[37] = a1;
  a[35] = a0;
}
#endif

#if 0
void nmea2latlon(char *a, struct int_latlon *latlon)
{
  char *blat = nthchar(a, 3, ','); // position lat
  char *bns  = nthchar(a, 4, ','); // position n/s
  char *blon = nthchar(a, 5, ','); // position lon
  char *bew  = nthchar(a, 6, ','); // position e/w
  double l;
  int deg, umin;

  if(blat[1] != ',')
  {
    l = atof(blat+1);
    deg = 1E-2 * l;
    umin = 1E6 * (l-deg*100);
    if(bns[1]!=',')
      if(bns[1]=='S')
        umin = -umin;
    latlon->lat_deg = deg;
    latlon->lat_umin = umin;
  }
  if(blon[1] != ',')
  {
    l = atof(blon+1);
    deg = 1E-2 * l;
    umin = 1E6 * (l-deg*100);
    if(bew[1]!=',')
      if(bew[1]=='W')
        umin = -umin;
    latlon->lon_deg = deg;
    latlon->lon_umin = umin;
  }
}
#endif

void nmea2dlatlon(char *a, double *lat, double *lon)
{
  char *blat = nthchar(a, 3, ','); // position lat
  char *bns  = nthchar(a, 4, ','); // position n/s
  char *blon = nthchar(a, 5, ','); // position lon
  char *bew  = nthchar(a, 6, ','); // position e/w
  double l, dl;
  int deg;

  if(blat[1] != ',')
  {
    l = atof(blat+1);
    deg = 1E-2 * l;
    dl = deg + 0.016666666666666666*(l-100*deg); // deg+min/60
    if(bns[1]!=',')
      if(bns[1]=='S')
        dl = -dl;
    *lat = dl;
  }
  if(blon[1] != ',')
  {
    l = atof(blon+1);
    deg = 1E-2 * l;
    dl = deg + 0.016666666666666666*(l-100*deg); // deg+min/60
    if(bew[1]!=',')
      if(bew[1]=='W')
        dl = -dl;
    *lon = dl;
  }
}

void latlon2double(struct int_latlon *latlon, double *lat, double *lon)
{
  *lat = latlon->lat_deg + abs(latlon->lat_umin)*1.66666666666666666e-8;
  if(latlon->lat_umin < 0)
    *lat = -*lat;
  *lon = latlon->lon_deg + abs(latlon->lon_umin)*1.66666666666666666e-8;
  if(latlon->lon_umin < 0)
    *lon = -*lon;
}

uint16_t nmea2iheading(char *nmea)
{
  char *b = nthchar(nmea, 8, ','); // position to heading
  #if 0
  // simplified parsing
  char str_heading[5] = "0000"; // storage for parsing
  str_heading[0] = b[1];
  str_heading[1] = b[2];
  str_heading[2] = b[3];
  str_heading[3] = b[5]; // skip b[4]=='.'
  //str_heading[4] = 0;
  uint16_t iheading = strtol(str_heading, NULL, 10); // parse as integer 0-360 -> 0-3600
  #else
  uint16_t iheading = 10*atof(b+1);
  #endif
  return iheading;
}

// parse NMEA ascii string -> return mph x100 speed (negative -1 if no fix)
int nmea2spd(char *a)
{
  //char *b = a+46; // simplified locating 7th ","
  char *b = nthchar(a, 7, ',');
  // simplified parsing, string has form ",000.00,"
  #if 0
  if (b[4] != '.' || b[7] != ',')
    return -1;
  return (b[1] - '0') * 10000 + (b[2] - '0') * 1000 + (b[3] - '0') * 100 + (b[5] - '0') * 10 + (b[6] - '0');
  #else
  // if no fix then string has form ",," field is empty
  // then return -1
  if (b[1] == ',')
    return -1;
  // string has general form e.g. ",0.0," or ",000.00,"
  float speed = atof(b+1);
  return (int)(speed*100+0.5);
  #endif
}

// write centiknots speed to nmea
// remember to fix crc after this
void spd2nmea(char *a, int ckt)
{
  char *b = nthchar(a, 7, ',');
  if(b[0] != ',' || b[1] == ',' || b[7] != ',') // ,000.00, check validity
    return;
  char buf[20];
  // FIXME include snprintf() missing
  snprintf(buf, 20, "%03d.%02d", ckt/100, ckt%100);
  memcpy(b+1, buf, 6);
}
