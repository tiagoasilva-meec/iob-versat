#ifndef VERSAT_SIM
#define VERSAT_SIM
#include <iostream>
#include <thread>
#include "versat.h"
#include <string.h>
#include <bitset>

#ifndef DATAPATH_W
#define DATAPATH_W 16
#endif

#if DATAPATH_W == 16
typedef int16_t versat_t;
typedef int32_t mul_t;
typedef uint32_t shift_t;
#elif DATAPATH_W == 8
typedef int8_t versat_t;
typedef int16_t mul_t;
typedef uint16_t shift_t;

#else
typedef int32_t versat_t;
typedef int64_t mul_t;
typedef uint64_t shift_t;

#endif
using namespace std;

#define SET_BITS(var, val, size) \
  for (int i = 0; i < size; i++) \
  {                              \
    var.set(i, val);             \
  }

#define SET_BITS_SEXT8(var, val, size) \
  for (int i = size - 1; i > 7; i--)   \
  {                                    \
    var.set(i, val);                   \
  }

#define SET_BITS_SEXT16(var, val, size) \
  for (int i = size - 1; i > 15; i--)   \
  {                                     \
    var.set(i, val);                    \
  }

#define MSB(var, size) var >> (size - 1)

//Macro functions to use cpu interface
#define MEMSET(base, location, value) (*((volatile int *)(base + (sizeof(int)) * location)) = value)
#define MEMGET(base, location) (*((volatile int *)(base + (sizeof(int)) * location)))

//constants
#define CONF_BASE (1 << (nMEM_W + MEM_ADDR_W + 1))
#define CONF_MEM_SIZE ((int)pow(2, CONF_MEM_ADDR_W))
//#define MEM_SIZE ((int)pow(2,MEM_ADDR_W))
#define MEM_SIZE (1 << MEM_ADDR_W)
#define RUN_DONE (1 << (nMEM_W + MEM_ADDR_W))

//
// VERSAT CLASSES
//

class CMem
{
private:
  versat_t mem[MEM_SIZE];
  versat_t read(uint32_t addr);
  void write(int32_t addr, versat_t data_in);

public:
  friend class CMemPort;
};

class CMemPort // 4 Loop AGU
{
private:
  //count delay during a run():
  int run_delay = 0;
  versat_t out;
  versat_t output_port[MEMP_LAT]; //output FIFO
  int i, j, k, l;

public:
  CMem *my_mem;
  int versat_base, mem_base, data_base;
  int iter, per, duty, sel, start, shift, incr, delay, in_wr;
  int rvrs = 0, ext = 0, iter2 = 0, per2 = 0, shift2 = 0, incr2 = 0;

  //Default constructor
  CMemPort();
  //Constructor with an associated base
  CMemPort(int versat_base, int i, int offset);
  //set MEMPort configuration to shadow register
  void update_shadow_reg_MEM();
  //start run
  void start_run();
  //update output buffer, write results to databus
  void update();

  versat_t output();
  int AGU();
  void writeConf();
  void setIter(int iter);
  void setPer(int per);
  void setDuty(int duty);
  void setSel(int sel);
  void setStart(int start);
  void setIncr(int incr);
  void setShift(int shift);
  void setDelay(int delay);
  void setExt(int ext);
  void setRvrs(int rvrs);
  void setInWr(int in_wr);
  void setIter2(int iter2);
  void setPer2(int per2);
  void setIncr2(int incr);
  void setShift2(int shift2);

  void write(int addr, int val);
  int read(int addr);
}; //end class CMEM

#if nALU > 0
class CALU
{
private:
  versat_t ina, inb, out;
  versat_t output_buff[ALU_LAT]; //output FIFO
public:
  int versat_base, alu_base;
  int opa, opb, fns;

  //Default constructor
  CALU();
  CALU(int versat_base, int i);

  //set ALU configuration to shadow register
  void update_shadow_reg_ALU();

  //start run
  void start_run();

  //update output buffer, write results to databus
  void update();
  versat_t output();

  void writeConf();
  void setOpA(int opa);
  void setOpB(int opb);
  void setFNS(int fns);
}; //end class CALU
#endif

#if nALULITE > 0
class CALULite
{
private:
  versat_t ina, inb, out;
  versat_t output_buff[ALULITE_LAT]; //output FIFO
public:
  int versat_base, alulite_base;
  int opa, opb, fns;

  //Default constructor
  CALULite();
  CALULite(int versat_base, int i);

  //set ALULite configuration to shadow register
  void update_shadow_reg_ALULite();
  //start run
  void start_run();

  //update output buffer, write results to databus
  void update();
  versat_t output();

  void writeConf();
  void setOpA(int opa);
  void setOpB(int opb);
  void setFNS(int fns);
}; //end class CALUALITE
#endif

#if nBS > 0
class CBS
{
private:
  versat_t in, out;
  versat_t output_buff[BS_LAT]; //output FIFO
public:
  int versat_base, bs_base;
  int data, shift, fns;

  //Default constructor
  CBS();
  CBS(int versat_base, int i);

  //set BS configuration to shadow register
  void update_shadow_reg_BS();

  //start run
  void start_run();

  //update output buffer, write results to databus
  void update();

  versat_t output();

  void writeConf();
  void setData(int data);
  void setShift(int shift);
  void setFNS(int fns);
}; //end class CBS
#endif

#if nMUL > 0
class CMul
{
private:
  versat_t opa, opb;
  versat_t output_buff[MUL_LAT]; //output FIFO
  versat_t out;

public:
  int versat_base, mul_base;
  int sela, selb, fns;
  //Default constructor
  CMul();

  CMul(int versat_base, int i);
  //set Mul configuration to shadow register
  void update_shadow_reg_Mul();

  //start run
  void start_run();

  //update output buffer, write results to databus
  void update();

  versat_t output();

  void writeConf();
  void setSelA(int sela);
  void setSelB(int selb);
  void setFNS(int fns);
}; //end class CMUL
#endif

#if nMULADD > 0
class CMulAdd
{
private:
  //SIM VARIABLES
  versat_t opa, opb, out;
  mul_t acc, acc_w;
  int cnt_iter, cnt_per, cnt_addr;
  //count delay during a run():
  int run_delay = 0;
  versat_t output_buff[MULADD_LAT]; //output FIFO
public:
  int versat_base, muladd_base;
  int sela, selb, fns, iter, per, delay, shift;

  //Default constructor
  CMulAdd();

  CMulAdd(int versat_base, int i);

  //set MulAdd configuration to shadow register
  void update_shadow_reg_MulAdd();

  //start run
  void start_run();

  //update output buffer, write results to databus
  void update();
  versat_t output(); //implemented as PIPELINED MULADD

  void writeConf();
  void setSelA(int sela);
  void setSelB(int selb);
  void setFNS(int fns);
  void setIter(int iter);
  void setPer(int per);
  void setDelay(int delay);
  void setShift(int shift);
}; //end class CMULADD
#endif

#if nMULADDLITE > 0
class CMulAddLite
{
private:
  //count delay during a run():
  int run_delay = 0;
  versat_t output_buff[MULADDLITE_LAT]; //output FIFO
public:
  int versat_base, muladdlite_base;
  int sela, selb, iter, per, delay, shift;
  int selc = 0, accIN = 0, accOUT = 0, batch = 0;

  //Default constructor
  CMulAddLite();

  CMulAddLite(int versat_base, int i);

  //set MulAdd configuration to shadow register
  void update_shadow_reg_MulAddLite();

  //start run
  void start_run();

  //update output buffer, write results to databus
  void update();
  versat_t output(); //TO DO: need to implemente MulAddLite functionalities

  void writeConf();
  void setSelA(int sela);
  void setSelB(int selb);
  void setSelC(int selc);
  void setIter(int iter);
  void setPer(int per);
  void setDelay(int delay);
  void setShift(int shift);
  void setAccIN(int accIN);
  void setAccOUT(int accOUT);
  void setBatch(int batch);
}; //end class CMULADDLITE
#endif

class CStage
{
private:
public:
  int versat_base;
  versat_t *databus;
  //versat_t* databus[N*2];
  //Versat Function Units
  CMemPort memA[nMEM];
  CMemPort memB[nMEM];
  CMem mem[nMEM];
#if nALU > 0
  CALU alu[nALU];
#endif
#if nALULITE > 0
  CALULite alulite[nALULITE];
#endif
#if nBS > 0
  CBS bs[nBS];
#endif
#if nMUL > 0
  CMul mul[nMUL];
#endif
#if nMULADD > 0
  CMulAdd muladd[nMULADD];
#endif
#if nMULADDLITE > 0
  CMulAddLite muladdlite[nMULADDLITE];
#endif

  //Default constructor
  CStage();
  //Default Constructor
  CStage(int versat_base);
  //clear Versat config
  void clearConf();

#ifdef CONF_MEM_USE
  //write current config in conf_mem
  void confMemWrite(int addr);

  //set addressed config in conf_mem as current config
  void confMemRead(int addr);
#endif

  //update shadow register with current configuration
  void update_shadow_reg();
  //set run start on all FUs
  void start_all_FUs();

  //set update output buffers on all FUs
  void update_all_FUs();

  //calculate new output on all FUs
  void output_all_FUs();

}; //end class CStage

//
//VERSAT global variables
//
#ifndef VERSAT_cpp // include guard
#define VERSAT_cpp
static int base;
CStage stage[nSTAGE];
CStage conf[nSTAGE];
CStage shadow_reg[nSTAGE];

/*databus vector
stage 0 is repeated in the start and at the end
stage order in databus
[ 0 | nSTAGE-1 | nSTAGE-2 | ... | 2  | 1 | 0 ]
^                                    ^
|                                    |
stage 0 databus                      stage 1 databus

*/
versat_t global_databus[(nSTAGE + 1) * N];

int sMEMA[nMEM], sMEMA_p[nMEM], sMEMB[nMEM], sMEMB_p[nMEM];
#if nALU > 0
int sALU[nALU], sALU_p[nALU];
#endif
#if nALULITE > 0
int sALULITE[nALULITE], sALULITE_p[nALULITE];
#endif
#if nMUL > 0
int sMUL[nMUL], sMUL_p[nMUL];
#endif
#if nMULADD > 0
int sMULADD[nMULADD], sMULADD_p[nMULADD];
#endif
#if nMULADDLITE > 0
int sMULADDLITE[nMULADDLITE], sMULADDLITE_p[nMULADDLITE];
#endif
#if nBS > 0
int sBS[nBS], sBS_p[nBS];
#endif

//
//VERSAT FUNCTIONS
//
inline void versat_init(int base_addr);

int run_done = 0;

void run_sim();

inline void run();

inline int done();

inline void globalClearConf();
#endif

#endif
