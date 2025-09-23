// drdy generator for ADXL355 accelerometer
// input is 1 kHz sync which is a single pulse in 40 MHz clock domain

// output is hardware sync which is 1 kHz with extended pulse width
// and drdy signal which is delayed sync

`default_nettype none
module adxl355_drdy
#(
  clk_out0_hz   = 40*1000000, // Hz, 40 MHz, input system clock rate
  sync_width_us = 20,         // output sync with extended width
  drdy_delay_us = 50,         // output delayed sync = drdy 1 kHz 1 pulse in 40 MHz domain
  timing_bits   = 20          // timer counters are this bits wide
)
(
  input  i_clk,       // 40 MHz system clock
  input  i_clk_sync,  // 1 kHz input sync narrow 1-clk-40MHz pulse width
  output o_clk_sync,  // 1 kHz extended pulse width
  output o_clk_drdy   // 1 kHz narrow 1-clk-40MHz pulse width  
);
  localparam real re_sync_count = sync_width_us * 1000000 / clk_out0_hz; // real number of counts for sync width
  localparam integer int_sync_count = re_sync_count; // integer number of counts for sync width

  reg counter_sync[timing_bits-1:0];
  assign o_clk_sync = ~(counter_sync[timing_bits-1]);
  always @(posedge i_clk)
  begin
    // reset pulse width counter
    if(i_clk_sync)
      counter_sync <= int_sync_count;
    else
      if(o_clk_sync)
        counter_sync <= counter_sync-1;
  end

  localparam real re_drdy_count = drdy_delay_us * 1000000 / clk_out0_hz; // real number of counts for drdy delay
  localparam integer int_drdy_count = re_drdy_count; // integer number of counts for drdy delay
  reg counter_drdy[timing_bits-1:0];
  reg prev_run_drdy;
  reg o_clk_drdy;
  wire run_drdy = ~(counter_drdy[timing_bits-1]);
  always @(posedge i_clk)
  begin
    // reset delayed pulse counter
    if(i_clk_sync)
      counter_drdy <= int_drdy_count;
    else
      if(run_drdy)
        counter_drdy <= counter_drdy-1;
    prev_run_drdy <= run_drdy;
    // outputs delayed 1-clk pulse used as drdy signal (data ready)
    o_clk_drdy <= prev_run_drdy == 1'b1 && run_drdy == 1'b0 ? 1'b1 : 1'b0;
  end

endmodule
`default_nettype wire
