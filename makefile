###############################################################################
#
# Name: Makefile
#
# Description: Makefile to build ucos linux port sample code.
#
# Author: Philip Mitchell
#
###############################################################################

#objects := $(patsubst %.c,%.o,$(wildcard *.c))
#CFLAGS += -I./ -I/usr/include
#sample:$(objects)
#	cc -o sample $(objects)

DEBUGGING = -ggdb
CC=gcc
#CFLAGS += -I./ $(DEBUGGING) -m32
CFLAGS += -I./ -I/usr/include $(DEBUGGING) 
objects := os_cpu_c.o ucos_ii.o sample.o
sample:$(objects)
	cc -o sample $(objects)  -lpthread

.PHONY: clean
clean:
	rm -rf *.o sample

