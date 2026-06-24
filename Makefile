CC	= gcc

CFLAGS += -g -m32 -x c++
CFLAGS += -I./Port 
CFLAGS += -I./SOURCE

SRCS = ./Port/os_cpu_c.c ./Port/utils.c ./SOURCE/ucos_ii.c 

EXEC_1 = ./EX1_x86L/bin.exec
EXEC_2 = ./EX2_x86L/bin.exec
EXEC_3 = ./EX3_x86L/bin.exec
EXEC_5 = ./EX5_x86L/bin.exec

all: $(EXEC_1) $(EXEC_2) $(EXEC_3) $(EXEC_5) 

$(EXEC_1):  #delegate to per-example gcc Makefile
	$(MAKE) -C EX1_x86L

$(EXEC_2):  #delegate to per-example gcc Makefile
	$(MAKE) -C EX2_x86L

$(EXEC_3):  #delegate to per-example gcc Makefile
	$(MAKE) -C EX3_x86L

$(EXEC_5):  #delegate to per-example gcc Makefile
	$(MAKE) -C EX5_x86L

clean:
	@echo ">>>>>>>>>>>> make clean <<<<<<<<<<<<<<"

	@rm -rf $(EXEC_1) $(EXEC_2) $(EXEC_3) $(EXEC_5) 
