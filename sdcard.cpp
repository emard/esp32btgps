#include "pins.h"
#include "sdcard.h"
#include "adxl355.h"
#include "adxrs290.h"
#include "nmea.h"
#include "kml.h"
#include "geostat.h"
#include "spidma.h"
#include <sys/time.h>
#include <WiFi.h> // for rds_report_ip()
#include "zip.h" // kml->kmz
#include "flac_encode.h" // wav->flac

// TODO
// too much of various code is put into this module
// big cleanup needed, code from here should be distributed
// to multiple modules for readability
// (sdcard, adxl master, adxl reader, audio player, ascii tagger)

// config file parsing
uint8_t GPS_MAC[6], OBD_MAC[6];
String  GPS_NAME, GPS_PIN, OBD_NAME, OBD_PIN, AP_PASS[AP_MAX], DNS_HOST;

File file_kml, file_accel, file_pcm, file_cfg, file_csv;
char filename_data[256];
char *filename_lastnmea = (char *)"/profilog/var/lastnmea.txt";
char *filename_fmfreq   = (char *)"/profilog/var/fmfreq.txt";
char lastnmea[256]; // here is read content from filename_lastnmea
char *linenmea; // pointer to current nmea line
int card_is_mounted = 0;
int logs_are_open = 0;
int pcm_is_open = 0;
int sensor_check_status = 0;
int speed_ckt = -1; // centi-knots speed (kt*100)
int speed_mms = -1; // mm/s speed
int speed_kmh = -1; // km/h speed
int fast_enough = 0; // for speed logging hysteresis
uint8_t KMH_START = 12, KMH_STOP = 6; // km/h start/stop speed hystereis
uint8_t KMH_BTN = 0; // debug btn2 for fake km/h
int mode_obd_gps = 0; // alternates 0:OBD and 1:GPS
uint8_t gps_obd_configured = 0; // existence of (1<<0):OBD config, (1<<1):GPS config
uint32_t MS_SILENCE_RECONNECT = 0; // [ms] milliseconds of silence to reconnect
float srvz_iri100, iri[2], iriavg, srvz2_iri20, iri20[2], iri20avg;
float temp[2]; // sensor temperature
char iri2digit[4] = "0.0";
char iri99avg2digit[4] = "0.0";
uint32_t iri99sum = 0, iri99count = 0, iri99avg = 0; // collect session average
// struct int_latlon last_latlon; // degrees and microminutes
double last_dlatlon[2];
struct tm tm, tm_session; // tm_session gives new filename_data when reconnected
uint8_t log_wav_kml = 3; // 1-wav 2-kml 3-both
uint8_t G_RANGE = 8; // +-2/4/8 g sensor range (at digital reading +-32000)
uint8_t FILTER_ADXL355_CONF = 0; // see datasheet adxl355 p.38 0:1kHz ... 10:0.977Hz
uint8_t FILTER_ADXRS290_CONF = 0; // see datasheet adxrs290 p.11 0:480Hz ... 7:20Hz
float   T_OFFSET_ADXL355_CONF[2]  = {0.0, 0.0}; // L,R
float   T_SLOPE_ADXL355_CONF[2]   = {1.0, 1.0}; // L,R
float   T_OFFSET_ADXRS290_CONF[2] = {0.0, 0.0}; // L,R
float   T_SLOPE_ADXRS290_CONF[2]  = {1.0, 1.0}; // L,R
uint8_t  KMH_REPORT1 = 30; // when speed_kmh >= KMH_REPORT1 use MM_REPORT1, else MM_REPORT2
uint32_t MM_REPORT1  = 100000, MM_REPORT2 = 20000; // mm report every travel distance 100 m, 20 m
uint32_t MM_FAST = 0; // [mm] insert fine points, 0 to disable for too fast drivi
uint32_t MM_SLOW = 0; // [mm] skip points, 0 to disable for too slow driving
uint32_t fm_freq[2] = {107900000, 87600000};
uint8_t fm_freq_cursor = 0; // cursor highlighting fm freq bitmask 0,1,2
uint8_t btn, btn_prev;
uint8_t finalize_busy = 0;

// SD status
uint64_t total_bytes = 0, used_bytes, free_bytes;
uint32_t free_MB;
uint8_t sdcard_ok = 1;

void SD_status(void)
{
  if(card_is_mounted)
  {
    total_bytes = SD_MMC.totalBytes();
    used_bytes = SD_MMC.usedBytes();
    free_bytes = total_bytes-used_bytes;
    free_MB = free_bytes / (1024 * 1024);
  }
}


void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    if(card_is_mounted == 0)
      return;
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void mount(void)
{
  if(card_is_mounted)
    return;
  if(!SD_MMC.begin()){
        sdcard_ok = 0;
        Serial.println("Card Mount Failed");
        return;
  }

  uint8_t cardType = SD_MMC.cardType();

  if(cardType == CARD_NONE){
        sdcard_ok = 0;
        Serial.println("No SD_MMC card attached");
        return;
  }

  Serial.print("SD_MMC Card Type: ");
  if(cardType == CARD_MMC){
        Serial.println("MMC");
  } else if(cardType == CARD_SD){
        Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
  } else {
        Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
  Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));
  Serial.printf("Free space: %lluMB\n", (SD_MMC.totalBytes()-SD_MMC.usedBytes()) / (1024 * 1024));

  sdcard_ok = 1;
  card_is_mounted = 1;
  logs_are_open = 0;
}

void umount(void)
{
  SD_status();
  SD_MMC.end();
  card_is_mounted = 0;
  logs_are_open = 0;
}

void ls(void)
{
  listDir(SD_MMC, "/", 0);
}

int open_pcm(char *wav)
{
  if(pcm_is_open)
    return 0;
  int n = 3584; // bytes to play for initiall buffer fill
  // to generate wav files:
  // espeak-ng -v hr -f speak.txt -w speak.wav; sox speak.wav --no-dither -r 11025 -b 8 output.wav reverse trim 1s reverse
  // "--no-dither" reduces noise
  // "-r 11025 -b 8" is sample rate 11025 Hz, 8 bits per sample
  // "reverse trim 1s reverse" cuts off 1 sample from the end, to avoid click
  file_pcm = SD_MMC.open(wav, FILE_READ);
  if(file_pcm)
  {
    sdcard_ok = 1;
    file_pcm.seek(44); // skip header to get data
    pcm_is_open = 1;
    play_pcm(n); // initially fill the play buffer, max buffer is 4KB
  }
  else
  {
    sdcard_ok = 0;
    Serial.print("can't open file ");
    pcm_is_open = 0;
    n = 0;
  }
  Serial.println(wav); // print which file is playing now
  return n;
}

void write_wav_header(void)
{
  uint8_t wavhdr[44] =
  {
    'R', 'I', 'F', 'F',
    0x00, 0x00, 0x00, 0x00, // chunk size bytes (len, including hdr), file growing, not yet known
    'W', 'A', 'V', 'E',
    // subchunk1: fmt
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00, // subchunk 1 size 16 bytes
    0x01, 0x00, // audio format = 1 (PCM)
    0x06, 0x00, // num channels = 6
    0xE8, 0x03, 0x00, 0x00, // sample rate = 1000 Hz
    0xE0, 0x2E, 0x00, 0x00, // byte rate = 12*1000 = 12000 byte/s
    0x0C, 0x00, // block align = 12 bytes
    0x10, 0x00, // bits per sample = 16 bits
    // subchunk2: data    
    'd', 'a', 't', 'a',
    0x00, 0x00, 0x00, 0x00, // chunk size bytes (len), file growing, not yet known       
  };
  file_accel.write(wavhdr, sizeof(wavhdr));
}

/* after write_logs, check beginning middle and end of buffer
   there should be different data from sensor noise.
   all equal data indicate sensor failure
   bit 1: right sensor ok
   bit 0: left  sensor ok
*/
int sensor_check(void)
{
  int retval = 0; // start with both sensors fail
  const int checkat[] = { // make it 12-even
    SPI_READER_BUF_SIZE*1/16 - SPI_READER_BUF_SIZE*1/16%12,
    SPI_READER_BUF_SIZE*2/16 - SPI_READER_BUF_SIZE*2/16%12,
    SPI_READER_BUF_SIZE*3/16 - SPI_READER_BUF_SIZE*3/16%12,
    SPI_READER_BUF_SIZE*4/16 - SPI_READER_BUF_SIZE*4/16%12,
    SPI_READER_BUF_SIZE*5/16 - SPI_READER_BUF_SIZE*5/16%12,
    SPI_READER_BUF_SIZE*6/16 - SPI_READER_BUF_SIZE*6/16%12,
    SPI_READER_BUF_SIZE*7/16 - SPI_READER_BUF_SIZE*7/16%12,
    SPI_READER_BUF_SIZE*8/16 - SPI_READER_BUF_SIZE*8/16%12 - 12,
  }; // list of indexes to check (0 not included)
  int lr[2] = {6, 12}; // l, r index of sensors in rx buf to check
  uint8_t v0[6], v[6]; // sensor readings
  int16_t g_prev, g, *gptr; // X-axis glitch finder

  int i, j, k;

  for(i = 0; i < 2; i++) // lr index
  {
    memcpy(v0, spi_master_rx_buf + lr[i], 6);
    // remove LSB
    for(k = 0; k < 6; k += 2)
      v0[k] |= 1;
    for(j = 0; j < sizeof(checkat)/sizeof(checkat[0]); j++) // checkat index
    {
      memcpy(v, spi_master_rx_buf + lr[i] + checkat[j], 6);
      // remove LSB
      for(k = 0; k < 6; k += 2)
        v[k] |= 1;
      if(memcmp(v, v0, 6) != 0)
        retval |= 1<<i;
    }
    // glitch comes from adxl355 hardware
    // with bad timing of sync/drdy/spi read
    // happens as a spike in X channel every 32 samples
    // spike has about 20000 difference.
    // To fix it sensor needs power off/on.
    // In normal operation this spike would never happen.
    // first sample sets
    // initial condition
    gptr = (int16_t *) (spi_master_rx_buf + lr[i]);
    g_prev = *gptr;
    // rest 32 samples are tested for glitch
    for(j = 1*12; j < 33*12; j+=12)
    {
      gptr = (int16_t *) (spi_master_rx_buf + lr[i] + j);
      g = *gptr;
      if(abs(g - g_prev) > 10000)
      {
        // X channel glitch detected
        // now it sets "no sensor" signal
        // TODO separate signal for glitch.
        // sensor with X-chanel -glitch can keep working
        // because only Z channel is needed
        retval &= ~(1<<i); // clr bit = no sensor
      }
      g_prev = g;
    }
  }
  return retval;
}

void store_last_sensor_reading(void)
{
  const int offset = SPI_READER_BUF_SIZE*4/8 - SPI_READER_BUF_SIZE*4/8%12 - 12-6;
  memcpy(last_sensor_reading, spi_master_rx_buf + offset, sizeof(last_sensor_reading));
}

void generate_filename_wav(struct tm *tm)
{
  #if 1
  sprintf(filename_data, (char *)"/profilog/data/%04d%02d%02d-%02d%02d.wav",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
  #else // one file per day
  sprintf(filename_data, (char *)"/profilog/data/%04d%02d%02d.wav",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
  #endif
}

void generate_filename_sta(struct tm *tm)
{
  #if 1
  sprintf(filename_data, (char *)"/profilog/data/%04d%02d%02d-%02d%02d.sta",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
  #else // one file per day
  sprintf(filename_data, (char *)"/profilog/data/%04d%02d%02d.sta",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
  #endif
}

void generate_filename_csv(struct tm *tm)
{
  #if 1
  sprintf(filename_data, (char *)"/profilog/data/%04d%02d%02d-%02d%02d.csv",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
  #else // one file per day
  sprintf(filename_data, (char *)"/profilog/data/%04d%02d%02d.csv",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
  #endif
}

void open_log_wav(struct tm *tm)
{
  generate_filename_wav(tm);
  #if 1
  //SD_MMC.remove(filename_data);
  file_accel = SD_MMC.open(filename_data, FILE_APPEND);
  // check appending file position (SEEK_CUR) and if 0 then write header
  if(file_accel.position() == 0)
    write_wav_header();
  #else
  SD_MMC.remove(filename_data);
  SD_MMC.remove((char *)"/profilog/data/accel.wav");
  file_accel = SD_MMC.open(filename_data, FILE_WRITE);
  write_wav_header();
  #endif
  #if 1
  Serial.print(filename_data);
  Serial.print(" @");
  Serial.println(file_accel.position());
  #endif
}

void flush_log_wav(void)
{
  file_accel.flush();
}

void write_log_wav(void)
{
  static uint8_t prev_half = 0;
  uint8_t half;
  uint16_t ptr;
  int retval = -1;

  ptr = (SPI_READER_BUF_SIZE + spi_slave_ptr() - 2) % SPI_READER_BUF_SIZE; // written content is 2 bytes behind pointer
  half = ptr >= SPI_READER_BUF_SIZE/2;
  if(half != prev_half)
  {
    spi_slave_read(half ? 0 : SPI_READER_BUF_SIZE/2, SPI_READER_BUF_SIZE/2);
    if(logs_are_open && fast_enough)
    {
      file_accel.write(spi_master_rx_buf+6, SPI_READER_BUF_SIZE/2);
      #if 0
      // print buffer ptr
      Serial.print("ptr ");
      Serial.println(ptr, DEC);
      #endif
    }
    prev_half = half;
    sensor_check_status = sensor_check();
    store_last_sensor_reading();
    //Serial.println(sensor_check_status);
  }
}

// write constant xyz value from last reading
// with the string mixed in
// string len: max 20 chars
void write_string_to_wav(char *a)
{
  uint8_t wsw[255], c;
  int j;

  if(logs_are_open == 0)
    return;
  if((log_wav_kml&1) == 0)
    return;

  for(j = 0; *a != 0; a++, j+=12)
  {
    c = *a;
    wsw[j     ] = (last_sensor_reading[ 0] & 0xFE) | (c & 1); c >>= 1;
    wsw[j +  1] =  last_sensor_reading[ 1];
    wsw[j +  2] = (last_sensor_reading[ 2] & 0xFE) | (c & 1); c >>= 1;
    wsw[j +  3] =  last_sensor_reading[ 3];
    wsw[j +  4] = (last_sensor_reading[ 4] & 0xFE) | (c & 1); c >>= 1;
    wsw[j +  5] =  last_sensor_reading[ 5];
    wsw[j +  6] = (last_sensor_reading[ 6] & 0xFE) | (c & 1); c >>= 1;
    wsw[j +  7] =  last_sensor_reading[ 7];
    wsw[j +  8] = (last_sensor_reading[ 8] & 0xFE) | (c & 1); c >>= 1;
    wsw[j +  9] =  last_sensor_reading[ 9];
    wsw[j + 10] = (last_sensor_reading[10] & 0xFE) | (c & 1);
    wsw[j + 11] =  last_sensor_reading[11];
  }
  file_accel.write(wsw, j);
}

void write_last_nmea(void)
{
  if (check_nmea_crc(lastnmea))
  {
    File file_lastnmea = SD_MMC.open(filename_lastnmea, FILE_WRITE);
    file_lastnmea.write((uint8_t *)lastnmea, strlen(lastnmea));
    file_lastnmea.write('\n');
    Serial.print("write last nmea: ");
    Serial.println(lastnmea);
    file_lastnmea.close();
  }
  #if 0 // debug
  else
  {
    Serial.print("last nmea not written\nbad crc:");
    Serial.println(lastnmea);
  }
  #endif
}

// file creation times should work with this
void set_system_time(time_t seconds_since_1980)
{
  timeval epoch = {seconds_since_1980, 0};
  const timeval *tv = &epoch;
  timezone utc = {0, 0};
  const timezone *tz = &utc;
  settimeofday(tv, tz);
}

uint8_t datetime_is_set = 0;
void set_date_from_tm(struct tm *tm)
{
  uint16_t year;
  uint8_t month, day, h, m, s;
  time_t t_of_day;
  if (datetime_is_set)
    return;
  t_of_day = mktime(tm);
  set_system_time(t_of_day);
  datetime_is_set = 1;
#if 0
  char *b = nthchar(a, 9, ',');
  if (b == NULL)
    return;
  year  = (b[ 5] - '0') * 10 + (b[ 6] - '0') + 2000;
  month = (b[ 3] - '0') * 10 + (b[ 4] - '0');
  day   = (b[ 1] - '0') * 10 + (b[ 2] - '0');
  h     = (a[ 7] - '0') * 10 + (a[ 8] - '0');
  m     = (a[ 9] - '0') * 10 + (a[10] - '0');
  s     = (a[11] - '0') * 10 + (a[12] - '0');
#endif
#if 1
  year  = tm->tm_year + 1900;
  month = tm->tm_mon  + 1;
  day   = tm->tm_mday;
  h     = tm->tm_hour;
  m     = tm->tm_min;
  s     = tm->tm_sec;
  char pr[80];
  //sprintf(pr, "datetime %04d-%02d-%02d %02d:%02d:%02d", year, month, day, h, m, s);
  //Serial.println(pr);
#endif
}

void read_last_nmea(void)
{
  File file_lastnmea = SD_MMC.open(filename_lastnmea, FILE_READ);
  //file_lastnmea.readBytes(lastnmea, strlen(lastnmea));
  if(!file_lastnmea)
    return;
  String last_nmea_line = file_lastnmea.readStringUntil('\n');
  strcpy(lastnmea, last_nmea_line.c_str());
  Serial.print("read last nmea: ");
  Serial.println(lastnmea);
  file_lastnmea.close();
  if(check_nmea_crc(lastnmea))
  {
    if (nmea2tm(lastnmea, &tm))
      set_date_from_tm(&tm);
    // nmea2latlon(lastnmea, &last_latlon); // parsing should not spoil lastnmea content
    nmea2dlatlon(lastnmea, &last_dlatlon[0], &last_dlatlon[1]);
  }
  else
    Serial.println("read last nmea bad crc");
  #if 0
  char latlon_spr[120];
  sprintf(latlon_spr, "parsed: %10.6f %10.6f", last_dlatlon[0], last_dlatlon[1]);
  Serial.println(latlon_spr);
  #endif
  // lastnmea[0] = 0; // prevent immediate next write, not needed as parsing does similar
}

void write_fmfreq(void)
{
  char fmfreq[32];
  if(card_is_mounted == 0)
    return;
  File file_fmfreq = SD_MMC.open(filename_fmfreq, FILE_WRITE);
  if(!file_fmfreq)
    return;
  sprintf(fmfreq, "%09d %09d\n", fm_freq[0], fm_freq[1]);
  file_fmfreq.write((uint8_t *)fmfreq, 20);
  file_fmfreq.close();
  Serial.print("write fmfreq: ");
  Serial.print(fmfreq);
}

void read_fmfreq(void)
{
  if(card_is_mounted == 0)
    return;
  File file_fmfreq = SD_MMC.open(filename_fmfreq, FILE_READ);
  if(!file_fmfreq)
    return;
  String fmfreq_line = file_fmfreq.readStringUntil('\n');
  file_fmfreq.close();
  fm_freq[0] = strtol(fmfreq_line.substring( 0, 9).c_str(), NULL, 10);
  fm_freq[1] = strtol(fmfreq_line.substring(10,19).c_str(), NULL, 10);
  Serial.print("read fmfreq: ");
  Serial.print(fm_freq[0]);
  Serial.print(" ");
  Serial.print(fm_freq[1]);
  Serial.println(" Hz");
}

// this function doesn't work
// seek() is not working
void finalize_wav_header(void)
{
  uint32_t pos = file_accel.position();
  uint32_t subchunk2size = pos - 44;
  uint8_t subchunk2size_bytes[4] = {(uint8_t)subchunk2size, (uint8_t)(subchunk2size>>8), (uint8_t)(subchunk2size>>16), (uint8_t)(subchunk2size>>24)};
  uint32_t chunksize = pos - 8;
  uint8_t chunksize_bytes[4] = {(uint8_t)chunksize, (uint8_t)(chunksize>>8), (uint8_t)(chunksize>>16), (uint8_t)(chunksize>>24)};
  // FIXME file_accel.seek() is not working
  file_accel.seek(4);
  file_accel.write(chunksize_bytes, 4);
  file_accel.seek(40);
  file_accel.write(subchunk2size_bytes, 4);
  Serial.print("finalization hdr position ");
  Serial.print(file_accel.position(), DEC);
  Serial.print(" logs finalized at pos ");
  Serial.println(pos, DEC);  
}

void close_log_wav(void)
{
  logs_are_open = 0;
  //finalize_wav_header();
  file_accel.close();
}

void write_kml_header(struct tm *tm)
{
  char name[23];
  snprintf(name, sizeof(name), "PROFILOG %04d%02d%02d-%02d%02d",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
  kml_header(name);
  file_kml.write((uint8_t *)kmlbuf, strlen(kmlbuf));
}

// save stat database to prevent data loss from
// unexpected reboot or power off
void write_stat_file(struct tm *tm)
{
  generate_filename_sta(tm);
  File file_stat = SD_MMC.open(filename_data, FILE_WRITE);
  if(file_stat)
  {
    file_stat.write((uint8_t *)&s_stat, sizeof(s_stat));
    file_stat.close();
    Serial.print("write stat: ");
    Serial.println(filename_data);
  }
  else
    Serial.println("write stat failed.");
}

void delete_stat_file(struct tm *tm)
{
  generate_filename_sta(tm);
  SD_MMC.remove(filename_data);
}

int read_stat_file(String filename_stat)
{
  File file_stat = SD_MMC.open(filename_stat, FILE_READ);
  if(file_stat)
  {
    file_stat.read((uint8_t *)&s_stat, sizeof(s_stat));
    file_stat.close();
    calculate_grid(s_stat.lat);
    Serial.print("read stat: ");
    Serial.println(filename_stat);
    return 1; // success
  }
  Serial.print("stat not found: ");
  Serial.println(filename_stat);
  return 0; // fail
}

void write_stat_arrows(void)
{
  if(logs_are_open == 0)
    return;
  if((log_wav_kml&2) == 0)
    return;

  char timestamp[23] = "2000-01-01T00:00:00.0Z";
  nmea2kmltime(lastnmea, timestamp);
  kml_buf_init();
  #if 0
  // debug write some dummy arrows
  for(int i = 0; i < 10; i++)
  {
    x_kml_arrow->lon       = 16.0+0.001*i;
    x_kml_arrow->lat       = 46.0;
    x_kml_arrow->value     = 1.0;
    x_kml_arrow->left      = 1.1;
    x_kml_arrow->right     = 1.2;
    x_kml_arrow->left_stdev  =  0.1;
    x_kml_arrow->right_stdev =  0.1;
    x_kml_arrow->n         = i;
    x_kml_arrow->heading   = 0.0;
    x_kml_arrow->speed_min_kmh = 80.0;
    x_kml_arrow->speed_max_kmh = 80.0;
    x_kml_arrow->timestamp = timestamp;
    kml_arrow(x_kml_arrow);
    file_kml.write((uint8_t *)kmlbuf, str_kml_arrow_len);
  }
  #endif
  printf("writing %d stat arrows to kml\n", s_stat.wr_snap_ptr);
  for(int i = 0; i < s_stat.wr_snap_ptr; i++)
  {
    x_kml_arrow->lon       = (float)(s_stat.snap_point[i].xm) / (float)lon2gridm;
    x_kml_arrow->lat       = (float)(s_stat.snap_point[i].ym) / (float)lat2gridm;
    x_kml_arrow->value     = (s_stat.snap_point[i].sum_iri[0][0]+s_stat.snap_point[i].sum_iri[0][1]) / (2*s_stat.snap_point[i].n);
    x_kml_arrow->left      =  s_stat.snap_point[i].sum_iri[0][0] / s_stat.snap_point[i].n;
    x_kml_arrow->right     =  s_stat.snap_point[i].sum_iri[0][1] / s_stat.snap_point[i].n;
    x_kml_arrow->left_stdev  =  0.0;
    x_kml_arrow->right_stdev =  0.0;
    uint8_t n = s_stat.snap_point[i].n;
    if(n > 0)
    {
      float sum1_left  = s_stat.snap_point[i].sum_iri[0][0];
      float sum2_left  = s_stat.snap_point[i].sum_iri[1][0];
      float sum1_right = s_stat.snap_point[i].sum_iri[0][1];
      float sum2_right = s_stat.snap_point[i].sum_iri[1][1];
      x_kml_arrow->left_stdev  =  sqrt(fabs( n*sum2_left  - sum1_left  * sum1_left  ))/n;
      x_kml_arrow->right_stdev =  sqrt(fabs( n*sum2_right - sum1_right * sum1_right ))/n;
    }
    x_kml_arrow->n         = n;
    x_kml_arrow->heading   = (float)(s_stat.snap_point[i].heading * (360.0/256));
    x_kml_arrow->speed_min_kmh = s_stat.snap_point[i].vmin;
    x_kml_arrow->speed_max_kmh = s_stat.snap_point[i].vmax;
    uint16_t dt = s_stat.snap_point[i].daytime; // 2-second ticks since midnight
    snprintf(timestamp+11, 12, "%02d:%02d:%02d.0Z", dt/1800,dt/30%60,(dt%30)*2);
    x_kml_arrow->timestamp = timestamp;
    kml_arrow(x_kml_arrow);
    file_kml.write((uint8_t *)kmlbuf, str_kml_arrow_len);
  }
}

void write_csv(String file_name)
{
  if((log_wav_kml&4) == 0)
    return;
  char linebuf[256];
  const char *heading_arrow[] = {"🡩","🡭","🡪","🡮","🡫","🡯","🡨","🡬"}; // 8 arrows UTF-8
  char timestamp[23] = "2000-01-01T00:00:00.0Z";
  nmea2kmltime(lastnmea, timestamp);
  file_csv = SD_MMC.open(file_name, FILE_WRITE);
  printf("writing %d stat arrows to %s\n", s_stat.wr_snap_ptr, file_name.c_str());
  sprintf(linebuf, "\"travel [m]\",\"IRI100 [mm/m]\",\"arrow\",\"heading [°]\",\"lon [°]\",\"lat [°]\",\"time\",\"left IRI100 [mm/m]\",\"left ±2σ [mm/m]\",\"right IRI100 [mm/m]\",\"right ±2σ [mm/m]\",\"repeat\",\"min [km/h]\",\"max [km/h]\",\"UTF-8 English (US)\"\n");
  file_csv.write((uint8_t *)linebuf, strlen(linebuf));
  #if 1
  for(int i = 0; i < s_stat.wr_snap_ptr; i++)
  {
    x_kml_arrow->lon       = (float)(s_stat.snap_point[i].xm) / (float)lon2gridm;
    x_kml_arrow->lat       = (float)(s_stat.snap_point[i].ym) / (float)lat2gridm;
    x_kml_arrow->value     = (s_stat.snap_point[i].sum_iri[0][0]+s_stat.snap_point[i].sum_iri[0][1]) / (2*s_stat.snap_point[i].n);
    x_kml_arrow->left      =  s_stat.snap_point[i].sum_iri[0][0] / s_stat.snap_point[i].n;
    x_kml_arrow->right     =  s_stat.snap_point[i].sum_iri[0][1] / s_stat.snap_point[i].n;
    x_kml_arrow->left_stdev  =  0.0;
    x_kml_arrow->right_stdev =  0.0;
    uint8_t n = s_stat.snap_point[i].n;
    if(n > 0)
    {
      float sum1_left  = s_stat.snap_point[i].sum_iri[0][0];
      float sum2_left  = s_stat.snap_point[i].sum_iri[1][0];
      float sum1_right = s_stat.snap_point[i].sum_iri[0][1];
      float sum2_right = s_stat.snap_point[i].sum_iri[1][1];
      x_kml_arrow->left_stdev  =  sqrt(fabs( n*sum2_left  - sum1_left  * sum1_left  ))/n;
      x_kml_arrow->right_stdev =  sqrt(fabs( n*sum2_right - sum1_right * sum1_right ))/n;
    }
    x_kml_arrow->n         = n;
    x_kml_arrow->heading   = (float)(s_stat.snap_point[i].heading * (360.0/256));
    x_kml_arrow->speed_min_kmh = s_stat.snap_point[i].vmin;
    x_kml_arrow->speed_max_kmh = s_stat.snap_point[i].vmax;
    uint16_t dt = s_stat.snap_point[i].daytime; // 2-second ticks since midnight
    snprintf(timestamp+11, 12, "%02d:%02d:%02d.0Z", dt/1800,dt/30%60,(dt%30)*2);
    x_kml_arrow->timestamp = timestamp;
    //kml_arrow(x_kml_arrow);
    //file_kml.write((uint8_t *)kmlbuf, str_kml_arrow_len);
    // 100, 4.87,"↓",-174.3,+015.999377,+45.808163,2025-09-03T07:13:52.0Z,4.99,0.00,4.75,0.00,1,32,32
    // 200, 5.16,"↓",-164.5,+015.999898,+45.807419,2025-09-03T07:14:00.0Z,5.24,0.00,5.08,0.00,1,38,38
    int arrow_index = ((int)(x_kml_arrow->heading + 22.5)) / 45 % 8;
    sprintf(linebuf, "%4d00,%5.2f, \"%s\",%6.1f,%10.6f,%10.6f,%23s,%5.2f,%5.2f,%5.2f,%5.2f,%2d,%3d,%3d\n",
     i+1, x_kml_arrow->value,
     heading_arrow[arrow_index],
     x_kml_arrow->heading,
     x_kml_arrow->lat, x_kml_arrow->lon,
     x_kml_arrow->timestamp,
     x_kml_arrow->left,  x_kml_arrow->left_stdev,
     x_kml_arrow->right, x_kml_arrow->right_stdev,
     x_kml_arrow->n,
     x_kml_arrow->speed_min_kmh, x_kml_arrow->speed_max_kmh
     );
    file_csv.write((uint8_t *)linebuf, strlen(linebuf));
  }
  #endif
  file_csv.close();
}

// from stat in memory write csv file
// filename is generated from timestamp "tm"
void write_csv_tm(struct tm *tm)
{
  generate_filename_csv(tm);
  write_csv(String(filename_data));
}

// finalize kml file, no more writes after this
void write_kml_footer(void)
{
  file_kml.write((uint8_t *)str_kml_footer_simple, strlen(str_kml_footer_simple));
}

// force = 0: if kml buffer is full write
// force = 1: write immediately what is in the buffer
void write_log_kml(uint8_t force)
{
  //printf("kmlbuf_pos %d\n", kmlbuf_pos);
  if(logs_are_open && fast_enough)
  {
    if(
      (force == 0 && kmlbuf_pos < kmlbuf_len-str_kml_line_len-1) // write if force or buffer full
    ||(kmlbuf_pos <= kmlbuf_start) // nothing to write
    )
      return;
    file_kml.write((uint8_t *)kmlbuf+kmlbuf_start, kmlbuf_pos-kmlbuf_start);
    kmlbuf_pos = str_kml_arrow_len; // consumed, default start next write past arrow
    kmlbuf_start = str_kml_arrow_len; // same as kmlbuf_pos default write to file from this point
  }
}

void generate_filename_kml(struct tm *tm)
{
  #if 1
  sprintf(filename_data, (char *)"/profilog/data/%04d%02d%02d-%02d%02d.kml",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
  #else // one file per day
  sprintf(filename_data, (char *)"/profilog/data/%04d%02d%02d.kml",
    tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
  #endif
}

void open_log_kml(struct tm *tm)
{
  generate_filename_kml(tm);
  file_kml = SD_MMC.open(filename_data, FILE_APPEND);
  // check appending file position (SEEK_CUR) and if 0 then write header
  if(file_kml.position() == 0)
    write_kml_header(tm);
  kml_buf_init(); // fill kml buffer with arrow and line content
  #if 1
  Serial.print(filename_data);
  Serial.print(" @");
  Serial.println(file_kml.position());
  #endif
}

void close_log_kml(void)
{
  file_kml.close();
}

void spi_slave_test(void)
{
  static uint8_t count = 0;
  uint16_t wptr;
  #if 0
  // begin spi slave test (use SPI_MODE3)
  spi_master_tx_buf[0] = 0; // 0: write ram
  spi_master_tx_buf[1] = 0; // addr [31:24] msb
  spi_master_tx_buf[2] = 0; // addr [23:16]
  spi_master_tx_buf[3] = 0; // addr [15: 8]
  spi_master_tx_buf[4] = 0; // addr [ 7: 0] lsb
  spi_master_tx_buf[5] = 0x11; // data
  spi_master_tx_buf[6] = 0x22; // data
  spi_master_tx_buf[7] = 0x33; // data
  spi_master_tx_buf[8] = count++; // data
  master_txrx(spi_master_tx_buf, 9); // write
  #endif // end writing
  wptr = spi_slave_ptr();
  spi_master_tx_buf[0] = 1; // 1: read ram
  spi_master_tx_buf[1] = 0; // addr [31:24] msb
  spi_master_tx_buf[2] = 0; // addr [23:16]
  spi_master_tx_buf[3] = 0; // addr [15: 8]
  spi_master_tx_buf[4] = 0; // addr [ 7: 0] lsb
  spi_master_tx_buf[5] = 0; // dummy
  master_tx_rx(spi_master_tx_buf, spi_master_rx_buf, 30); // read
  for(int i = 6; i < 30; i++)
  {
    Serial.print(spi_master_rx_buf[i], HEX);
    Serial.print(" ");
  }
  Serial.print(wptr, DEC);
  Serial.println("");
  // end spi slave test
}


void parse_mac(uint8_t *mac, String a)
{
  for(int i = 0; i < 6; i++)
    mac[i] = strtol(a.substring(i*3,i*3+2).c_str(), NULL, 16);
}

void check_gps_obd_config()
{
  // check existence of OBD config
  gps_obd_configured = OBD_NAME.length() > 0 ? 1 : 0;
  if(gps_obd_configured == 0)
    for(uint8_t i = 0; i < 6; i++)
      if(OBD_MAC[i])
        gps_obd_configured = 1;

  // check existence of GPS config
  gps_obd_configured |= GPS_NAME.length() > 0 ? 2 : 0;
  if((gps_obd_configured & 2) == 0)
    for(uint8_t i = 0; i < 6; i++)
      if(GPS_MAC[i])
        gps_obd_configured |= 2;
}

void read_cfg(void)
{
  char *filename_cfg = (char *)"/profilog/config/profilog.cfg";
  file_cfg = SD_MMC.open(filename_cfg, FILE_READ);
  int linecount = 0;
  Serial.print("*** open ");
  Serial.println(filename_cfg);
  int ap_n = 0; // AP counter
  while(file_cfg.available())
  {
    String cfgline = file_cfg.readStringUntil('\n');
    linecount++;
    if(cfgline.length() < 2) // skip empty and short lines
      continue;
    if(cfgline[0] == '#') // skip comments
      continue;
    int delimpos = cfgline.indexOf(':'); // delimiter position
    if(delimpos < 2) // skip lines without proper ":" delimiter
      continue;
    String varname = cfgline.substring(0, delimpos);
    varname.trim(); // inplace trim leading and trailing whitespace
    String varvalue = cfgline.substring(delimpos+1); // to end of line
    varvalue.trim(); // inplace trim leading and trailing whitespace
    if     (varname.equalsIgnoreCase("ap_pass" )) {if(ap_n<AP_MAX) AP_PASS[ap_n++] = varvalue; }
    else if(varname.equalsIgnoreCase("dns_host")) DNS_HOST = varvalue;
    else if(varname.equalsIgnoreCase("gps_name")) GPS_NAME = varvalue;
    else if(varname.equalsIgnoreCase("gps_mac" )) parse_mac(GPS_MAC, varvalue);
    else if(varname.equalsIgnoreCase("gps_pin" )) GPS_PIN  = varvalue;
    else if(varname.equalsIgnoreCase("obd_name")) OBD_NAME = varvalue;
    else if(varname.equalsIgnoreCase("obd_mac" )) parse_mac(OBD_MAC, varvalue);
    else if(varname.equalsIgnoreCase("obd_pin" )) OBD_PIN  = varvalue;
    else if(varname.equalsIgnoreCase("silence_reconnect" )) MS_SILENCE_RECONNECT  = 1000*strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("log_mode")) log_wav_kml = strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("red_iri" )) red_iri = strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("g_range" )) G_RANGE = strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("filter_adxl355" )) FILTER_ADXL355_CONF  = strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("filter_adxrs290")) FILTER_ADXRS290_CONF = strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("tl_offset_adxl355" )) T_OFFSET_ADXL355_CONF[0]  = strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("tl_slope_adxl355" )) T_SLOPE_ADXL355_CONF[0]  = strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("tr_offset_adxl355" )) T_OFFSET_ADXL355_CONF[1]  = strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("tr_slope_adxl355" )) T_SLOPE_ADXL355_CONF[1]  = strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("tl_offset_adxrs290")) T_OFFSET_ADXRS290_CONF[0] = strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("tl_slope_adxrs290")) T_SLOPE_ADXRS290_CONF[0] = strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("tr_offset_adxrs290")) T_OFFSET_ADXRS290_CONF[1] = strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("tr_slope_adxrs290")) T_SLOPE_ADXRS290_CONF[1] = strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("kmh_report1")) KMH_REPORT1 = strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("m_report1")) MM_REPORT1 = 1000*strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("m_report2")) MM_REPORT2 = 1000*strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("m_slow")) MM_SLOW = 1000*strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("m_fast")) MM_FAST = 1000*strtof(varvalue.c_str(), NULL);
    else if(varname.equalsIgnoreCase("kmh_start")) KMH_START = strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("kmh_stop")) KMH_STOP = strtol(varvalue.c_str(), NULL,10);
    else if(varname.equalsIgnoreCase("kmh_btn" )) KMH_BTN = strtol(varvalue.c_str(), NULL,10);
    else
    {
      Serial.print(filename_cfg);
      Serial.print(": error in line ");
      Serial.println(linecount);
    }
  }
  char macstr[80];
  Serial.print("GPS_NAME : "); Serial.println(GPS_NAME);
  sprintf(macstr, "GPS_MAC  : %02X:%02X:%02X:%02X:%02X:%02X",
    GPS_MAC[0], GPS_MAC[1], GPS_MAC[2], GPS_MAC[3], GPS_MAC[4], GPS_MAC[5]);
  Serial.println(macstr);
  Serial.print("GPS_PIN  : "); Serial.println(GPS_PIN);
  Serial.print("OBD_NAME : "); Serial.println(OBD_NAME);
  sprintf(macstr, "OBD_MAC  : %02X:%02X:%02X:%02X:%02X:%02X",
    OBD_MAC[0], OBD_MAC[1], OBD_MAC[2], OBD_MAC[3], OBD_MAC[4], OBD_MAC[5]);
  Serial.println(macstr);
  Serial.print("OBD_PIN  : "); Serial.println(OBD_PIN);
  Serial.print("SILENCE_RECONNECT  : "); Serial.println(MS_SILENCE_RECONNECT/1000);
  for(int i = 0; i < ap_n; i++)
  { Serial.print("AP_PASS  : "); Serial.println(AP_PASS[i]); }
  Serial.print("DNS_HOST : "); Serial.println(DNS_HOST);
  Serial.print("LOG_MODE : "); Serial.println(log_wav_kml);
  char chr_red_iri[20]; sprintf(chr_red_iri, "%.1f", red_iri);
  Serial.print("RED_IRI  : "); Serial.println(chr_red_iri);
  Serial.print("G_RANGE  : "); Serial.println(G_RANGE);
  Serial.print("FILTER_ADXL355     : "); Serial.println(FILTER_ADXL355_CONF);
  Serial.print("FILTER_ADXRS290    : "); Serial.println(FILTER_ADXRS290_CONF);
  for(int i = 0; i < 2; i++)
  {
    char lr = i ? 'R' : 'L';
    sprintf(macstr, "T%c_OFFSET_ADXL355  : %.1f", lr, T_OFFSET_ADXL355_CONF[i]);
    Serial.println(macstr);
    sprintf(macstr, "T%c_SLOPE_ADXL355   : %.1f", lr, T_SLOPE_ADXL355_CONF[i]);
    Serial.println(macstr);
    sprintf(macstr, "T%c_OFFSET_ADXRS290 : %.1f", lr, T_OFFSET_ADXRS290_CONF[i]);
    Serial.println(macstr);
    sprintf(macstr, "T%c_SLOPE_ADXRS290  : %.1f", lr, T_SLOPE_ADXRS290_CONF[i]);
    Serial.println(macstr);
  }
  Serial.print("M_REPORT1   : "); Serial.println(MM_REPORT1/1000);
  Serial.print("M_REPORT2   : "); Serial.println(MM_REPORT2/1000);
  sprintf(macstr, "%.1f", MM_SLOW*1.0E-3);
  Serial.print("M_SLOW      : "); Serial.println(macstr);
  sprintf(macstr, "%.1f", MM_FAST*1.0E-3);
  Serial.print("M_FAST      : "); Serial.println(macstr);
  Serial.print("KMH_REPORT1 : "); Serial.println(KMH_REPORT1);
  Serial.print("KMH_START   : "); Serial.println(KMH_START);
  Serial.print("KMH_STOP    : "); Serial.println(KMH_STOP);
  Serial.print("KMH_BTN     : "); Serial.println(KMH_BTN);
  Serial.print("*** close ");
  Serial.println(filename_cfg);
  file_cfg.close();
  check_gps_obd_config();
}

void open_logs(struct tm *tm)
{
  if(logs_are_open != 0)
    return;
  if(log_wav_kml&1)
    open_log_wav(tm);
  if(log_wav_kml&2)
    open_log_kml(tm);
  logs_are_open = 1;
}

void write_logs()
{
  if(log_wav_kml&1)
    write_log_wav();
  //if(log_wav_kml&2)
  //  write_log_kml(0); // write logs, no force
}

void flush_logs()
{
  if(logs_are_open == 0)
    return;
  if(log_wav_kml&1)
    flush_log_wav();
}

void close_logs()
{
  if(logs_are_open == 0)
    return;
  if(log_wav_kml&1)
    close_log_wav();
  if(log_wav_kml&2)
    close_log_kml();
  logs_are_open = 0;
}

// file_name should have full file path
void finalize_kml(File &kml, String file_name)
{
  kml.seek(kml.size() - 7);
  String file_end = kml.readString();
  if(file_end != "</kml>\n")
  {
    Serial.print("Finalizing ");
    Serial.println(file_name);
    // kml.close(); // crash with arduino esp32 v2.0.2
    file_kml = SD_MMC.open(file_name, FILE_APPEND);
    // try to open file name with .sta extension instead of .kml
    String file_name_sta = file_name.substring(0,file_name.length()-4) + ".sta";
    int write_csv_flag = 0;
    logs_are_open = 1;
    if(read_stat_file(file_name_sta)) // if .sta file read succeeds, add arrows to .kml, delete .sta file.
    {
      kml_init();
      file_kml.write((uint8_t *)str_kml_arrows_folder, strlen(str_kml_arrows_folder));
      write_stat_arrows();
      write_csv_flag = 1;
      SD_MMC.remove(file_name_sta);
    }
    file_kml.write((uint8_t *)str_kml_footer_simple, strlen(str_kml_footer_simple));
    file_kml.close();
    // generate .csv file
    if(write_csv_flag)
    {
      String file_name_csv = file_name.substring(0,file_name.length()-4) + ".csv";
      write_csv(file_name_csv);
    }
    logs_are_open = 0;
  }
  #if 1
  else // .kml file has proper ending zip it to .kmz
  {
    // .kml -> ZIP -> .kmz
    String file_name_kmz = file_name.substring(0,file_name.length()-4) + ".kmz";
    File file_kmz = SD_MMC.open(file_name_kmz, FILE_READ);
    size_t file_kmz_size = 0;
    if(file_kmz) // .kmz exists (opened for read)
    {
      file_kmz_size = file_kmz.size(); // find its size
      file_kmz.close(); // close for reading, will reopen for writing
    }
    if(file_kmz_size == 0) // .kmz doesn't exist or has size = 0 -> ZIP
    {
      kml.seek(0); // rewind
      int expect_zip_time_s = kml.size()/300000;
      Serial.printf("%s ETA %02d:%02d min:sec", file_name_kmz.c_str(), expect_zip_time_s/60, expect_zip_time_s%60);
      File file_kmz = SD_MMC.open(file_name_kmz, FILE_WRITE);
      zip(file_kmz, kml, "doc.kml");
      Serial.println(" done.");
      file_kmz.close();
      // can not close and remove now, because
      // "File kml" is needed after return from this function
      // file_kml.close();
      // SD_MMC.remove(file_name); // when we have .kmz, remove .kml
    }
  }
  #endif
}

// file_name should have full file path
void encode_wav_to_flac(File &file_wav, String file_name)
{
  // .wav -> .flac
  String file_name_flac = file_name.substring(0,file_name.length()-4) + ".flac";
  File file_flac = SD_MMC.open(file_name_flac, FILE_READ);
  size_t file_flac_size = 0;
  if(file_flac) // .kmz exists (opened for read)
  {
    file_flac_size = file_flac.size(); // find its size
    file_flac.close(); // close for reading, will reopen for writing
  }
  if(file_flac_size == 0) // .flac doesn't exist or has size = 0 -> FLAC ENCODE
  {
    file_wav.seek(0); // rewind wav file
    int expect_flac_time_s = file_wav.size()/600000;
    Serial.printf("%s ETA %02d:%02d min:sec", file_name_flac.c_str(), expect_flac_time_s/60, expect_flac_time_s%60);
    File file_flac = SD_MMC.open(file_name_flac, FILE_WRITE);
    flac_encode(file_flac, file_wav);
    Serial.println(" done.");
    file_flac.close();
    // can not close and remove now, because
    // "File wav" is needed after return from this function
    // file_wav.close();
    // SD_MMC.remove(file_name); // when we have .flac, remove .wav
  }
}

// finalize everyting except
// the file that would be opened
// as session based on time (tm)
void finalize_data(struct tm *tm){
    uint8_t zip_lcd_msg[256]; // LCD message buf
    if(card_is_mounted == 0)
      return;
    if(logs_are_open)
      return;
    const char *dirname = (char *)"/profilog/data";
    Serial.printf("Finalizing directory: %s\n", dirname);
    char *txzipmsgptr = (char *)zip_lcd_msg+5;

    File root = SD_MMC.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }
    char todaystr[10];
    sprintf(todaystr, "%04d%02d%02d", 1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday);
    Serial.println(todaystr); // debug
    int lcd_n=0; // counts printed LCD lines
    const int max_lcd_n=12;
    // generate_filename_kml(tm); TODO generate kml and wav filename move here
    // to save CPU
    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
            char *is_kml = strstr(file.name(),".kml");
            char *is_wav = strstr(file.name(),".wav");
            char *is_today = strstr(file.name(),todaystr);
            // print on LCD
            // if both wav and kml are enabled then list only wav
            // if only kml is configured, then list kml
            // TODO handle kmz and flac
            if(is_today && (is_wav != NULL || (log_wav_kml == 2 && is_kml != NULL)) )
            {
              int wrap_lcd_n = lcd_n % max_lcd_n; // wraparound last N lines
              char *txbufptr = (char *)spi_master_tx_buf+5+(wrap_lcd_n<<5);
              memset(txbufptr, 32, 32); // clear line
              strcpy(txbufptr, is_today);
              if(is_wav)
                sprintf(txbufptr+17, " %4d min", file.size()/720000);
              if(is_kml)
                sprintf(txbufptr+17, " %4d min", file.size()/360000); // approx minutes
              lcd_n++;
            }
            // postprocess (ZIP)
            if(is_kml)
            {
              String full_path = String(file.name());
              if((file.name())[0] != '/')
                full_path = String(dirname) + "/" + String(file.name());
              generate_filename_kml(tm);
              if(strcmp(full_path.c_str(), filename_data) != 0) // different name
              {
                zip_lcd_msg[0] = 0; // 0: write ram
                zip_lcd_msg[1] = 0xC; // addr [31:24] msb LCD addr
                zip_lcd_msg[2] = 0; // addr [23:16] (0:normal, 1:invert)
                zip_lcd_msg[3] = 0; // addr [15: 8]
                zip_lcd_msg[4] = 1+(1<<5); // addr [ 7: 0] lsb HOME X=0 Y=1
                memset(txzipmsgptr, 32, 64); // clear 2 lines
                strcpy(txzipmsgptr, file.name());
                uint16_t eta_s = file.size()/300000; // 300 K/s ZIP speed
                sprintf(txzipmsgptr+17, " %02d:%02d ZIP", eta_s/60, eta_s%60);
                master_txrx(zip_lcd_msg, 5+64); // write to LCD
                finalize_kml(file, full_path); // and ZIP kml->kmz
              }
            }
            if(is_wav)
            {
              String full_path = String(file.name());
              if((file.name())[0] != '/')
                full_path = String(dirname) + "/" + String(file.name());
              generate_filename_wav(tm);
              if(strcmp(full_path.c_str(), filename_data) != 0) // different name
              {
                zip_lcd_msg[0] = 0; // 0: write ram
                zip_lcd_msg[1] = 0xC; // addr [31:24] msb LCD addr
                zip_lcd_msg[2] = 0; // addr [23:16] (0:normal, 1:invert)
                zip_lcd_msg[3] = 0; // addr [15: 8]
                zip_lcd_msg[4] = 1+(1<<5); // addr [ 7: 0] lsb HOME X=0 Y=1
                memset(txzipmsgptr, 32, 64); // clear 2 lines
                strcpy(txzipmsgptr, file.name());
                uint16_t eta_s = file.size()/600000; // 600 K/s FLAC speed
                sprintf(txzipmsgptr+17, " %02d:%02d FLAC", eta_s/60, eta_s%60);
                master_txrx(zip_lcd_msg, 5+64); // write to LCD
                encode_wav_to_flac(file, full_path);
              }
            }
        }
        file = root.openNextFile();
    }
    // write to LCD
    if(lcd_n)
    {
      int limit_lcd_n = lcd_n > max_lcd_n ? max_lcd_n : lcd_n;
      spi_master_tx_buf[0] = 0; // 0: write ram
      spi_master_tx_buf[1] = 0xC; // addr [31:24] msb LCD addr
      spi_master_tx_buf[2] = 0; // addr [23:16] (0:normal, 1:invert)
      spi_master_tx_buf[3] = 0; // addr [15: 8]
      spi_master_tx_buf[4] = 1+(3<<5); // addr [ 7: 0] lsb HOME X=0 Y=3
      master_txrx(spi_master_tx_buf, 5+(limit_lcd_n<<5)); // write to LCD
    }
}
