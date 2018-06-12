DEFAULT_MAKE=/opt/pzsdk.ver4.0/make/default_pzcl_host.mk
TARGET=solve
PZCL_KERNEL_DIRS = kernel.sc2
CPPSRC = main.cpp to_board.cpp
CCOPT = -O3 -std=c++11 -march=native -fopenmp -Wall -Wextra
LDOPT = -fopenmp
#CPPSRC += ../common/pzclutil.cpp
include $(DEFAULT_MAKE)
