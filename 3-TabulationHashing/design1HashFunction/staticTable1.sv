`timescale 1ns / 1ps
`default_nettype none

module staticTable1 #(
   parameter Nloc = 256,                      // Number of table locations
   parameter Dbits = 32,                      // Number of bits at each table location
   parameter initfile = "init.mem"         // Name of table initialization file
)(
   input wire [$clog2(Nloc)-1 : 0] readAddr,     // Address for specifying memory location
                                        
   output wire [Dbits-1 : 0] dataOut1            // Data read from memory asynchronously
   );

   logic [Dbits-1 : 0] static_table [Nloc-1 : 0];
   
   initial $readmemh(initfile, static_table, 0, Nloc-1); // Initialize memory contents from a provided initialization file

   assign dataOut1 = static_table[readAddr];                  // Memory read: read continuously, no clock involved
  
endmodule