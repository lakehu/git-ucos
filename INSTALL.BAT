ECHO OFF
CLS
ECHO ***************************************************************************
ECHO *                          uC/OS-II, The Real-Time Kernel
ECHO *
ECHO *                                  INSTALLATION
ECHO *
ECHO *
ECHO * Usage  :  A:INSTALL [drive]
ECHO *
ECHO *           where: 'drive' is an optional destination drive letter
ECHO *
ECHO * Example:  A:INSTALL D
ECHO *           would install uC/OS-II, The Real-Time Kernel on drive D:
ECHO ***************************************************************************
ECHO *
IF "%1" == "" GOTO CURRENT
%1:
:CURRENT
CD   \
MD   \SOFTWARE
CD   \SOFTWARE
COPY A:uCOS-II.EXE
uCOS-II -d -n \SOFTWARE
DEL  uCOS-II.EXE
CD   \SOFTWARE\uCOS-II\EX1_x86L
ECHO *CLS
ECHO ***************************************************************************
ECHO *                         uC/OS-II, The Real-Time Kernel
ECHO *                             INSTALLATION COMPLETE.
ECHO ***************************************************************************
