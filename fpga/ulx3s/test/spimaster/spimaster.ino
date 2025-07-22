#include <Arduino.h>
#include <SPI.h>

#define LED_ON    1
#define LED_OFF   0
#define PIN_LED   5

#define PIN_SCK   0
#define PIN_MISO 35
#define PIN_MOSI 16
#define PIN_CSN  17

SPISettings master_spi_settings = SPISettings(4000000/*HZ*/, MSBFIRST, SPI_MODE3);
SPIClass *master = NULL;
char txtmsg[256];

// buf tx will be transmitted and
// buf overwritten with rx
void master_txrx(uint8_t *buf, int len)
{
  master->beginTransaction(master_spi_settings);
  digitalWrite(PIN_CSN, 0);
  master->transfer(buf, len);
  digitalWrite(PIN_CSN, 1);
  master->endTransaction();
}

uint8_t spi_master_tx_buf[256];

// print to LCD screen
void lcd_print(uint8_t x, uint8_t y, uint8_t invert, char *a)
{
  spi_master_tx_buf[0] = 0; // 0: write ram
  spi_master_tx_buf[1] = 0xC; // addr [31:24] msb LCD addr
  spi_master_tx_buf[2] = invert; // addr [23:16] (0:normal, 1:invert)
  spi_master_tx_buf[3] = (y>>3); // addr [15: 8]
  spi_master_tx_buf[4] = 1+x+((y&7)<<5); // addr [ 7: 0] lsb
  int l = strlen(a);
  memcpy(spi_master_tx_buf+5, a, l);
  master_txrx(spi_master_tx_buf, 5+l); // write to LCD
}

void setup()
{
  Serial.begin(115200);
  master = new SPIClass(VSPI);
  master->begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CSN);
  pinMode(PIN_CSN, OUTPUT);
  pinMode(PIN_LED, OUTPUT); 
}

void loop()
{
  static uint32_t counter = 0;
  digitalWrite(PIN_LED, counter&1);
  sprintf(txtmsg, "%08X", counter);
  lcd_print(0/*x*/, counter%15/*y*/, counter/15&1/*invert*/, txtmsg);
  Serial.println(txtmsg);
  counter++;
  delay(500);
}
