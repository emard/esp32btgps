// Manage Libraries -> ESP32DMASPI
// Version 0.1.0 tested
// Version 0.1.1 and 0.1.2 compile with arduino 1.8.19
// Version 0.2.0 doesn't compile
#include <ESP32DMASPIMaster.h> // Version 0.1.0 tested
extern ESP32DMASPI::Master master;

// hardware buffer size in bytes at fpga core (must be divisible by 12)
// 3072, 6144, 9216, 12288, 15360
#define SPI_READER_BUF_SIZE 9216

extern uint8_t* spi_master_tx_buf;
extern uint8_t* spi_master_rx_buf;
static const uint32_t BUFFER_SIZE = (SPI_READER_BUF_SIZE+6+4) & 0xFFFFFFFC; // multiply of 4
extern uint8_t  last_sensor_reading[12];

void spi_init(void);
void rds_init(void);
void spi_speed_write(int spd);
void spi_srvz_read(uint32_t *srvz);
uint8_t spi_btn_read(void);
void spi_rds_write(void);
uint16_t spi_slave_ptr(void);
void spi_slave_read(uint16_t a, uint16_t n);
// uint8_t adxl355_available(void);

void rds_message(struct tm *tm);
void rds_report_ip(struct tm *tm);
void set_fm_freq(void);
void cold_init_sensors(void);
void warm_init_sensors(void);
void clr_lcd(void);
void lcd_print(uint8_t x, uint8_t y, uint8_t invert, char *a);
void write_tag(char *a);
int play_pcm(int n);
int open_pcm(char *wav); // open wav filename
void beep_pcm(int n);
void write_rds(uint8_t *a, int n);
void spi_slave_test(void);
void spi_direct_test(void);
