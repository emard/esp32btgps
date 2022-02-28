#include "pins.h"
#include "spidma.h"
#include "adxl355.h"
#include "adxrs290.h"
#include "sdcard.h" // G_RANGE and similar

ESP32DMASPI::Master master;

uint8_t* spi_master_tx_buf;
uint8_t* spi_master_rx_buf;
uint8_t  last_sensor_reading[12];
uint8_t adxl355_regio = 1; // REG I/O protocol 1:ADXL355 0:ADXRS290
uint8_t adxl_devid_detected = 0; // 0xED for ADXL355, 0x92 for ADXRS290

void adxl355_write_reg(uint8_t a, uint8_t v)
{
  if(adxl355_regio)
    spi_master_tx_buf[0] = a*2; // adxl355 write reg addr a
  else
    spi_master_tx_buf[0] = a; // adxrs290 write reg addr a
  spi_master_tx_buf[1] = v;
  //digitalWrite(PIN_CSN, 0);
  master.transfer(spi_master_tx_buf, 2);
  //digitalWrite(PIN_CSN, 1);
}

uint8_t adxl355_read_reg(uint8_t a)
{
  if(adxl355_regio)
    spi_master_tx_buf[0] = a*2+1; // adxl355 read reg addr a
  else
    spi_master_tx_buf[0] = a|0x80; // adxrs290 read reg addr a
  //digitalWrite(PIN_CSN, 0);
  master.transfer(spi_master_tx_buf, spi_master_rx_buf, 2);
  //digitalWrite(PIN_CSN, 1);
  return spi_master_rx_buf[1];
}

// write ctrl byte to spi ram slave addr 0xFF000000
void adxl355_ctrl(uint8_t x)
{
  spi_master_tx_buf[0] = 0;    // cmd write to spi ram slave
  spi_master_tx_buf[1] = 0xFF; // address 0xFF ...
  spi_master_tx_buf[2] = 0x00; // adresss
  spi_master_tx_buf[3] = 0x00; // adresss
  spi_master_tx_buf[4] = 0x00; // adresss
  spi_master_tx_buf[5] = x;
  master.transfer(spi_master_tx_buf, 6);
}

//                   sensor type         sclk polarity         sclk phase
#define CTRL_SELECT (adxl355_regio<<2)|((!adxl355_regio)<<3)|((!adxl355_regio)<<4)

// turn sensor power on, set range, filtering, sync mode
void init_sensors(void)
{
  if(adxl_devid_detected == 0xED) // ADXL355
  {
    adxl355_write_reg(ADXL355_POWER_CTL, 0); // 0: turn device ON
    // i=1-3 range 1:+-2g, 2:+-4g, 3:+-8g
    // high speed i2c, INT1,INT2 active high
    adxl355_write_reg(ADXL355_RANGE, G_RANGE == 2 ? 1 : G_RANGE == 4 ? 2 : /* G_RANGE == 8 ? */ 3 );
    // LPF FILTER i=0-10, 1kHz/2^i, 0:1kHz ... 10:0.977Hz
    adxl355_write_reg(ADXL355_FILTER, FILTER_ADXL355_CONF);
    // sync: 0:internal, 2:external sync with interpolation, 5:external clk/sync < 1066 Hz no interpolation, 6:external clk/sync with interpolation
    adxl355_write_reg(ADXL355_SYNC, 0xC0 | 2); // 0: internal, 2: takes external sync to drdy pin, 0xC0 undocumented, seems to prevent glitches
  }
  if(adxl_devid_detected == 0x92) // ADXRS290 Gyro
  {
    adxl355_write_reg(ADXRS290_POWER_CTL, ADXRS290_POWER_GYRO | ADXRS290_POWER_TEMP); // turn device ON
    // [7:4] HPF 0.011-11.30 Hz, [2:0] LPF 480-20 Hz, see datasheet
    adxl355_write_reg(ADXRS290_FILTER, FILTER_ADXRS290_CONF);
  }
}

// from core indirect (automatic sensor reading)
// temporary switch to core direct (SPI to sensors)
// read temperatures
// switch back to core indirect (automatic sensor reading)
void read_temperature(void)
{
  if(adxl_devid_detected == 0xED) // ADXL355
  {
    for(uint8_t lr = 0; lr < 2; lr++)
    {
      adxl355_ctrl(lr|2|CTRL_SELECT); // 2 core direct mode, 4 SCLK inversion
      // repeatedly read raw temperature registers until 2 same readings
      uint16_t T[2] = {0xFFFF,0xFFFE}; // any 2 different numbers that won't accidentally appear at reading
      for(int i = 0; i < 1000 && T[0] != T[1]; i++)
        T[i&1] = ((adxl355_read_reg(ADXL355_TEMP2) & 0xF)<<8) | adxl355_read_reg(ADXL355_TEMP1);
      temp[lr] = T_OFFSET_ADXL355_CONF[lr] + 25.0 + (T[0]-ADXL355_TEMP_AT_25C)*ADXL355_TEMP_SCALE*T_SLOPE_ADXL355_CONF[lr]; // convert to deg C
    }
  }
  if(adxl_devid_detected == 0x92) // ADXRS290 Gyro
  {
    for(uint8_t lr = 0; lr < 2; lr++)
    {
      adxl355_ctrl(lr|2|CTRL_SELECT); // 2 core direct mode, 4 SCLK inversion
      // repeatedly read raw temperature registers until 2 same readings
      uint16_t T[2] = {0xFFFF,0xFFFE}; // any 2 different numbers that won't accidentally appear at reading
      for(int i = 0; i < 1000 && T[0] != T[1]; i++)
        T[i&1] = ((adxl355_read_reg(ADXRS290_TEMP_H) & 0xF)<<8) | adxl355_read_reg(ADXRS290_TEMP_L);
      temp[lr] = T_OFFSET_ADXRS290_CONF[lr] + T[0]*ADXRS290_TEMP_SCALE*T_SLOPE_ADXRS290_CONF[lr]; // convert to deg C
    }
  }
}

void warm_init_sensors(void)
{
  adxl355_ctrl(2|CTRL_SELECT);
  delay(2); // wait for request direct mode to be accepted
  init_sensors();
  read_temperature();
  adxl355_ctrl(CTRL_SELECT); // 2:request core indirect mode
  delay(2); // wait for direct mode to finish
}

void debug_sensors_print(void)
{
  char sprintf_buf[80];
  if(adxl_devid_detected == 0xED) // ADXL355
  {
    // print to check is Accel working
    for(int i = 0; i < 1000; i++)
    {
      sprintf(sprintf_buf, "ID=%02X%02X X=%02X%02X Y=%02X%02X Z=%02X%02X",
        adxl355_read_reg(0),
        adxl355_read_reg(1),
        adxl355_read_reg(ADXL355_XDATA3),
        adxl355_read_reg(ADXL355_XDATA2),
        adxl355_read_reg(ADXL355_YDATA3),
        adxl355_read_reg(ADXL355_YDATA2),
        adxl355_read_reg(ADXL355_ZDATA3),
        adxl355_read_reg(ADXL355_ZDATA2)
      );
      Serial.println(sprintf_buf);
    }
  }
  if(adxl_devid_detected == 0x92) // ADXRS290 Gyro
  {
    // print to check is Gyro working
    for(int i = 0; i < 1000; i++)
    {
      sprintf(sprintf_buf, "X=%02X%02X Y=%02X%02X",
        adxl355_read_reg(ADXRS290_GYRO_XH),
        adxl355_read_reg(ADXRS290_GYRO_XL),
        adxl355_read_reg(ADXRS290_GYRO_YH),
        adxl355_read_reg(ADXRS290_GYRO_YL)
      );
      Serial.println(sprintf_buf);
    }
  }
}

void cold_init_sensors(void)
{
  uint8_t chipid[4];
  uint32_t serialno[2];
  char sprintf_buf[80];

  // autodetect regio protocol.
  // try 2 different regio protocols
  // until read reg(1) = 0x1D (common for both sensor types)
  // 2<<0 direct mode L/R switch when reading data from sensor
  // 2<<1 direct mode request
  // 2<<2 sensor type
  // 2<<3 clock polarity
  // 2<<4 clock phase
  adxl355_ctrl(2|CTRL_SELECT);
  delay(2); // wait for request direct mode to be accepted
  if(adxl_devid_detected == 0)
  {
    master.setFrequency(4000000); // 5 MHz max ADXRS290
    for(int8_t j = 1; j >= 0; j--)
    {
      // first try 1:ADXL355, then 0:ADXRS290
      // otherwise ADXL355 will be spoiled
      adxl355_regio = j;
      for(int8_t lr = 0; lr < 2; lr++)
      {
        adxl355_ctrl(lr|2|CTRL_SELECT); // 2 core direct mode, 4 SCLK inversion
        if(adxl355_read_reg(1) == 0x1D)
          goto read_chip_id; // ends for-loop
      }
    }
  }
  read_chip_id:;
  // now read full 4 bytes of chip id
  for(uint8_t i = 0; i < 4; i++)
    chipid[i] = adxl355_read_reg(i);
  if(chipid[0] == 0xAD && chipid[1] == 0x1D) // ADXL device
    adxl_devid_detected = chipid[2];
  serialno[0] = 0;
  serialno[1] = 0;
  if(adxl_devid_detected == 0xED) // ADXL355
    master.setFrequency(4000000); // 8 MHz max ADXL355, no serial number
  if(adxl_devid_detected == 0x92) // ADXRS290 gyroscope has serial number
  { // read serial number
    master.setFrequency(4000000); // 5 MHz max ADXRS290, read serial number
    for(uint8_t lr = 0; lr < 2; lr++)
    {
      adxl355_ctrl(lr|2|CTRL_SELECT); // 2 core direct mode, 4 SCLK inversion
      for(uint8_t i = 0; i < 4; i++)
        serialno[lr] = (serialno[lr] << 8) | adxl355_read_reg(i|4);
    }
  }
  sprintf(sprintf_buf, "ADX CHIP ID: %02X %02X %02X %02X S/N L: %08X R: %08X",
      chipid[0], chipid[1], chipid[2], chipid[3], serialno[0], serialno[1]
  );
  Serial.println(sprintf_buf);
  init_sensors();
  //debug_sensors_print();
  read_temperature();
  sprintf(sprintf_buf, "TL=%4.1f'C TR=%4.1f'C", temp[0], temp[1]);
  Serial.println(sprintf_buf);
  adxl355_ctrl(CTRL_SELECT); // 2:request core indirect mode
  delay(2); // wait for direct mode to finish
}


uint8_t adxl355_available(void)
{
  // read number of entries in the fifo
  spi_master_tx_buf[0] = ADXL355_FIFO_ENTRIES*2+1; // FIFO_ENTRIES read request
  //digitalWrite(PIN_CSN, 0);
  master.transfer(spi_master_tx_buf, spi_master_rx_buf, 2);
  //digitalWrite(PIN_CSN, 1);
  return spi_master_rx_buf[1]/3;
}

// read current write pointer of the spi slave
uint16_t spi_slave_ptr(void)
{
  spi_master_tx_buf[0] = 1; // 1: read ram
  spi_master_tx_buf[1] = 1; // addr [31:24] msb
  spi_master_tx_buf[2] = 0; // addr [23:16]
  spi_master_tx_buf[3] = 0; // addr [15: 8]
  spi_master_tx_buf[4] = 0; // addr [ 7: 0] lsb
  spi_master_tx_buf[5] = 0; // dummy
  master.transfer(spi_master_tx_buf, spi_master_rx_buf, 8); // read, last 2 bytes are ptr value
  return spi_master_rx_buf[6]+(spi_master_rx_buf[7]<<8);
}

// read n bytes from SPI address a and write to dst
// result will appear at spi_master_rx_buf at offset 6
void spi_slave_read(uint16_t a, uint16_t n)
{
  spi_master_tx_buf[0] = 1; // 1: read ram
  spi_master_tx_buf[1] = 0; // addr [31:24] msb
  spi_master_tx_buf[2] = 0; // addr [23:16]
  spi_master_tx_buf[3] = a >> 8; // addr [15: 8]
  spi_master_tx_buf[4] = a; // addr [ 7: 0] lsb
  spi_master_tx_buf[5] = 0; // dummy
  master.transfer(spi_master_tx_buf, spi_master_rx_buf, 6+n); // read, last 2 bytes are ptr value
}

// read fifo to 16-buffer
uint8_t adxl355_rdfifo16(void)
{
  uint8_t n = adxl355_available();
  //n = 1; // debug
  for(uint8_t i = 0; i != n; i++)
  {
    spi_master_tx_buf[0] = ADXL355_FIFO_DATA*2+1; // FIFO_DATA read request
    //spi_master_tx_buf[0] = ADXL355_XDATA3*2+1; // current data
    //digitalWrite(PIN_CSN, 0);
    master.transfer(spi_master_tx_buf, spi_master_rx_buf, 10);
    //digitalWrite(PIN_CSN, 1);
    //spi_master_rx_buf[1] = 10; // debug
    //spi_master_rx_buf[2] = 12; // debug
    for(uint8_t j = 0; j != 3; j++)
    {
      uint16_t a = 1+i*6+j*2;
      uint16_t b = 1+j*3;
      for(uint8_t k = 0; k != 2; k++)
        spi_master_tx_buf[a-k] = spi_master_rx_buf[b+k];
        //spi_master_tx_buf[a-k] = i; // debug
    }
  }
  return n; // result is in spi_master_tx_buf
}

void spi_init(void)
{
    // to use DMA buffer, use these methods to allocate buffer
    spi_master_tx_buf = master.allocDMABuffer(BUFFER_SIZE);
    spi_master_rx_buf = master.allocDMABuffer(BUFFER_SIZE);

    // adxl355   needs SPI_MODE1 (all lines directly connected)
    // spi_slave needs SPI_MODE3
    // adxl355  direct can use SPI_MODE3 with sclk inverted
    // adxrs290 direct can use SPI_MODE3 with sclk normal
    master.setDataMode(SPI_MODE3); // for DMA, only 1 or 3 is available
    master.setFrequency(4000000); // Hz 5 MHz initial, after autodect ADXL355: 8 MHz, ADXRS290: 5 MHz
    master.setMaxTransferSize(BUFFER_SIZE); // bytes
    master.setDMAChannel(1); // 1 or 2 only
    master.setQueueSize(1); // transaction queue size
    pinMode(PIN_CSN, OUTPUT);
    digitalWrite(PIN_CSN, 1);
    // VSPI = CS:  5, CLK: 18, MOSI: 23, MISO: 19
    // HSPI = CS: 15, CLK: 14, MOSI: 13, MISO: 12
    master.begin(VSPI, PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CSN); // use -1 if no CSN
}

// must be called after spi_init when buffer is allocated
void rds_init(void)
{
  rds.setmemptr(spi_master_tx_buf+5);
}

// speed in mm/s
void spi_speed_write(int spd)
{
  uint32_t icvx  = spd > 0 ? (adxl355_regio ? 40181760/spd : 5719) : 0; // spd is in mm/s
  uint16_t vx    = spd > 0 ? spd : 0;
  spi_master_tx_buf[0] = 0; // 0: write ram
  spi_master_tx_buf[1] = 0x2; // addr [31:24] msb
  spi_master_tx_buf[2] = 0; // addr [23:16]
  spi_master_tx_buf[3] = 0; // addr [15: 8]
  spi_master_tx_buf[4] = 0; // addr [ 7: 0] lsb
  spi_master_tx_buf[5] = vx>>8;
  spi_master_tx_buf[6] = vx;
  spi_master_tx_buf[7] = icvx>>24;
  spi_master_tx_buf[8] = icvx>>16;
  spi_master_tx_buf[9] = icvx>>8;
  spi_master_tx_buf[10]= icvx;
  master.transfer(spi_master_tx_buf, 5+4+2); // write speed binary
}

// returns 
// [um/m] sum abs(vz) over 100m/0.05m = 2000 points integer sum
// [um/m] sum abs(vz) over  20m/0.05m =  400 points integer sum
void spi_srvz_read(uint32_t *srvz)
{
  spi_master_tx_buf[0] = 1; // 1: read ram
  spi_master_tx_buf[1] = 0x2; // addr [31:24] msb
  spi_master_tx_buf[2] = 0; // addr [23:16]
  spi_master_tx_buf[3] = 0; // addr [15: 8]
  spi_master_tx_buf[4] = 0; // addr [ 7: 0] lsb
  spi_master_tx_buf[5] = 0; // dummy
  master.transfer(spi_master_tx_buf, spi_master_rx_buf, 6+4*4); // read srvz binary
  srvz[0] = (spi_master_rx_buf[ 6]<<24)|(spi_master_rx_buf[ 7]<<16)|(spi_master_rx_buf[ 8]<<8)|(spi_master_rx_buf[ 9]);
  srvz[1] = (spi_master_rx_buf[10]<<24)|(spi_master_rx_buf[11]<<16)|(spi_master_rx_buf[12]<<8)|(spi_master_rx_buf[13]);
  srvz[2] = (spi_master_rx_buf[14]<<24)|(spi_master_rx_buf[15]<<16)|(spi_master_rx_buf[16]<<8)|(spi_master_rx_buf[17]);
  srvz[3] = (spi_master_rx_buf[18]<<24)|(spi_master_rx_buf[19]<<16)|(spi_master_rx_buf[20]<<8)|(spi_master_rx_buf[21]);
}

uint8_t spi_btn_read(void)
{
  spi_master_tx_buf[0] = 1; // 1: read ram
  spi_master_tx_buf[1] = 0xB; // addr [31:24] msb
  spi_master_tx_buf[2] = 0; // addr [23:16]
  spi_master_tx_buf[3] = 0; // addr [15: 8]
  spi_master_tx_buf[4] = 0; // addr [ 7: 0] lsb
  spi_master_tx_buf[5] = 0; // dummy
  master.transfer(spi_master_tx_buf, spi_master_rx_buf, 6+1); // read srvz binary
  return spi_master_rx_buf[6];
}

void spi_rds_write(void)
{
  spi_master_tx_buf[0] = 0; // 0: write ram
  spi_master_tx_buf[1] = 0xD; // addr [31:24] msb
  spi_master_tx_buf[2] = 0; // addr [23:16]
  spi_master_tx_buf[3] = 0; // addr [15: 8]
  spi_master_tx_buf[4] = 0; // addr [ 7: 0] lsb
  rds.ta(0);
  rds.ps((char *)"RESTART ");
  //               1         2         3         4         5         6
  //      1234567890123456789012345678901234567890123456789012345678901234
  rds.rt((char *)"Restart breaks normal functioning. Firmware needs maintenance.  ");
  rds.ct(2000,0,1,0,0,0);
  master.transfer(spi_master_tx_buf, 5+(4+16+1)*13); // write RDS binary
  if(0)
  {
    for(int i = 0; i < 5+(4+16+1)*13; i++)
    {
      Serial.print(spi_master_tx_buf[i], HEX);
      Serial.print(" ");
    }
    Serial.println("RDS set");
  }
}
