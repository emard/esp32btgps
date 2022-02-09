#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "nmea.h"
#include "kml.h"
#include "geostat.h"

void wavreader(char *filename)
{
  int f = open(filename, O_RDONLY);
  lseek(f, 44, SEEK_SET);
  int16_t b[6];
  char a,c;
  char nmea[256];
  int nmea_len=0;

  for(;;)
  {
    if(read(f, b, sizeof(b)) != sizeof(b))
      break; // eof
    a = b[0]&1 | ((b[1]&1)<<1) | ((b[2]&1)<<2) | ((b[3]&1)<<3) | ((b[4]&1)<<4) | ((b[5]&1)<<5);
    if(a != 32 && a != 33)
    {
      c = a;
      if((a & 0x20) == 0)
        c ^= 0x40;
      if(nmea_len < sizeof(nmea)-2)
        nmea[nmea_len++] = c;
    }
    if(a == 32 && nmea_len > 3)
    {
      nmea[nmea_len] = 0;
      if(nmea[0] == '$')
        if(check_crc(nmea, nmea_len))
          nmea_proc(nmea, nmea_len);
      if(nmea[0] == 'L')
        if(check_crc(nmea, nmea_len))
          iri_proc(nmea, nmea_len);
      nmea_len = 0;
    }
  }
  close(f);
}

int main(int argc, char *argv[])
{
  float lat1, lon1, lat2, lon2, dlat, dlon;

  lat1=46.000; lon1=16.000;
  lat2=46.001; lon2=16.000;
  dlat = lat2-lat1;
  printf("lat1=%.6f° lon1=%.6f° lat2=%.6f° lon2=%.6f° dist=%.1fm\n",
    lat1, lon1, lat2, lon2, distance(lat1, lon1, lat2, lon2));
  printf("dlat=%.6f° dist=%.1fm\n",
    dlat, dlat*dlat2m);

  lat1=46.000; lon1=16.000;
  lat2=46.000; lon2=16.001;
  dlon = lon2-lon1;
  printf("lat1=%.6f° lon1=%.6f° lat2=%.6f° lon2=%.6f° dist=%.1fm\n",
    lat1, lon1, lat2, lon2, distance(lat1, lon1, lat2, lon2));
  printf("dlon=%.6f° dist=%.1fm\n",
    dlon, dlon*dlon2m(lat1));

  lon1=16.001; lat1=46.001;
  calculate_grid(lat1);
  printf("lon2grid=%d lat2grid=%d\n", lon2grid, lat2grid);
  int xgrid = lon1*lon2grid;
  int ygrid = lat1*lat2grid;
  int exgrid = ((int)(lon1*lon2gridm)) & (hash_grid_spacing_m-1);
  int eygrid = ((int)(lat1*lat2gridm)) & (hash_grid_spacing_m-1);
  printf("lon1=%.6f° lat1=%.6f° -> grid x,y = %d,%d -> hash x,y = %d,%d -> edge x,y = %d,%d\n",
    lon1, lat1, xgrid, ygrid, xgrid&31, ygrid&31, exgrid, eygrid);
  clear_storage();
  #if 0
  // debug code to test storage
  store_lon_lat(16.0000, 46.0000);
  store_lon_lat(16.0001, 46.0000);
  store_lon_lat(16.0000, 46.0001);
  store_lon_lat(16.0001, 46.0001);
  store_lon_lat(15.9997, 45.9996);
  print_storage();
  float lon[] = { 16.0000, 16.0001, 16.0000, 16.0001, 16.0002, 16.0002, 16.0000, 15.9999, 15.9998 };
  float lat[] = { 46.0000, 46.0000, 46.0001, 46.0001, 46.0002, 45.9999, 45.9999, 45.9999, 45.9996 };
  for(int i = 0; i < sizeof(lon)/sizeof(lon[0]); i++)
  {
    int index = find_xya(floor(lon[i] * lon2gridm), floor(lat[i] * lat2gridm), 0, 0);
    if(index != -1)
    {
      lon2 = (float)snap_point[index].xm / (float)lon2gridm;
      lat2 = (float)snap_point[index].ym / (float)lat2gridm;
      float dist = distance(lat[i], lon[i], lat2, lon2);
      printf("find lon=%.6f° lat=%.6f° lon2=%.6f° lat2=%.6f° -> %d -> dist=%.1f\n",
        lon[i], lat[i], lon2, lat2, index, dist);
    }
    else
      printf("find lon=%.6f° lat=%.6f° -> %d\n", lon[i], lat[i], index);
  }
  #endif
  for(int i = 1; i < argc; i++)
    wavreader(argv[i]);
  print_storage();
  write_storage2kml("/tmp/circle.kml");
}
