DEFAULT_MAKE=/opt/pzsdk.ver4.0/make/default_pzcl_host.mk
TARGET=solve
PZCL_KERNEL_DIRS = kernel.sc1
PZCL_KERNEL_DIRS += kernel.sc1-64
CPPSRC = main.cpp to_board.cpp
CCOPT = -O3 -std=c++11 -march=native
LDOPT = -fopenmp
#CPPSRC += ../common/pzclutil.cpp
include $(DEFAULT_MAKE)
