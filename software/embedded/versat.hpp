#ifndef INCLUDED_VERSAT
#define INCLUDED_VERSAT

#include <stdint.h>
#include <stddef.h>

#include "versatCommon.hpp"
#include "utils.hpp"

struct Versat{
};

struct FUDeclaration{
};

struct Accelerator{
   Versat* versat;
   bool locked;
};

Versat* InitVersat(int base,int numberConfigurations);

// Accelerator functions
void AcceleratorRun(Accelerator* accel);
void VersatUnitWrite(FUInstance* instance,int address, int value);
int32_t VersatUnitRead(FUInstance* instance,int address);

#define GetInstanceByName(ACCEL,...) GetInstanceByName_(ACCEL,NUMBER_ARGS(__VA_ARGS__),__VA_ARGS__)
FUInstance* GetInstanceByName_(Accelerator* accel,int argc, ...);
FUInstance* GetInstanceByName_(FUInstance* inst,int argc, ...);

void CalculateDelay(Versat* versat,Accelerator* accel); // In versat space, simple extract delays from configuration data
FUInstance* CreateFUInstance(Accelerator* accel,FUDeclaration* type,SizedString entityName);

void Hook(Versat* versat,Accelerator* accel,FUInstance* inst);
// Functions that perform no useful work are simple pre processed out
#define SetDebug(...) (0)
#define ClearConfigurations(...) ((void)0)
#define ActivateMergedAccelerator(...) ((void)0)
#define CreateAccelerator(...) ((Accelerator*)0)
#define RegisterFU(...) ((FUDeclaration*)0)
#define DisplayAcceleratorMemory(...) ((void)0)
#define OutputVersatSource(...) ((void)0)
#define ConnectUnits(...) ((void)0)
#define OutputGraphDotFile(...) ((void)0)
#define RemoveFUInstance(...) ((void)0)
#define SetDelayRecursive(...) ((void)0)
#define Flatten(...) ((Accelerator*)0)
#define GetTypeByName(...) ((FUDeclaration*)0)
#define ParseCommandLineOptions(...) ((void)0)
#define OutputMemoryMap(...) ((void)0)
#define OutputUnitInfo(...) ((void)0)
#define RegisterTypes(...) ((void)0)
#define ParseVersatSpecification(...) ((void)0)
#define MergeAccelerators(...) ((FUDeclaration*)0)
#define Free(...) ((void)0)
#define DisplayUnitConfiguration(...) ((void)0)

#endif // INCLUDED_VERSAT
