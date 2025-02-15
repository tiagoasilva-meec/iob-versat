#include <new>

#include "versat_accel.h"
#include "versat.hpp"
#include "utils.hpp"

#include "verilated.h"
#include "verilated_vcd_c.h"

static VerilatedVcdC* tfp = NULL;
VerilatedContext* contextp = new VerilatedContext;

static int zeros[99] = {};
static Array<int> zerosArray = {zeros,99};

#{for module modules}
#include "V@{module.name}.h"
static V@{module.name}* dut = NULL;
#{end}

#define INIT(unit) \
   unit->run = 0; \
   unit->clk = 0; \
   unit->rst = 0; \
   unit->running = 1;

#define UPDATE(unit) \
   unit->clk = 0; \
   unit->eval(); \
   tfp->dump(contextp->time()); \
   contextp->timeInc(1); \
   unit->clk = 1; \
   unit->eval(); \
   tfp->dump(contextp->time()); \
   contextp->timeInc(1);

#define RESET(unit) \
   unit->rst = 1; \
   UPDATE(unit); \
   unit->rst = 0;

#define START_RUN(unit) \
   unit->run = 1; \
   UPDATE(unit); \
   unit->run = 0;

#define PREAMBLE(type) \
   type* self = &data->unit; \
   VCDData* vcd = &data->vcd;

static char* Bin(unsigned int value){
   static char buffer[33];
   buffer[32] = '\0';

   unsigned int val = value;
   for(int i = 31; i >= 0; i--){
      buffer[i] = '0' + (val & 0x1);
      val >>= 1;
   }

   return buffer;
}

// static Array<int> zerosArray defined in versat.cpp

template<typename T>
static int32_t MemoryAccessNoAddress(FUInstance* inst,int address,int value,int write){
   T* self = (T*) inst->extraData;

   if(write){
      self->valid = 1;
      self->wstrb = 0xf;
      self->wdata = value;

      self->eval();

      while(!self->ready){
         inst->declaration->updateFunction(inst,zerosArray);
      }

      self->valid = 0;
      self->wstrb = 0x00;
      self->wdata = 0x00000000;

      inst->declaration->updateFunction(inst,zerosArray);

      return 0;
   } else {
      self->valid = 1;
      self->wstrb = 0x0;

      self->eval();

      while(!self->ready){
         inst->declaration->updateFunction(inst,zerosArray);
      }

      int32_t res = self->rdata;

      self->valid = 0;

      self->eval();
      while(self->ready){
         inst->declaration->updateFunction(inst,zerosArray);
      }

      return res;
   }
}

template<typename T>
static int32_t MemoryAccess(FUInstance* inst,int address,int value,int write){
   T* self = (T*) inst->extraData;

   if(write){
      self->valid = 1;
      self->wstrb = 0xf;

      self->addr = address;
      self->wdata = value;

      self->eval();

      while(!self->ready){
         inst->declaration->updateFunction(inst,zerosArray);
      }

      self->valid = 0;
      self->wstrb = 0x00;
      self->addr = 0x00000000;
      self->wdata = 0x00000000;

      inst->declaration->updateFunction(inst,zerosArray);

      return 0;
   } else {
      self->valid = 1;
      self->wstrb = 0x0;
      self->addr = address;

      self->eval();

      while(!self->ready){
         inst->declaration->updateFunction(inst,zerosArray);
      }

      int32_t res = self->rdata;

      self->valid = 0;
      self->addr = 0;

      self->eval();
      while(self->ready){
         inst->declaration->updateFunction(inst,zerosArray);
      }

      return res;
   }
}

struct DatabusAccess{
   int counter;
   int latencyCounter;
};

static const int INITIAL_MEMORY_LATENCY = 5;
static const int MEMORY_LATENCY = 0;

// Memories are evaluated after databus because most of the time
// databus is used to read data to memories
// while memory to databus usually passes through a register that holds the data.
// and therefore order of evaluation usually favours databus first and memories after

#{for module modules}

void @{module.name}_VCDFunction(FUInstance* inst,FILE* out,VCDMapping& currentMapping,Array<int>,bool firstTime,bool printDefinitions){
   if(printDefinitions){
   #{for external module.externalInterfaces}
      #{set id external.interface}
      #{if external.type}
      #{for port 2}
      // DP
      fprintf(out,"$var wire @{external.bitsize} %s ext_dp_addr_@{id}_port_@{port} $end\n",currentMapping.Get());
      currentMapping.Increment();
      fprintf(out,"$var wire @{external.datasize} %s ext_dp_out_@{id}_port_@{port} $end\n",currentMapping.Get());
      currentMapping.Increment();
      fprintf(out,"$var wire @{external.datasize} %s ext_dp_in_@{id}_port_@{port} $end\n",currentMapping.Get());
      currentMapping.Increment();
      fprintf(out,"$var wire 1 %s ext_dp_enable_@{id}_port_@{port} $end\n",currentMapping.Get());
      currentMapping.Increment();
      fprintf(out,"$var wire 1 %s ext_dp_write_@{id}_port_@{port} $end\n",currentMapping.Get());
      currentMapping.Increment();
      #{end}
      #{else}
      // 2P
      {
      }
      #{end}
   #{end}
   } else {
   #{for external module.externalInterfaces}
      #{set id external.interface}
      #{if external.type}
      #{for port 2}
      // DP
      {
      V@{module.name}* self = (V@{module.name}*) inst->extraData;

      fprintf(out,"b%s %s\n",Bin(self->ext_dp_addr_@{id}_port_@{port}),currentMapping.Get());
      currentMapping.Increment();
      fprintf(out,"b%s %s\n",Bin(self->ext_dp_out_@{id}_port_@{port}),currentMapping.Get());
      currentMapping.Increment();
      fprintf(out,"b%s %s\n",Bin(self->ext_dp_in_@{id}_port_@{port}),currentMapping.Get());
      currentMapping.Increment();
      fprintf(out,"%d%s\n",self->ext_dp_enable_@{id}_port_@{port} ? 1 : 0,currentMapping.Get());
      currentMapping.Increment();
      fprintf(out,"%d%s\n",self->ext_dp_write_@{id}_port_@{port} ? 1 : 0,currentMapping.Get());
      currentMapping.Increment();
      }
      #{end}
      #{else}
      // 2P
      {
      }
      #{end}
   #{end}
   }
}

static void CloseWaveform(){
   if(tfp){
      tfp->close();
   }
}

int32_t* @{module.name}_InitializeFunction(FUInstance* inst){
   tfp = new VerilatedVcdC;

   memset(inst->extraData,0,inst->declaration->extraDataOffsets.max);

   V@{module.name}* self = new (inst->extraData) V@{module.name}();

   if(dut){
      printf("Initialize function is being called multiple times\n");
      exit(-1);
   }

   dut = self;
   self->trace(tfp, 99);
   tfp->open("system.vcd");

   atexit(CloseWaveform);

   INIT(self);

#{for i module.inputDelays.size}
   self->in@{i} = 0;
#{end}

   RESET(self);

   return NULL;
}

int32_t* @{module.name}_StartFunction(FUInstance* inst){
#{if module.outputLatencies}
   static int32_t out[@{module.outputLatencies.size}];
#{end}

   V@{module.name}* self = (V@{module.name}*) inst->extraData;

#{if module.configs}
AcceleratorConfig* config = (AcceleratorConfig*) inst->config;

#{for i module.configs.size}
#{set wire module.configs[i]}
   self->@{wire.name} = config->@{configsHeader[i].name};
#{end}
#{end}

#{if module.nDelays}
#{for i module.nDelays}
   self->delay@{i} = config->TOP_Delay@{i};
#{end}
#{end}

#{for i module.nIO}
{
   DatabusAccess* memoryLatency = ((DatabusAccess*) &self[1]) + @{i};

   memoryLatency->counter = 0;
   memoryLatency->latencyCounter = INITIAL_MEMORY_LATENCY;
}\
#{end}

   START_RUN(self);

#{if module.hasDone}
   inst->done = self->done;
#{end}

#{if module.outputLatencies}
   #{for i module.outputLatencies.size}
   out[@{i}] = self->out@{i};
   #{end}

   return out;
#{else}
   return NULL;
#{end}
}

int32_t* @{module.name}_UpdateFunction(FUInstance* inst,Array<int> inputs){
#{if module.outputLatencies}
   static int32_t out[@{module.outputLatencies.size}];
#{end}

   V@{module.name}* self = (V@{module.name}*) inst->extraData;

#{for i module.inputDelays.size}
   self->in@{i} = inputs[@{i}]; //GetInputValue(node,@{i}); 
#{end}

   self->eval();

#{for i module.nIO}
{
   self->databus_ready_@{i} = 0;
   self->databus_last_@{i} = 0;

   DatabusAccess* access = ((DatabusAccess*) &self[1]) + @{i};

   if(self->databus_valid_@{i}){
      if(access->latencyCounter > 0){
         access->latencyCounter -= 1;
      } else {
         int* ptr = (int*) (self->databus_addr_@{i});
         
         if(self->databus_wstrb_@{i} == 0){
            if(ptr == nullptr){
               self->databus_rdata_@{i} = 0xfeeffeef; // Feed bad data if not set (in pc-emul is needed otherwise segfault)
            } else {
               self->databus_rdata_@{i} = ptr[access->counter];
            }            
         } else { // self->databus_wstrb_@{i} != 0
            if(ptr != nullptr){
               ptr[access->counter] = self->databus_wdata_@{i};
            }
         }
         self->databus_ready_@{i} = 1;
         
         if(access->counter == self->databus_len_@{i}){
            access->counter = 0;
            self->databus_last_@{i} = 1;
         } else {
            access->counter += 1;            
         }

         access->latencyCounter = MEMORY_LATENCY;
      }
   }

   self->eval();
}
#{end}

#{for external module.externalInterfaces}
   #{set id external.interface}
   #{if external.type}
   // DP
   #{for port 2}
   int saved_dp_enable_@{id}_port_@{port} = self->ext_dp_enable_@{id}_port_@{port};
   int saved_dp_write_@{id}_port_@{port} = self->ext_dp_write_@{id}_port_@{port};
   int saved_dp_addr_@{id}_port_@{port} = self->ext_dp_addr_@{id}_port_@{port};
   int saved_dp_data_@{id}_port_@{port} = self->ext_dp_out_@{id}_port_@{port};

   #{end}
   #{else}
   // 2P
   int saved_2p_r_enable_@{id} = self->ext_2p_read_@{id};
   int saved_2p_r_addr_@{id} = self->ext_2p_addr_in_@{id}; // Instead of saving address, should access memory and save data. Would simulate better what is actually happening
   int saved_2p_w_enable_@{id} = self->ext_2p_write_@{id};
   int saved_2p_w_addr_@{id} = self->ext_2p_addr_out_@{id};
   int saved_2p_w_data_@{id} = self->ext_2p_data_out_@{id};

   #{end}
#{end}

   UPDATE(self); // This line causes posedge clk events to activate

//TODO: There might be a bug with the fact that we do not align the base address.
//      I do not remember how external memory is allocated and destributed, check later and confirm this part
//      At a minimum, we should align to make sure that 
{
   int baseAddress = 0;
#{for external module.externalInterfaces}
   #{set id external.interface}
   #{if external.type}
   // DP
   #{for port 2}
      if(saved_dp_enable_@{id}_port_@{port} && !saved_dp_write_@{id}_port_@{port}){
         int readOffset = saved_dp_addr_@{id}_port_@{port};
         Assert(readOffset < (1 << inst->declaration->externalMemory[@{id}].bitsize));
         self->ext_dp_in_@{id}_port_@{port} = inst->externalMemory[baseAddress + readOffset];
      }
   #{end}
   #{else}
   // 2P
      if(saved_2p_r_enable_@{id}){
         int readOffset = saved_2p_r_addr_@{id};
         Assert(readOffset < (1 << inst->declaration->externalMemory[@{id}].bitsize));
         self->ext_2p_data_in_@{id} = inst->externalMemory[baseAddress + readOffset];
      }
   #{end}
   baseAddress += (1 << inst->declaration->externalMemory[@{id}].bitsize);
#{end}
}  

{
   int baseAddress = 0;
#{for external module.externalInterfaces}
   #{set id external.interface}
   #{if external.type}
   // DP
   #{for port 2}
      if(saved_dp_enable_@{id}_port_@{port} && saved_dp_write_@{id}_port_@{port}){
         int writeOffset = saved_dp_addr_@{id}_port_@{port};
         Assert(writeOffset < (1 << inst->declaration->externalMemory[@{id}].bitsize));
         inst->externalMemory[baseAddress + writeOffset] = saved_dp_data_@{id}_port_@{port};
      }
   #{end}
   #{else}
   // 2P
      if(saved_2p_w_enable_@{id}){
         int writeOffset = saved_2p_w_addr_@{id};
         Assert(writeOffset < (1 << inst->declaration->externalMemory[@{id}].bitsize));
         inst->externalMemory[baseAddress + writeOffset] = saved_2p_w_data_@{id};
      }
   #{end}
   baseAddress += (1 << inst->declaration->externalMemory[@{id}].bitsize);
#{end}
}

#{if module.states}
AcceleratorState* state = (AcceleratorState*) inst->state;
#{for i module.states.size}
#{set wire module.states[i]}
   state->@{statesHeader[i]} = self->@{wire.name};
#{end}
#{end}

#{if module.hasDone}
   inst->done = self->done;
#{end}

   self->eval();

#{if module.outputLatencies}
   #{for i module.outputLatencies.size}
      out[@{i}] = self->out@{i};
   #{end}

   return out;
#{else}
   return NULL;
#{end}
}

int32_t* @{module.name}_DestroyFunction(FUInstance* inst){
   V@{module.name}* self = (V@{module.name}*) inst->extraData;

   self->~V@{module.name}();

   return nullptr;
}

int @{module.name}_ExtraDataSize(){
   int extraSize = sizeof(V@{module.name});

   #{if module.nIO}
   extraSize += sizeof(DatabusAccess) * @{module.nIO};
   #{end}

   return extraSize;
}

#{if module.signalLoop}
void SignalFunction@{module.name}(FUInstance* inst){
   V@{module.name}* self = (V@{module.name}*) inst->extraData;

   self->signal_loop = 1;

   UPDATE(self);

   self->signal_loop = 0;
   self->eval();
}
#{end}

FUDeclaration @{module.name}_CreateDeclaration(){
   FUDeclaration decl = {};

   #{if module.inputDelays}
   static int inputDelays[] =  {#{join "," for delay module.inputDelays}@{delay}#{end}};
   decl.inputDelays = Array<int>{inputDelays,@{module.inputDelays.size}};
   #{end}

   #{if module.outputLatencies}
   static int outputLatencies[] = {#{join "," for latency module.outputLatencies}@{latency}#{end}};
   decl.outputLatencies = Array<int>{outputLatencies,@{module.outputLatencies.size}};
   #{end}

   decl.name = STRING("@{module.name}");

   decl.extraDataOffsets.max = @{module.name}_ExtraDataSize();

   #{if module.externalInterfaces.size}
   static ExternalMemoryInterface externalMemory[@{module.externalInterfaces.size}];

   #{for external module.externalInterfaces}
   externalMemory[@{index}].interface = @{external.interface};
   externalMemory[@{index}].bitsize = @{external.bitsize};
   externalMemory[@{index}].datasize =  @{external.datasize};
   externalMemory[@{index}].type = @{external.type};
   #{end}
   decl.externalMemory = C_ARRAY_TO_ARRAY(externalMemory);
   #{end}

   decl.initializeFunction = @{module.name}_InitializeFunction;
   decl.startFunction = @{module.name}_StartFunction;
   decl.updateFunction = @{module.name}_UpdateFunction;
   decl.destroyFunction = @{module.name}_DestroyFunction;
   decl.printVCD = @{module.name}_VCDFunction;

   #{if module.configs}
   static Wire @{module.name}ConfigWires[] = {#{join "," for wire module.configs} #{if !wire.isStatic} {STRING("@{wire.name}"),@{wire.bitsize},@{wire.isStatic}} #{end} #{end}};
   decl.configs = Array<Wire>{@{module.name}ConfigWires,@{module.configs.size}};
   #{end}

   #{if module.states}
   static Wire @{module.name}StateWires[] = {#{join "," for wire module.states} {STRING("@{wire.name}"),@{wire.bitsize}} #{end}};
   decl.states = Array<Wire>{@{module.name}StateWires,@{module.states.size}};
   #{end}

   #{if module.hasDone}
   decl.implementsDone = true;
   #{end}

   #{if module.isSource}
   decl.delayType = decl.delayType | DelayType::DELAY_TYPE_SINK_DELAY;
   #{end}

   #{if module.nIO}
   decl.nIOs = @{module.nIO};
   #{end}

   #{if module.signalLoop}
   decl.signalFunction = SignalFunction@{module.name};
   #{end}

   #{if module.memoryMapped}
   decl.isMemoryMapped = true;
   decl.memoryMapBits = @{module.memoryMappedBits};
   #{if module.memoryMappedBits == 0}
   decl.memAccessFunction = MemoryAccessNoAddress<V@{module.name}>;
   #{else}
   decl.memAccessFunction = MemoryAccess<V@{module.name}>;
   #{end}
   #{end}

   decl.delayOffsets.max = @{module.nDelays};

   return decl;
}

FUDeclaration* @{module.name}_Register(Versat* versat){
   FUDeclaration decl = @{module.name}_CreateDeclaration();

   return RegisterFU(versat,decl);
}

#{end}

extern "C" void RegisterAllVerilogUnits@{namespace}(Versat* versat){
   #{for module modules}
   @{module.name}_Register(versat);
   #{end}
}

extern "C" void InitializeVerilator(){
   Verilated::traceEverOn(true);
}

#undef INIT
#undef UPDATE
#undef RESET
#undef START_RUN
#undef PREAMBLE
