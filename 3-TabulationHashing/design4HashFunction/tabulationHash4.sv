`timescale 1ns / 1ps
`default_nettype none
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 03/28/2022 10:59:18 PM
// Design Name: 
// Module Name: tabulationHash
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


module tabulationHash4(
    input wire clk,
    input reg [44:0] virtualPageNumber,
    input reg [1:0] hashID, 
    output reg [31:0] hashOutput
    );
    
    wire [31:0] t1Output1; 
    wire [31:0] t1Output2;
    wire [31:0] t1Output3;
    wire [31:0] t1Output4;
    
    wire [31:0] t2Output1; 
    wire [31:0] t2Output2;
    wire [31:0] t2Output3;
    wire [31:0] t2Output4;
    
    wire [31:0] t3Output1; 
    wire [31:0] t3Output2;
    wire [31:0] t3Output3;
    wire [31:0] t3Output4;
    
    wire [31:0] t4Output1; 
    wire [31:0] t4Output2;
    wire [31:0] t4Output3;
    wire [31:0] t4Output4;
    
    wire [31:0] t5Output1; 
    wire [31:0] t5Output2;
    wire [31:0] t5Output3;
    wire [31:0] t5Output4;
    
    wire [31:0] t6Output1; 
    wire [31:0] t6Output2;
    wire [31:0] t6Output3;
    wire [31:0] t6Output4;
    
    
   
    staticTable4 #(.Nloc(256), .Dbits(32) , .initfile("table1_initfile.mem")) table1(.readAddr(virtualPageNumber[44:37]) , .dataOut1(t1Output1),.dataOut2(t1Output2),.dataOut3(t1Output3),.dataOut4(t1Output4));
    staticTable4 #(.Nloc(256), .Dbits(32) , .initfile("table2_initfile.mem")) table2(.readAddr(virtualPageNumber[36:29]) , .dataOut1(t2Output1),.dataOut2(t2Output2),.dataOut3(t2Output3),.dataOut4(t2Output4));
    staticTable4 #(.Nloc(256), .Dbits(32) , .initfile("table3_initfile.mem")) table3(.readAddr(virtualPageNumber[28:21]) , .dataOut1(t3Output1),.dataOut2(t3Output2),.dataOut3(t3Output3),.dataOut4(t3Output4));
    staticTable4 #(.Nloc(256), .Dbits(32) , .initfile("table4_initfile.mem")) table4(.readAddr(virtualPageNumber[20:13]) , .dataOut1(t4Output1),.dataOut2(t4Output2),.dataOut3(t4Output3),.dataOut4(t4Output4));
    staticTable4 #(.Nloc(256), .Dbits(32) , .initfile("table5_initfile.mem")) table5(.readAddr(virtualPageNumber[12:5]) , .dataOut1(t5Output1),.dataOut2(t5Output2),.dataOut3(t5Output3),.dataOut4(t5Output4));
    staticTable4 #(.Nloc(32),  .Dbits(32) , .initfile("table6_initfile.mem")) table6(.readAddr(virtualPageNumber[4:0]) , .dataOut1(t6Output1),.dataOut2(t6Output2),.dataOut3(t6Output3),.dataOut4(t6Output4));
    
    wire [31:0] t1OutputFinal ; 
    wire [31:0] t2OutputFinal ;
    wire [31:0] t3OutputFinal ;
    wire [31:0] t4OutputFinal ;
    wire [31:0] t5OutputFinal ;
    wire [31:0] t6OutputFinal ; 
    
    assign t1OutputFinal = (hashID == 2'b00) ? t1Output1 :
                           (hashID == 2'b01) ? t1Output2 : 
                           (hashID == 2'b10) ? t1Output3 :
                           (hashID == 2'b11) ? t1Output4 : 
                           31'bx ; 
                           
    assign t2OutputFinal = (hashID == 2'b00) ? t2Output1 :
                           (hashID == 2'b01) ? t2Output2 : 
                           (hashID == 2'b10) ? t2Output3 :
                           (hashID == 2'b11) ? t2Output4 : 
                           31'bx ; 
   
    assign t3OutputFinal = (hashID == 2'b00) ? t3Output1 :
                           (hashID == 2'b01) ? t3Output2 : 
                           (hashID == 2'b10) ? t3Output3 :
                           (hashID == 2'b11) ? t3Output4 : 
                           31'bx ; 
                           
    assign t4OutputFinal = (hashID == 2'b00) ? t4Output1 :
                           (hashID == 2'b01) ? t4Output2 : 
                           (hashID == 2'b10) ? t4Output3 :
                           (hashID == 2'b11) ? t4Output4 : 
                           31'bx ; 
                           
    assign t5OutputFinal = (hashID == 2'b00) ? t5Output1 :
                           (hashID == 2'b01) ? t5Output2 : 
                           (hashID == 2'b10) ? t5Output3 :
                           (hashID == 2'b11) ? t5Output4 : 
                           31'bx ; 
                           
    assign t6OutputFinal = (hashID == 2'b00) ? t6Output1 :
                           (hashID == 2'b01) ? t6Output2 : 
                           (hashID == 2'b10) ? t6Output3 :
                           (hashID == 2'b11) ? t6Output4 : 
                           31'bx ; 
                           
    always @(posedge clk) begin
        hashOutput <= t1OutputFinal ^ t2OutputFinal ^ t3OutputFinal ^ t4OutputFinal ^ t5OutputFinal ^ t6OutputFinal;
    end
    
endmodule