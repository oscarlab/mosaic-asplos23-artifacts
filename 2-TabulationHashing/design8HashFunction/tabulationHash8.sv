`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 10/17/2022 10:02:07 PM
// Design Name: 
// Module Name: tabulationHash8
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


module tabulationHash8(
    input wire clk, 
    input reg [44:0] virtualPageNumber,
    input reg [2:0] hashID, 
    output reg [31:0] hashOutput
    );
    wire [31:0] t1Output1; 
    wire [31:0] t1Output2;
    wire [31:0] t1Output3;
    wire [31:0] t1Output4;
    wire [31:0] t1Output5;
    wire [31:0] t1Output6;
    wire [31:0] t1Output7;
    wire [31:0] t1Output8;
    
    wire [31:0] t2Output1; 
    wire [31:0] t2Output2;
    wire [31:0] t2Output3;
    wire [31:0] t2Output4;
    wire [31:0] t2Output5;
    wire [31:0] t2Output6;
    wire [31:0] t2Output7;
    wire [31:0] t2Output8;
    
    wire [31:0] t3Output1; 
    wire [31:0] t3Output2;
    wire [31:0] t3Output3;
    wire [31:0] t3Output4;
    wire [31:0] t3Output5;
    wire [31:0] t3Output6;
    wire [31:0] t3Output7;
    wire [31:0] t3Output8;
    
    wire [31:0] t4Output1; 
    wire [31:0] t4Output2;
    wire [31:0] t4Output3;
    wire [31:0] t4Output4;
    wire [31:0] t4Output5;
    wire [31:0] t4Output6;
    wire [31:0] t4Output7;
    wire [31:0] t4Output8;
    
    wire [31:0] t5Output1; 
    wire [31:0] t5Output2;
    wire [31:0] t5Output3;
    wire [31:0] t5Output4;
    wire [31:0] t5Output5;
    wire [31:0] t5Output6;
    wire [31:0] t5Output7;
    wire [31:0] t5Output8;
    
    wire [31:0] t6Output1; 
    wire [31:0] t6Output2;
    wire [31:0] t6Output3;
    wire [31:0] t6Output4;
    wire [31:0] t6Output5;
    wire [31:0] t6Output6;
    wire [31:0] t6Output7;
    wire [31:0] t6Output8;
    
    
    
    
   
    staticTable8 #(.Nloc(256), .Dbits(32) , .initfile("table1_initfile.mem")) table1(.readAddr(virtualPageNumber[44:37]) , .dataOut1(t1Output1),.dataOut2(t1Output2),.dataOut3(t1Output3),.dataOut4(t1Output4),.dataOut5(t1Output5),.dataOut6(t1Output6),.dataOut7(t1Output7),.dataOut8(t1Output8));
    staticTable8 #(.Nloc(256), .Dbits(32) , .initfile("table2_initfile.mem")) table2(.readAddr(virtualPageNumber[36:29]) , .dataOut1(t2Output1),.dataOut2(t2Output2),.dataOut3(t2Output3),.dataOut4(t2Output4),.dataOut5(t2Output5),.dataOut6(t2Output6),.dataOut7(t2Output7),.dataOut8(t2Output8));
    staticTable8 #(.Nloc(256), .Dbits(32) , .initfile("table3_initfile.mem")) table3(.readAddr(virtualPageNumber[28:21]) , .dataOut1(t3Output1),.dataOut2(t3Output2),.dataOut3(t3Output3),.dataOut4(t3Output4),.dataOut5(t3Output5),.dataOut6(t3Output6),.dataOut7(t3Output7),.dataOut8(t3Output8));
    staticTable8 #(.Nloc(256), .Dbits(32) , .initfile("table4_initfile.mem")) table4(.readAddr(virtualPageNumber[20:13]) , .dataOut1(t4Output1),.dataOut2(t4Output2),.dataOut3(t4Output3),.dataOut4(t4Output4),.dataOut5(t4Output5),.dataOut6(t4Output6),.dataOut7(t4Output7),.dataOut8(t4Output8));
    staticTable8 #(.Nloc(256), .Dbits(32) , .initfile("table5_initfile.mem")) table5(.readAddr(virtualPageNumber[12:5]) , .dataOut1(t5Output1),.dataOut2(t5Output2),.dataOut3(t5Output3),.dataOut4(t5Output4),.dataOut5(t5Output5),.dataOut6(t5Output6),.dataOut7(t5Output7),.dataOut8(t5Output8));
    staticTable8 #(.Nloc(32),  .Dbits(32) , .initfile("table6_initfile.mem")) table6(.readAddr(virtualPageNumber[4:0]) , .dataOut1(t6Output1),.dataOut2(t6Output2),.dataOut3(t6Output3),.dataOut4(t6Output4),.dataOut5(t6Output5),.dataOut6(t6Output6),.dataOut7(t6Output7),.dataOut8(t6Output8));
    
    wire [31:0] t1OutputFinal ; 
    wire [31:0] t2OutputFinal ;
    wire [31:0] t3OutputFinal ;
    wire [31:0] t4OutputFinal ;
    wire [31:0] t5OutputFinal ;
    wire [31:0] t6OutputFinal ; 
    
    assign t1OutputFinal = (hashID == 3'b000) ? t1Output1 :
                           (hashID == 3'b001) ? t1Output2 : 
                           (hashID == 3'b010) ? t1Output3 :
                           (hashID == 3'b011) ? t1Output4 : 
                           (hashID == 3'b100) ? t1Output5 :
                           (hashID == 3'b101) ? t1Output6 : 
                           (hashID == 3'b110) ? t1Output7 :
                           (hashID == 3'b111) ? t1Output8 :
                           31'bx ;
                           
    assign t2OutputFinal = (hashID == 3'b000) ? t2Output1 :
                           (hashID == 3'b001) ? t2Output2 : 
                           (hashID == 3'b010) ? t2Output3 :
                           (hashID == 3'b011) ? t2Output4 : 
                           (hashID == 3'b100) ? t2Output5 :
                           (hashID == 3'b101) ? t2Output6 : 
                           (hashID == 3'b110) ? t2Output7 :
                           (hashID == 3'b111) ? t2Output8 :
                           31'bx ; 
   
    assign t3OutputFinal = (hashID == 3'b000) ? t3Output1 :
                           (hashID == 3'b001) ? t3Output2 : 
                           (hashID == 3'b010) ? t3Output3 :
                           (hashID == 3'b011) ? t3Output4 : 
                           (hashID == 3'b100) ? t3Output5 :
                           (hashID == 3'b101) ? t3Output6 : 
                           (hashID == 3'b110) ? t3Output7 :
                           (hashID == 3'b111) ? t3Output8 :
                           31'bx ; 
                           
    assign t4OutputFinal = (hashID == 3'b000) ? t4Output1 :
                           (hashID == 3'b001) ? t4Output2 : 
                           (hashID == 3'b010) ? t4Output3 :
                           (hashID == 3'b011) ? t4Output4 : 
                           (hashID == 3'b100) ? t4Output5 :
                           (hashID == 3'b101) ? t4Output6 : 
                           (hashID == 3'b110) ? t4Output7 :
                           (hashID == 3'b111) ? t4Output8 :
                           31'bx ;
                           
                           
                           
    assign t5OutputFinal = (hashID == 3'b000) ? t5Output1 :
                           (hashID == 3'b001) ? t5Output2 : 
                           (hashID == 3'b010) ? t5Output3 :
                           (hashID == 3'b011) ? t5Output4 : 
                           (hashID == 3'b100) ? t5Output5 :
                           (hashID == 3'b101) ? t5Output6 : 
                           (hashID == 3'b110) ? t5Output7 :
                           (hashID == 3'b111) ? t5Output8 :
                           31'bx ;
                           
    assign t6OutputFinal = (hashID == 3'b000) ? t6Output1 :
                           (hashID == 3'b001) ? t6Output2 : 
                           (hashID == 3'b010) ? t6Output3 :
                           (hashID == 3'b011) ? t6Output4 : 
                           (hashID == 3'b100) ? t6Output5 :
                           (hashID == 3'b101) ? t6Output6 : 
                           (hashID == 3'b110) ? t6Output7 :
                           (hashID == 3'b111) ? t6Output8 :
                           31'bx ; 
                
    always @(posedge clk) begin
        hashOutput <= t1OutputFinal ^ t2OutputFinal ^ t3OutputFinal ^ t4OutputFinal ^ t5OutputFinal ^ t6OutputFinal;
    end 
 
endmodule
