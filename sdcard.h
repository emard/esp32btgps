#ifndef SDCARD_H
#define SDCARD_H

#include "FS.h"
#include "SD_MMC.h"
#include "RDS.h"
#include "nmea.h"
extern int card_is_mounted;
extern int pcm_is_open;
extern int sensor_check_status;
extern int speed_ckt; // centi-knots speed, 100 ckt = 1 kt = 0.514444 m/s = 1.852 km/h
extern int speed_mms; // mm/s speed
extern int speed_kmh; // km/h speed
extern int fast_enough; // logging flag when fast enough
extern int mode_obd_gps;
extern uint8_t gps_obd_configured;
extern float srvz_iri100, iri[2], iriavg, srvz2_iri20, iri20[2], iri20avg;
extern float temp[2]; // sensor temperature
extern char iri2digit[4];
extern char iri99avg2digit[4];
extern uint32_t iri99sum, iri99count, iri99avg; // collect session average
extern char lastnmea[256];
extern char *linenmea;
extern uint8_t last_sensor_reading[12];
// extern struct int_latlon last_latlon;
extern double last_dlatlon[2];
extern RDS rds;
// config file parsing
extern uint8_t GPS_MAC[6], OBD_MAC[6];
#define AP_MAX 16 /* max number of APs */
extern String  GPS_NAME, GPS_PIN, OBD_NAME, OBD_PIN, AP_PASS[], DNS_HOST;
extern uint32_t MS_SILENCE_RECONNECT;
extern uint8_t datetime_is_set;
extern struct tm tm, tm_session; // tm_session gives new filename when reconnected
extern uint64_t free_bytes;
extern uint32_t free_MB;
extern uint8_t sdcard_ok;
extern uint8_t log_wav_kml; // 1-wav 2-kml 3-both
extern uint8_t KMH_START, KMH_STOP;
extern uint8_t KMH_BTN;
extern uint8_t G_RANGE; // +-2/4/8 g sensor range for reading +-32000
extern uint8_t FILTER_ADXL355_CONF; // see datasheet adxl355 p.38 0:1kHz ... 10:0.977Hz
extern uint8_t FILTER_ADXRS290_CONF; // see datasheet adxrs290 p.11 0:480Hz ... 7:20Hz
extern float T_OFFSET_ADXL355_CONF[2]  ; // L,R
extern float T_SLOPE_ADXL355_CONF[2]   ; // L,R
extern float T_OFFSET_ADXRS290_CONF[2] ; // L,R
extern float T_SLOPE_ADXRS290_CONF[2]  ; // L,R
extern uint8_t KMH_REPORT1;
extern uint32_t MM_REPORT1, MM_REPORT2; // mm report each travel distance
extern uint32_t MM_SLOW;
extern uint32_t MM_FAST;
extern uint32_t fm_freq[2];
extern uint8_t fm_freq_cursor;
extern uint8_t btn, btn_prev;
extern File file_kml, file_accel, file_pcm, file_cfg;
extern uint64_t total_bytes, used_bytes, free_bytes;
extern uint32_t free_MB;
extern uint8_t finalize_busy;

void mount(void);
void umount(void);
void ls(void);
void SD_status();
int open_pcm(char *wav); // open wav filename
void open_logs(struct tm *tm);
void write_logs(void);
void write_string_to_wav(char *a);
void flush_logs(void);
void write_last_nmea(void);
void read_last_nmea(void);
void write_fmfreq(void);
void read_fmfreq(void);
void set_date_from_tm(struct tm *tm);
void write_log_kml(uint8_t force);
void write_stat_file(struct tm *tm);
void delete_stat_file(struct tm *tm);
int read_stat_file(String filename_stat);
void write_stat_arrows(void);
void write_csv_tm(struct tm *tm);
void finalize_data(struct tm *tm, uint8_t enable_compression);
void close_logs(void);
void read_cfg(void);
void store_last_sensor_reading(void);

#endif
