`timescale 1ns / 1ps
`default_nettype none

module staticTable8 #(
   parameter Nloc = 256,                      // Number of table locations
   parameter Dbits = 32,                      // Number of bits at each table location
   parameter initfile = "init.mem"         // Name of table initialization file
)(
   input wire [$clog2(Nloc)-1 : 0] readAddr,     // Address for specifying memory location
                                        
   output wire [Dbits-1 : 0] dataOut1,            // Data read from memory asynchronously
   output wire [Dbits-1 : 0] dataOut2,
   output wire [Dbits-1 : 0] dataOut3,
   output wire [Dbits-1 : 0] dataOut4,
   output wire [Dbits-1 : 0] dataOut5,
   output wire [Dbits-1 : 0] dataOut6,
   output wire [Dbits-1 : 0] dataOut7,
   output wire [Dbits-1 : 0] dataOut8
   );

   logic [Dbits-1 : 0] static_table [Nloc-1 : 0];
   
   initial $readmemh(initfile, static_table, 0, Nloc-1); // Initialize memory contents from a provided initialization file


   assign dataOut1 = static_table[readAddr];                  // Memory read: read continuously, no clock involved
   assign dataOut2 = static_table[readAddr+1];
   assign dataOut3 = static_table[readAddr+2];
   assign dataOut4 = static_table[readAddr+3];
   assign dataOut5 = static_table[readAddr+4];
   assign dataOut6 = static_table[readAddr+5];
   assign dataOut7 = static_table[readAddr+6];
   assign dataOut8 = static_table[readAddr+7];
   

endmodule
