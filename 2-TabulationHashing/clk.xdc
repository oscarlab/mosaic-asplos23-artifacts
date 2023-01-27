##The following specifies the clock at a period of 2ns
set_property -dict { PACKAGE_PIN E3 IOSTANDARD LVCMOS33 } [get_ports { clk }];
create_clock -add -name sys_clk_pin -period 2 -waveform {0 1.0} [get_ports { clk }];