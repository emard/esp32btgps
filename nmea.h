#ifndef NMEA_H
#define NMEA_H
#include <stdint.h>
#include <time.h>
struct int_latlon
{
  int lat_deg, lat_umin, lon_deg, lon_umin;
};
struct double_latlon
{
  double lat, lon;
};
struct gprmc
{
  char kmltime[23]; // 2025-08-19T14:32:47.0Z
  char fix; // A=automatic
  double lat, lon; // float fits up to 4 digits, GLO provides 6 digits
  float speed_kt; // knots
  float heading; // 0-360 deg
  struct tm tm; // resolution 1 s
  uint16_t tm_msec; // additional for resolution 0.001 s
};
int nmea2s(char *nmea);
int nmea2tm(char *a, struct tm *t);
int nmea2kmltime(char *nmea, char *kml);
// void nmea2latlon(char *a, struct int_latlon *latlon);
void nmea2dlatlon(char *a, double *lat, double *lon);
void latlon2double(struct int_latlon *latlon, double *lat, double *lon);
inline uint8_t hex2int(char a);
uint8_t write_nmea_crc(char *a);
int check_nmea_crc(char *a);
char *nthchar(char *a, int n, char c);
uint16_t nmea2iheading(char *nmea);
int nmea2spd(char *a);
void spd2nmea(char *a, int ckt);
void nmea2gprmc(char *line, struct gprmc *gprmc);
#endif
