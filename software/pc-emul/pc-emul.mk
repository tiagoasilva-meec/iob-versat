include $(VERSAT_DIR)/software/software.mk
include $(VERSAT_DIR)/sharedHardware.mk

SRC+=$(VERSAT_PC_DIR)/versat.c

VERSAT_INCLUDE:= -I $(VERSAT_COMPILER_DIR) -I $(VERSAT_COMMON_DIR) -I $(VERSAT_SW_DIR)

INCLUDE+=$(VERSAT_INCLUDE)

