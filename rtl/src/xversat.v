`timescale 1ns / 1ps

// Top defines
`include "xversat.vh"
`include "xconfdefs.vh"

// FU defines              
`include "xmemdefs.vh"
`include "xaludefs.vh"
`include "xalulitedefs.vh"
`include "xmuldefs.vh"
`include "xmuladddefs.vh"
`include "xbsdefs.vh"

module xversat #(
		 parameter			ADDR_W = 32,
		 parameter			DATA_W = 32
	 	)(
                 input                    	clk,
                 input                    	rst,

                 //data/control interface
                 input                    	valid,
                 input [ADDR_W-1:0]     	addr,
                 input                    	we,
                 input [DATA_W-1:0]      	rdata,
                 output reg [DATA_W-1:0]	wdata
                 );

   //data buses for each versat   
   wire [`DATABUS_W-1:0] stage_databus [`nSTAGE:0];
   wire run_done = ~addr[1+`nMEM_W+`MEM_ADDR_W] & addr[`nMEM_W+`MEM_ADDR_W];

   //
   // ADDRESS DECODER
   //

   //select stage
   reg [`nSTAGE-1:0] stage_valid;
   always @ * begin
     integer j;
     stage_valid = {`nSTAGE{1'b0}};
       for(j=0; j<`nSTAGE; j=j+1)
          if (addr[`CTR_ADDR_W-1 -: `nSTAGE_W] == j[`nSTAGE_W-1:0] || run_done)
            stage_valid[j] = valid;
   end 

   //check done in all stages
   reg [`nSTAGE-1:0] done;
   always @ * begin
     integer j;
     for(j = 0; j < `nSTAGE; j++)
       done[j] = stage_wdata[j][0];
   end

   //select stage ctr data
   reg [DATA_W-1:0] stage_wdata [`nSTAGE-1:0];
   always @ * begin
      integer j;
      wdata = {DATA_W{1'b0}};
      for(j=0; j<`nSTAGE; j=j+1) begin
         if(run_done)
           wdata = {{DATA_W-1{1'b0}}, &done};       
         else if (stage_valid[j])
           wdata = stage_wdata[j];
      end
   end

   //
   // INSTANTIATE THE VERSAT STAGES
   //
   
   genvar i;
   generate
      for (i=0; i < `nSTAGE; i=i+1) begin : versat_array
        xstage # (
		      .DATA_W(DATA_W)
	) stage (
                      .clk(clk),
                      .rst(rst),

                      //data/control interface
                      .valid(stage_valid[i]),
                      .we(we),
                      .addr(addr[`CTR_ADDR_W-`nSTAGE_W-1:0]),
                      .rdata(rdata),
                      .wdata(stage_wdata[i]),
                      
                      //flow interface
                      .flow_in(stage_databus[i]),
                      .flow_out(stage_databus[i+1])
                      );
      end
    endgenerate

   //connect last stage back to first stage: ring topology
   assign stage_databus[0] = stage_databus [`nSTAGE];     

endmodule
