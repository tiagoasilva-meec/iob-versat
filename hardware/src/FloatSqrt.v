`timescale 1ns / 1ps

module FloatSqrt #(
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

    input [31:0]                  delay0,
    
    (* versat_latency = 31 *) output [DATA_W-1:0]       out0
    );

reg start;
reg [31:0] delay;
wire done;

always @(posedge clk,posedge rst)
begin
     if(rst) begin
          start <= 1'b0;
          delay <= 0;
     end else if(run) begin
          delay <= delay0;
          start <= 1'b1;
     end else begin
          start <= 1'b0;
          if(|delay == 0) begin
               delay <= 0;
          end else begin
               delay <= delay - 1;
               if(delay == 1) begin
                    start <= 1'b1;
               end
          end
     end
end

fp_sqrt sqrt(
     .start(start),
     .done(done),

     .op(in0),

     .res(out0),

     .overflow(),
     .underflow(),
     .exception(),

     .clk(clk),
     .rst(rst)
     );

endmodule
