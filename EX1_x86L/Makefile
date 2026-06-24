CC	= gcc

CFLAGS += -g -m32 -x c++
CFLAGS += -I../Port
CFLAGS += -I../SOURCE
CFLAGS += -I./gcc/SOURCE

SRCS = ../Port/os_cpu_c.c ../Port/utils.c ../SOURCE/ucos_ii.c

APP  = ./gcc/SOURCE/test.c

EXEC = ./bin.exec

all: $(EXEC)

$(EXEC):$(SRCS) $(APP)  #complie appcation
	$(CC) $(CFLAGS) $(SRCS) $(APP) -o $(EXEC)
	@echo "-- Build Target Appcation ($(EXEC)) Successful --"

clean:
	@echo ">>>>>>>>>>>> make clean <<<<<<<<<<<<<<"
	@rm -rf $(EXEC)
