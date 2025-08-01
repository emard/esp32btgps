// passthru for ADXL355 accelerometer
// use with:
// adxl355.filter(0)
// adxl355.sync(2) # DRDY=SYNC input, internal oscillator
// adxl355.multird16()

// TODO use spi_ram_x to reset addr for RAM

/*
https://wiki.analog.com/resources/eval/user-guides/eval-adicup360/hardware/adxl355
PMOD connected to GP,GN 14-17
Pin Number  Pin Function         Mnemonic  ULX3S
Pin 1       Chip Select          CS        GN17  LED0
Pin 2       Master Out Slave In  MOSI      GN16  LED1
Pin 3       Master In Slave Out  MISO      GN15  LED2
Pin 4       Serial Clock         SCLK      GN14  LED3
Pin 5       Digital Ground       DGND
Pin 6       Digital Power        VDD
Pin 7       Interrupt 1          INT1      GP17  LED4
Pin 8       Not Connected        NC        GP16
Pin 9       Interrupt 2          INT2      GP15  LED6
Pin 10      Data Ready           DRDY      GP14  LED7
Pin 11      Digital Ground       DGND
Pin 12      Digital Power        VDD

PMOD connected to GP,GN 21-24 (add +7 to previous GP,GN numbers)
Pin Number  Pin Function         Mnemonic  ULX3S
Pin 1       Chip Select          CS        GN24  LED0
Pin 2       Master Out Slave In  MOSI      GN23  LED1
Pin 3       Master In Slave Out  MISO      GN22  LED2
Pin 4       Serial Clock         SCLK      GN21  LED3
Pin 5       Digital Ground       DGND
Pin 6       Digital Power        VDD
Pin 7       Interrupt 1          INT1      GP24  LED4
Pin 8       Not Connected        NC        GP23
Pin 9       Interrupt 2          INT2      GP22  LED6
Pin 10      Data Ready           DRDY      GP21  LED7
Pin 11      Digital Ground       DGND
Pin 12      Digital Power        VDD
*/

// makefile constraints/ulx3s_v20.lpf
//`define ULX3S_v30x

// makefile constraints/ulx3s_v316.lpf
`define ULX3S_v31x

`default_nettype none
module top_adxl355log
#(
  ram_len      = 9*1024,     // buffer size 3,6,9,12,15
  wav_addr_bits= 12,         // 2**n, default 2**12 = 4096 bytes = 4 KB audio PCM FIFO buffer
  C_prog_release_timeout = 26, // esp32 programming default n=26, 2^n / 25MHz = 2.6s
  spi_direct   = 0,          // 0: spi slave (SPI_MODE2), 1: direct to adxl (SPI_MODE0 or SPI_MODE2)
  autospi_clkdiv = 5,        // SPI CLK = 40/2/(2**(autospi_clkdiv-1)+1) MHz, 1:10, 2:6.6, 3:4, 4:2.2, 5:1.1 MHz
  clk_out0_hz  =  40*1000000,// Hz,  40 MHz, PLL generated internal clock
  clk_out1_hz  = 240*1000000,// Hz, 240 MHz, PLL generated clock for FM transmitter
  clk_out2_hz  = 120*1000000,// Hz, 120 MHz, PLL generated clock for SPI LCD
  lcd_hex      = 0,          // enable HEX display (either HEX or TEXT, not both)
  lcd_txt      = 1,          // enable TEXT display
  pps_n        = 10,         // N, 1 Hz, number of PPS pulses per interval
  pps_s        = 1,          // s, 1 s, PPS interval
  clk_sync_hz  = 1000,       // Hz, 1 kHz SYNC pulse, sample rate
  pa_sync_bits = 30          // bit size of phase accumulator, less->faster convergence larger jitter , more->slower convergence less jitter
)
(
  input         clk_25mhz,
  input   [6:0] btn,
  output  [7:0] led,
  output  [3:0] audio_l, audio_r,
  output        gp14, // ADXL355 DRDY
  input         gp15, // ADXL355 INT2
  input         gp17, // ADXL355 INT1
  input         gn15, // ADXL355 MISO
  output        gn14,gn16,gn17, // ADXL355 SCLK,MOSI,CSn
  output        gp21, // ADXL355 DRDY
  input         gp22, // ADXL355 INT2
  input         gp24, // ADXL355 INT1
  input         gn22, // ADXL355 MISO
  output        gn21,gn23,gn24, // ADXL355 SCLK,MOSI,CSn
  output        gp19,gp20, // external antenna 1,2
  input         ftdi_nrts,
  input         ftdi_ndtr,
  output        ftdi_rxd,
  input         ftdi_txd,
  output        wifi_en,
  output        wifi_rxd,
  input         wifi_txd,
  inout         wifi_gpio0,
  inout         wifi_gpio12, wifi_gpio2,
  input         wifi_gpio13, wifi_gpio4,
  input         wifi_gpio15, wifi_gpio14,
  `ifdef ULX3S_v30x
  input         wifi_gpio16, wifi_gpio17, // ULX3S v2.x.x - v3.0.x
  `endif
  `ifdef ULX3S_v31x
  input         wifi_gpio26, wifi_gpio27, // ULX3S v3.1.x
  `endif
  input         wifi_gpio25, // PPS to FPGA
  output        wifi_gpio32, // IRQ PPS feedback
  output        wifi_gpio33, // MISO
  //inout   [3:0] sd_d, // wifi_gpio 13,12,4,2
  //input         sd_cmd, sd_clk, // wifi_gpio 15,14
  output        sd_wp, // BGA pin exists but not connected on PCB
  output        oled_csn,
  output        oled_clk,
  output        oled_mosi,
  output        oled_dc,
  output        oled_resn,
  output        ant_433mhz
);
  // TX/RX passthru
  assign ftdi_rxd = wifi_txd;
  assign wifi_rxd = ftdi_txd;

  // Programming logic
  // SERIAL  ->  ESP32
  // DTR RTS -> EN IO0,2
  //  1   1     1   1
  //  0   0     1   1
  //  1   0     0   1
  //  0   1     1   0
  
  reg  [1:0] R_prog_in;
  wire [1:0] S_prog_in  = { ftdi_ndtr, ftdi_nrts };
  wire [1:0] S_prog_out = S_prog_in == 2'b10 ? 2'b01 
                        : S_prog_in == 2'b01 ? 2'b10 : 2'b11;

  // detecting programming ESP32 and reset timeout
  reg [C_prog_release_timeout:0] R_prog_release = ~0; // all bits 1 to prevent PROG early on BOOT
  always @(posedge clk_25mhz)
  begin
    R_prog_in <= S_prog_in;
    if(S_prog_out == 2'b01 && R_prog_in == 2'b11)
      R_prog_release <= 0; // keep resetting during ESP32 programming
    else
      if(R_prog_release[C_prog_release_timeout] == 1'b0)
        R_prog_release <= R_prog_release + 1; // increment until MSB=0
  end
  // wifi_gpio2 for programming must go together with wifi_gpio0
  // wifi_gpio12 (must be 0 for esp32 fuse unprogrammed)
  assign wifi_gpio12 = R_prog_release[C_prog_release_timeout] ? 1'bz : 1'b0;
  assign wifi_gpio2  = R_prog_release[C_prog_release_timeout] ? 1'bz : S_prog_out[0];
  //assign sd_d  = R_prog_release[C_prog_release_timeout] ? 4'hz : { 3'b101, S_prog_out[0] }; // wifi_gpio 13,12,4,2
  //assign sd_wp = sd_clk | sd_cmd | sd_d; // force pullup for 4'hz above for listed inputs to make SD MMC mode work
  assign sd_wp = wifi_gpio15 | wifi_gpio14 | wifi_gpio13 | wifi_gpio12 | wifi_gpio4 | wifi_gpio2 /*| wifi_gpio0*/; // force pullup for listed inputs to make SD MMC mode work
  // sd_wp is not connected on PCB, just to prevent optimizer from removing pullups

  //assign wifi_en = S_prog_out[1];
  // assign wifi_gpio0 = R_prog_release[C_prog_release_timeout] ? 1'bz : S_prog_out[0] & btn[0]; // holding BTN0 will hold gpio0 LOW, signal for ESP32 to take control
  assign wifi_gpio0 = R_prog_release[C_prog_release_timeout] ? 1'bz : S_prog_out[0]; // holding BTN0 will hold gpio0 LOW, signal for ESP32 to take control

  assign wifi_en = S_prog_out[1] & ~(&btn[6:3]); // press BTN3,4,5,6 to reboot ESP32
  //assign wifi_gpio0 = S_prog_out[0];

  wire int1 = gp17;
  wire int2 = gp15;
  wire drdy; // gp14;
  assign gp14 = drdy; // adxl0 
  assign gp21 = drdy; // adxl1

  // base clock for making 1024 kHz for ADXL355
  wire [3:0] clocks;
  ecp5pll
  #(
      .in_hz(25*1000000),
    .out0_hz(clk_out0_hz),
    .out1_hz(clk_out1_hz),
    .out2_hz(clk_out2_hz)
  )
  ecp5pll_inst
  (
    .clk_i(clk_25mhz),
    .clk_o(clocks)
  );
  wire clk = clocks[0]; // 40 MHz system clock
  wire clk_fmdds = clocks[1]; // 240 MHz FM clock
  wire clk_lcd = clocks[2]; // 120 MHz LCD clock
  wire clk_pixel = clk;

  wire [6:0] btn_rising, btn_debounce;
  btn_debounce
  #(
    .bits(19)
  )
  btn_debounce_inst
  (
    .clk(clk),
    .btn(btn),
    .debounce(btn_debounce),
    .rising(btn_rising)
  );

  wire csn, mosi, miso, sclk;
  wire rd_csn, rd_mosi, rd0_miso, rd1_miso, rd_sclk; // spi reader

  wire        ram_rd, ram_wr;
  wire [31:0] ram_addr;
  wire  [7:0] ram_di, ram_do;

  wire spi_ram_wr, spi_ram_x;
  reg [1:0] r_accel_addr3 = 0; // counts 0-2 to skip every 3rd byte for ADXL355
  reg r_accel_addr0 = 0; // alignment with wav buf
  wire  [7:0] spi_ram_data;
  localparam ram_addr_bits = $clog2(ram_len-1);
  reg [ram_addr_bits-1:0] spi_ram_addr = 0, r_spi_ram_addr;
  reg [7:0] ram[0:ram_len-1];
  reg [7:0] R_ram_do;
  wire spi_ram_miso; // muxed
  wire spi_bram_cs; // "chip" select line for address detection of bram buffer addr 0x00...
  wire spi_ctrl_cs; // control byte select addr 0xFF..
  // r_ctrl[7:3]:reserved, r_ctrl[2]:sclk_inv, r_ctrl[1]:direct_en, r_ctrl[0]:sensor lr
  // if r_ctrl doesn't start from 0, adxl355 won't initialize
  // accel reading of XYZ axis will have 12 MSBs mostly fixed
  // and rest LSBs with very small fluctuations when acceleation appled
  // reg [7:0] r_ctrl = 0; // holding BTN0 for web server doesn't work
  // reg [7:0] r_ctrl = 2; // ADXRS290 initializes but doesn't work correctly (R sensor offset too low)
  reg [7:0] r_ctrl = 8'b00011000; // initialization works for both ADXL355 and ADXRS290
  wire ctrl_direct_lr = r_ctrl[0]; // mux direct 0: left, 1: right
  wire ctrl_direct = r_ctrl[1]; // mux switch 1:direct, 0:reader core
  wire ctrl_sclk_inv = r_ctrl[2]; // SPI direct clk invert 1:invert, 0:normal
  wire ctrl_sensor_type = r_ctrl[2]; // 1: ADXL355 accelerometer, 0:ADXRS290 gyroscope
  wire ctrl_skip_3rd = r_ctrl[2]; // skip every 3rd byte 1: ADXL355 skip every 3rd byte, 0: ADXRS290 no skip (collect every byte)
  wire ctrl_swap_lh = r_ctrl[2]; // swap L/H byte 1:ADXL355 swap, 0:ADXRS290 don't swap
  wire ctrl_sclk_polarity = r_ctrl[3]; // SPI autoreader clk polarity, 0: ADXL355, 1:ADXRS290
  wire ctrl_sclk_phase = r_ctrl[4]; // SPI autoreader clk phase, 0: ADXL355, 1:ADXRS290
  wire direct_en;
  wire [7:0]   calc_result[0:15]; // 16-byte (4x32-bit)
  reg  [7:0] r_calc_result[0:15]; // 16-byte (4x32-bit)
  wire [7:0] w_calc_result[0:15]; // 16-byte (4x32-bit)

  wire spi_bram_cs = ram_addr[27:24] == 4'h0; // read from 0x00xxxxxx read sensor 6-ch 16-bit WAV 1000 Hz data BRAM
  wire spi_bptr_cs = ram_addr[27:24] == 4'h1; // read from 0x01xxxxxx read sensor 6-ch 16-bit WAV 1000 Hz data BRAM pointer
  wire spi_calc_cs = ram_addr[27:24] == 4'h2; // read/write to 0x02xxxxxx writes 32-bit speed mm/s and const/speed
  wire spi_wav_cs  = ram_addr[27:24] == 4'h5; // write to 0x05xxxxxx writes unsigned 1-ch 8-bit 11025 Hz WAV PCM
  wire spi_tag_cs  = ram_addr[27:24] == 4'h6; // write to 0x06xxxxxx writes 6-bit tags
  wire spi_freq_cs = ram_addr[27:24] == 4'h7; // write to 0x07xxxxxx writes 2x32-bit FM freq[0] and freq[1]
  wire spi_btn_cs  = ram_addr[27:24] == 4'hB; // read from 0x0Bxxxxxx reads BTN state
                                              // write to 0x0Cxxxxxx writes 480 bytes of LCD TEXT data see spi_osd_v.v
  wire spi_rds_cs  = ram_addr[27:24] == 4'hD; // write to 0x0Dxxxxxx writes 273 bytes of RDS encoded data for text display
                                              // write to 0x0Exxxxxx enables LCD text see spi_osd_v.v
  wire spi_ctrl_cs = ram_addr[27:24] == 4'hF;

  generate
  if(spi_direct)
  begin
    // ADXL355 connections (FPGA is master to ADXL355)
    assign gn17 = csn;
    assign gn16 = mosi;
    assign miso = gn15;
    assign gn14 = (sclk ^ ctrl_sclk_inv); // invert sclk to use SPI_MODE0 instead of SPI_MODE2
  end
  else
  begin
    // ADXL355 0 connections (FPGA is master to ADXL355)
    assign gn17 = direct_en ?  csn  : rd_csn;
    assign gn14 = direct_en ? (sclk ^ ctrl_sclk_inv) : rd_sclk;
    assign gn16 = direct_en ?  mosi : rd_mosi;
    assign rd0_miso = gn15; // adxl0 miso directly to reader core

    // ADXL355 1 connections (FPGA is master to ADXL355)
    assign gn24 = direct_en ?  csn  : rd_csn;
    assign gn21 = direct_en ? (sclk ^ ctrl_sclk_inv) : rd_sclk;
    assign gn23 = direct_en ?  mosi : rd_mosi;
    assign rd1_miso = gn22; // adxl1 miso directly to reader core

    assign miso = direct_en ? (ctrl_direct_lr ? rd1_miso : rd0_miso) : spi_ram_miso; // mux miso to esp32

    spirw_slave_v
    #(
        .c_read_cycle(0), // 0-7 adjust if SPI master reads rotated byte
        .c_addr_bits(32),
        .c_sclk_capable_pin(1'b0)
    )
    spirw_slave_v_inst
    (
        .clk(clk),
        .csn(csn),
        .sclk(~sclk),
        .mosi(mosi),
        .miso(spi_ram_miso), // muxed for direct
        .rd(ram_rd),
        .wr(ram_wr),
        .addr(ram_addr),
        .data_in(ram_do),
        .data_out(ram_di)
    );
    //assign ram_do = ram_addr[7:0];
    //assign ram_do = 8'h5A;
    // SPI autoreader to BRAM write cycle, optionally skipping every 3rd byte for sensor type ADXL355
    wire spi_ram_wr_skip = spi_ram_wr
                        && (r_accel_addr3 != 2 || ctrl_skip_3rd == 0)  // skip 3rd byte LSB
                        && (r_accel_addr0 == 0 || spi_ram_addr  != 0); // skip if misalignment
    //wire spi_ram_wr_skip = spi_ram_wr; // debug, no skip
    // SPI autoreader optional address swap L/H byte
    wire [ram_addr_bits-1:0] spi_ram_addr_swap = {spi_ram_addr[ram_addr_bits-1:1],spi_ram_addr[0]^ctrl_swap_lh}; // normal
    //wire [ram_addr_bits-1:0] spi_ram_addr_swap = spi_ram_addr; // debug, no swap
    always @(posedge clk)
    begin
      if(spi_ram_wr_skip)
      begin
        // invert bit[0] to swap lsb/msb byte for wav format for ADXL355
        ram[spi_ram_addr_swap] <= spi_ram_data; // normal SPI reader core provided write address (slice usage normal about 30% at 12F)
        //ram[spi_ram_addr_swap] <= r_accel_addr; // debug sample alignment (slice usage will rise to 100% at 12F)
        //ram[{spi_ram_addr_swap] <= r_accel_addr3; // debug 3rd skip
        if(spi_ram_addr == ram_len-1) // auto-increment and wraparound
          spi_ram_addr <= 0;
        else
          spi_ram_addr <= spi_ram_addr + 1;
      end
      if(ram_rd)
      begin
        if(spi_bptr_cs && ram_addr[0] == 1'b0)
          r_spi_ram_addr <= spi_ram_addr; // latch address MSB
        // ram_addr: SPI slave core provided read address
        // spi_ram_addr: SPI reader core autoincrementing address
        R_ram_do <= spi_bram_cs ? ram[ram_addr]
                  : spi_bptr_cs ? (ram_addr[0] ? r_spi_ram_addr[ram_addr_bits-1:8] : r_spi_ram_addr[7:0])
                  : spi_calc_cs ? w_calc_result[ram_addr] // calc result array latched (right sensor more than left??)
                  //: spi_calc_cs ? calc_result[ram_addr] // calc result array unlatched (occasional read while modify?)
                  : /* spi_btn_cs  ? */ btn_debounce;
      end
    end
    assign ram_do = R_ram_do;
    //assign ram_do = spi_ram_data; // debug
    //assign ram_do = 8'h5A; // debug
    always @(posedge clk)
    begin
      if(ram_wr && spi_ctrl_cs) // spi slave writes ctrl byte
        r_ctrl <= ram_di;
    end
  end
  endgenerate

  // ESP32 connections direct to ADXL355 (FPGA is slave for ESP32)
  `ifdef ULX3S_v30x
  assign csn  = wifi_gpio17;
  assign mosi = wifi_gpio16;
  `endif
  `ifdef ULX3S_v31x
  assign csn  = wifi_gpio27;
  assign mosi = wifi_gpio26;
  `endif
  assign wifi_gpio33 = miso;
  //assign wifi_gpio33 = 0; // debug, should print 00
  //assign wifi_gpio33 = 1; // debug, should print FF
  assign sclk = wifi_gpio0;

  // generate PPS signal (1 Hz, 100 ms duty cycle)
  localparam pps_cnt_max = clk_out0_hz*pps_s/pps_n; // cca +-20000 tolerance
  localparam pps_width   = pps_cnt_max/10;   
  reg [$clog2(pps_cnt_max)-1:0] pps_cnt;
  reg pps, pps_pulse;
  always @(posedge clk)
  begin
    if(pps_cnt == pps_cnt_max-1)
    begin
      pps_cnt <= 0;
      pps_pulse <= 1;
    end
    else
    begin
      pps_cnt <= pps_cnt+1;
      pps_pulse <= 0;
    end
    if(pps_cnt == 0)
      pps <= 1;
    else if(pps_cnt == pps_width-1)
      pps <= 0;
  end

  //wire pps_btn = pps & ~btn[1];
  //wire pps_btn = wifi_gpio5 & ~btn[1];
  //wire pps_btn = wifi_gpio25 & ~btn[1]; // debug
  wire pps_btn = wifi_gpio25; // normal
  //wire pps_btn = ftdi_nrts & ~btn[1];
  wire pps_feedback;
  assign wifi_gpio32 = pps_feedback;
  assign pps_feedback = pps_btn; // ESP32 needs its own PPS feedback

  wire [7:0] phase;
  wire pps_valid, sync_locked;
  adxl355_sync
  #(
    .clk_out0_hz(clk_out0_hz), // Hz, 40 MHz, PLL internal clock
    .pps_n(pps_n),             // N, 1 Hz when pps_s=1
    .pps_s(pps_s),             // s, 1 s PPS interval
    .pps_tol_us(500),          // us, 500 us, default +- tolerance for pulse rising edge
    .clk_sync_hz(clk_sync_hz), // Hz, 1 kHz SYNC clock, sample rate
    .pa_sync_bits(pa_sync_bits)// PA bit size
  )
  adxl355_clk_inst
  (
    .i_clk(clk),
    .i_pps(pps_btn), // rising edge sensitive
    .o_cnt(phase), // monitor phase angle
    .o_pps_valid(pps_valid),
    .o_locked(sync_locked),
    .o_clk_sync(drdy)
  );

  // sync counter
  reg [11:0] cnt_sync, cnt_sync_prev;
  reg [1:0] sync_shift, pps_shift;
  always @(posedge clk)
  begin
    pps_shift <= {pps_btn, pps_shift[1]};
    if(pps_shift == 2'b10) // rising
    begin
      cnt_sync_prev <= cnt_sync;
      cnt_sync <= 0;
    end
    else
    begin
      sync_shift <= {drdy, sync_shift[1]};
      if(sync_shift == 2'b01) // falling to avoid sampling near edge
      begin
        cnt_sync <= cnt_sync+1;
      end
    end
  end

  // LED monitoring

  //assign led[7:4] = {drdy,int2,int1,1'b0};
  //assign led[3:0] = {gn27,gn26,gn25,gn24};
  //assign led[3:0] = {sclk,wifi_gpio35,mosi,csn};
  //assign led[3:0] = {sclk,miso,mosi,csn};

  assign led[7:4] = phase[7:4];
  assign led[3] = pps_valid;
  //assign led[2] = wifi_en;
  //assign led[1] = sync_locked;
  assign led[0] = pps_btn;

  //assign led = phase;
  //assign led = cnt_sync_prev[7:0]; // should show 0xE8 from 1000 = 0x3E8

  // rising edge detection of drdy (sync)
  reg [1:0] r_sync_shift;
  reg sync_pulse;
  always @(posedge clk)
  begin
    r_sync_shift <= {drdy, r_sync_shift[1]};
    sync_pulse <= r_sync_shift == 2'b10 ? 1 : 0;
  end

  // SPI reader
  reg [autospi_clkdiv:0] r_sclk_en;
  always @(posedge clk)
  begin
    if(r_sclk_en[autospi_clkdiv])
      r_sclk_en <= 0;
    else
      r_sclk_en <= r_sclk_en+1;
  end
  wire sclk_en = r_sclk_en[autospi_clkdiv];

  reg [7:0] spi_read_cmd;
  reg [3:0] spi_read_len;
  reg [17:0] spi_tag_byte_select;
  always @(posedge clk)
  begin
    spi_read_cmd <= ctrl_sensor_type ? /*ADXL355*/ 8*2+1 : /*ADXRS290*/ 8+128; // normal
    //spi_read_cmd <= ctrl_sensor_type ? /*ADXL355*/ 1 : /*ADXRS290*/ 128; // debug read ID
    // cmd ADXL355  0*2+1 to read id, 8*2+1 to read xyz, 17*2+1 to read fifo
    // cmd ADXRS290   128 to read id, 8+128 to read xy
    spi_read_len <= ctrl_sensor_type ? /*ADXL355*/    10 : /*ADXRS290*/ 7; // normal
    // spi_read_len <= 10; // debug
    // len ADLX355  10 = 1+9, 1 byte transmitted and 9 bytes received
    // len ADXRS290  7 = 1+6, 1 byte transmitted and 6 bytes received
    spi_tag_byte_select <= ctrl_sensor_type ? /*ADXL355*/ 18'b010_010_010_010_010_010 : /*ADXRS290*/ 12'b01_01_01_01_01_01;
  end

  reg r_tag_en = 0;
  always @(posedge clk)
    r_tag_en <= ram_wr & spi_tag_cs;
  wire tag_en = ram_wr & spi_tag_cs & ~r_tag_en;
  wire [5:0] w_tag = ram_di[6:5] == 0 ? 6'h20 : ram_di[5:0]; // control chars<32 convert to space 32 " "
  wire spi2ram_wr, spi2ram_wr16;
  adxl355rd
  adxl355rd_inst
  (
    .clk(clk), .clk_en(sclk_en),
    .direct(ctrl_direct),
    .direct_en(direct_en),
    .cmd(spi_read_cmd),
    .len(spi_read_len), // 10 = 1+9, 1 byte transmitted and 9 bytes received
    .tag_byte_select(spi_tag_byte_select), // which byte to tag
    .tag_pulse(pps_pulse),
    .tag_en(tag_en),  // write signal from SPI
    .tag(w_tag), // 6-bit char from SPI
    .sync(sync_pulse), // start reading a sample
    .sclk_phase(ctrl_sclk_phase),
    .sclk_polarity(ctrl_sclk_polarity),
    .adxl_csn(rd_csn),
    .adxl_sclk(rd_sclk),
    .adxl_mosi(rd_mosi),
    .adxl0_miso(rd0_miso), // normal
    .adxl1_miso(rd1_miso), // normal
    //.adxl0_miso(0), // debug
    //.adxl1_miso(0), // debug
    .wrdata(spi_ram_data), // received byte to be written to BRAM
    .wr(spi_ram_wr), // signal to write received byte to BRAM
    .x(spi_ram_x) // signal to reset BRAM addr before first received byte in a sample
  );
  //assign spi_ram_x = sync_pulse;

  // store one sample in reg memory
  reg [7:0] r_accel[0:17]; // 18 bytes
  reg [4:0] r_accel_addr;
  always @(posedge clk)
  begin
    if(spi_ram_x)
    begin
      r_accel_addr <= 0;
      r_accel_addr3 <= 0; // TODO check what is logged in file with this line enabled
      r_accel_addr0 <= 0; // address alighment
    end
    else
    begin
      if(spi_ram_wr)
      begin
        r_accel[r_accel_addr] <= spi_ram_data;
        r_accel_addr <= r_accel_addr + 1;
        r_accel_addr3 <= r_accel_addr3 == 2 ? 0 : r_accel_addr3 + 1;
        r_accel_addr0 <= 1;
      end
    end
  end

  reg [15:0] axl, ayl, azl, axr, ayr, azr;
  always @(posedge clk)
  if(ctrl_sensor_type) // 1:ADXL355
  begin
    axl <= {r_accel[ 0], r_accel[ 1]};
    ayl <= {r_accel[ 3], r_accel[ 4]};
    azl <= {r_accel[ 6], r_accel[ 7]};
    axr <= {r_accel[ 9], r_accel[10]};
    ayr <= {r_accel[12], r_accel[13]};
    azr <= {r_accel[15], r_accel[16]};
  end
  else // 0:ADXRS290
  begin
    azl <= {r_accel[ 1], r_accel[ 0]}; // actually X but swapped with Z for calc
    ayl <= {r_accel[ 3], r_accel[ 2]};
    axl <= {r_accel[ 5], r_accel[ 4]}; // actually Z but swapped with X for calc
    azr <= {r_accel[ 7], r_accel[ 6]}; // actually X but swapped with Z for calc
    ayr <= {r_accel[ 9], r_accel[ 8]};
    axr <= {r_accel[11], r_accel[10]}; // actually Z but swapped with X for calc
  end

  //assign led = {spi_ram_x, spi_ram_wr, rd_miso, rd_mosi, rd_sclk, rd_csn};
  //assign led = r_accel_l;
  //assign led = r_ctrl;

  // FM/RDS transmitter
  
  // DEBUG: triangle wave beep
  reg [15:0] beep;
  reg beep_dir;
  always @(posedge clk)
  begin
    if(beep_dir)
    begin
      if(~beep == 0)
        beep_dir <= 0;
      else
        beep <= beep+1;
    end
    else
    begin
      if(beep == 0)
        beep_dir <= 1;
      else
        beep <= beep-1;
    end
  end

  // unsigned 8-bit 11025 Hz WAV data FIFO buffer
  // espeak-ng -v hr -f speak.txt --stdout | sox - --no-dither -r 11025 -b 8 speak.wav
  reg r_wav_en = 0;
  always @(posedge clk)
    r_wav_en <= ram_wr & spi_wav_cs;
  wire wav_en = ram_wr & spi_wav_cs & ~r_wav_en;
  reg [7:0] wav_fifo[0:2**wav_addr_bits-1];
  reg [wav_addr_bits-1:0] r_wwav = 0, r_rwav = 0;
  always @(posedge clk)
  begin
    if(wav_en)
    begin // push to FIFO
      wav_fifo[r_wwav] <= ram_di;
      r_wwav <= r_wwav+1;
    end
  end

  localparam wav_hz = 11025; // Hz wav sample rate normal
  localparam wav_cnt_max = clk_out0_hz / wav_hz - 1;
  reg r_wav_latch = 0;
  reg [15:0] wav_cnt; // counter to divide 40 MHz -> 11025 Hz
  // r_wav_latch pulses at 11025 Hz rate
  always @(posedge clk)
  begin
    if(wav_cnt == wav_cnt_max)
    begin
      wav_cnt <= 0;
      r_wav_latch <= 1;
    end
    else
    begin
      wav_cnt <= wav_cnt+1;
      r_wav_latch <= 0;
    end
  end

  reg [7:0] wav_data; // latched wav data to be played by FM
  always @(posedge clk)
  begin
    if(r_wav_latch)
    begin
      if(r_wwav != r_rwav) // FIFO empty?
      begin
        wav_data <= wav_fifo[r_rwav]; // normal
        r_rwav <= r_rwav+1;
      end
    end
  end

  wire [15:0] wav_data_signed = {~wav_data[7],wav_data[6:0],8'h00}; // unsigned 8-bit to signed 16-bit
  reg [7:0] rds_ram[0:272]; // it has (4+16+1)*13=273 elements, 0-272
  //initial
  //  $readmemh("message_ps.mem", rds_ram);
  // SPI writes to RDS RAM
  always @(posedge clk)
  begin
    if(ram_wr & spi_rds_cs)
      rds_ram[ram_addr[8:0]] <= ram_di;
  end

  reg [7:0] r_fm_freq[0:7];
  always @(posedge clk)
  begin
    if(ram_wr & spi_freq_cs)
      r_fm_freq[ram_addr[2:0]] <= ram_di;
  end
  wire [31:0] fm_freq1 = {r_fm_freq[3],r_fm_freq[2],r_fm_freq[1],r_fm_freq[0]};
  wire [31:0] fm_freq2 = {r_fm_freq[7],r_fm_freq[6],r_fm_freq[5],r_fm_freq[4]};
  // FM core reads from RDS RAM
  wire antena1, antena2;
  wire [8:0] rds_addr;
  reg  [7:0] rds_data;
  always @(posedge clk)
    rds_data <= rds_ram[rds_addr];
  fmgen_rds
  fmgen_rds_inst
  (
    .clk(clk), // 40 MHz
    .clk_fmdds(clk_fmdds), // 240 MHz
    //.pcm_in_left( btn[1] ? beep[15:1] : wav_data_signed), // debug
    //.pcm_in_right(btn[2] ? beep[15:1] : wav_data_signed), // debug
    .pcm_in_left( wav_data_signed), // normal
    .pcm_in_right(wav_data_signed), // normal
    .cw_freq1(fm_freq1),
    .cw_freq2(fm_freq2),
    .rds_addr(rds_addr),
    .rds_data(rds_data),
    .fm_antenna1(antena1), // 107.9 MHz
    .fm_antenna2(antena2)  //  87.6 MHz
  );
  assign ant_433mhz = antena1; // internal antenna
  assign gp19       = antena1; // external antenna
  assign gp20       = antena2; // external antenna

  wire [1:0] dac;
  dacpwm
  #(
    .c_pcm_bits(8),
    .c_dac_bits(2)
  )
  dacpwm_inst
  (
    .clk(clk),
    .pcm(wav_data_signed[15:8]),
    .dac(dac)
  );
  
  assign audio_l[1:0] = dac;
  assign audio_r[1:0] = dac;

  localparam a_default = 16384; // default sensor reading 1g acceleration

  reg [ 7:0] vx_ram[0:7]; // 8-byte: 2-byte=16-bit speed [um/s], 4-byte=32-bit const/speed, 2-byte unused
  reg [15:0] vx   = 0;
  reg [31:0] cvx  = 0;
  reg slope_reset = 0;
  always @(posedge clk)
  begin
    if(ram_wr & spi_calc_cs)
    begin
      vx_ram[ram_addr[2:0]] <= ram_di;
      if(ram_addr[2:0] == 5)
      begin
        vx   <= {vx_ram[0],vx_ram[1]}; // mm/s vx speed
        cvx  <= {vx_ram[2],vx_ram[3],vx_ram[4],ram_di}; // c/vx speed
      end
    end
    slope_reset <= vx == 0; // speed 0
  end

  // NOTE reg delay, trying to fix synth problems
  reg slope_reset2;
  always @(posedge clk)
  begin
    slope_reset2 <= slope_reset;
  end

  // fault detection: after each slope reset
  // turn on fault signal if vz is zero
  generate
    reg [1:0] r_fault;
    if(0)
    begin
      always @(posedge clk)
      begin
        if(slope_reset2)
          r_fault <= 0;
        else
        begin
          if(sync_pulse)
            r_fault <= r_fault | {azl[15:2]==0 || ~azl[15:2]==0, azr[15:2]==0 || ~azr[15:2]==0}; // if z-axis reads near 0, set fault
        end
      end
      assign led[2:1] = ~r_fault; // on fault LED will turn OFF
    end
  endgenerate

  reg  signed [31:0] ma = a_default;
  reg  signed [31:0] mb = a_default;

  always @(posedge clk)
  begin
    if(btn_rising[3])
      ma <= ma + 32'h10;
    else if(btn_rising[4])
      ma <= ma - 32'h10;
    if(btn_rising[6])
      mb <= mb + 32'h10;
    else if(btn_rising[5])
      mb <= mb - 32'h10;
  end

  wire [7:0] disp_x, disp_y;
  wire [15:0] disp_color;

  wire [127:0] data;
  generate
  if(lcd_hex)
  hex_decoder_v
  #(
    .c_data_len(128),
    .c_grid_6x8(1)
  )
  hex_decoder_inst
  (
    .clk(clk),
    .data(data),
    .x(disp_x[7:1]),
    .y(disp_y[7:1]),
    .color(disp_color)
  );
  endgenerate

  // OSD overlay
  wire [7:0] osd_vga_r, osd_vga_g, osd_vga_b;
  wire osd_vga_hsync, osd_vga_vsync, osd_vga_blank;
  wire vga_hsync;
  wire vga_vsync;
  wire vga_blank;
  wire [7:0] vga_r, vga_g, vga_b;
  wire [15:0] vga_rgb = {vga_r[7:3],vga_g[7:2],vga_b[7:3]};
  generate
  if(lcd_txt)
  begin
  vga
  // trellis doesn't pass parameters to vhdl submodule
  // parameters instead of here, are passed in vhdl generic parameters
  //#(
  //  .c_resolution_x(240),
  //  .c_hsync_front_porch(1800),
  //  .c_hsync_pulse(1),
  //  .c_hsync_back_porch(1800),
  //  .c_resolution_y(240),
  //  .c_vsync_front_porch(1),
  //  .c_vsync_pulse(1),
  //  .c_vsync_back_porch(1),
  //  .c_bits_x(12),
  //  .c_bits_y(8)
  //)
  vga_instance
  (
    .clk_pixel(clk_pixel),
    .clk_pixel_ena(1'b1),
    .test_picture(1'b1),
    .vga_r(vga_r),
    .vga_g(vga_g),
    .vga_b(vga_b),
    .vga_hsync(vga_hsync),
    .vga_vsync(vga_vsync),
    .vga_blank(vga_blank)
  );
  
  spi_osd_v
  #(
    .c_sclk_capable_pin(1'b0),
    .c_start_x     ( 0), .c_start_y( 0), // xy centering
    .c_char_bits_x ( 5), .c_chars_y(15), // xy size, slightly less than full screen
    .c_bits_x      (12), .c_bits_y ( 8), // xy counters bits
    .c_inverse     ( 1),
    .c_transparency( 0),
    .c_init_on     ( 1),
    .c_char_file   ("osd.mem"),
    .c_font_file   ("font_bizcat8x16.mem")
  )
  spi_osd_v_instance
  (
    .clk_pixel(clk_pixel),
    .clk_pixel_ena(1),
    .i_r(0),
    .i_g(0),
    .i_b(0),
    .i_hsync(vga_hsync),
    .i_vsync(vga_vsync),
    .i_blank(vga_blank),
    .i_csn(csn),
    .i_sclk(~sclk),
    .i_mosi(mosi),
    .o_r(osd_vga_r),
    .o_g(osd_vga_g),
    .o_b(osd_vga_b),
    .o_hsync(osd_vga_hsync),
    .o_vsync(osd_vga_vsync),
    .o_blank(osd_vga_blank)
  );
  assign disp_color = {osd_vga_r,osd_vga_g};
  end
  endgenerate

  lcd_video
  #(
    .c_clk_spi_mhz(clk_out2_hz/1000000),
    .c_vga_sync(lcd_txt),
    .c_reset_us(2000), // adjust for reliable start of LCD, wrong value = blank screen
    .c_init_file(lcd_txt ? "st7789_linit.mem" : "st7789_linit_xflip.mem"), // flip for HEX display
    //.c_init_size(75), // long init
    //.c_init_size(35), // standard init (not long)
    .c_clk_phase(0),
    .c_clk_polarity(1),
    .c_x_size(240),
    .c_y_size(240),
    .c_color_bits(16)
  )
  lcd_video_instance
  (
    .reset(0),
    .clk_pixel(clk_pixel), // 40 MHz
    .clk_pixel_ena(1),
    .clk_spi(clk_lcd), // 120 MHz
    .clk_spi_ena(1),
    .blank(osd_vga_blank),
    .hsync(osd_vga_hsync),
    .vsync(osd_vga_vsync),
    .x(disp_x),
    .y(disp_y),
    .color(disp_color),
    .spi_resn(oled_resn),
    .spi_clk(oled_clk),
    //.spi_csn(oled_csn), // 8-pin ST7789
    .spi_dc(oled_dc),
    .spi_mosi(oled_mosi)
  );
  assign oled_csn = 1; // 7-pin ST7789
  
  reg [19:0] autoenter;
  always @(posedge clk)
    if(autoenter[19] == 0)
      autoenter <= autoenter + 1;
    else
      autoenter <= 0;
  wire autofire = autoenter[19];

  wire slope_ready;
  wire [31:0] slope_l, slope_r;
  slope
  //#(
  //  .a_default(a_default)
  //)
  slope_inst
  (
    .clk(clk),
    //.reset(1'b0),
    //.reset(btn_debounce[1]), // debug
    .reset(slope_reset2), // check is reset working (delayed, sometimes fixes synth problems)
    .enter(sync_pulse), // normal
    //.enter(btn_rising[1]), // debug
    //.enter(autofire), // debug
    .hold(1'b0), // normal
    //.hold(1'b1), // don't remove slope DC (DC offset control possible malfunction, synth problems)
    //.hold(btn_debounce[1]), // debug
    //.vx(22000), // vx in mm/s, 22000 um = 22 mm per 1kHz sample
    //.cvx(40181760/22000), // int_vx_scale/vx, vx in mm/s, 1826 for 22000 mm/s
    .vx(vx), // vx in mm/s
    .cvx(cvx), // int_vx_scale/vx, vx in mm/s
    //.azl(ma), // btn
    //.azr(mb), // btn
    .axl(axl), // X from left  sensor
    .axr(axr), // X from right sensor
    .azl(azl), // Z from left  sensor
    .azr(azr), // Z from right sensor
    .slope_l(slope_l), // um/m
    .slope_r(slope_r), // um/m
    //.slope_l(data[127:96]),
    //.slope_r(data[95:64]),
    //.d0(slope_ag),
    //.d1(slope_aa),
    .ready(slope_ready)
  );

  wire [63:0] srvz, srvz2;
  calc
  calc_inst
  (
    .clk(clk),
    .enter(slope_ready),  // normal
    //.enter(btn_rising[1]), // debug
    //.enter(autofire), // debug
    .slope_l(slope_l), // um/m
    .slope_r(slope_r),
    //.slope_l(ma), // um/m
    //.slope_r(mb),
    //.vz_l(data[127:96]),
    //.vz_r(data[95:64])
    .srvz_l(srvz[63:32]),
    .srvz_r(srvz[31: 0]),
    .srvz2_l(srvz2[63:32]),
    .srvz2_r(srvz2[31: 0])
    //.d0(data[ 63:32]),
    //.d1(data[ 31:0 ]),
    //.d2(data[127:96]),
    //.d3(data[ 95:64])
  );
  //assign data[63:32] = ma;
  //assign data[31:0]  = mb;
  //assign data[63:32] = {ayl, axl};
  //assign data[31:0]  = {ayr, axr};
  //assign data[63:32] = {0, azl};
  //assign data[31:0]  = {0, azr};
  assign data[ 63:32]  = slope_l;
  assign data[ 31: 0]  = slope_r;
  assign data[127:96]  = srvz[63:32];
  assign data[ 95:64]  = srvz[31: 0];
  //assign data[127:96]  = slope_aa;
  //assign data[ 95:64]  = slope_ag;
  //assign data[ 63:32]  = vx;
  //assign data[ 31: 0]  = cvx;

  // latch calc result when reading 1st byte
  generate
    genvar i;
    for(i = 0; i < 4; i++)
    begin
      assign calc_result[ 3-i] = srvz [(i+4)*8+7:(i+4)*8]; // sum for IRI-100 left
      assign calc_result[ 7-i] = srvz [(i+0)*8+7:(i+0)*8]; // sum for IRI-100 right
      assign calc_result[11-i] = srvz2[(i+4)*8+7:(i+4)*8]; // sum for IRI-20  left
      assign calc_result[15-i] = srvz2[(i+0)*8+7:(i+0)*8]; // sum for IRI-20  right
    end
  endgenerate

  always @(posedge clk)
  begin
    if(~spi_calc_cs) // store to reg when cs = 0
    begin
      r_calc_result[ 0] <= calc_result[ 0];
      r_calc_result[ 1] <= calc_result[ 1];
      r_calc_result[ 2] <= calc_result[ 2];
      r_calc_result[ 3] <= calc_result[ 3];
      r_calc_result[ 4] <= calc_result[ 4];
      r_calc_result[ 5] <= calc_result[ 5];
      r_calc_result[ 6] <= calc_result[ 6];
      r_calc_result[ 7] <= calc_result[ 7];
      r_calc_result[ 8] <= calc_result[ 8];
      r_calc_result[ 9] <= calc_result[ 9];
      r_calc_result[10] <= calc_result[10];
      r_calc_result[11] <= calc_result[11];
      r_calc_result[12] <= calc_result[12];
      r_calc_result[13] <= calc_result[13];
      r_calc_result[14] <= calc_result[14];
      r_calc_result[15] <= calc_result[15];
    end
  end

  generate
    genvar i;
    for(i = 0; i < 16; i++)
    begin
      assign w_calc_result[i] = r_calc_result[i];
    end
  endgenerate

endmodule
`default_nettype wire
