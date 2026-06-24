###############################################################################
#                                  uC/OS-II
#                            The Real-Time Kernel
#                         
#              (c) Copyright 2002, Jean J. Labrosse, Weston, FL
#                             All Rights Reserved
#
#
# Filename    : TEST.MAK
###############################################################################
#
#/*$PAGE*/
###############################################################################
#                                   TOOLS
###############################################################################
#

BORLAND=C:\BC45

CC=$(BORLAND)\BIN\BCC
ASM=$(BORLAND)\BIN\TASM
LINK=$(BORLAND)\BIN\TLINK
TOUCH=$(BORLAND)\BIN\TOUCH

###############################################################################
#                                DIRECTORIES
###############################################################################
#

LST=..\LST
OBJ=..\OBJ
SOURCE=..\SOURCE
TARGET=..\TEST
WORK=..\WORK

OS=\SOFTWARE\uCOS-II\SOURCE
PC=\SOFTWARE\BLOCKS\PC\BC45
PORT=\SOFTWARE\uCOS-II\Ix86L\BC45

###############################################################################
#                               COMPILER FLAGS
#
# -1                 Generate 80186 code
# -B                 Compile and call assembler
# -c                 Compiler to .OBJ
# -f                 Use the Borland Floating-Point Emulation Library (default)
# -G                 Select code for speed
# -I                 Path to include   directory
# -k-                Standard stack frame
# -L                 Path to libraries directory
# -ml                Large memory model
# -N-                Do not check for stack overflow
# -n                 Path to object directory
# -O                 Optimize jumps
# -Ob                Dead code elimination
# -Oe                Global register allocation
# -Og                Optimize globally
# -Ol                Loop optimization
# -Om                Invariant code motion
# -Op                Copy propagation
# -Ov                Induction variable
# -v                 Source debugging ON
# -vi                Turn inline expansion ON
# -wpro              Error reporting: call to functions with no prototype
# -Z                 Suppress redundant loads
###############################################################################
#

C_FLAGS=-c -f -ml -1 -G -O -Ogemvlbpi -Z -d -n..\obj -k- -v -vi- -wpro -I$(BORLAND)\INCLUDE -L$(BORLAND)\LIB

###############################################################################
#                               ASSEMBLER FLAGS
#
# /MX                Case sensitive on globals
# /ZI                Full debug info
# /O                 Generate overlay code
###############################################################################
#

ASM_FLAGS=/MX /ZI /O 

###############################################################################
#                                LINKER FLAGS
###############################################################################
#
LINK_FLAGS= 

###############################################################################
#                               MISCELLANEOUS
###############################################################################
#
INCLUDES=      $(SOURCE)\includes.h  \
               $(SOURCE)\os_cfg.h    \
               $(PORT)\os_cpu.h      \
               $(PC)\PC.H            \
               $(OS)\ucos_ii.h               

###############################################################################
#                          CREATION OF .EXE FILE
###############################################################################

$(TARGET)\TEST.EXE:                  \
               $(WORK)\includes.h    \
               $(OBJ)\OS_CPU_A.OBJ   \
               $(OBJ)\OS_CPU_C.OBJ   \
               $(OBJ)\PC.OBJ         \
               $(OBJ)\TEST.OBJ       \
               $(OBJ)\uCOS_II.OBJ    \
               $(SOURCE)\TEST.LNK
               COPY    $(SOURCE)\TEST.LNK
               $(LINK) $(LINK_FLAGS)     @TEST.LNK
               COPY    $(OBJ)\TEST.EXE   $(TARGET)\TEST.EXE
               COPY    $(OBJ)\TEST.MAP   $(TARGET)\TEST.MAP
               DEL     TEST.MAK


###############################################################################
#                        CREATION OF .OBJ (Object) FILES
###############################################################################

$(OBJ)\OS_CPU_A.OBJ:                  \
               $(PORT)\OS_CPU_A.ASM   

               COPY   $(PORT)\OS_CPU_A.ASM  OS_CPU_A.ASM
	           $(ASM) $(ASM_FLAGS)  $(PORT)\OS_CPU_A.ASM,$(OBJ)\OS_CPU_A.OBJ


$(OBJ)\OS_CPU_C.OBJ:                  \
               $(PORT)\os_cpu_c.c     \

               COPY   $(PORT)\os_cpu_c.c    os_cpu_c.c
               $(CC)  $(C_FLAGS)            os_cpu_c.c


$(OBJ)\PC.OBJ:                        \
               $(PC)\PC.C             \
               $(INCLUDES)

               COPY   $(PC)\PC.C            PC.C
               $(CC)  $(C_FLAGS)            PC.C


$(OBJ)\TEST.OBJ:                      \
               $(SOURCE)\test.c       \
               $(INCLUDES)

               COPY   $(SOURCE)\test.c      test.c
               $(CC)  $(C_FLAGS)            test.c


$(OBJ)\uCOS_II.OBJ:                   \
               $(OS)\ucos_ii.c        \
               $(INCLUDES)

               COPY   $(OS)\ucos_ii.c       ucos_ii.c
               $(CC)  $(C_FLAGS)            ucos_ii.c


$(WORK)\includes.h:                   \
               $(INCLUDES)

               COPY   $(SOURCE)\includes.h  includes.h
               COPY   $(SOURCE)\os_cfg.h    os_cfg.h
               COPY   $(PC)\PC.H            PC.H
               COPY   $(PORT)\os_cpu.h      os_cpu.h
               COPY   $(OS)\ucos_ii.h       ucos_ii.h
