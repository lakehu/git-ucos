/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*
*                        (c) Copyright 1992-1998, Jean J. Labrosse, Plantation, FL
*                                           All Rights Reserved
*
*                                                  V2.00
*
* File : uCOS_II.C
* By   : Jean J. Labrosse
*********************************************************************************************************
*/

#define  OS_GLOBALS                           /* Declare GLOBAL variables                              */
#include "includes.h"


#define  OS_MASTER_FILE                       /* Prevent the following files from including includes.h */
#include "\software\uCOS-II\source\os_core.c"
#include "\software\uCOS-II\source\os_mbox.c"
#include "\software\uCOS-II\source\os_mem.c"
#include "\software\uCOS-II\source\os_q.c"
#include "\software\uCOS-II\source\os_sem.c"
#include "\software\uCOS-II\source\os_task.c"
#include "\software\uCOS-II\source\os_time.c"

