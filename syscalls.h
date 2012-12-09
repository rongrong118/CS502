
/*********************************************************************

  syscalls.h

        This include file is used by only the OS502.

        Revision History:
        1.0 August 1990:        Initial release
        1.1 Jan 1991:           Make system calls portable 
                                by using union of pointer 
                                and long.  Add incls for 
                                scheduler_printer.
        1.2 Dec 1991;           Allow interrupts to occur 
                                in user code and in CALL 
                                statements.
        1.5 Aug 1993;           Add READ_MODIFY & 
                                DEFINE_SHARED_AREA support.
        2.0 Jan 2000;           Small changes
        2.1 May 2001;           Fix STEP macro.  DISK macros.
        2.2 Jul 2002;           Make code appropriate for undergrads.
        2.3 Aug 2004;           Modify Memory defines to work in kernel
        3.1 Aug 2004:           hardware interrupt runs on separate thread
        3.11 Aug 2004:          Support for OS level locking
	3.30 July 2006:         Modify POP_THE_STACK to apply to base only
*********************************************************************/

#include        "stdio.h"

        /* Definition of System Call numbers                        */

#define         SYSNUM_MEM_READ                        0
#define         SYSNUM_MEM_WRITE                       1
#define         SYSNUM_READ_MODIFY                     2
#define         SYSNUM_GET_TIME_OF_DAY                 3
#define         SYSNUM_SLEEP                           4
#define         SYSNUM_GET_PROCESS_ID                  5
#define         SYSNUM_CREATE_PROCESS                  6
#define         SYSNUM_TERMINATE_PROCESS               7
#define         SYSNUM_SUSPEND_PROCESS                 8
#define         SYSNUM_RESUME_PROCESS                  9
#define         SYSNUM_CHANGE_PRIORITY                 10
#define         SYSNUM_SEND_MESSAGE                    11
#define         SYSNUM_RECEIVE_MESSAGE                 12
#define         SYSNUM_DISK_READ                       13
#define         SYSNUM_DISK_WRITE                      14
#define         SYSNUM_DEFINE_SHARED_AREA              15


extern void     charge_time_and_check_events( INT32 );
extern int      BaseThread();

#ifndef COST_OF_CALL
#define         COST_OF_CALL                            2L
#endif


#define         CALL( fff )                                     \
                {                                               \
		extern BOOL  POP_THE_STACK;                     \
                charge_time_and_check_events( COST_OF_CALL );   \
                fff;                                            \
                if( POP_THE_STACK && BaseThread() )             \
                    return;                                     \
                }                                               \

/*      ZCALL is used only within the hardware - and for calls TO
        the hardware.  The OS should NOT use this for calls between
        its own routines.  For it's own calls, use CALL above.     */

#define         ZCALL( fff )                                    \
                {                                               \
		extern BOOL  POP_THE_STACK;                     \
                fff;                                            \
                if( POP_THE_STACK && BaseThread() )             \
                    return;                                     \
                }                                               \


/*      Macros used to make the test programs more readable        */

#ifndef         COST_OF_CPU_INSTRUCTION
#define         COST_OF_CPU_INSTRUCTION                 1L
#endif

                                /* Some compilers require a short
                                   in a switch statement!       */

#define         SELECT_STEP  switch( (INT16)Z502_PROGRAM_COUNTER )

#define         STEP( sss )                                       \
                                                                  \
        case sss:                                                 \
        charge_time_and_check_events( COST_OF_CPU_INSTRUCTION );  \
        Z502_PROGRAM_COUNTER++;;

#define         GO_NEXT_TO( ggg )   Z502_PROGRAM_COUNTER = ggg;



/*      Macro expansions for each of the system calls           */

#ifdef  USER
#define         MEM_READ( arg1, arg2 )                          \
				{												\
                    SYS_CALL_CALL_TYPE = SYSNUM_MEM_READ;       \
                    Z502_ARG1.VAL       = arg1;             \
                    Z502_ARG2.PTR       = (void *)arg2;     \
                    return;                                     \
                }
#endif
#ifndef  USER
#define         MEM_READ( arg1, arg2 )   Z502_MEM_READ( arg1, arg2 )
#endif 

                                             
#ifdef  USER
#define         MEM_WRITE( arg1, arg2 )                         \
                {                                               \
                    SYS_CALL_CALL_TYPE = SYSNUM_MEM_WRITE;      \
                    Z502_ARG1.VAL       = arg1;             \
                    Z502_ARG2.PTR       = (void *)arg2;     \
                    return;                                         \
                } 
#endif
#ifndef  USER 
#define         MEM_WRITE( arg1, arg2 )   Z502_MEM_WRITE( arg1, arg2 ); 
#endif

#ifdef  USER
#define         READ_MODIFY( arg1, arg2, arg3, arg4 )           \
                {                                               \
                    SYS_CALL_CALL_TYPE = SYSNUM_READ_MODIFY;    \
                    Z502_ARG1.VAL       = arg1;             \
                    Z502_ARG2.VAL       = arg2;             \
                    Z502_ARG3.VAL       = arg3;             \
                    Z502_ARG4.PTR       = (void *)arg4;     \
                    return;                                     \
                }
#endif
#ifndef  USER
#define         READ_MODIFY( arg1, arg2 )   Z502_READ_MODIFY( arg1, arg2 ); 
#endif

#define         GET_TIME_OF_DAY( arg1 )                         \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_GET_TIME_OF_DAY;    \
                Z502_ARG1.PTR       = (void *)arg1;             \
                return;                                         \
                }                                               \

#define         SLEEP( arg1 )                                   \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_SLEEP;              \
                Z502_ARG1.VAL       = arg1;                     \
                return;                                         \
                }                                               \

#define         CREATE_PROCESS( arg1, arg2, arg3, arg4, arg5 )  \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_CREATE_PROCESS;     \
                Z502_ARG1.PTR       = (void *)arg1;         \
                Z502_ARG2.PTR       = (void *)arg2;         \
                Z502_ARG3.VAL       = arg3;                 \
                Z502_ARG4.PTR       = (void *)arg4;         \
                Z502_ARG5.PTR       = (void *)arg5;         \
                return;                                         \
                }                                               \

#define         GET_PROCESS_ID( arg1, arg2, arg3 )              \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_GET_PROCESS_ID;     \
                Z502_ARG1.PTR       = (void *)arg1;         \
                Z502_ARG2.PTR       = (void *)arg2;         \
                Z502_ARG3.PTR       = (void *)arg3;         \
                return;                                         \
                }                                               \

#define         TERMINATE_PROCESS( arg1, arg2 )                 \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_TERMINATE_PROCESS;  \
                Z502_ARG1.VAL       = arg1;                 \
                Z502_ARG2.PTR       = (void *)arg2;         \
                return;                                         \
                }                                               \

#define         SUSPEND_PROCESS( arg1, arg2 )                   \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_SUSPEND_PROCESS;    \
                Z502_ARG1.VAL       = arg1;                 \
                Z502_ARG2.PTR       = (void *)arg2;         \
                return;                                         \
                }                                               \

#define         RESUME_PROCESS( arg1, arg2 )                    \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_RESUME_PROCESS;     \
                Z502_ARG1.VAL       = arg1;                 \
                Z502_ARG2.PTR       = (void *)arg2;         \
                return;                                         \
                }                                               \

#define         CHANGE_PRIORITY( arg1, arg2, arg3 )             \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_CHANGE_PRIORITY;    \
                Z502_ARG1.VAL       = arg1;                 \
                Z502_ARG2.VAL       = arg2;                 \
                Z502_ARG3.PTR       = (void *)arg3;         \
                return;                                         \
                }                                               \


#define         SEND_MESSAGE( arg1, arg2, arg3, arg4 )          \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_SEND_MESSAGE;       \
                Z502_ARG1.VAL       = arg1;                 \
                Z502_ARG2.PTR       = (void *)arg2;         \
                Z502_ARG3.VAL       = arg3;                 \
                Z502_ARG4.PTR       = (void *)arg4;         \
                return;                                         \
                }                                               \


#define         RECEIVE_MESSAGE( arg1, arg2, arg3, arg4, arg5, arg6 ) \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_RECEIVE_MESSAGE;    \
                Z502_ARG1.VAL       = arg1;                 \
                Z502_ARG2.PTR       = (void *)arg2;         \
                Z502_ARG3.VAL       = arg3;                 \
                Z502_ARG4.PTR       = (void *)arg4;         \
                Z502_ARG5.PTR       = (void *)arg5;         \
                Z502_ARG6.PTR       = (void *)arg6;         \
                return;                                         \
                }                                               \

#define         DISK_READ( arg1, arg2, arg3 )                   \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_DISK_READ;          \
                Z502_ARG1.VAL       = arg1;                 \
                Z502_ARG2.VAL       = arg2;                 \
                Z502_ARG3.PTR       = (void *)arg3;         \
                return;                                         \
                }                                               \

#define         DISK_WRITE( arg1, arg2, arg3 )                  \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_DISK_WRITE;         \
                Z502_ARG1.VAL       = arg1;                 \
                Z502_ARG2.VAL       = arg2;                 \
                Z502_ARG3.PTR       = (void *)arg3;         \
                return;                                         \
                }                                               \

#define         DEFINE_SHARED_AREA( arg1, arg2, arg3, arg4, arg5 )  \
                {                                               \
                SYS_CALL_CALL_TYPE = SYSNUM_DEFINE_SHARED_AREA; \
                Z502_ARG1.VAL       = arg1;                 \
                Z502_ARG2.VAL       = arg2;                 \
                Z502_ARG3.PTR       = (void *)arg3;         \
                Z502_ARG4.PTR       = (void *)arg4;         \
                Z502_ARG5.PTR       = (void *)arg5;         \
                return;                                         \
                }                                               \

/*      This section includes items needed in the scheduler printer.
        It's also useful for those routines that want to communicate
        with the scheduler printer.                                       */

#define         SP_FILE_MODE            (INT16)0
#define         SP_TIME_MODE            (INT16)1
#define         SP_ACTION_MODE          (INT16)2
#define         SP_TARGET_MODE          (INT16)3
#define         SP_STATE_MODE_START     (INT16)4
#define         SP_NEW_MODE             (INT16)4
#define         SP_RUNNING_MODE         (INT16)5
#define         SP_READY_MODE           (INT16)6
#define         SP_WAITING_MODE         (INT16)7
#define         SP_SUSPENDED_MODE       (INT16)8
#define         SP_SWAPPED_MODE         (INT16)9
#define         SP_TERMINATED_MODE      (INT16)10

#define         SP_NUMBER_OF_STATES     SP_TERMINATED_MODE-SP_NEW_MODE+1
#define         SP_MAX_NUMBER_OF_PIDS   (INT16)10
#define         SP_LENGTH_OF_ACTION     (INT16)8


/*      This string is printed out when requested as the header         */

#define         SP_HEADER_STRING                              \
" Time Target Action  Run New Done       State Populations \n"
