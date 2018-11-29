DEFAULT_MAKE_DIR=/opt/pzsdk.ver4.0/make
PZC_TARGET_ARCH=sc2
TARGET=solver.pz
PZCSRC = ../pzc/solver.pzc ../pzc/board.pzc ../pzc/table.pzc
CLANG_OPT=-O3 -std=c++14
PZC_INC_DIR+=-I ../../pzatomic/inc
PZRUNTIME+=../../pzatomic/pzo/pzatomic-sc2.pzsdk-4.0.pzo
vpath %.pzc ../pzc
include $(DEFAULT_MAKE_DIR)/default_pzcl_kernel.mk
