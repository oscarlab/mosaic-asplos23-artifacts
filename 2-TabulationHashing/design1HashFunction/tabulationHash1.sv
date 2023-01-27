`timescale 1ns / 1ps
`default_nettype none
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 03/28/2022 10:59:18 PM
// Design Name: 
// Module Name: tabulationHash2
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module tabulationHash1(
    input wire clk,
    input reg [44:0] virtualPageNumber, 
    output reg [31:0] hashOutput
    );
    
    wire [31:0] t1OutputFinal ; 
    wire [31:0] t2OutputFinal ;
    wire [31:0] t3OutputFinal ;
    wire [31:0] t4OutputFinal ;
    wire [31:0] t5OutputFinal ;
    wire [31:0] t6OutputFinal ; 
    
    staticTable1 #(.Nloc(256), .Dbits(32) , .initfile("table1_initfile.mem")) table1(.readAddr(virtualPageNumber[44:37]) , .dataOut1(t1OutputFinal));
    staticTable1 #(.Nloc(256), .Dbits(32) , .initfile("table2_initfile.mem")) table2(.readAddr(virtualPageNumber[36:29]) , .dataOut1(t2OutputFinal));
    staticTable1 #(.Nloc(256), .Dbits(32) , .initfile("table3_initfile.mem")) table3(.readAddr(virtualPageNumber[28:21]) , .dataOut1(t3OutputFinal));
    staticTable1 #(.Nloc(256), .Dbits(32) , .initfile("table4_initfile.mem")) table4(.readAddr(virtualPageNumber[20:13]) , .dataOut1(t4OutputFinal));
    staticTable1 #(.Nloc(256), .Dbits(32) , .initfile("table5_initfile.mem")) table5(.readAddr(virtualPageNumber[12:5]) , .dataOut1(t5OutputFinal));
    staticTable1 #(.Nloc(32),  .Dbits(32) , .initfile("table6_initfile.mem")) table6(.readAddr(virtualPageNumber[4:0]) , .dataOut1(t6OutputFinal));
                               
    always @(posedge clk) begin
        hashOutput <= t1OutputFinal ^ t2OutputFinal ^ t3OutputFinal ^ t4OutputFinal ^ t5OutputFinal ^ t6OutputFinal;
    end
     
    
endmodule