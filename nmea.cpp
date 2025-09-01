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

// write centiknots speed to nmea
// remember to fix crc after this
// old code still used by OBD
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

void nmea2gprmc(char *line, struct gprmc *gprmc)
{
  char *rmcfield[12];
  int i, n;

  for(i = 0; i < 12; i++)
    rmcfield[i] = NULL;
  gprmc->tm_msec = 0;
  rmcfield[0] = nthchar(line, 1, ',');
  for(n = 1; n < 12 && rmcfield[n-1] != NULL; n++)
    rmcfield[n] = nthchar(rmcfield[n-1], 2, ',');
  //printf("%s\n", line);
  //for(i = 0; i < n; i++)
  //  printf("%s\n", rmcfield[i]);
  nmea2kmltime(line, gprmc->kmltime);
  nmea2tm(line, &gprmc->tm);
  gprmc->tm_msec = 0;
  if(rmcfield[0])
  {
    if(rmcfield[0][7] == '.')
    {
      double subsec = atof(rmcfield[0]+7);
      gprmc->tm_msec = 1000 * subsec;
    }
  }
  if(rmcfield[1])
    gprmc->fix = rmcfield[1][1];
  else
    gprmc->fix = ' ';
  if(gprmc->fix == 'A')
  {
    nmea2dlatlon(line, &gprmc->lat, &gprmc->lon);
    gprmc->speed_kt = 0.0;
    if(rmcfield[6])
      if(rmcfield[6][1] != ',')
        gprmc->speed_kt = atof(rmcfield[6]+1);
    if(rmcfield[7])
      gprmc->heading = atof(rmcfield[7]+1);
  }
  else // no fix
  {
    gprmc->lat = gprmc->lon = 100.0;
    gprmc->speed_kt = -1.0;
    gprmc->heading = 0.0;
  }
}
