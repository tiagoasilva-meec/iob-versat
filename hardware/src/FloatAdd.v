`timescale 1ns / 1ps

module FloatAdd #(
         parameter DATA_W = 32
              )
    (
    //control
    input                         clk,
    input                         rst,
    
    input                         running,
    input                         run,
    
    //input / output data
    input [DATA_W-1:0]            in0,
    input [DATA_W-1:0]            in1,
    
    (* versat_latency = 5 *) output [DATA_W-1:0]       out0
    );

fp_add adder(
     .start(1'b1),
     .done(),

     .op_a(in0),
     .op_b(in1),

     .res(out0),

     .overflow(),
     .underflow(),
     .exception(),

     .clk(clk),
     .rst(rst)
     );

endmodule
