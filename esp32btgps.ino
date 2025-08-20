// Arduino 1.8.19
// preferences -> Additional Boards Manger URLs:
// https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

// boards manager -> esp32 v1.0.6 install
// well tested, works ok
// set Board->ESP32 Arduino->ESP32 Dev Module
// CPU Frequency: 240 MHz
// Partition Scheme: No OTA (2MB APP/2MB SPIFFS)
// Manage Libraries -> ESP32DMASPI v0.1.2 install
// #define IDF3 1
// #define IDF4 0

// boards manager -> esp32 v2.0.3 install
// in loop_web(): monitorWiFi() can not be used
// disable warnings for 4-byte alignment
// edit ~/Arduino/libraries/ESP32DMASPI/ESP32DMASPIMaster.h transfer()
// printf("[WARN] DMA buffer size must be multiples of 4 bytes\n");
// set Board->ESP32 Arduino->ESP32 Dev Module
// CPU Frequency: 240 MHz
// Partition Scheme: No OTA (2MB APP/2MB SPIFFS)
// Manage Libraries -> ESP32DMASPI v0.2.0 install
// #define IDF3 0
// #define IDF4 1

// Arduino 2.3.6
// Tools -> Boards Manager -> esp32 by Espressif Systems
// Board: "ESP32 Dev Module"
// CPU Frequency: "240 MHz (WiFi/BT)"
// Core Debug Level: "None"
// Erase All Flash Before Sketch Upload: "Enabled"
// Events Run On: "Core 1"
// Flash Frequency: "80 MHz"
// Arduino Runs On: "Core 1"
// Flash Mode: "QIO"
// Flash Size: "4MB (32Mb)"
// JTAG Adapter: "Disabled"
// Partition Scheme: "No OTA (2MB APP/2MB SPIFFS)"
// PSRAM: "Enabled" (geostat.h snap_point_max 1000 up to 100km)
// PSRAM: "Disabled" (geostat.h snap_point_max 50 up to 5km)
// ESP32 without PSRAM is useless and may reboot often
// #define IDF3 0
// #define IDF4 1

#include "pins.h"
#include "spidma.h"
#include "web.h"
#include "kml.h"
#include "geostat.h"
#include "version.h"
#include <sys/time.h>
#include <WiFi.h> // to speak IP

#include "BluetoothSerial.h"

// MCPWM API is different between IDF3/4
// define one as 1, other as 0:
#if __has_include("esp_idf_version.h")
  #include "esp_idf_version.h"
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    #define IDF3 0 // esp32 v1.0.x
    #define IDF4 1 // esp32 v2.0.x
  #else
    #define IDF3 1 // esp32 v1.0.x
    #define IDF4 0 // esp32 v2.0.x
  #endif
#else
  #define IDF3 1 // esp32 v1.0.x
  #define IDF4 0 // esp32 v2.0.x
#endif

// PPS and IRQ connected with wire
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "driver/mcpwm.h"
// SD card 4-bit mode
#include "sdcard.h"
// NMEA simple parsing for $GPRMC NMEA sentence
#include "nmea.h"

BluetoothSerial SerialBT;

// TODO: read address from SD card gps.mac
//uint8_t GPS_MAC[6] = {0x10, 0xC6, 0xFC, 0x84, 0x35, 0x2E};
// String GPS_NAME = "Garmin GLO #4352e"; // new

//uint8_t GPS_MAC[6] = {0x10, 0xC6, 0xFC, 0x14, 0x6B, 0xD0};
// String GPS_NAME = "Garmin GLO #46bd0"; // old from rpi box

// const char *GPS_PIN = "1234"; //<- standard pin would be provided by default

bool connected = false;
char *speakfile = NULL;
char *nospeak[] = {NULL};
char **speakfiles = nospeak;

// optional loop handler functions
void loop_run(void), loop_web(void);
void (*loop_pointer)() = &loop_run;

static char *digit_file[] =
{
  (char *)"/profilog/speak/0.wav",
  (char *)"/profilog/speak/1.wav",
  (char *)"/profilog/speak/2.wav",
  (char *)"/profilog/speak/3.wav",
  (char *)"/profilog/speak/4.wav",
  (char *)"/profilog/speak/5.wav",
  (char *)"/profilog/speak/6.wav",
  (char *)"/profilog/speak/7.wav",
  (char *)"/profilog/speak/8.wav",
  (char *)"/profilog/speak/9.wav",
  NULL
};
static char *speak2digits[] = {digit_file[0], digit_file[0], NULL, NULL};
static char *sensor_status_file[] =
{
  (char *)"/profilog/speak/nsensor.wav",
  (char *)"/profilog/speak/nright.wav",
  (char *)"/profilog/speak/nleft.wav",
  NULL
};
static char *speakaction[] = {(char *)"/profilog/speak/restart.wav", NULL, NULL};

static char *sensor_balance_file[] =
{
  NULL, // ok
  (char *)"/profilog/speak/lstrong.wav",
  (char *)"/profilog/speak/rstrong.wav",
};

static char *speakip[16] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, };

uint32_t t_ms; // t = ms();
uint32_t ct0; // first char in line millis timestamp
uint32_t ct0_prev; // for travel calculation
char line[256]; // incoming line of data from bluetooth GPS/OBD
int line_i = 0; // line index
char line_terminator = '\n'; // '\n' for GPS, '\r' for OBD
uint32_t line_tprev; // to determine time of latest incoming complete line
uint32_t line_tdelta; // time between prev and now
int travel_mm = 0; // travelled mm (v*dt)
int travel_report1, travel_report1_prev = 0; // previous 100 m travel
int travel_report2, travel_report2_prev = 0; // previous  20 m travel
int session_log = 0; // request new timestamp filename when reconnected
int speak_search = 0; // 0 - don't speak search, 1-search gps
uint32_t srvz[4];
int stopcount = 0;

// form nmea and travel in GPS mode;
int daytime = 0;
int daytime_prev = 0; // seconds x10 (0.1s resolution) for dt

char prompt_obd = '>'; // after prompt send obd_request_kmh
char *obd_request_kmh = (char *)"010D\r";
uint8_t obd_retry = 3; // bit pattern for OBD retry in case of silence

// int64_t esp_timer_get_time() returns system microseconds
int64_t IRAM_ATTR us()
{
  return esp_timer_get_time();
}

// better millis
uint32_t IRAM_ATTR ms()
{
  return (uint32_t) (esp_timer_get_time() / 1000LL);
}

// read raw 32-bit CPU ticks, running at 240 MHz, wraparound 18s
inline uint32_t IRAM_ATTR cputix()
{
  uint32_t ccount;
  asm volatile ( "rsr %0, ccount" : "=a" (ccount) );
  return ccount;
}

// cputix related
#define M 1000000
#define G 1000000000
#define ctMHz 240
#define PPSHz 10
// nominal period for PPSHz
#define Period1Hz 63999
// constants to help PLL
const int16_t phase_target = 0;
const int16_t period = 1000 / PPSHz;
const int16_t halfperiod = period / 2;

// rotated log of 256 NMEA timestamps and their cputix timestamps
uint8_t inmealog = 0;
int32_t nmea2ms_log[256]; // difference nmea-ms() log
int64_t nmea2ms_sum;
int32_t nmea2ms_dif;

// this ISR is software PLL that will lock to GPS signal
// by adjusting frequency of MCPWM signal to match
// time occurence of the ISR with averge difference between
// GPS clock and internal ESP32 clock.
// average difference is calculated at NMEA reception in loop()

// connect MCPWM PPS pin output to some input pin for interrupt
// workaround to create interrupt at each MCPWM cycle because
// MCPWM doesn't or I don't know how to generate software interrupt.
// the ISR will sample GPS tracking timer and calculate
// MCPWM period correction to make a PLL that precisely locks to
// GPS, it does the PPS signal recovery
static void IRAM_ATTR isr_handler()
{
  uint32_t ct = cputix(); // hi-resolution timer 18s wraparound
  static uint32_t ctprev;
  uint32_t t = ms();
  static uint32_t tprev;
  int32_t ctdelta2 = (ct - ctprev) / ctMHz; // us time between irq's
  ctprev = ct;
  int16_t phase = (nmea2ms_dif + t) % period;
  int16_t period_correction = (phase_target - phase + 2 * period + halfperiod) % period - halfperiod;
  //if(period_correction < -30 || period_correction > 30)
  //  period_correction /= 2; // fast convergence
  //else
  period_correction /= 4; // slow convergence and hysteresis around 0
  if (period_correction > 1530) // upper limit to prevent 16-bit wraparound
    period_correction = 1530;
  #if IDF3
  MCPWM0.timer[0].period.period = Period1Hz + period_correction;
  #endif
  #if IDF4
  MCPWM0.timer[0].timer_cfg0.timer_period = Period1Hz + period_correction;
  #endif
#if 0
  // debug PPS sync
  Serial.print(nmea2ms_dif, DEC); // average nmea time - ms() time
  Serial.print(" ");
  Serial.print(ctdelta2, DEC); // microseconds between each irq measured by CPU timer
  Serial.print(" ");
  Serial.print(phase, DEC); // less:PPS early, more:PPS late
  //Serial.print(" ");
  //Serial.print((uint32_t)us(), DEC);
  Serial.println(" irq");
#endif
}

/* test 64-bit functions */
#if 0
void test64()
{
  uint64_t G = 1000000000; // 1e9 giga
  uint64_t x = 72 * G; // 72e9
  uint64_t r = x + 1234;
  uint32_t y = x / G;
  uint32_t z = r % G;
  Serial.println(z, DEC);
}
#endif

// fill sum and log with given difference d
void init_nmea2ms(int32_t d)
{
  // initialize nmea to ms() statistics sum
  nmea2ms_sum = d * 256;
  for (int i = 0; i < 256; i++)
    nmea2ms_log[i] = d;
}

void setup() {
  Serial.begin(115200);
  //set_system_time(1527469964);
  //set_date_time(2021,4,1,12,30,45);
  //pinMode(PIN_BTN, INPUT);
  //attachInterrupt(PIN_BTN, isr_handler, FALLING);

  delay(1000); // easier ESP32 programming!
  // This 1s delay is not needed for normal function but
  // Without this delay, too many ESP32 programming retries are required or
  // "passthru" bitstream flashed in FPGA before programming ESP32

  spi_init();
  rds_init();
  spi_rds_write();
  clr_lcd();
  lcd_print(14,0,0,(char *)"MHz");

  int web = (spi_btn_read() & 2); // hold BTN1 and plug power to enable web server
  if(web)
  {
    loop_pointer = &loop_web;
    mount();
    read_cfg();
    read_fmfreq();
    set_fm_freq();
    read_last_nmea();
    finalize_data(&tm);
    lcd_print(22,0,0,(char *)"WiFi");
    lcd_print(0,1,0,(char *)"L");
    lcd_print(1,1,0,(char *)LOGGER_VERSION);
    lcd_print(7,1,0,(char *)"C");
    lcd_print(8,1,0,(char *)CORE_VERSION);
    web_setup();
    speakaction[0] = (char *)"/profilog/speak/webserver.wav"; // TODO say web server maybe IP too
    speakaction[1] = NULL;
    speakfiles = speakaction;
    return;
  }

  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PIN_PPS);   // Initialise channel MCPWM0A on PPS pin
  mcpwm_config_t pwm_config;
  pwm_config.frequency = PPSHz;
  pwm_config.cmpr_a = 0;
  pwm_config.cmpr_b = 0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
  mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, PPSHz); // Hz
  mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 10.0); // 10% DTC
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
  // above mcpwm functions doesn't set frequency with sufficient
  // precision for PPS 10 Hz signal (control LEDs will not light up).
  // following direct register access is required (control LEDs will light up).
  // freq will be fine-adjusted by phase-locking to GPS time data.
  #if IDF3
  MCPWM0.clk_cfg.prescale = 24;                      // Set the 160MHz clock prescaler to 24 (160MHz/(24+1)=6.4MHz)
  MCPWM0.timer[0].period.prescale = 100 / PPSHz - 1; // Set timer 0 prescaler to 9 (6.4MHz/(9+1))=640kHz)
  MCPWM0.timer[0].period.period = 63999;             // Set the PWM period to 10Hz (640kHz/(63999+1)=10Hz)
  MCPWM0.channel[0].cmpr_value[0].val = 6400;        // Set the counter compare for 10% duty-cycle
  MCPWM0.channel[0].generator[0].utez = 2;           // Set the PWM0A output to go high at the start of the timer period
  MCPWM0.channel[0].generator[0].utea = 1;           // Clear on compare match
  MCPWM0.timer[0].mode.mode = 1;                     // Set timer 0 to increment
  MCPWM0.timer[0].mode.start = 2;                    // Set timer 0 to free-run
  #endif
  #if IDF4
  MCPWM0.clk_cfg.clk_prescale = 24;                  // Set the 160MHz clock prescaler to 24 (160MHz/(24+1)=6.4MHz)
  MCPWM0.timer[0].timer_cfg0.timer_period_upmethod = 0; // immediate update
  MCPWM0.timer[0].timer_cfg0.timer_prescale = 100 / PPSHz - 1; // Set timer 0 prescaler to 9 (6.4MHz/(9+1))=640kHz)
  MCPWM0.timer[0].timer_cfg0.timer_period = 63999;   // Set the PWM period to 10Hz (640kHz/(63999+1)=10Hz)
  MCPWM0.timer[0].timer_cfg0.val = 0;
  MCPWM0.operators[0].gen_cfg0.gen_cfg_upmethod = 0; // immediate update
  MCPWM0.operators[0].generator[0].val = 6400;       // Set the counter compare for 10% duty-cycle
  MCPWM0.operators[0].generator[0].gen_utez = 2;     // Set the PWM0A output to go high at the start of the timer period
  MCPWM0.operators[0].generator[0].gen_utea = 1;     // Clear on compare match
  MCPWM0.timer[0].timer_cfg1.timer_mod = 1;          // Set timer 0 to increment
  MCPWM0.timer[0].timer_cfg1.timer_start = 2;        // Set timer 0 to free-run
  #endif

  t_ms = ms();
  line_tprev = t_ms-5000;
  // line_tprev sets 5s silence in the past. reconnect comes at 10s silence.
  // speech starts at 7s silence. Before speech, 2s must be allowed
  // for sensors to start reading data, otherwise it will false report
  // "no sensors".
  linenmea = line; // they are same and never change
  kml_init(); // sets globals using strlen/strstr

  pinMode(PIN_IRQ, INPUT);
  attachInterrupt(PIN_IRQ, isr_handler, RISING);

  init_nmea2ms(0);
  mount();
  read_cfg();
  read_fmfreq();
  set_fm_freq();
  read_last_nmea();
  umount();
  // calculate_grid(46); // TODO recalculate later from first nmea in session
  clear_storage();

  // accelerometer range +-2/4/8g can be changed from cfg file
  // ADXL should be initialized after reading cfg file
  delay(100);
  cold_init_sensors();
  init_srvz_iri(); // depends on sensor type detected

  SerialBT.begin("PROFILOG", true);
  SerialBT.setPin((const char *)GPS_PIN.c_str(), GPS_PIN.length());
  Serial.println("Bluetooth master started");
  Serial.println(esp_get_idf_version()); // v4.4-beta1-189-ga79dc75f0a
  //printf("sizeof(s_stat) = %d\n", sizeof(s_stat));
  speakaction[0] = (char *)"/profilog/speak/restart.wav";
  speakaction[1] = NULL;
  speakfiles = speakaction;

  reset_kml_line(x_kml_line);
  spi_speed_write(0); // normal
}

#if 0
void reconnect()
{
  // connect(address) is fast (upto 10 secs max), connect(name) is slow (upto 30 secs max) as it needs
  // to resolve name to address first, but it allows to connect to different devices with the same name.
  // Set CoreDebugLevel to Info to view devices bluetooth address and device names

  //connected = SerialBT.connect(name); // slow with String name
  //connected = SerialBT.connect(GPS_MAC); // fast with uint8_t GPS_MAC[6]

  if(bt_name.length())
    connected = SerialBT.connect(bt_name); // 30s connect with String name
  else
    connected = SerialBT.connect(mac_address); // 15s connect with uint8_t GPS_MAC[6] but some devices reboot esp32

  // return value "connected" doesn't mean much
  // it is sometimes true even if not connected.
}
#endif

#if 0
void set_date_time(int year, int month, int day, int h, int m, int s)
{
  time_t t_of_day;
  struct tm t;

  t.tm_year = year - 1900; // year since 1900
  t.tm_mon  = month - 1;  // Month, 0 - jan
  t.tm_mday = day;        // Day of the month
  t.tm_hour = h;
  t.tm_min  = m;
  t.tm_sec  = s;
  t_of_day  = mktime(&t);
  set_system_time(t_of_day);
}
#endif


#if 0
// debug tagger: constant test string
char tag_test[256] = "$ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789*00\n";
#endif

// speech handler, call from loop()
// plays sequence of wav files
// speakfiles = {(char *)"/profilog/speak/file1.wav", (char *)"/profilog/speak/file2.wav", ... , NULL };
void speech()
{
  static uint32_t tprev_wav = t_ms, tprev_wavp = t_ms;
  static uint32_t tspeak_ready;
  uint32_t tdelta_wav, tdelta_wavp;
  if (speakfile == NULL && pcm_is_open == 0) // do we have more files to speak?
  {
    if(speakfiles)
      if(*speakfiles)
        speakfile = *speakfiles++;
  }
  // before starting new word we must check
  // that previous word has been spoken
  // both speech end timing methods work
#if 1
  // coarse estimate of speech end
  tdelta_wavp = t_ms - tprev_wavp; // estimate time after last word is spoken
  if (speakfile != NULL && pcm_is_open == 0 && tdelta_wavp > 370) // 370 ms after last word
#else
  // fine estimate of speech end
  if (speakfile != NULL && pcm_is_open == 0 && (((int32_t)t_ms) - (int32_t)tspeak_ready) > 0) // NULL: we are ready to speak new file,
#endif
  {
    // start speech
    mount();
    if(card_is_mounted)
    {
      open_pcm(speakfile); // load buffer with start of the file
      if(pcm_is_open)
      {
        tprev_wavp = ms(); // reset play timer from now, after start of PCM file
        tspeak_ready = tprev_wavp+370;
        tprev_wav = t_ms; // prevent too often starting of the speech
      }
      else
      {
        beep_pcm(1024); // beep if it can't speak (open failed)
        speakfile = NULL; // consumed
      }
    }
    else
    {
      beep_pcm(1024); // beep if it can't speak (mount failed)
      speakfile = NULL; // consumed
    }
  }
  else
  {
    // continue speaking from remaining parts of the file
    // refill wav-play buffer
    tdelta_wavp = t_ms - tprev_wavp; // how many ms have passed since last refill
    if (tdelta_wavp > 200 && pcm_is_open) // 200 ms is about 2.2KB to refill
    {
      int remaining_bytes;
      remaining_bytes = play_pcm(tdelta_wavp * 11); // approx 11 samples per ms at 11025 rate
      tprev_wavp = t_ms;
      if (!pcm_is_open)
      {
        speakfile = NULL; // consumed
        tspeak_ready = t_ms + remaining_bytes / 11 + 359; // estimate when PCM will be ready
      }
    }
  }
}

#if 0
// terminal mode
void loop_obd() {
  if (Serial.available()) {
    SerialBT.write(Serial.read());
    delay(20); // OBD needs slow char-by-char input
  }
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
  speech();
}
#endif

void report_search(void)
{
  if (*speakfiles == NULL && pcm_is_open == 0) // NULL: we are ready to speak new file,
  {
    // After 7s silence, report searching for GPS/OBD.
    // It must speak early to finish before reconnect.
    // At 10s silence bluetooth recoonect starts.
    // During reconnect, CPU is busy and can not 
    // feed speech data.
    if(line_tdelta > 7000 && speak_search > 0)
    {
      int mode_xor = gps_obd_configured == 3 ? 1 : 0; // if both then alternate speech in advance
      if(mode_obd_gps ^ mode_xor)
        speakaction[0] = (char *)"/profilog/speak/searchgps.wav";
      else
        speakaction[0] = (char *)"/profilog/speak/searchobd.wav";
      speakaction[1] = sensor_status_file[sensor_check_status]; // normal
      #if 0 // debug false report all combinations no sensors
      static uint8_t x = 0;
      if(++x > 2) x = 0;
      speakaction[1] = sensor_status_file[x];
      #endif
      speakfiles = speakaction;
      speak_search = 0; // consumed, next BT connect will enable it
    }
  }
}

// every 100m travel report IRI
// with speech and RDS
void report_iri(void)
{
  uint8_t flag_report = 0;
  if (travel_report1_prev != travel_report1) // normal: update RDS every 100 m
  {
    travel_report1_prev = travel_report1;
    if(speed_kmh >= (int)KMH_REPORT1)
      flag_report = 1;
  }
  if (travel_report2_prev != travel_report2) // normal: update RDS every 20 m
  {
    travel_report2_prev = travel_report2;
    if(speed_kmh < (int)KMH_REPORT1)
      flag_report = 2;
  }
  if (flag_report)
  {
    // for speed above 30 km/h, report IRI-100
    // for speed below 30 km/h, report IRI-20
    uint8_t iri99 = 0;
    if(flag_report == 1) // IRI-100 normal
      iri99 = iriavg*10;
    else // flag_report == 2 // IRI-20 speed < 30 km/h
      iri99 = iri20avg*10;
    if(iri99 > 99)
      iri99 = 99;
    else // iri99 < 99
    {
      if(flag_report == 1) // average IRI-100 readings with iri < 9.9
      {
        iri99sum += iri99;
        iri99count++;
        iri99avg = iri99sum/iri99count;
        iri99avg2digit[0]='0'+iri99avg/10;
        iri99avg2digit[2]='0'+iri99avg%10;
      }
    }
    iri2digit[0]='0'+iri99/10;
    iri2digit[2]='0'+iri99%10;
    // strcpy(lastnmea, line); // copy line to last nmea as tmp buffer
    rds_message(&tm);
    if (speakfile == NULL && pcm_is_open == 0 && fast_enough > 0)
    {
      speak2digits[0] = digit_file[iri2digit[0]-'0'];
      speak2digits[1] = digit_file[iri2digit[2]-'0'];
      int balance = 0;
      if(sensor_check_status == 3)
        balance = (srvz[0]>>1) > srvz[1] ? 1
                : (srvz[1]>>1) > srvz[0] ? 2
                : 0;
      speak2digits[2] = sensor_balance_file[balance];
      speakfiles = speak2digits;
    }
  }
}

// every minute report status
// with speech and RDS
// search, signal, sensor presence
void report_status(void)
{
  static uint8_t prev_min;
  if (tm.tm_min != prev_min)
  { // speak every minute
    // TODO this file save parts should go to
    // separate function
    flush_logs(); // save data just in case
    if(sensor_check_status)
      warm_init_sensors();
    else
    {
      cold_init_sensors();
      init_srvz_iri();
    }
    if (speakfile == NULL && *speakfiles == NULL && pcm_is_open == 0)
    {
      rds_message(&tm);
      if (speed_ckt < 0) // no signal
      {
        speakaction[0] = (char *)"/profilog/speak/wait.wav";
        speakaction[1] = sensor_status_file[sensor_check_status];
      }
      else
      {
        if (fast_enough)
        {
          //speakaction[0] = (char *)"/profilog/speak/record.wav";
          speakaction[0] = sensor_status_file[sensor_check_status];
          speakaction[1] = NULL;
        }
        else
        {
          if(sensor_check_status)
          { // at least one sensor
            if(free_MB >= 8)
              speakaction[0] = (char *)"/profilog/speak/ready.wav";
            else
              speakaction[0] = (char *)"/profilog/speak/nfree.wav";
            speakaction[1] = sensor_status_file[sensor_check_status];
          }
          else
          { // no sensors
            speakaction[0] = sensor_status_file[sensor_check_status];
            speakaction[1] = NULL;
          }
        }
      }
      speakfiles = speakaction;
      speak_search = 0;
    }
    prev_min = tm.tm_min;
  }
}

// tm must be set now
// open logs if fast enough
// when reconnected it's a new "session"
// and logs are open with new filename
// from tm_session timestamp
void handle_session_log(void)
{
  if(fast_enough)
  {
    if(session_log == 0)
    {
      tm_session = tm;
      session_log = 1;
    }
    if(session_log != 0)
    {
      mount();
      open_logs(&tm_session);
    }
  }
}

// calculate travel from
// speed and internal timer
// used for OBD
void travel_ct0(void)
{
  if(fast_enough)
  {
    int travel_dt = ((int32_t) ct0) - ((int32_t) ct0_prev); // ms since last time
    if(travel_dt < 10000) // ok reports are below 10s difference
      travel_mm += speed_mms * travel_dt / 1000;
    travel_report1 = travel_mm / MM_REPORT1; // normal: report every 100 m
    travel_report2 = travel_mm / MM_REPORT2; // normal: report every  20 m
    //travel_report1 = travel_mm / 1000; // debug: report every 1 m
  }
  // set ct0_prev always (also when stopped)
  // to prevent building large delta time when fast enough
  ct0_prev = ct0; // for travel_dt
}

void handle_fast_enough(void)
{
  if (speed_kmh > KMH_START) // normal
  {
    if (fast_enough == 0)
    {
      Serial.print(speed_kmh);
      Serial.println(" km/h fast enough - start logging");
    }
    fast_enough = 1;
  }
  if (speed_kmh < KMH_STOP && speed_kmh >= 0) // normal
  { // tunnel mode: ignore negative speed (no signal) when fast enough
    if (fast_enough)
    {
      char stop_delimiter[40];
      snprintf(stop_delimiter, sizeof(stop_delimiter), " # TL%.1fTR%.1f*00 ", temp[0], temp[1]);
      write_nmea_crc(stop_delimiter+3);
      write_string_to_wav(stop_delimiter);
      close_logs(); // save data in case of power lost
      write_last_nmea();
      if(s_stat.wr_snap_ptr != 0)
        write_stat_file(&tm_session);
      reset_kml_line(x_kml_line);
      stopcount++;
      Serial.print(speed_kmh);
      Serial.println(" km/h not fast enough - stop logging");
      travel_mm = 0; // we stopped, reset travel
    }
    fast_enough = 0;
  }
}

// call this after read_cfg and chip detection
// ADXL355 needs G_RANGE
void init_srvz_iri(void)
{
  // default ADXRS290, ignore G_RANGE, use as if G_RANGE=2
  srvz_iri100 = 0.5e-6;
  srvz2_iri20 = 2.5e-6;
  if(adxl_devid_detected == 0xED) // ADXL355
  {
    // 1e3  for IRI  [mm/m]
    // 1e-6 for srvz [um/m]
    // 1e-3 = 1e3 * 1e-6
    // 0.5e-6 = (1e-3 * 0.05/100) 2g range, coefficients_pack.vhd: interval_mm :=  50; length_m := 100
    srvz_iri100 = G_RANGE == 2 ? 0.5e-6 : G_RANGE == 4 ? 1.0e-6 : /* G_RANGE == 8 ? */  2.0e-6 ; // normal IRI-100
    srvz2_iri20 = G_RANGE == 2 ? 2.5e-6 : G_RANGE == 4 ? 5.0e-6 : /* G_RANGE == 8 ? */ 10.0e-6 ; // normal IRI-20
    // 2.5e-6 = (1e-3 * 0.25/100) 2g range, coefficients_pack.vhd: interval_mm := 250; length_m := 100
    // 2.5e-6 = (1e-3 * 0.05/20 ) 2g range, coefficients_pack.vhd: interval_mm :=  50; length_m :=  20
    //srvz_iri100 = G_RANGE == 2 ?  2.5e-6 : G_RANGE == 4 ?  5.0e-6 : /* G_RANGE == 8 ? */ 10.0e-6 ;
    //srvz2_iri20 = G_RANGE == 2 ? 12.5e-6 : G_RANGE == 4 ? 25.0e-6 : /* G_RANGE == 8 ? */ 50.0e-6 ;
    Serial.println("ADXL355");
  }
  else
    Serial.println("ADXRS290");
}

void get_iri(void)
{
  spi_srvz_read(srvz);
  iri[0] = srvz[0]*srvz_iri100;
  iri[1] = srvz[1]*srvz_iri100;
  iriavg = sensor_check_status == 0 ? 0.0
         : sensor_check_status == 1 ? iri[0]
         : sensor_check_status == 2 ? iri[1]
         : (iri[0]+iri[1])/2;  // 3, average of both sensors
  iri20[0] = srvz[2]*srvz2_iri20;
  iri20[1] = srvz[3]*srvz2_iri20;
  iri20avg = sensor_check_status == 0 ? 0.0
         : sensor_check_status == 1 ? iri20[0]
         : sensor_check_status == 2 ? iri20[1]
         : (iri20[0]+iri20[1])/2;  // 3, average of both sensors
}

void handle_reconnect(void)
{
  //if(session_log != 0) // if session enabled
  if(s_stat.wr_snap_ptr != 0 && session_log != 0) // if session enabled and any stat arrows are in storage
  {
    mount();
    if(card_is_mounted)
    {
      open_logs(&tm_session);
      write_stat_arrows(); // write arrows with final statistics
      delete_stat_file(&tm_session);
    }
  }
  close_logs();
  write_last_nmea();
  session_log = 0; // request new timestamp file name when reconnected
  finalize_data(&tm); // finalize all except current session (if one file per day)
  umount();
  clear_storage(); // finalize reads .sta file to s_stat, here we clear s_stat
  // this fixes when powered on while driving with fast_enough speed,
  // it prevents long line over the globe from lat=0,lon=0 to current point
  reset_kml_line(x_kml_line);
  iri99sum = iri99count = iri99avg = 0; // reset iri99 average

  if(sensor_check_status)
    warm_init_sensors();
  else
  {
    cold_init_sensors();
    init_srvz_iri();
  }

  if(gps_obd_configured == 3)
    mode_obd_gps ^= 1; // toggle mode 0:OBD 1:GPS
  else
  {
    if((gps_obd_configured & 1))
      mode_obd_gps = 0;
    if((gps_obd_configured & 2))
      mode_obd_gps = 1;
  }
  String   bt_name     = mode_obd_gps ? GPS_NAME : OBD_NAME;
  uint8_t *mac_address = mode_obd_gps ? GPS_MAC  : OBD_MAC;
  rds_message(NULL);
  //Serial.print("trying mode ");
  //Serial.println(mode_obd_gps);

  // connect(address) is fast (upto 10 secs max), connect(name) is slow (upto 30 secs max) as it needs
  // to resolve name to address first, but it allows to connect to different devices with the same name.
  // Set CoreDebugLevel to Info to view devices bluetooth address and device names

  if(bt_name.length())
    connected = SerialBT.connect(bt_name); // 30s connect with String name
  else
    connected = SerialBT.connect(mac_address); // 15s connect with uint8_t GPS_MAC[6] but some devices reboot esp32

  // return value "connected" doesn't mean much
  // it is sometimes true even if not connected.
  line_terminator = mode_obd_gps ? '\n' : '\r';
  obd_retry = ~0; // initial retry OBD to start reports
  datetime_is_set = 0; // set datetime again
  line_i = 0; // reset line buffer write pointer
  speed_ckt = speed_mms = speed_kmh = -1; // reset speeds to reset tunnet mode when GPS connection is lost
  speak_search = 1; // request speech report on searching for GPS/OBD
}

// report based on GPS daytime
// used for GPS
void travel_gps(void)
{
  if(fast_enough)
  {
    int travel_dt = (864000 + daytime - daytime_prev) % 864000; // seconds*10 since last time
    if(travel_dt < 100) // ok reports are below 10s difference
      travel_mm += speed_mms * travel_dt / 10;
    travel_report1 = travel_mm / MM_REPORT1; // normal: report every 100 m
    travel_report2 = travel_mm / MM_REPORT2; // normal: report every  20 m
  }
  daytime_prev = daytime; // for travel_dt
}

// log nmea time for PPS phase lock
void nmea_time_log(void)
{
  daytime = nmea2s(line + 7); // x10, 0.1s resolution
  int32_t nmea2ms = daytime * 100 - ct0; // difference from nmea to timer
  if (nmea2ms_sum == 0) // sum is 0 only at reboot
    init_nmea2ms(nmea2ms); // speeds up convergence
  nmea2ms_sum += nmea2ms - nmea2ms_log[inmealog]; // moving sum
  nmea2ms_dif = nmea2ms_sum / 256;
  nmea2ms_log[inmealog++] = nmea2ms;
}

// from nmea line draw a kml line,
// remembers last point, keeps alternating 2-points
void draw_kml_line(char *line)
{
  static int ipt = 0; // current point index, alternates 0/1
  static char timestamp[23] = "2000-01-01T00:00:00.0Z";
  if(log_wav_kml&2)
  { // only if kml mode is enabled, save CPU if when not enabled
    // strcpy(lastnmea, line); // copy line to last nmea as tmp buffer (overwritten by parser)
    // parse lastnmea -> ilatlon (parsing should not spoil nmea string)
    // nmea2latlon(line, &ilatlon);
    // latlon2double(&ilatlon, &(x_kml_line->lat[ipt]), &(x_kml_line->lon[ipt]));
    nmea2dlatlon(line, &(x_kml_line->lat[ipt]), &(x_kml_line->lon[ipt]));
    if(fabs(x_kml_line->lat[0]) <= 90.0 && fabs(x_kml_line->lat[1]) <= 90.0)
    {
      x_kml_line->value     = iri20avg;
      x_kml_line->left20    = iri20[0];
      x_kml_line->right20   = iri20[1];
      x_kml_line->left100   = iri[0];
      x_kml_line->right100  = iri[1];
      x_kml_line->speed_kmh = speed_mms*3.6e-3;
      nmea2kmltime(line, timestamp);
      x_kml_line->timestamp = timestamp;
      kml_line(x_kml_line);
      #if 0
      // TODO: don't write arrows now, but
      // collect statistics and write it
      // after GPS is turned off.
      if(travel_report1_prev != travel_report1) // every 100m draw arrow
      {
        x_kml_arrow->lat   = x_kml_line->lat[ipt];
        x_kml_arrow->lon   = x_kml_line->lon[ipt];
        x_kml_arrow->value = iriavg;
        x_kml_arrow->left  = iri[0];
        x_kml_arrow->left_stdev = 0.0;
        x_kml_arrow->right = iri[1];
        x_kml_arrow->right_stdev = 0.0;
        x_kml_arrow->n = 1;
        x_kml_arrow->speed_kmh = x_kml_line->speed_kmh;
        char *b = nthchar(line, 8, ','); // position to heading
        char str_heading[5] = "0000"; // storage for parsing
        str_heading[0] = b[1];
        str_heading[1] = b[2];
        str_heading[2] = b[3];
        str_heading[3] = b[5]; // skip b[4]=='.'
        //str_heading[4] = 0;
        int iheading = strtol(str_heading, NULL, 10); // parse as integer
        x_kml_arrow->heading   = iheading*0.1; // convert to float
        x_kml_arrow->timestamp = timestamp;
        kml_arrow(x_kml_arrow);
      }
      #endif
      write_log_kml(0); // normal
    }
    //write_log_kml(1); // debug (for OBD)
    ipt ^= 1; // toggle 0/1
  }
}

#define NEAR_FM_FREQ_HZ 300000
int is_near_frequency(void)
{
  return fm_freq[fm_freq_cursor-1] > fm_freq[(fm_freq_cursor ^ 3)-1] - NEAR_FM_FREQ_HZ
      && fm_freq[fm_freq_cursor-1] < fm_freq[(fm_freq_cursor ^ 3)-1] + NEAR_FM_FREQ_HZ;
}

void btn_handler(void)
{
  static uint32_t next_ms;
  static uint32_t next_cursor_ms;
  static uint8_t write_required;
  const uint32_t hold_ms = 400, repeat_ms = 200, cursor_ms = 5000;
  btn = spi_btn_read();
  uint8_t ev_btn_press = 0;
  if(btn != btn_prev)
  {
    btn_prev = btn;
    ev_btn_press = 1;
    next_ms = t_ms+hold_ms;
  }
  uint8_t ev_btn_repeat = 0;
  if( btn != 1 ) // if any button is pressed
  if( (int32_t)t_ms - (int32_t)next_ms > 0 )
  {
    ev_btn_repeat = 1;
    next_ms = t_ms+repeat_ms;
  }

  if(fm_freq_cursor) // if cursor in ON
  if( (int32_t)t_ms - (int32_t)next_cursor_ms > 0 )
  {
    if(write_required)
    {
      if(card_is_mounted)
      { // write then turn off cursor
        write_fmfreq();
        write_required = 0;
        fm_freq_cursor = 0;
        set_fm_freq(); // update LCD display with cursor removed
      }
    }
    else // write not required, turn off cursor
    {
      fm_freq_cursor = 0;
      set_fm_freq(); // update LCD display with cursor removed
    }
  }
  
  if( (ev_btn_press | ev_btn_repeat) )
  {
    if(btn & 8) // up: increase freq
    {
      if(fm_freq_cursor == 1 || fm_freq_cursor == 2)
      {
        if((ev_btn_repeat))
          fm_freq[fm_freq_cursor-1] = (fm_freq[fm_freq_cursor-1]/1000000 + 1) * 1000000;
        else
          fm_freq[fm_freq_cursor-1] += 50000;
        if(is_near_frequency()) // skip near frequency of other antenna
          fm_freq[fm_freq_cursor-1] = fm_freq[(fm_freq_cursor ^ 3)-1] + NEAR_FM_FREQ_HZ;
        if(fm_freq[fm_freq_cursor-1] > 108000000)
          fm_freq[fm_freq_cursor-1] = 108000000;
        write_required = 1;
        next_cursor_ms = t_ms + cursor_ms;
      }
    }
    if(btn & 16) // down: decrease freq
    {
      if(fm_freq_cursor == 1 || fm_freq_cursor == 2)
      {
        if(ev_btn_repeat)
          fm_freq[fm_freq_cursor-1] = ((fm_freq[fm_freq_cursor-1]+999999)/1000000 - 1) * 1000000;
        else
          fm_freq[fm_freq_cursor-1] -= 50000; // normal
        if(is_near_frequency()) // skip near frequency of other antenna
          fm_freq[fm_freq_cursor-1] = fm_freq[(fm_freq_cursor ^ 3)-1] - NEAR_FM_FREQ_HZ;
        if(fm_freq[fm_freq_cursor-1] < 87500000)
          fm_freq[fm_freq_cursor-1] = 87500000;
        write_required = 1;
        next_cursor_ms = t_ms + cursor_ms;
      }
    }
    if(btn & 32) // left: cursor to 1st freq
    {
      if(fm_freq_cursor == 1)
        fm_freq_cursor = 0;
      else
      {
        fm_freq_cursor = 1;
        next_cursor_ms = t_ms + cursor_ms;
      }
    }
    if(btn & 64) // right: cursor to 2nd freq
    {
      if(fm_freq_cursor == 2)
        fm_freq_cursor = 0;
      else
      {
        fm_freq_cursor = 2;
        next_cursor_ms = t_ms + cursor_ms;
      }
    }
    if((btn & (8|16|32|64)))
      set_fm_freq();
  }
}

void handle_gps_line_complete(void)
{
  line[line_i-1] = 0; // replace \n termination with 0
  //Serial.println(line);
  //if(nmea[1]=='P' && nmea[3]=='R') // print only $PGRMT, we need Version 3.00
  if (line_i > 38 && line_i < 90 // accept lines of expected length
  &&  line[1] == 'G' // accept 1st letter is G
  &&  line[3] == 'R' && line[4] == 'M' && line[5] == 'C') // accept 3,4,5th letters are RMC or 4th is G, accept $GPRMC and $GPGGA
  {
    if (check_nmea_crc(line)) // filter out NMEA sentences with bad CRC
    {
      // Serial.println(line);
      // there's bandwidth for only one NMEA sentence at 10Hz (not two sentences)
      // time calculation here should receive no more than one NMEA sentence for one timestamp
      write_tag(line); // write as early as possible, but BTN debug can't change speed
      speed_ckt = nmea2spd(line); // parse speed to centi-knots, -1 if no signal
      if(KMH_BTN) // debug
      {
        // int btn = spi_btn_read();    // debug
        if((btn & 4))
        {
          speed_ckt = KMH_BTN*54; // debug BTN2 4320 ckt = 80 km/h = 22 m/s
          //spd2nmea(line, speed_ckt); // debug: write new ckt to nmea line (FIXME CRC is not recalculated)
          //write_nmea_crc(line+1); // debug CRC for NMEA part
        }
        if((btn & 8)) speed_ckt = -1;   // debug BTN3 tunnel, no signal
        if((btn & 16)) speed_ckt = 0;   // debug BTN3 stop
      }
      //write_tag(line); // debug write after BTN2 has written speed_ckt
      if(speed_ckt >= 0) // for tunnel mode keep speed if no signal (speed_ckt < 0)
      {
        speed_mms = (speed_ckt *  5268) >> 10;
        speed_kmh = (speed_ckt * 19419) >> 20;
      }
      spi_speed_write(fast_enough > 0 ? speed_mms : 0);
      nmea_time_log();
      write_logs();
      handle_fast_enough();
      if (nmea2tm(line, &tm))
      {
        static uint8_t toggle_flag = 0; // IRI-100/20 toggle flag
        get_iri();
        //printf("%10d %10d %10d %10d\n", srvz[0], srvz[1], srvz[2], srvz[3]); // debug
        char iri_tag[40];
        if(toggle_flag)
          sprintf(iri_tag, " L%05.2fR%05.2f*00 ",
            iri[0]>99.99?99.99:iri[0],
            iri[1]>99.99?99.99:iri[1]);
        else
          sprintf(iri_tag, " L%05.2fS%05.2f*00 ",
            iri20[0]>99.99?99.99:iri20[0],
            iri20[1]>99.99?99.99:iri20[1]);
        write_nmea_crc(iri_tag+1);
        write_tag(iri_tag);
        set_date_from_tm(&tm);
        handle_session_log(); // will open logs if fast enough (new filename when reconnected)
        travel_gps(); // calculate travel length
        strcpy(lastnmea, line); // copy line to last nmea for storage
        // prevent long line jumps from last stop to current position:
        // when signal inbetween was lost or cpu rebooted
        if(speed_ckt >= 0 && fast_enough > 0) // valid GPS FIX signal and moving, draw line and update statistics
        {
          draw_kml_line(line);
          stat_speed_kmh = speed_mms*3.6e-3;
          stat_nmea_proc(line, line_i-1);
        }
        #if 0
        // TODO tunnel mode
        if(speed_ckt < 0 && fast_enough > 0) // no GPS fix but fast enough, tunnel mode
        {
          // generate line with incrementing steps
          // constant speed, keep direction, ignore earth curvature,
          // similar to OBD, see obd_line_complete() below
        }
        #endif
        report_iri();
        report_status();
        toggle_flag ^= 1; // 0/1 alternating IRI-100 and IRI-20, no bandwidth for both
      }
    }
  }
  #if 0
  else
  {
    // Android 15 Samsung A25 will be silent if no GPS signal.
    // without signal it doesn't send GPRMC.
    // on BT connect it just sends one
    // $PSAMAID,tPe*2E
    // after that it is silent.
    // with very weak signal it sends
    // $GPRMC,,V,,,,,,,,,,N*53
    // currently in such condition no sentences from A25 contain time
    // with some signal but without FIX it sends like
    // $GPRMC,082703.000,V,,,,,,,180825,,,N*45
    if (line_i > 30 && line_i < 90 // accept lines of expected length
    && line[1] == 'P' // accept 1st letter is P
    && line[2] == 'S' && line[3] == 'A' && line[4] == 'M' && line[5] == 'C')
    // accept 2,3,4,5th letters are SAMC for PSAMCLK
    {
      // Serial.println(line);
      // TODO nmea.cpp parser for different NMEA sentence
    }
  }
  #endif
}

// convert OBD reading to a fake NMEA line
// and proceed same as NMEA
void handle_obd_line_complete(void)
{
  //line[line_i-1] = 0; // replace \r termination with 0
  //write_tag(line); // debug
  //Serial.println(line); // debug
  if(KMH_BTN) // debug
  if(line[1] == 'E') // match 'E' in "SEARCHING"
  {
    strcpy(line, "00 00 00"); // debug last hex represents 0 km/h
    // int btn = spi_btn_read(); // debug
    if((btn & 4)) sprintf(line, "00 00 %02X", KMH_BTN); // debug BTN2 default 80 km/h
  }
  if(line_i > 0 && line[line_i-1] == prompt_obd)
    SerialBT.print(obd_request_kmh); // next request
  else
  if(line[0] == 'S' // strcmp(line,"STOPPED") == 0
  || line[0] == 'U' // strcmp(line,"UNABLE TO CONNECT") == 0
  || line[0] == 'N' // strcmp(line,"NO DATA") == 0
  || line[0] == 'E' // strcmp(line,"ERR93") == 0
  )
    speed_kmh = -1; // negative means no signal (engine not connected)
  else
  if(line_i >= 8 && line[5] == ' ') // >8 bytes long and 5th byte is space
    // "00 00 00" ignore first 2 hex, last 3rd hex integer km/h
    // parse last digit
    speed_kmh = strtol(line+6, NULL ,16); // normal
  speed_mms = (speed_kmh * 284444) >> 10; // mm/s
  speed_ckt = (speed_kmh *  52679) >> 10; // centi-knots
  spi_speed_write(fast_enough ? speed_mms : 0); // normal

  if(speed_kmh > 0)
  {
    char iri_tag[120]; // fake time and location
    // TODO calculate travel coefficient for any part of globe NSEW
    int travel_lat = (travel_mm * 138)>>8; // coarse approximate mm to microminutes
    int travel_lon = stopcount << 16; // microminutes position next line using stopcount
    struct int_latlon last_latlon, fake_latlon;
    get_iri();
    // TODO support negative umin (lat S, lon W)
    last_latlon.lat_deg  =  last_dlatlon[0]; // converts to integer
    last_latlon.lat_umin = (last_dlatlon[0]-last_latlon.lat_deg)*60E6; // converts to integer
    last_latlon.lon_deg  =  last_dlatlon[1]; // converts to integer
    last_latlon.lon_umin = (last_dlatlon[1]-last_latlon.lon_deg)*60E6; // converts to integer
    fake_latlon.lat_deg  = last_latlon.lat_deg  + (abs(last_latlon.lat_umin) + travel_lat)/60000000;
    fake_latlon.lat_umin =                        (abs(last_latlon.lat_umin) + travel_lat)%60000000;
    fake_latlon.lon_deg  = last_latlon.lon_deg  + (abs(last_latlon.lon_umin) + travel_lon)/60000000;
    fake_latlon.lon_umin =                        (abs(last_latlon.lon_umin) + travel_lon)%60000000;
    // this fake NMEA has the same format like real NMEA from GPS
    // different is magnetic north, here is 000.0, GPS has nonzero like 003.4
    sprintf(iri_tag,
" $GPRMC,%02d%02d%02d.0,V,%02d%02d.%06d,%c,%03d%02d.%06d,%c,%03d.%02d,000.0,%02d%02d%02d,000.0,E,N*00 L%05.2fR%05.2f*00 ",
          tm.tm_hour, tm.tm_min, tm.tm_sec,      // hms
          fake_latlon.lat_deg,
          abs(fake_latlon.lat_umin)/1000000,
          abs(fake_latlon.lat_umin)%1000000,
          fake_latlon.lat_umin >= 0 ? 'N' : 'S',
          fake_latlon.lon_deg,
          abs(fake_latlon.lon_umin)/1000000,
          abs(fake_latlon.lon_umin)%1000000,
          fake_latlon.lon_umin >= 0 ? 'E' : 'W',
          speed_ckt/100, speed_ckt%100,
          tm.tm_mday, tm.tm_mon+1, tm.tm_year%100,   // dmy
          iri[0]>99.99?99.99:iri[0], iri[1]>99.99?99.99:iri[1]
    );
    write_nmea_crc(iri_tag+1); // CRC for NMEA part
    // safety check for space at expected place, sprintf length can be unpredictable
    if(iri_tag[80] == ' ')
      write_nmea_crc(iri_tag+80); // CRC for IRI part
    //Serial.println(iri_tag+81); // debug
    write_tag(iri_tag);
    travel_ct0();
    iri_tag[80] = 0; // null terminate for lastnmea save
    draw_kml_line(iri_tag+1); // for kml file generation
    strcpy(lastnmea, iri_tag+1); // for saving last nmea line
  }
  write_logs(); // use SPI_MODE1
  handle_fast_enough(); // will close logs if not fast enough
  getLocalTime(&tm, 0);
  handle_session_log(); // will open logs if fast enough (new filename when reconnected)
  report_iri();
  report_status();
  obd_retry = ~0;
}

void handle_obd_silence(void)
{
  if(!connected)
    return;
  if(line_tdelta > 7000 && (obd_retry & 1) != 0)
  {
    SerialBT.print(obd_request_kmh); // read speed km/h (without car, should print "SEARCHING...")
    obd_retry &= ~1;
  }
  #if 0
  else
  if(line_tdelta > 12000 && (obd_retry & 2) != 0)
  {
    SerialBT.print(obd_request_kmh); // read speed km/h (without car, should print "SEARCHING...")
    obd_retry &= ~2;
  }
  #endif
}

// ms increment of tdelta to trigger data acquisition at 10Hz rate
// TODO make it configurable
#define FINE_TDELTA_INC 200

void loop_run(void)
{
  char c;
  static uint32_t fine_tdelta;
  t_ms = ms();
  line_tdelta = t_ms - line_tprev;
  // handle incoming data as lines and reconnect on 10s silence
  if (connected && SerialBT.available() > 0)
  {
    c = 0;
    while(SerialBT.available() > 0 && c != line_terminator && c != prompt_obd) // line not complete
    {
      if (line_i == 0)
        ct0 = ms(); // time when first char in line came
      // read returns char or -1 if unavailable
      c = SerialBT.read();
      if (line_i < sizeof(line) - 3)
        // if (line_i != 0 || c != '\n' || c != '\r') // line can't start with line terminator
          line[line_i++] = c;
    }
    if(c == line_terminator || c == prompt_obd ) // line complete
    { // GPS has '\n', OBD has '\r' line terminator and '>' prompt
      line[line_i] = 0; // additionally null-terminate string
      if(mode_obd_gps)
        handle_gps_line_complete();
      else
        handle_obd_line_complete();
      line_tprev = t_ms; // record time, used to detect silence
      fine_tdelta = FINE_TDELTA_INC;
      line_i = 0; // line consumed, start new
      #ifdef PIN_LED
      // BT LED ON
      pinMode(PIN_LED, OUTPUT);
      digitalWrite(PIN_LED, LED_ON);
      #endif
    }
  }
  if(mode_obd_gps == 0)
    handle_obd_silence();
  // (*handle_silence)(); // in OBD mode this will retry sending data query

  // finale check for serial line silence to determine if
  // GPS/OBD needs to be reconnected
  // reported 15s silence is possible http://4river.a.la9.jp/gps/report/GLO.htm
  // for practical debugging we wait for less here

  if( (MS_SILENCE_RECONNECT > 0 && line_tdelta > MS_SILENCE_RECONNECT) // 10 seconds of serial silence? then reconnect
  ||  (MS_SILENCE_RECONNECT == 0 && SerialBT.connected() == false && line_tdelta > 7000) // after bluetooth disconnect, immediately reconnect
  )
  {
      #ifdef PIN_LED
      // BT LED OFF
      pinMode(PIN_LED, INPUT);
      digitalWrite(PIN_LED, LED_OFF);
      #endif
      //Serial.println("reconnect");
      handle_reconnect();
      // reconnect has waited too much, we must reload t_ms
      // this will prevent immediately GPS search,
      // give it 2s to start feeding data
      t_ms = ms();
      line_tprev = t_ms;
      line_tdelta = 0;
      fine_tdelta = FINE_TDELTA_INC;
  }
  else
    write_logs();
  // calculated line_tdelta for current speed
  // to trigger equal-distance event generation
  // configurable to trigger every 2m
  // in case NMEA report is missing or the tunnel mode
  if(line_tdelta > fine_tdelta)
  {
  #if 0
    get_iri();
    Serial.print("tdelta ");
    Serial.print(fine_tdelta); // uint32_t
    Serial.print(" iri20 ");
    Serial.println(iri20avg); // float
    fine_tdelta += FINE_TDELTA_INC;
  #endif
  }
  report_search();
  btn_handler();
  speech(); // runs speech PCM
}

void speak_report_ip(struct tm *tm)
{
  if(tm->tm_sec%20 == 0) // speak every 20s
  if(speakfile == NULL && *speakfiles == NULL && pcm_is_open == 0)
  {
    String str_IP = WiFi.localIP().toString();
    const char *c_str_IP = str_IP.c_str();
    int i, n = str_IP.length();
    if(n > 15)
      n = 15;
    for(i = 0; i < n; i++)
    {
      char c = c_str_IP[i];
      if(c >= '0' && c <= '9')
        speakip[i] = digit_file[c-'0'];
      else
        speakip[i] = (char *)"/profilog/speak/point.wav";
    }
    speakip[i] = NULL; // terminator
    speakfiles = speakip;
  }
}

void loop_web(void)
{
  t_ms = ms();
  server.handleClient();
  int is_connected = monitorWiFi();
  getLocalTime(&tm, 0);
  rds_report_ip(&tm);
  if(is_connected)
  {
    #ifdef PIN_LED
    // WiFi LED ON
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LED_ON);
    #endif
    speak_report_ip(&tm);
  }
  else
  {
    #ifdef PIN_LED
    // WiFi LED OFF
    pinMode(PIN_LED, INPUT);
    digitalWrite(PIN_LED, LED_OFF);
    #endif
  }
  btn_handler();
  speech();
}

void loop(void)
{
  (*loop_pointer)();
}
