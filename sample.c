/*
*********************************************************************************************************
*                                               sample.c
*
* Description:	This sample program uses the ucos linux port to start 5 simple tasks.
*
* Author: Philip Mitchell
*
*********************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include "ucos_ii.h"

/* Function common to all tasks */

void MyTask( void *p_arg )
{
	char* sTaskName = (char*)p_arg;

#if OS_CRITICAL_METHOD == 3 
    OS_CPU_SR     cpu_sr = 0;
#endif

	while(1)
	{
		/* printf uses mutex to get terminal access, therefore must enter critical section */
		OS_ENTER_CRITICAL();
		printf( "Name: %s\n", sTaskName );
		OS_EXIT_CRITICAL();

		/* Delay so other tasks may execute. */
		OSTimeDly(1);
	}/* while */

}


int main (void)
{
	/* pthreads allocates its own memory for task stacks. This UCOS linux port needs a minimum stack size
		in order to pass the function information within the port. */

	INT8U Stk1[ OSMinStkSize() ];
	INT8U Stk2[ OSMinStkSize() ];
	INT8U Stk3[ OSMinStkSize() ];
	INT8U Stk4[ OSMinStkSize() ];
	INT8U Stk5[ OSMinStkSize() ];

	char sTask1[] = "Task 1";
	char sTask2[] = "Task 2";
	char sTask3[] = "Task 3";
	char sTask4[] = "Task 4";
	char sTask5[] = "Task 5";

	printf("OSMinStkSize=%d\n",OSMinStkSize());
	OSInit();

/*
 Note  OS_STK_GROWTH is defined in OS_CPU.h
 STACK parameter in OSTaskCreate is the top address
 Also please take care of stack pointer usage in OSTaskStkInit/OS_CPU_C.C
*/
	OSTaskCreate( MyTask, sTask1, (void*)&Stk1[ OSMinStkSize() ], 4 );
	OSTaskCreate( MyTask, sTask2, (void*)&Stk2[ OSMinStkSize() ], 5 );
	OSTaskCreate( MyTask, sTask3, (void*)&Stk3[ OSMinStkSize() ], 6 );
	OSTaskCreate( MyTask, sTask4, (void*)&Stk4[ OSMinStkSize() ], 7 );
	OSTaskCreate( MyTask, sTask5, (void*)&Stk5[ OSMinStkSize() ], 8 );

    OSStart();
}

