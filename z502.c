/*********************************************************************

  z502.c

   This is the start of the code and declarations for the Z502 simulator.

    Revision History:       
        1.0 August      1990  Initial coding
        1.1 December    1990: Lots of minor problems cleaned up.
                              Portability attempted.
        1.4 December    1992  Lots of minor changes.
        1.5 August      1993  Lots of minor changes.
        1.6 June        1995  Significant debugging aids added.
        1.7 Sept.       1999  Minor compile issues.
        2.0 Jan         2000  A number of fixes and improvements.
        2.1 May         2001  Bug Fixes:
                              Disks no longer make cylinders visible
                              Hardware displays resource usage
        2.2 July        2002  Make appropriate for undergraduates.
        2.3 Dec         2002  Allow Mem calls from OS - conditional
                              on   POP_THE_STACK
                              Clear the STAT_VECTOR at the end
                              of software_trap
        3.0 August      2004: Modified to support memory mapped IO
        3.1 August      2004: hardware interrupt runs on separate thread
        3.11 August     2004: Support for OS level locking
        3.12 Sept.      2004: Make threads schedule correctly
        3.14 November   2004: Impliment DO_DEVICE_DEBUG in meaningful way.
        3.15 November   2004: More DO_DEVICE_DEBUG to handle disks
        3.20 May        2005: Get threads working on windows
        3.26 October    2005: Make the hardware thread safe.
        3.30 July       2006: Threading works on multiprocessor
        3.40 July       2008: Minor improvements
        3.50 August     2009: Fixes for 64 bit machines and multiprocessor
        3.51 Sept.      2011: Fixed locking issue due to mem_common - Linux
        3.52 Sept.      2011: Fixed locking issue due to mem_common - Linux
                              Release the lock when leaving mem_common
        3.60  August    2012: Used student supplied code to add support
                              for Mac machines
**********************************************************************

        mem_common();                   INTERNAL: a routine used by both
                                        MEM_READ & MEM_WRITE.
        Z502_MEM_READ();                hardware memory read request.
        Z502_MEM_WRITE();               hardware memory write request.
        Z502_READ_MODIFY();             atomic test and set.
        Z502_HALT();                    halts the CPU.
        Z502_IDLE();                    machine halts until interrupt 
                                        occurs.
        Z502_MAKE_CONTEXT();            the hardware creates the context
                                        upon which a new process will run.
        Z502_DESTROY_CONTEXT();         destroy the context for a process.
        Z502_SWITCH_CONTEXT();          run a new context.
        change_context();               INTERNAL:  changes process that
                                        is currently running.
        charge_time_and_check_events(); INTERNAL: increment the simulation
                                        clock and see if there is interrupt.
        hardware_interrupt();           INTERNAL: calls the user's
                                        interrupt handler.
        hardware_fault();               INTERNAL: calls user
                                                  hardware fault handler.
        software_trap();                INTERNAL: calls user
                                                  software fault handler.
        z502_internal_panic();          INTERNAL: halt machine with a message.
        add_event();                    INTERNAL: schedule event to
                                                  interrupt in the future.
        get_next_ordered_event();       INTERNAL: determine who has caused
                                        the last interrupt.
        dequeue_item();                 INTERNAL: remove item from event queue.
        get_next_event_time();          INTERNAL: determine the time at
                                        which the next event will occur.
        get_sector_struct();            INTERNAL: get information about disk.
        create_sector_struct();         INTERNAL: create a structure to
                                        be used to hold disk info.
        main();                         contains the simulation entry
                                        and the base level loop.
************************************************************************/

/************************************************************************

        GLOBAL VARIABLES:  These declarations are visible to both the 
                Z502 simulator and the OS502 if the OS declares them
                extern.

************************************************************************/

#define                   HARDWARE_VERSION  "3.60"
// #define                 __USE_UNIX98
// #define                  DEBUG_LOCKS
// #define                  DEBUG_CONDITION
#include                 "global.h"
#include                 "syscalls.h"
#include                 "z502.h"
#include                 "protos.h"
#include                 <stdio.h>
#include                 <stdlib.h>
#include                 <memory.h>
#ifdef NT
#include                 <windows.h>
#include                 <winbase.h>
#include                 <sys/types.h>
#endif

#ifdef LINUX
#include                 <pthread.h>
#include                 <unistd.h>
#include                 <asm/errno.h>
#include                 <sys/time.h>
#include                 <sys/resource.h>
#endif

#ifdef MAC
#include                 <pthread.h>
#include                 <unistd.h>
#include                 <errno.h>
#include                 <sys/time.h>
#include                 <sys/resource.h>
#endif


//  These are routines internal to the hardware, not visible to the OS
//  Prototypes that allow the OS to get to this hardware are in protos.h

void            mem_common( INT32, char *, BOOL );
void            do_memory_debug( INT16, INT16 );
void            memory_mapped_io( INT32, INT32 *, BOOL );
void            change_context( void );
void            charge_time_and_check_events( INT32 );
void            hardware_clock( INT32 * );
void            hardware_timer( INT32 );
void            hardware_read_disk(  INT16, INT16, char * );
void            hardware_write_disk( INT16, INT16, char * );
void            hardware_interrupt( void );
void            hardware_fault( INT16, INT16 );
void            software_trap( void );
void            z502_internal_panic( INT32 );
void            PrintEventQueue();
void            add_event( INT32, INT16, INT16, EVENT ** ); 
void            get_next_ordered_event( INT32 *, INT16 *, INT16 *, INT32 * ); 
void            dequeue_item( EVENT *, INT32 * );
void            get_next_event_time( INT32 * );
void            get_sector_struct( INT16, INT16, char **, INT32 * );
void            create_sector_struct( INT16, INT16, char ** );
void            print_ring_buffer( void );
void            print_hardware_stats( void );
int             GetMyTid( );
void            PrintLockDebug( char *Text, int Action, char *LockCaller, int Mutex, int Return );

/*      This is Physical Memory which is used in part 2 of the project. */

char            MEMORY[ PHYS_MEM_PGS * PGSIZE ];  

BOOL            POP_THE_STACK;               /* Don't mess with this    */

/*      These are the args used when the hardware is called.  See usage
        in base.c.                                                      */

INT32           CALLING_ARGC;           
char            **CALLING_ARGV;


/*      Declaration of Z502 Registers                           */

Z502CONTEXT       *Z502_CURRENT_CONTEXT;
UINT16            *Z502_PAGE_TBL_ADDR;
INT16             Z502_PAGE_TBL_LENGTH;
INT16             Z502_PROGRAM_COUNTER;
INT16             Z502_MODE;
Z502_ARG          Z502_ARG1;
Z502_ARG          Z502_ARG2;
Z502_ARG          Z502_ARG3;
Z502_ARG          Z502_ARG4;
Z502_ARG          Z502_ARG5;
Z502_ARG          Z502_ARG6;
long              Z502_REG_1;
long              Z502_REG_2;
long              Z502_REG_3;
long              Z502_REG_4;
long              Z502_REG_5;
long              Z502_REG_6;
long              Z502_REG_7;
long              Z502_REG_8;
long              Z502_REG_9;
INT16             STAT_VECTOR[SV_VALUE+1][ LARGEST_STAT_VECTOR_INDEX + 1 ];
void              *TO_VECTOR[TO_VECTOR_TYPES];


    /*****************************************************************

        LOCAL VARIABLES:  These declarations should be visible only 
                to the Z502 simulator.

    *****************************************************************/

UINT32          current_simulation_time  = 0;
INT16           event_ring_buffer_index  = 0;

EVENT           event_queue;
INT32           NumberOfInterruptsStarted = 0;
INT32           NumberOfInterruptsCompleted = 0;
SECTOR          sector_queue[MAX_NUMBER_OF_DISKS + 1];
DISK_STATE      disk_state[MAX_NUMBER_OF_DISKS + 1];
TIMER_STATE     timer_state;
HARDWARE_STATS  hardware_stats;
BOOL            z502_machine_kill_or_save = SWITCH_CONTEXT_SAVE_MODE;
Z502CONTEXT     *z502_machine_next_context_ptr;
RING_EVENT      event_ring_buffer[ EVENT_RING_BUFFER_SIZE ];
INT32           InterlockRecord[MEMORY_INTERLOCK_SIZE]; 
INT32           EventLock = -1;                          // Change from UINT32 - 08/2012
INT32           InterruptLock = -1;
INT32           HardwareLock = -1;
UINT32          InterruptCondition = 0;
int             NextConditionToAllocate = 0;
int             BaseTid;
int             InterruptTid;
#ifdef   NT
HANDLE          LocalEvent[10];
#endif

#if defined LINUX || defined MAC
pthread_mutex_t LocalMutex[300];
pthread_cond_t  LocalCondition[10];
int             NextMutexToAllocate = 0;
#endif
/*      These are used for system call support                     */

INT32           SYS_CALL_CALL_TYPE;


    /*****************************************************************
    mem_common

      This code simulates a memory access.  Actions include:
          o Take a page fault if any of the following occur;
              + Illegal virtual address,
              + Page table doesn't exist,
              + Address is larger than page table,
              + Page table entry exists, but page is invalid.
          o The page exists in physical memory, so get the physical address.  
            Be careful since it may wrap across frame boundaries.
          o Copy data to/from caller's location.
          o Set referenced/modified bit in page table.
          o Advance time and see if an interrupt has occurred.
    *****************************************************************/

void   mem_common( INT32 virtual_address, char *data_ptr, BOOL read_or_write )
    {
    INT16       virtual_page_number;
    INT32       phys_pg;
    INT16       physical_address[4];
    INT32       page_offset;
    INT16       index;
    INT32       ptbl_bits;
    INT16       invalidity;
    BOOL        page_is_valid;
    char        Debug_Text[32];

    strcpy( Debug_Text, "mem_common");
    GetLock( HardwareLock, Debug_Text );
    if ( virtual_address >= Z502MEM_MAPPED_MIN )
    {
        memory_mapped_io( virtual_address, (INT32 *)data_ptr, read_or_write );
        ReleaseLock( HardwareLock, Debug_Text );
        return;
    }
    virtual_page_number = (INT16)( ( virtual_address >= 0 ) ?
                          virtual_address/PGSIZE : -1 );
    page_offset = virtual_address % PGSIZE;

    page_is_valid = FALSE;

    /*  Loop until the virtual page passes all the tests        */

    while( page_is_valid == FALSE )     
        {
        invalidity = 0;
        if ( virtual_page_number >= VIRTUAL_MEM_PGS )
           invalidity = 1;
        if ( virtual_page_number <  0 ) 
           invalidity = 2;
        if ( Z502_PAGE_TBL_ADDR == NULL )
           invalidity = 3;
        if ( virtual_page_number >= Z502_PAGE_TBL_LENGTH) 
           invalidity = 4;
        if ( (invalidity == 0 ) &&
             ( Z502_PAGE_TBL_ADDR[(UINT16)virtual_page_number] 
                                   & PTBL_VALID_BIT) == 0 )   
           invalidity = 5;

        do_memory_debug( invalidity, virtual_page_number );
        if ( invalidity > 0 )
            {
            if (Z502_CURRENT_CONTEXT->structure_id != CONTEXT_STRUCTURE_ID)
                {
                printf( "Z502_CURRENT_CONTEXT is invalid in mem_common\n");
                printf( "Something in the OS has destroyed this location.\n");
                z502_internal_panic( ERR_OS502_GENERATED_BUG );
            }
            Z502_CURRENT_CONTEXT->fault_in_progress = TRUE;
            // The fault handler will do it's own locking - 11/13/11
            ReleaseLock( HardwareLock, Debug_Text );
            ZCALL( hardware_fault( INVALID_MEMORY, virtual_page_number));
            // Regain the lock to protect the memory check - 11/13/11
            GetLock( HardwareLock, Debug_Text );
        }
        else
            page_is_valid = TRUE;
    }                                           /* END of while         */

    phys_pg = Z502_PAGE_TBL_ADDR[virtual_page_number] & PTBL_PHYS_PG_NO;
    physical_address[0] = (INT16)(phys_pg * (INT32)PGSIZE + page_offset);
    physical_address[1] = physical_address[0] + 1; /* first guess */
    physical_address[2] = physical_address[0] + 2; /* first guess */
    physical_address[3] = physical_address[0] + 3; /* first guess */

    page_is_valid = FALSE;
    if ( page_offset > PGSIZE - 4 )     /* long int wraps over page */
        {
        while ( page_is_valid == FALSE )
            {
            invalidity = 0;
            if ( virtual_page_number + 1 >= VIRTUAL_MEM_PGS ) invalidity = 6;
            if ( virtual_page_number + 1 >= 
                                Z502_PAGE_TBL_LENGTH )    invalidity = 7;
            if ( ( Z502_PAGE_TBL_ADDR[(UINT16)virtual_page_number + 1] 
                                & PTBL_VALID_BIT ) == 0 )     invalidity = 8;
            do_memory_debug( invalidity, (short)(virtual_page_number + 1) );
            if ( invalidity > 0 )
                {
                if (Z502_CURRENT_CONTEXT->structure_id 
                                                != CONTEXT_STRUCTURE_ID )
                    {
                    printf( "Z502_CURRENT_CONTEXT invalid in mem_common\n");
                    printf( "The OS has destroyed this location.\n");
                    z502_internal_panic( ERR_OS502_GENERATED_BUG      );
                }
                Z502_CURRENT_CONTEXT->fault_in_progress = TRUE;
                ZCALL(hardware_fault( INVALID_MEMORY, 
                                         (INT16)(virtual_page_number + 1) ) );
            }
            else
                page_is_valid = TRUE;
        }                                       /* End of while         */

        phys_pg = Z502_PAGE_TBL_ADDR[virtual_page_number + 1] 
                                                        & PTBL_PHYS_PG_NO;
        for ( index = PGSIZE - (INT16)page_offset; index <= 3; index++ )
            physical_address[index] = (INT16)(( phys_pg - 1 ) 
                                    * (INT32)PGSIZE + page_offset 
                                    + (INT32)index);
    }                                           /* End of if page       */

    if ( phys_pg < 0  || phys_pg > PHYS_MEM_PGS - 1 )
        {
        printf( "The physical address is invalid in mem_common\n");
        printf( "Physical page = %d, Virtual Page = %d\n", 
                        phys_pg, virtual_page_number );
        z502_internal_panic( ERR_OS502_GENERATED_BUG      );
    }
    if ( Z502_CURRENT_CONTEXT->structure_id != CONTEXT_STRUCTURE_ID )
        {
        printf( "Z502_CURRENT_CONTEXT is invalid in mem_common\n");
        printf( "Something in the OS has destroyed this location.\n");
        z502_internal_panic( ERR_OS502_GENERATED_BUG      );
    }
    Z502_CURRENT_CONTEXT->fault_in_progress = FALSE;


    if ( read_or_write == SYSNUM_MEM_READ )
        {
        data_ptr[0] = MEMORY[ physical_address[0] ];
        data_ptr[1] = MEMORY[ physical_address[1] ];
        data_ptr[2] = MEMORY[ physical_address[2] ];
        data_ptr[3] = MEMORY[ physical_address[3] ];
        ptbl_bits = PTBL_REFERENCED_BIT;
    }

    if ( read_or_write == SYSNUM_MEM_WRITE )
        {
        MEMORY[ physical_address[0] ] = data_ptr[0];
        MEMORY[ physical_address[1] ] = data_ptr[1];
        MEMORY[ physical_address[2] ] = data_ptr[2];
        MEMORY[ physical_address[3] ] = data_ptr[3];
        ptbl_bits = PTBL_REFERENCED_BIT | PTBL_MODIFIED_BIT;
    }

    Z502_PAGE_TBL_ADDR[ virtual_page_number ]         |= ptbl_bits;
    if ( page_offset > PGSIZE - 4 )
        Z502_PAGE_TBL_ADDR[ virtual_page_number + 1 ] |= ptbl_bits;
  
    charge_time_and_check_events( COST_OF_MEMORY_ACCESS );
    if ( Z502_MODE != KERNEL_MODE )
        POP_THE_STACK = TRUE;
    ReleaseLock( HardwareLock, Debug_Text );
}                                       /* End of mem_common        */

    /*****************************************************************
        do_memory_debug

                Print out details about why a page fault occurred.

    *****************************************************************/

void    do_memory_debug( INT16 invalidity, INT16 vpn )
    {
    if ( DO_MEMORY_DEBUG == 0 ) return;
    printf( "MEMORY DEBUG: ");
    if ( invalidity == 0 )
        {
        printf( "Virtual Page Number %d was successful\n", vpn );
    }
    if ( invalidity == 1 )
        {
        printf( "You asked for a virtual page, %d, greater than the\n", vpn );
        printf( "\t\tmaximum number of virtual pages, %d\n", VIRTUAL_MEM_PGS );
    }
    if ( invalidity == 2 )
        {
        printf( "You asked for a virtual page, %d, less than\n", vpn );
        printf( "\t\tzero.  What are you thinking?\n" );
    }
    if ( invalidity == 3 )
        {
        printf( "You have not yet defined a page table that is visible\n");
        printf( "\t\tto the hardware.  Z502_PAGE_TBL_ADDR must\n");
        printf( "\t\tcontain the address of the page table.\n" );
    }
    if ( invalidity == 4 )
        {
        printf( "You have requested a page, %d, that is larger\n", vpn );
        printf( "\t\tthan the size of the page table you defined.\n" );
        printf( "\t\tThe hardware thinks that the length of this table\n");
        printf( "\t\tis %d\n", Z502_PAGE_TBL_LENGTH );
    }
    if ( invalidity == 5 )
        {
        printf( "You have not initialized the slot in the page table\n");
        printf( "\t\tcorresponding to virtual page %d\n", vpn );
        printf( "\t\tYou must aim this virtual page at a physical frame\n");
        printf( "\t\tand mark this page table slot as valid.\n");
    }
    if ( invalidity == 6 )
        {
        printf( "The address you asked for crosses onto a second page.\n");
        printf( "\t\tThis second page took a fault.\n");
        printf( "You asked for a virtual page, %d, greater than the\n", vpn );
        printf( "\t\tmaximum number of virtual pages, %d\n", VIRTUAL_MEM_PGS );
    }

    if ( invalidity == 7 )
        {
        printf( "The address you asked for crosses onto a second page.\n");
        printf( "\t\tThis second page took a fault.\n");
        printf( "You have requested a page, %d, that is larger\n", vpn );
        printf( "\t\tthan the size of the page table you defined.\n" );
        printf( "\t\tThe hardware thinks that the length of this table\n");
        printf( "\t\tis %d\n", Z502_PAGE_TBL_LENGTH );
    }
    if ( invalidity == 8 )
        {
        printf( "The address you asked for crosses onto a second page.\n");
        printf( "\t\tThis second page took a fault.\n");
        printf( "You have not initialized the slot in the page table\n");
        printf( "\t\tcorresponding to virtual page %d\n", vpn );
        printf( "\t\tYou must aim this virtual page at a physical frame\n");
        printf( "\t\tand mark this page table slot as valid.\n");
    }
}                               /* End of do_memory_debug               */

    /*****************************************************************
        Z502_MEM_READ   and   Z502_MEM_WRITE

                Set a flag and call common code

    *****************************************************************/

void    Z502_MEM_READ( INT32 virtual_address, INT32 *data_ptr )
    {
    ZCALL( mem_common( virtual_address, (char *)data_ptr, 
                       (BOOL)SYSNUM_MEM_READ ) ); 
}                                       /* End  Z502_MEM_READ  */


void    Z502_MEM_WRITE( INT32 virtual_address, INT32 *data_ptr )
    {
    ZCALL( mem_common( virtual_address, (char *)data_ptr, 
                       (BOOL)SYSNUM_MEM_WRITE ) );
}                                       /* End  Z502_MEM_WRITE  */

/*************************************************************************
    Z502_READ_MODIFY

    Do atomic modify of a memory location.  If the input parameters are
    incorrect, then return SuccessfulAction = FALSE.  
    If the memory location
    is already locked by someone else, and we're asked to lock
                return SuccessfulAction = FALSE.
    If the lock was obtained here, return  SuccessfulAction = TRUE.
    If the lock was held by us or someone else, and we're asked to UNlock,
                return SuccessfulAction = TRUE.
    NOTE:  There are 10 lock locations set aside for the hardware's use.
           It is assumed that the hardware will have initialized these locks
           early on so that they don't interfere with this mechanism.
*************************************************************************/

void    Z502_READ_MODIFY( INT32 VirtualAddress, INT32 NewLockValue,
                          INT32 Suspend, INT32 *SuccessfulAction )
{
    int    WhichRecord;
    // GetLock( HardwareLock, "Z502_READ_MODIFY" );   JB - 7/26/06
    if (   VirtualAddress < MEMORY_INTERLOCK_BASE
        || VirtualAddress >= MEMORY_INTERLOCK_BASE + MEMORY_INTERLOCK_SIZE + 10
        || ( NewLockValue != 0 && NewLockValue != 1 )
        || ( Suspend != TRUE   && Suspend != FALSE )   )
    {
        *SuccessfulAction = FALSE;
        return;
    }
    WhichRecord = VirtualAddress - MEMORY_INTERLOCK_BASE + 10;
    if ( InterlockRecord[ WhichRecord ] == -1 )
         CreateLock( &(InterlockRecord[ WhichRecord ]) );
    if ( NewLockValue == 1 && Suspend == FALSE )
        *SuccessfulAction 
                   = GetTryLock( InterlockRecord[ WhichRecord ] );
    if ( NewLockValue == 1 && Suspend == TRUE )
    {
        *SuccessfulAction
             = GetLock( InterlockRecord[ WhichRecord ], "Z502_READ_MODIFY");
    }
    if ( NewLockValue == 0 )
    {
        *SuccessfulAction 
             = ReleaseLock( InterlockRecord[ WhichRecord ], "Z502_READ_MODIFY" );
    }
    // ReleaseLock( HardwareLock, "Z502_READ_MODIFY" );   JB - 7/26/06

}                                       /* End  Z502_READ_MODIFY  */


/*************************************************************************
    memory_mapped_io

    We talk to devices via certain memory addresses found in memory 
    hyperspace.  In other words, these memory addresses don't point to
    real physical memory.  You must be privileged to touch this hardware.

*************************************************************************/

void    memory_mapped_io( INT32 address, INT32 *data, BOOL read_or_write )
{
    static INT32       MemoryMappedIOInterruptDevice = -1;
    static INT32       MemoryMappedIODiskDevice      = -1;
    static MEMORY_MAPPED_DISK_STATE
                       MemoryMappedDiskState;
    INT32              index;

    // GetLock ( HardwareLock, "memory_mapped_io" );
    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() )  {
        ZCALL( hardware_fault( PRIVILEGED_INSTRUCTION, 0 ) );
        return;
    }
        charge_time_and_check_events( COST_OF_MEMORY_MAPPED_IO );
    switch( address )
    {
        /*  Here we either get the device that's caused the interrupt, or
         *  we set the device id that we want to query further.  */
        
        case Z502InterruptDevice: {
            if ( read_or_write == SYSNUM_MEM_READ ) {
                    *data = -1;
                for ( index = 0; index <= LARGEST_STAT_VECTOR_INDEX; index++ )
                    if ( STAT_VECTOR[SV_ACTIVE][ index ] != 0 )
                        *data = index;
            }
            else
                if ( *data >= 0 && *data <= LARGEST_STAT_VECTOR_INDEX )
                    MemoryMappedIOInterruptDevice = *data;
                else
                    MemoryMappedIOInterruptDevice = -1;
            break;
        }

        case Z502InterruptStatus: {
            *data = ERR_BAD_DEVICE_ID;
            if ( MemoryMappedIOInterruptDevice != -1 )
                *data = STAT_VECTOR[SV_VALUE][MemoryMappedIOInterruptDevice];
            break;
        }

        case Z502InterruptClear: {
            if ( MemoryMappedIOInterruptDevice != -1 && *data == 0 )
            {
                STAT_VECTOR[SV_VALUE][MemoryMappedIOInterruptDevice] = 0;
                STAT_VECTOR[SV_ACTIVE][MemoryMappedIOInterruptDevice] = 0;
                MemoryMappedIOInterruptDevice = -1;
            }
            break;
        }
        case Z502ClockStatus: {
             hardware_clock( data );
            break;
        }
        case Z502TimerStart: {
            hardware_timer( *data );
            break;
        }
        case Z502TimerStatus: {
            if ( timer_state.timer_in_use > 0 )
                *data = DEVICE_IN_USE;
            else
                *data = DEVICE_FREE;
            break;
        }
        /*  When we get the disk ID, set up the structure that we will
         *  use to keep track of its state as user inputs the data.  */
        case Z502DiskSetID: {
            MemoryMappedIODiskDevice      = -1;
            if (   *data  >= 1  && *data  <=  MAX_NUMBER_OF_DISKS )
            {
                MemoryMappedIODiskDevice                   = *data;
                MemoryMappedDiskState.sector               = -1;
                MemoryMappedDiskState.action               = -1;
                MemoryMappedDiskState.buffer               = (char *)-1;
            }
            else
            {
                if ( DO_DEVICE_DEBUG )
                {
                    printf( "------ BEGIN DO_DEVICE DEBUG - IN Z502DiskSetID ---------------- \n");
                    printf( "ERROR:  Trying to set device ID of the disk, but gave an invalid ID\n");
                    printf( "You gave data of %d but it must be in the range of  1 =<  data  <= %d\n",
                                    *data, MAX_NUMBER_OF_DISKS );
                    printf( "-------- END DO_DEVICE DEBUG - ----------------------------------\n");
                }
            }
            break;
        }
        case Z502DiskSetSector: {
            if ( MemoryMappedIODiskDevice != -1 )
                MemoryMappedDiskState.sector = (short)*data;
            else
            {
                if ( DO_DEVICE_DEBUG )
                {
                    printf( "------ BEGIN DO_DEVICE DEBUG - IN Z502DiskSetSector ------------ \n");
                    printf( "ERROR:  You must define the Device ID before setting the sector\n");
                    printf( "-------- END DO_DEVICE DEBUG - ----------------------------------\n");
                }
            }

            break;
        }
        case Z502DiskSetup4: {
            break;
        }
        case Z502DiskSetAction: {
            if ( MemoryMappedIODiskDevice != -1 )
                MemoryMappedDiskState.action = (INT16)*data;
            else
            {
                if ( DO_DEVICE_DEBUG )
                {
                    printf( "------ BEGIN DO_DEVICE DEBUG - IN Z502DiskSetAction ------------ \n");
                    printf( "ERROR:  You must define the Device ID before setting the action\n");
                    printf( "-------- END DO_DEVICE DEBUG - ----------------------------------\n");
                }
            }
            break;
        }
        case Z502DiskSetBuffer: {
            if ( MemoryMappedIODiskDevice != -1 )
                MemoryMappedDiskState.buffer = (char *)data;
            else
            {
                if ( DO_DEVICE_DEBUG )
                {
                    printf( "------ BEGIN DO_DEVICE DEBUG - IN Z502DiskSetBuffer ------------ \n");
                    printf( "ERROR:  You must define the Device ID before setting the buffer\n");
                    printf( "-------- END DO_DEVICE DEBUG - ----------------------------------\n");
                }
            }
            break;
        }
        /*  Make sure we have the state properly prepared
         *  and then do a read or write.  Clear the state. */
        case Z502DiskStart: {
            if ( *data == 0 
                 && MemoryMappedIODiskDevice != -1 
                 && MemoryMappedDiskState.action != -1 
                 && MemoryMappedDiskState.buffer != (char *)-1 
                 && MemoryMappedDiskState.sector != -1 )
            {
                if ( MemoryMappedDiskState.action == 0 )
                    hardware_read_disk( (INT16)MemoryMappedIODiskDevice, 
                                  MemoryMappedDiskState.sector, 
                                  MemoryMappedDiskState.buffer );
                if ( MemoryMappedDiskState.action == 1 )
                    hardware_write_disk((INT16)MemoryMappedIODiskDevice, 
                                  MemoryMappedDiskState.sector, 
                                  MemoryMappedDiskState.buffer );
            }
            else
            {
                if ( DO_DEVICE_DEBUG )
                {
                    printf( "------ BEGIN DO_DEVICE DEBUG - IN Z502DiskSetStart ------------- \n");
                    printf( "ERROR:  You have not specified all the disk pre-conditions before\n");
                    printf( "        starting the disk,  OR you didn't put the correct parameter\n");
                    printf( "        on this start command.\n");
                    printf( "-------- END DO_DEVICE DEBUG - ----------------------------------\n");
                }
            }
            MemoryMappedIODiskDevice     = -1;
            MemoryMappedDiskState.action = -1;
            MemoryMappedDiskState.buffer = (char *)-1;
            MemoryMappedDiskState.sector = -1;
            break;
        }
        case Z502DiskStatus: {
            if ( MemoryMappedIODiskDevice == -1 )
                *data = ERR_BAD_DEVICE_ID;
            else
            {
                if ( disk_state[MemoryMappedIODiskDevice].disk_in_use == TRUE )
                    *data = DEVICE_IN_USE;
                else
                    *data = DEVICE_FREE;
            }
            break;
        }
        default: 
            break;
    }                                    /* End of switch */
    // ReleaseLock ( HardwareLock, "memory_mapped_io" );

}                                       /* End  memory_mapped_io  */

/*************************************************************************

        hardware_read_disk

            This code simulates a disk read.  Actions include:
                o If not in KERNEL_MODE, then cause priv inst trap.
                o Do range check on disk_id, sector; give
                  interrupt error = ERR_BAD_PARAM if illegal.
                o If an event for this disk already exists ( the disk
                  is already busy ), then give interrupt error                   
                  ERR_DISK_IN_USE.
                o Search for sector structure off of hashed value.
                o If search fails give interrupt error = 
                  ERR_NO_PREVIOUS_WRITE
                o Copy data from sector to buffer.
                o From disk_state information, determine how long
                  this request will take.
                o Request a future interrupt for this event.
                o Advance time and see if an interrupt has occurred.

**************************************************************************/

void    hardware_read_disk( INT16 disk_id, INT16 sector, char *buffer_ptr )
    {
    INT32       local_error;
    char        *sector_ptr;
    INT32       access_time;
    INT16       error_found;

    error_found = 0;
    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() )  {
        ZCALL( hardware_fault( PRIVILEGED_INSTRUCTION, 0 ) );
        return;
    }

    if (   disk_id  < 1  || disk_id  >  MAX_NUMBER_OF_DISKS )
        {
        disk_id = 1;                    /* To aim at legal vector  */
        error_found = ERR_BAD_PARAM;
    }
    if ( sector   < 0  || sector   >= NUM_LOGICAL_SECTORS )
        error_found = ERR_BAD_PARAM;


    if ( error_found == 0 )
        {
        get_sector_struct( disk_id, sector, &sector_ptr, &local_error );
        if ( local_error != 0 )
            error_found = ERR_NO_PREVIOUS_WRITE;

        if ( disk_state[disk_id].disk_in_use  == TRUE )
            error_found = ERR_DISK_IN_USE;
    }

    /* If we found an error, add an event that will cause an immediate
       hardware interrupt.                                              */

    if ( error_found != 0 )
        {
        if ( DO_DEVICE_DEBUG )
        {
            printf( "------ BEGIN DO_DEVICE DEBUG - IN read_disk ------------- \n");
            printf( "ERROR:  Something screwed up in your disk request.  The error\n");
            printf( "        code is %d that you can look up in global.h\n", error_found );
            printf( "        The disk is now going to generate an interrupt to tell \n");
            printf( "        you about that error.\n");
            printf( "-------- END DO_DEVICE DEBUG - ----------------------------------\n");
        }
        add_event( current_simulation_time, 
                   (INT16)(DISK_INTERRUPT + disk_id - 1),
                   error_found, &disk_state[disk_id].event_ptr );
    }
    else
        {
        memcpy( buffer_ptr, sector_ptr, PGSIZE );

        access_time = current_simulation_time + 100
                  + abs( disk_state[disk_id].last_sector - sector )/20;
        hardware_stats.disk_reads[disk_id]++;
        hardware_stats.time_disk_busy[disk_id] 
                        += access_time - current_simulation_time;
        if ( DO_DEVICE_DEBUG )
        {
            printf( "------ BEGIN DO_DEVICE DEBUG - IN read_disk ------------- \n");
            printf( "Time now = %d:  Disk read will cause interrupt at time = %d\n",
                            current_simulation_time, access_time );
            printf( "-------- END DO_DEVICE DEBUG - ----------------------------------\n");
        }
        add_event( access_time, (INT16)(DISK_INTERRUPT + disk_id - 1),
                        (INT16)ERR_SUCCESS, &disk_state[disk_id].event_ptr );
        disk_state[disk_id].last_sector         = sector;
    }
    disk_state[disk_id].disk_in_use = TRUE;
    // printf("1. Setting %d TRUE\n", disk_id );
    charge_time_and_check_events( COST_OF_DISK_ACCESS );

}                                       /* End of hardware_read_disk    */



    /*****************************************************************

        hardware_write_disk

            This code simulates a disk write.  Actions include:
                o If not in KERNEL_MODE, then cause priv inst trap.
                o Do range check on disk_id, sector; give
                  interrupt error = ERR_BAD_PARAM if illegal.
                o If an event for this disk already exists ( the disk
                  is already busy ), then give interrupt error 
                  ERR_DISK_IN_USE.
                o Search for sector structure off of hashed value.
                o If search fails give create a sector on the
                  simulated disk.
                o Copy data from buffer to sector.
                o From disk_state information, determine how long
                  this request will take.
                o Request a future interrupt for this event.
                o Advance time and see if an interrupt has occurred.

    *****************************************************************/

void    hardware_write_disk( INT16 disk_id,INT16 sector, char *buffer_ptr )
{
    INT32       local_error;
    char        *sector_ptr;
    INT32       access_time;
    INT16       error_found;

    error_found = 0;
    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() )   {
        ZCALL( hardware_fault(PRIVILEGED_INSTRUCTION, 0));
        return;
    }

    if (   disk_id  < 1  || disk_id  >  MAX_NUMBER_OF_DISKS )
        {
        disk_id = 1;                    /* To aim at legal vector  */
        error_found = ERR_BAD_PARAM;
    }
    if ( sector < 0  || sector >= NUM_LOGICAL_SECTORS )
        error_found = ERR_BAD_PARAM;

    if ( disk_state[disk_id].disk_in_use == TRUE )
        error_found = ERR_DISK_IN_USE;


    if ( error_found != 0 )
    {
        if ( DO_DEVICE_DEBUG )
        {
            printf( "------ BEGIN DO_DEVICE DEBUG - IN write_disk ------------- \n");
            printf( "ERROR:  Something screwed up in your disk request.  The error\n");
            printf( "        code is %d that you can look up in global.h\n", error_found );
            printf( "        The disk is now going to generate an interrupt to tell \n");
            printf( "        you about that error.\n");
            printf( "-------- END DO_DEVICE DEBUG - ----------------------------------\n");
        }
        add_event( current_simulation_time, 
                   (INT16)(DISK_INTERRUPT + disk_id - 1),
                   error_found, &disk_state[disk_id].event_ptr );
    }
    else
        {
        get_sector_struct( disk_id, sector, &sector_ptr, &local_error );
        if ( local_error != 0 )   /* No structure for this sector exists */
            create_sector_struct( disk_id, sector, &sector_ptr );

        memcpy( sector_ptr, buffer_ptr, PGSIZE );

        access_time = (INT32)current_simulation_time + 100
                    + abs( disk_state[disk_id].last_sector - sector )/20;
        hardware_stats.disk_writes[disk_id]++;
        hardware_stats.time_disk_busy[disk_id] 
                        += access_time - current_simulation_time;
        if ( DO_DEVICE_DEBUG )
        {
            printf( "------ BEGIN DO_DEVICE DEBUG - IN write_disk ------------- \n");
            printf( "Time now = %d:  Disk write will cause interrupt at time = %d\n",
                            current_simulation_time, access_time );
            printf( "-------- END DO_DEVICE DEBUG - ----------------------------------\n");
        }
        add_event( access_time, (INT16)(DISK_INTERRUPT + disk_id - 1),
                        (INT16)ERR_SUCCESS, &disk_state[disk_id].event_ptr );
        disk_state[disk_id].last_sector         = sector;
    }
    disk_state[disk_id].disk_in_use = TRUE;
    // printf("2. Setting %d TRUE\n", disk_id );
    charge_time_and_check_events( COST_OF_DISK_ACCESS );

}                                       /* End of hardware_write_disk   */


    /*****************************************************************

        hardware_timer()

            This is the routine that sets up a clock to interrupt
            in the future.  Actions include:
                o If not in KERNEL_MODE, then cause priv inst trap.
                o If time is illegal, generate an interrupt immediately.
                o Purge any outstanding timer events.
                o Request a future event.
                o Advance time and see if an interrupt has occurred.

    *****************************************************************/

void    hardware_timer( INT32 time_to_delay )
{
    INT32         error;
    UINT32         CurrentTimerExpirationTime = 9999999;

    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() )  {
        ZCALL( hardware_fault(PRIVILEGED_INSTRUCTION, 0 ));
        return;
    }
    
    if ( DO_DEVICE_DEBUG )                // Print lots of info
    {
        printf( "------ BEGIN DO_DEVICE DEBUG - START TIMER --------- \n");
        if ( timer_state.timer_in_use > 0 )
            printf( "The timer is already in use - you will destroy previous timer request\n");
        else
            printf( "The timer is not currently running\n");
        if ( time_to_delay < 0 )
            printf( "TROUBLE - you are asking to delay for a negative time!!\n");
        if ( timer_state.timer_in_use > 0 )
            CurrentTimerExpirationTime = timer_state.event_ptr->time_of_event;
        if ( CurrentTimerExpirationTime < current_simulation_time + time_to_delay )
        {
            printf( "TROUBLE - you are replacing the current timer value of %d with a time of %d\n",
                CurrentTimerExpirationTime, current_simulation_time + time_to_delay );
            printf( "which is even further in the future.  This is not wise\n" );
        }
        printf( "Time Now = %d, Delaying for time = %d,  Interrupt will occur at = %d\n",
                current_simulation_time, time_to_delay,
                current_simulation_time + time_to_delay );
        printf( "-------- END DO_DEVICE DEBUG - ---------------------- \n");
    }
    if ( timer_state.timer_in_use > 0 )
        {
        dequeue_item( timer_state.event_ptr, &error );
        if ( error != 0 )
            {
            printf( "Internal error - we tried to retrieve a timer\n");
            printf( "event, but failed in hardware_timer.\n");
            z502_internal_panic ( ERR_Z502_INTERNAL_BUG );
        }
        timer_state.timer_in_use--;
    }

    if ( time_to_delay < 0 )                    /* Illegal time       */
        {
        add_event( current_simulation_time, TIMER_INTERRUPT, 
                   (INT16)ERR_BAD_PARAM, &timer_state.event_ptr );
        return;
    }

    add_event( current_simulation_time + time_to_delay,
                   TIMER_INTERRUPT, (INT16)ERR_SUCCESS, &timer_state.event_ptr );
    timer_state.timer_in_use++;
    charge_time_and_check_events( COST_OF_TIMER );

}                                       /* End of hardware_timer  */


    /*****************************************************************

        hardware_clock()

            This is the routine that makes the current simulation
            time visible to the OS502.  Actions include:
                o If not in KERNEL_MODE, then cause priv inst trap.
                o Read the simulation time from 
                  "current_simulation_time"
                o Return it to the caller.

    *****************************************************************/

void    hardware_clock( INT32   *current_time_returned )
    {
    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() ) {
        *current_time_returned = -1;    /* return bogus value      */
        ZCALL( hardware_fault( PRIVILEGED_INSTRUCTION, 0 ) );
        return;
    }

    charge_time_and_check_events( COST_OF_CLOCK );
    *current_time_returned = (INT32) current_simulation_time;

}                                       /* End of hardware_clock       */


    /*****************************************************************

        Z502_HALT()

            This is the routine that ends the simulation.
            Actions include:
                o If not in KERNEL_MODE, then cause priv inst trap.
                o Wrapup any outstanding work and terminate.

    *****************************************************************/

void    Z502_HALT( void  )     {
    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() )  {
        ZCALL( hardware_fault( PRIVILEGED_INSTRUCTION, 0 ) );
        return;
    }
    print_hardware_stats( );

    printf( "The Z502 halts execution and Ends at Time %d\n",
                  current_simulation_time );
    GoToExit(0);
}                                       /* End of Z502_HALT        */



    /*****************************************************************

        Z502_IDLE()

            This is the routine that idles the Z502 until the next
            interrupt.  Actions include:
                o If not in KERNEL_MODE, then cause priv inst trap.
                o If there's nothing to wait for, print a message 
                  and halt the machine.
                o Get the next event and cause an interrupt.

    *****************************************************************/

void    Z502_IDLE( void )
    {
    INT32       time_of_next_event;
    static INT32  NumberOfIdlesWithNothingOnEventQueue = 0;

    GetLock ( HardwareLock, "Z502_IDLE" );
    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() )  {
        ZCALL( hardware_fault( PRIVILEGED_INSTRUCTION, 0 ) );
        return;
    }

    get_next_event_time( &time_of_next_event );
    if ( DO_DEVICE_DEBUG )
    {
        printf( "------ BEGIN DO_DEVICE DEBUG - IN Z502_IDLE ----------------- \n");
        printf( "The time is now = %d: ", current_simulation_time);
        if ( time_of_next_event < 0 )
            printf( "There's no event that we're waiting for - timer isn't started\n");
        else
            printf( "Now kicking interrupt handler to handle event at time = %d\n", 
                            time_of_next_event );
        printf( "-------- END DO_DEVICE DEBUG - -------------------------------\n");
    }
    if ( time_of_next_event < 0 )
        NumberOfIdlesWithNothingOnEventQueue++;
    else
        NumberOfIdlesWithNothingOnEventQueue = 0;
    if ( NumberOfIdlesWithNothingOnEventQueue > 10 )
        {
        printf( "ERROR in Z502_IDLE.  IDLE will wait forever since\n" );
        printf( "there is no event that will cause an interrupt.\n" );
        printf( "This occurs because when you decided to call 'Z502_IDLE'\n");
        printf( "there was an event waiting - but the event was triggered\n");
        printf( "too soon. Avoid this error by:\n");
        printf( "1. using   'ZCALL' instead of CALL for all routines between\n");
        printf( "   the event-check and Z502_IDLE\n");
        printf( "2. limiting the work or routines (like printouts) between\n");
        printf( "   the event-check and Z502_IDLE\n");
        z502_internal_panic( ERR_OS502_GENERATED_BUG );
    }
    if ( ( time_of_next_event > 0 )
      && ( current_simulation_time < (UINT32)time_of_next_event ) )
        current_simulation_time = time_of_next_event;
    ReleaseLock ( HardwareLock, "Z502_IDLE" );
    SignalCondition( InterruptCondition, "Z502_IDLE" );
}                                       /* End of Z502_IDLE         */



    /*****************************************************************

        Z502_MAKE_CONTEXT()

            This is the routine that sets up a new context.
            Actions include:
                o If not in KERNEL_MODE, then cause priv inst trap.
                o Allocate a structure for a context.  The "calloc"
                  sets the contents of this memory to 0.
                o Ensure that memory was actually obtained.
                o Initialize the structure.
                o Advance time and see if an interrupt has occurred.
                o Return the structure pointer to the caller.

    *****************************************************************/

void    Z502_MAKE_CONTEXT( void       **ReturningContextPointer,
                           void       *starting_address, 
                           BOOL        user_or_kernel )
    {
    Z502CONTEXT     *our_ptr;

    GetLock ( HardwareLock, "Z502_MAKE_CONTEXT" );
    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() )
        {
        ZCALL( hardware_fault( PRIVILEGED_INSTRUCTION, 0 ) );
        return;
    }

    our_ptr      = (Z502CONTEXT *)calloc( 1, sizeof( Z502CONTEXT ) );
    if ( our_ptr == NULL )
        {
        printf( "We didn't complete the calloc in MAKE_CONTEXT.\n" );
        z502_internal_panic( ERR_OS502_GENERATED_BUG      );
    }

    our_ptr->structure_id       = CONTEXT_STRUCTURE_ID;
    our_ptr->entry              = (void *)starting_address;
    our_ptr->page_table_ptr     = NULL;
    our_ptr->page_table_len     = 0;
    our_ptr->pc                 = 0;
    our_ptr->program_mode       = user_or_kernel;
    our_ptr->fault_in_progress  = FALSE;
    *ReturningContextPointer    = (void *)our_ptr;

    charge_time_and_check_events( COST_OF_MAKE_CONTEXT );
    ReleaseLock ( HardwareLock, "Z502_MAKE_CONTEXT" );

}                                       /* End of Z502_MAKE_CONTEXT */



    /*****************************************************************

        Z502_DESTROY_CONTEXT()

            This is the routine that removes a context.
            Actions include:
                o If not in KERNEL_MODE, then cause priv inst trap.
                o Validate structure_id on context.  If bogus, return
                  fault error = ERR_ILLEGAL_ADDRESS.
                o Free the memory pointed to by the pointer.
                o Advance time and see if an interrupt has occurred.

    *****************************************************************/

void    Z502_DESTROY_CONTEXT( void **IncomingContextPointer )
{
    Z502CONTEXT **context_ptr = (Z502CONTEXT **)IncomingContextPointer;

    GetLock ( HardwareLock , "Z502_DESTROY_CONTEXT");
    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() )
        {
        ZCALL( hardware_fault( PRIVILEGED_INSTRUCTION, 0 ) );
        return;
    }

    if ( *context_ptr == Z502_CURRENT_CONTEXT )
        {
        printf( "PANIC:  Attempt to destroy context of the currently " );
        printf( "running process.\n" );
        z502_internal_panic( ERR_OS502_GENERATED_BUG );
    }

    if ( (*context_ptr)->structure_id != CONTEXT_STRUCTURE_ID )
        ZCALL( hardware_fault( CPU_ERROR, (INT16)ERR_ILLEGAL_ADDRESS ) );

    (*context_ptr)->structure_id = 0;
    free( *context_ptr );
    ReleaseLock ( HardwareLock, "Z502_DESTROY_CONTEXT" );

}                               /* End of Z502_DESTROY_CONTEXT  */




    /*****************************************************************

        Z502_SWITCH_CONTEXT()

            This is the routine that sets up a new context.
            Actions include:
                o Disable interrupts - strange things happen if we 
                  interrupt while we're scheduling.
                o If not in KERNEL_MODE, then cause priv inst trap.
                o Validate structure_id on context.  If bogus, return
                  fault error = ERR_ILLEGAL_ADDRESS.
                o Validate kill_or_save parameter.  If bogus, return
                  fault error = ERR_BAD_PARAM.
                o Save the context in a well known place.
                o Back out all CALLS by setting "POP_THE_STACK" and
                  returning; we'll come in at change_context.

        change_context()

                o Clear "POP_THE_STACK" disabling the "CALL" mechanism.
                o Get current context from Z502_CURRENT_CONTEXT.
                o Validate structure_id on context.  If bogus, panic.
                o If this is the initial process, ignore KILL/SAVE.
                o If KILL_SELF, then call DESTROY_CONTEXT.
                o If SAVE_SELF, put the registers into the context.
                o Advance time and see if an interrupt has occurred.
                o If "next_context_ptr" is null, run last process again.
                o Move stuff from new context to registers.
                o If this context took a memory fault, that fault 
                  is unresolved.  Instead of going back to the user 
                  context, try the memory reference again.
                o Call the starting address.

    *****************************************************************/

void    Z502_SWITCH_CONTEXT( BOOL kill_or_save, 
                             void **IncomingContextPointer )
{
    Z502CONTEXT **context_ptr = (Z502CONTEXT **)IncomingContextPointer;

    GetLock ( HardwareLock, "Z502_SWITCH_CONTEXT" );
    // We need to be in kernel mode or be in interrupt handler
    if ( Z502_MODE != KERNEL_MODE  && InterruptTid != GetMyTid() )
        {
        ZCALL( hardware_fault( PRIVILEGED_INSTRUCTION, 0 ) );
        return;
    }

    if ( (*context_ptr)->structure_id != CONTEXT_STRUCTURE_ID )
        ZCALL( hardware_fault( CPU_ERROR, (INT16)ERR_ILLEGAL_ADDRESS ) );

    if (   kill_or_save != SWITCH_CONTEXT_KILL_MODE
        && kill_or_save != SWITCH_CONTEXT_SAVE_MODE )
        ZCALL( hardware_fault( CPU_ERROR, (INT16)ERR_BAD_PARAM ) );

    z502_machine_kill_or_save           = kill_or_save;
    z502_machine_next_context_ptr       = *context_ptr;
    ReleaseLock ( HardwareLock, "Z502_SWITCH_CONTEXT" );
    charge_time_and_check_events( COST_OF_SWITCH_CONTEXT );
    POP_THE_STACK                       = TRUE;
}                               /* End of Z502_SWITCH_CONTEXT  */



void    change_context( )
    {
    Z502CONTEXT     *curr_ptr;
    void        (*routine)( void );

    GetLock ( HardwareLock, "change_context" );
    POP_THE_STACK = FALSE;
    curr_ptr = Z502_CURRENT_CONTEXT;
    hardware_stats.context_switches++;

    if ( Z502_CURRENT_CONTEXT != NULL )
        {
        if ( curr_ptr->structure_id != CONTEXT_STRUCTURE_ID )
            {
            printf( "CURRENT_CONTEXT is invalid in change_context\n");
            z502_internal_panic( ERR_OS502_GENERATED_BUG      );
        }
        if ( z502_machine_kill_or_save == SWITCH_CONTEXT_KILL_MODE )
            {
            curr_ptr->structure_id = 0;
            free( curr_ptr );
        }

        if ( z502_machine_kill_or_save == SWITCH_CONTEXT_SAVE_MODE )
            {
            curr_ptr->call_type         = SYS_CALL_CALL_TYPE;
            curr_ptr->arg1              = Z502_ARG1;
            curr_ptr->arg2              = Z502_ARG2;
            curr_ptr->arg3              = Z502_ARG3;
            curr_ptr->arg4              = Z502_ARG4;
            curr_ptr->arg5              = Z502_ARG5;
            curr_ptr->arg6              = Z502_ARG6;
            curr_ptr->reg1              = Z502_REG_1;
            curr_ptr->reg2              = Z502_REG_2;
            curr_ptr->reg3              = Z502_REG_3;
            curr_ptr->reg4              = Z502_REG_4;
            curr_ptr->reg5              = Z502_REG_5;
            curr_ptr->reg6              = Z502_REG_6;
            curr_ptr->reg7              = Z502_REG_7;
            curr_ptr->reg8              = Z502_REG_8;
            curr_ptr->reg9              = Z502_REG_9;
            curr_ptr->page_table_ptr    = Z502_PAGE_TBL_ADDR;
            curr_ptr->page_table_len    = Z502_PAGE_TBL_LENGTH;
            curr_ptr->pc                = Z502_PROGRAM_COUNTER;
        }
    }

    curr_ptr = z502_machine_next_context_ptr;
    z502_machine_kill_or_save = SWITCH_CONTEXT_SAVE_MODE;
    if ( curr_ptr == NULL )
        curr_ptr = Z502_CURRENT_CONTEXT;
    if ( curr_ptr->structure_id != CONTEXT_STRUCTURE_ID )
        {
        printf( "CURRENT_CONTEXT is invalid in change_context\n");
        z502_internal_panic( ERR_OS502_GENERATED_BUG  );
    }
    if ( InterruptTid == GetMyTid() )   // Are we running on hardware thread
        {
        printf( "Trying to switch context while at interrupt level > 0.\n");
        printf( "This is NOT advisable and will lead to strange results.\n");
    }

    Z502_CURRENT_CONTEXT        = curr_ptr;
    Z502_PAGE_TBL_ADDR          = curr_ptr->page_table_ptr;
    Z502_PAGE_TBL_LENGTH        = curr_ptr->page_table_len;
    Z502_PROGRAM_COUNTER        = curr_ptr->pc;
    Z502_MODE                   = curr_ptr->program_mode;
    Z502_REG_1                  = curr_ptr->reg1;
    Z502_REG_2                  = curr_ptr->reg2;
    Z502_REG_3                  = curr_ptr->reg3;
    Z502_REG_4                  = curr_ptr->reg4;
    Z502_REG_5                  = curr_ptr->reg5;
    Z502_REG_6                  = curr_ptr->reg6;
    Z502_REG_7                  = curr_ptr->reg7;
    Z502_REG_8                  = curr_ptr->reg8;
    Z502_REG_9                  = curr_ptr->reg9;

    Z502_ARG1                   = curr_ptr->arg1; 
    Z502_ARG2                   = curr_ptr->arg2; 
    Z502_ARG3                   = curr_ptr->arg3; 
    Z502_ARG4                   = curr_ptr->arg4; 
    Z502_ARG5                   = curr_ptr->arg5; 
    Z502_ARG6                   = curr_ptr->arg6; 

    /*  If this context took a memory fault, that fault is unresolved.
        Instead of going back to the user context, try the memory
        reference again.  We do that by simply going back to base
        level where all the memory references are done anyway.      */

    if ( curr_ptr->fault_in_progress == TRUE )
        {
        SYS_CALL_CALL_TYPE          = curr_ptr->call_type;
        // ReleaseLock ( HardwareLock );       // 12/8/05  lock not released on exit
        return;
    }

    /* We're now running the new context - return to the OS for any
       work to be done before going to the user program.            */

    Z502_MODE               = KERNEL_MODE;
    os_switch_context_complete( );
    Z502_MODE               = curr_ptr->program_mode;

    SYS_CALL_CALL_TYPE = -1;            /* Invalidate it            */
    routine                    = (void (*)(void))curr_ptr->entry;
    ReleaseLock ( HardwareLock, "change_context" );

    // Allow interrupts to occur since scheduling is done
    (*routine)();

    /*  If the sys call type is invalid, the user program simply
        returned.  This isn't allowed; panic.  It is legal, however
        for a process in KERNEL_MODE to simply return.             */

    if ( SYS_CALL_CALL_TYPE == -1 && ( Z502_MODE == USER_MODE ) )
        {
        printf("User program did a simple return; use \n" );
        printf("proper system calls only.\n" );
        z502_internal_panic( ERR_OS502_GENERATED_BUG  );
    }
    
}                               /* End of change_context           */


/*****************************************************************

    charge_time_and_check_events()

    This is the routine that will increment the simulation 
    clock and then check that no event has occurred.
    Actions include:
        o Increment the clock.
        o IF interrupts are masked, don't even think about 
          trying to do an interrupt.
        o If interrupts are NOT masked, determine if an interrupt
          should occur.  If so, then signal the interrupt thread.

    ******************************************************************/

void    charge_time_and_check_events( INT32 time_to_charge )
{
    INT32       time_of_next_event;

    current_simulation_time += time_to_charge;
    hardware_stats.number_charge_times++;

    //printf( "Charge_Time... -- current time = %ld\n", current_simulation_time );
    get_next_event_time( &time_of_next_event );
    if (  time_of_next_event > 0 && 
          time_of_next_event <= (INT32)current_simulation_time )
    {
            SignalCondition( InterruptCondition, "Charge_Time" );
    }
}                       /* End of charge_time_and_check_events      */

    /*****************************************************************

        hardware_interrupt()

    NOTE:  This code runs as a separate thread.
    This is the routine that will cause the hardware interrupt
    and will call OS502.      Actions include:
            o Wait for a signal from base level.
            o Get the next event - we expect the time has expired, but if
              it hasn't do nothing.
            o If it's a device, show that the device is no longer busy.
            o Set up registers which user interrupt handler will see.
            o Call the interrupt handler.

        Simply return if no event can be found.
    *****************************************************************/

void    hardware_interrupt( void  )
    {
    INT32       time_of_event;
    INT32       index;
    INT16       event_type;
    INT16       event_error;
    INT32       local_error;
    INT32       TimeToWaitForCondition = 30;     // Millisecs before Condition will go off
    void        (*interrupt_handler)( void );

    InterruptTid = GetMyTid();
    while( TRUE )
    {
        get_next_event_time(&time_of_event);
        while ( time_of_event < 0 || time_of_event > (INT32)current_simulation_time )
        {
            GetLock( InterruptLock, "hardware_interrupt-1" );
            WaitForCondition( InterruptCondition, InterruptLock, TimeToWaitForCondition );
            ReleaseLock( InterruptLock, "hardware_interrupt-1" );
            get_next_event_time( &time_of_event );
            // PrintEventQueue( );
#ifdef DEBUG_CONDITION
            printf( "Hardware_Interrupt: time = %d: next event = %d\n",
                            current_simulation_time, time_of_event );
#endif
        }

        // We got here because there IS an event that needs servicing.

        GetLock ( HardwareLock , "hardware_interrupt-2");
        NumberOfInterruptsStarted++;
        get_next_ordered_event(&time_of_event, &event_type, 
                               &event_error, &local_error);
        if ( local_error != 0 )
        {
            printf( "In hardware_interrupt we expected to find an event\n");
            printf( "Something in the OS has destroyed this location.\n");
            z502_internal_panic( ERR_OS502_GENERATED_BUG      );
        }

        if (   event_type >= DISK_INTERRUPT 
            && event_type <= DISK_INTERRUPT + MAX_NUMBER_OF_DISKS - 1 )
            {
            /* Note - if we get a disk error, we simply enqueued an event
               and incremented (hopefully momentarily) the disk_in_use value */
            if( disk_state[event_type - DISK_INTERRUPT+1].disk_in_use == FALSE )
                {
                printf( "False interrupt - the Z502 got an interrupt from a\n");
                printf( "DISK - but that disk wasn't in use.\n" );
                z502_internal_panic( ERR_Z502_INTERNAL_BUG );
            }

            //  NOTE:  Here when we take a disk interrupt, we clear the busy of ALL
            //  disks because we assume that the user will handle all of them with
            //  the interrupt that's about to be done.
            for ( index = DISK_INTERRUPT; index <= DISK_INTERRUPT + MAX_NUMBER_OF_DISKS - 1; index++ )
            {
                if ( STAT_VECTOR[SV_ACTIVE][ index ] != 0 )
                {
			// Bugfix 08/2012 - disk_state contains MAX_NUMBER_OF_DISKS elements
			// We were spraying some unknown memory locations
                    disk_state[index - DISK_INTERRUPT].disk_in_use = FALSE;
                    // printf("3. Setting %d FALSE\n", index );
                }
            }
            //  We MAYBE should be clearing all these as well - and not just the current one.

            disk_state[event_type - DISK_INTERRUPT+1].disk_in_use   = FALSE;
            // printf("3. Setting %d FALSE\n", event_type );
            disk_state[event_type - DISK_INTERRUPT+1].event_ptr   = NULL;
        }
        if ( event_type == TIMER_INTERRUPT && event_error == ERR_SUCCESS )
            {
            if( timer_state.timer_in_use <= 0 )
                {
                printf( "False interrupt - the Z502 got an interrupt from a\n");
                printf( "TIMER - but that timer wasn't in use.\n" );
                z502_internal_panic( ERR_Z502_INTERNAL_BUG );
            }
            timer_state.timer_in_use--;
            timer_state.event_ptr           = NULL;
        }

        /*  NOTE: The hardware clears these in main, but not after that     */
        STAT_VECTOR[SV_ACTIVE][ event_type ] = 1;
        STAT_VECTOR[SV_VALUE][ event_type ]  = event_error;

        if ( DO_DEVICE_DEBUG )
        {
            printf( "------ BEGIN DO_DEVICE DEBUG - CALLING INTERRUPT HANDLER --------- \n");
            printf( "The time is now = %d: Handling event that was scheduled to happen at = %d\n",
                            current_simulation_time, time_of_event );
            printf( "The hardware is now about to enter your interrupt_handler in base.c\n");
            printf( "-------- END DO_DEVICE DEBUG - ---------------------- \n");
        }

        /*  If we've come here from Z502_IDLE, then the current time may be 
            less than the event time. Then we must increase the 
            current_simulation_time to match the time given by the event.  */
/*
        if ( ( INT32 )current_simulation_time < time_of_event )
            current_simulation_time              = time_of_event;
*/
        if ( Z502_CURRENT_CONTEXT->structure_id != CONTEXT_STRUCTURE_ID )
        {
            printf( "Z502_REG_CURRENT_CONTEXT is invalid in hard_interrupt\n");
            printf( "Something in the OS has destroyed this location.\n");
            z502_internal_panic( ERR_OS502_GENERATED_BUG      );
        }
        ReleaseLock( HardwareLock, "hardware_interrupt-2" );

        interrupt_handler = (void (*)(void))TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR];
        (*interrupt_handler)();

        /* Here we clean up after returning from the user's interrupt handler */

        GetLock ( HardwareLock , "hardware_interrupt-3");   // I think this is needed
        if ( Z502_CURRENT_CONTEXT->structure_id != CONTEXT_STRUCTURE_ID )
            {
            printf( "Z502_REG_CURRENT_CONTEXT is invalid in hard_interrupt\n");
            printf( "Something in the OS has destroyed this location.\n");
            z502_internal_panic( ERR_OS502_GENERATED_BUG      );
        }

        ReleaseLock( HardwareLock, "hardware_interrupt-3" );
        NumberOfInterruptsCompleted++;
    }                            /* End of while TRUE           */
}                               /* End of hardware_interrupt   */


    /*****************************************************************

        hardware_fault()

            This is the routine that will cause the hardware fault
            and will call OS502.
            Actions include:
                o Set up the registers which the fault handler 
                  will see.
                o Call the fault_handler.

    *****************************************************************/

void    hardware_fault( INT16 fault_type, INT16 argument )
    
    {
    void        (*fault_handler)( void );

    STAT_VECTOR[SV_ACTIVE][ fault_type ] = 1;
    STAT_VECTOR[SV_VALUE][ fault_type ]  = (INT16)argument;
    Z502_MODE = KERNEL_MODE;
    hardware_stats.number_faults++;
    fault_handler = 
              ( void (*)(void))TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR];

    //  We're about to get out of the hardware - release the lock
    // ReleaseLock( HardwareLock );
    ZCALL( (*fault_handler)() );
    // GetLock( HardwareLock, "hardware_fault" );
    if ( Z502_CURRENT_CONTEXT->structure_id != CONTEXT_STRUCTURE_ID )
        {
        printf( "Z502_REG_CURRENT_CONTEXT is invalid in hardware_fault\n");
        printf( "Something in the OS has destroyed this location.\n");
        z502_internal_panic( ERR_OS502_GENERATED_BUG      );
    }
    Z502_MODE = Z502_CURRENT_CONTEXT->program_mode;
}                                       /* End of hardware_fault        */




    /*****************************************************************

        software_trap()

            This is the routine that will cause the software trap
            and will call OS502.
            Actions include:
                o Set up the registers which the OS trap handler 
                  will see.
                o Call the trap_handler.

    *****************************************************************/

void    software_trap( void )
    {
    void        (*trap_handler)( void );
    // INT32       time_of_next_event;

    Z502_MODE = KERNEL_MODE;
    charge_time_and_check_events( COST_OF_SOFTWARE_TRAP );
    /*
    current_simulation_time += COST_OF_SOFTWARE_TRAP;
    get_next_event_time( &time_of_next_event );
    if ( (  time_of_next_event > 0 )
       &&(  time_of_next_event <= (INT32)current_simulation_time ))
    {
        SignalCondition( InterruptCondition, "SoftwareTrap" );
    }
*/
//    STAT_VECTOR[SV_ACTIVE][ SOFTWARE_TRAP ] = 1;
//    STAT_VECTOR[SV_VALUE][ SOFTWARE_TRAP ]  = (INT16)SYS_CALL_CALL_TYPE;
    trap_handler = (void (*)(void))TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR];
    (*trap_handler)();
    STAT_VECTOR[SV_ACTIVE][ SOFTWARE_TRAP ] = 0;
    STAT_VECTOR[SV_VALUE][ SOFTWARE_TRAP ]  = 0;
    POP_THE_STACK = TRUE;
}                                       /* End of software_trap    */



    /*****************************************************************

        panic()

            When we come here, it's all over.  The hardware will come
            to a grinding halt.
            Actions include:
                o Print out who caused the problem.
                o Get out of the simulation.

    *****************************************************************/

void    z502_internal_panic( INT32 panic_type )

    {
    if ( panic_type == ERR_Z502_INTERNAL_BUG )
        printf( "PANIC: Occurred because of bug in simulator.\n");
    if ( panic_type == ERR_OS502_GENERATED_BUG )
        printf( "PANIC: Because OS502 used hardware wrong.\n");
    print_ring_buffer();
    GoToExit(0);
}                                       /* End of panic          */


    /*****************************************************************

        add_event()

            This is the routine that will add an event to the queue.
            Actions include:
                o Do lots of sanity checks.
                o Allocate a structure for the event.
                o Fill in the structure.
                o Enqueue it.
            Store data in ring buffer for possible debugging.

    *****************************************************************/

void    add_event( INT32   time_of_event, 
                   INT16   event_type, 
                   INT16   event_error, 
                   EVENT **returned_event_ptr ) 
    {
    EVENT       *ep;
    EVENT       *temp_ptr;
    EVENT       *last_ptr;
    INT16       erbi;        /* Short for event_ring_buffer_index    */

    if ( time_of_event < ( INT32 )current_simulation_time )     {
        printf( "time_of_event < current_sim.._time in add_event\n" );
        printf( "time_of_event = %d,  current_simulation_time = %d\n",
                             time_of_event, current_simulation_time );
        z502_internal_panic( ERR_Z502_INTERNAL_BUG );
    }
    if (   event_type < 0 || event_type > LARGEST_STAT_VECTOR_INDEX )     {
        printf( "Illegal event_type= %d  in add_event.\n", event_type );
        z502_internal_panic( ERR_Z502_INTERNAL_BUG );
    }
    ep = ( EVENT *)malloc ( sizeof( EVENT ) );
    if ( ep == NULL )     {
        printf( "We didn't complete the malloc in add_event.\n" );
        z502_internal_panic( ERR_OS502_GENERATED_BUG      );
    }
    GetLock( EventLock, "add_event" );

    ep->queue                = (INT32 *)NULL;
    ep->time_of_event        = time_of_event;
    ep->ring_buffer_location = event_ring_buffer_index;
    ep->structure_id         = EVENT_STRUCTURE_ID;
    ep->event_type           = event_type;
    ep->event_error          = event_error;
    *returned_event_ptr      = ep; 

    
    erbi = event_ring_buffer_index;
    event_ring_buffer[erbi].time_of_request             = current_simulation_time;
    event_ring_buffer[erbi].expected_time_of_event      = time_of_event;
    event_ring_buffer[erbi].real_time_of_event          = -1;
    event_ring_buffer[erbi].event_type                  = event_type;
    event_ring_buffer[erbi].event_error                 = event_error;
    event_ring_buffer_index  = (++erbi) % EVENT_RING_BUFFER_SIZE;
    
    temp_ptr                = &event_queue;     /* The header queue  */
    temp_ptr->time_of_event = -1;
    last_ptr                = temp_ptr;

    while(1)
        {
        if ( temp_ptr->time_of_event > time_of_event ) /* we're past */
            {
            ep->queue           = last_ptr->queue;
            last_ptr->queue     = (INT32 *)ep;
            break;
        }
        if ( temp_ptr->queue == NULL )
            {
            temp_ptr->queue = (INT32 *)ep;
            break;
        }
        last_ptr        = temp_ptr;
        temp_ptr        = ( EVENT *)temp_ptr->queue;
    }                                   /* End of while     */
    if ( ReleaseLock( EventLock, "add_event" ) == FALSE )
        printf( "Took error on ReleaseLock in add_event\n");
    // PrintEventQueue();
    if ( (  time_of_event > 0 )
       &&(  time_of_event <= (INT32)current_simulation_time ))
    {
        // Bugfix 09/2011 - There are situations where the hardware lock
        // is held in mem_common - but we need to release it so that
        // we can do the signal and the interrupt thread will not be
        // stuck when it tries to get the lock.
        // Here we try to get the lock.  After we try, we will now
        //   hold the lock so we can then release it.
        GetTryLock( HardwareLock );
        if ( ReleaseLock( HardwareLock, "AddEvent" ) == FALSE )
            printf( "Took error on ReleaseLock in add_event\n");
        SignalCondition( InterruptCondition, "AddEvent" );
    }
    return;
}                                       /* End of add_event            */


    /*****************************************************************

      get_next_ordered_event()

            This is the routine that will remove an event from
            the queue.  Actions include:
                o Gets the next item from the event queue.
                o Fills in the return arguments.
                o Frees the structure.
      We come here only when we KNOW time is past.  We take an error
      if there's nothing on the queue.
    *****************************************************************/

void    get_next_ordered_event( INT32   *time_of_event, 
                                INT16   *event_type, 
                                INT16   *event_error, 
                                INT32   *local_error ) 

    {
    EVENT       *ep;
    INT16       rbl;        /* Ring Buffer Location                */

    GetLock( EventLock, "get_next_ordered_ev" );
    if (event_queue.queue == NULL )
        {
        *local_error = ERR_Z502_INTERNAL_BUG;
        if ( ReleaseLock( EventLock, "get_next_ordered_ev" ) == FALSE )
            printf( "Took error on ReleaseLock in get_next_ordered_event\n");
        return;
    }
    ep                  = (EVENT *)event_queue.queue;
    event_queue.queue   = ep->queue;
    ep->queue           = NULL;

    if ( ep->structure_id != EVENT_STRUCTURE_ID )
        {
        printf( "Bad structure id read in get_next_ordered_event.\n" );
        z502_internal_panic( ERR_Z502_INTERNAL_BUG );
    }
    *time_of_event      = ep->time_of_event;
    *event_type         = ep->event_type;
    *event_error        = ep->event_error;
    *local_error        = ERR_SUCCESS;
    rbl                 = ep->ring_buffer_location;

    if ( event_ring_buffer[rbl].expected_time_of_event == *time_of_event )
                event_ring_buffer[rbl].real_time_of_event =current_simulation_time;
//        else
//                printf( "XXX %d %d\n", current_simulation_time, *time_of_event );

    if ( ReleaseLock( EventLock, "get_next_ordered_ev" ) == FALSE )
        printf( "Took error on ReleaseLock in get_next_ordered_event\n");
    ep->structure_id    = 0;            /* make sure this isn't mistaken */
    free ( ep );

}                       /* End of get_next_ordered_event            */

    /*****************************************************************

      PrintEventQueue()

      Print out the times that are on the event Q.
    *****************************************************************/

void    PrintEventQueue( )
    {
    EVENT       *ep;

    GetLock( EventLock, "PrintEventQueue" );
    printf( "Event Queue: ");
    ep                  = (EVENT *)event_queue.queue;
    while (ep != NULL )  {
        printf( "  %d", ep->time_of_event );
        ep = (EVENT *)ep->queue;
    }
    printf( "  NULL\n");
    ReleaseLock( EventLock, "PrintEventQueue" );
    return;
}                       /* End of PrintEventQueue            */



    /*****************************************************************

        dequeue_item()

            Deque a specified item from the event queue.
            Actions include:
                o Start at the head of the queue.
                o Hunt along until we find a matching identifier.
                o Dequeue it.
                o Return the pointer to the structure to the caller.

            error not 0 means the event wasn't found;
    *****************************************************************/

void    dequeue_item( EVENT   *event_ptr, INT32  *error )

    {
    EVENT               *last_ptr;
    EVENT               *temp_ptr;

    if ( event_ptr->structure_id != EVENT_STRUCTURE_ID )
        {
        printf( "Bad structure id read in dequeue_item.\n" );
        z502_internal_panic( ERR_Z502_INTERNAL_BUG );
    }

    GetLock( EventLock, "dequeue_item" );
    *error      = 0;
    temp_ptr    = (EVENT *)event_queue.queue;
    last_ptr    = &event_queue;
    while (1)
        {
        if ( temp_ptr == NULL )
            {
            *error = 1;
            break;
        }
        if ( temp_ptr == event_ptr )
            {
            last_ptr->queue = temp_ptr->queue;
            event_ptr->queue= (INT32 *)NULL;
            break;
        }
        last_ptr = temp_ptr;
        temp_ptr = (EVENT *)temp_ptr->queue;
    }                                   /* End while                */ 
    if ( ReleaseLock( EventLock, "dequeue_item" ) == FALSE )
        printf( "Took error on ReleaseLock in dequeue_item\n");
}                                       /* End   dequeue_item       */ 


    /*****************************************************************

        get_next_event_time()

            Look in the event queue.  Don't dequeue anything,
            but just read the time of the first event.

            return a -1 if there's nothing on the queue 
            - the caller must check for this.
    *****************************************************************/

void    get_next_event_time( INT32   *time_of_next_event )

    {
    EVENT               *ep;

    GetLock( EventLock, "get_next_event_time" );
    *time_of_next_event = -1;
    if ( event_queue.queue == NULL )
    {
        if ( ReleaseLock(EventLock,"get_next_event_time") == FALSE )
            printf( "Took error on ReleaseLock in get_next_event_time\n");
        return;
    }
    ep = ( EVENT *)event_queue.queue;
    if ( ep->structure_id != EVENT_STRUCTURE_ID )
        {
        printf( "Bad structure id read in get_next_event_time.\n" );
        z502_internal_panic( ERR_Z502_INTERNAL_BUG );
    }
    *time_of_next_event = ep->time_of_event;
    if ( ReleaseLock(EventLock,"get_next_event_time") == FALSE )
        printf( "Took error on ReleaseLock in get_next_event_time\n");
}                               /* End of get_next_event_time       */

    /*****************************************************************

        print_hardware_stats()

    This routine is called when the simulation halts.  It prints out
        the various usages of the hardware that have occurred during the
        simulation.
    *****************************************************************/

void    print_hardware_stats( void )
        {
        INT32   i, temp;
        double  util;                                   /* This is in range 0 - 1       */

        printf( "Hardware Statistics during the Simulation\n");
        for ( i = 0; i < MAX_NUMBER_OF_DISKS; i++ )
                {
                temp = hardware_stats.disk_reads[i] 
                         + hardware_stats.disk_writes[i];
                if ( temp > 0 )
                        {
                        printf( "Disk %2d: Disk Reads = %5d: Disk Writes = %5d: ",
                                i, hardware_stats.disk_reads[i], 
                                hardware_stats.disk_writes[i] );
                        util = (double)hardware_stats.time_disk_busy[i] / 
                                        (double)current_simulation_time;
                        printf( "Disk Utilization = %6.3f\n", util );
                }
        }
        if ( hardware_stats.number_faults > 0 )
                printf( "Faults = %5d:  ", hardware_stats.number_faults );
        if ( hardware_stats.context_switches > 0 )
                printf( "Context Switches = %5d:  ", hardware_stats.context_switches );
        printf( "CALLS = %5d:  ", hardware_stats.number_charge_times );
        printf( "Masks = %5d\n", hardware_stats.number_mask_set_seen );

}                            /* End of print_hardware_stats          */
    /*****************************************************************

        print_ring_buffer()

            This is called by the panic code to print out what's
            been happening with events and interrupts.
    *****************************************************************/

void    print_ring_buffer( void )
    {
    INT16       index;
    INT16       next_print;

    if ( event_ring_buffer[0].time_of_request == 0 )
        return;                         /* Never used - ignore       */
    GetLock( EventLock, "print_ring_buffer" );
    next_print = event_ring_buffer_index;

    printf( "Current time is %d\n\n", current_simulation_time );
    printf( "Record of Hardware Requests:\n\n" );
        printf( "This is a history of the last events that were requested.  It\n");
        printf( "is a ring buffer so note that the times are not in order.\n\n");
        printf( "Column A: Time at which the OS made a request of the hardware.\n");
        printf( "Column B: Time at which the hardware was expected to give an\n");
        printf( "          interrupt.\n");
        printf( "Column C: The actual time at which the hardware caused an\n");
        printf( "          interrupt.  This should be the same or later than \n");
        printf( "          Column B.  If this number is -1, then the event \n");
        printf( "          occurred because the request was superceded by a \n");
        printf( "          later event.\n");
        printf( "Column D: Device Type.  4 = Timer, 5... are disks \n");
        printf( "Column E: Device Status.  0 indicates no error.  You should\n");
        printf( "          worry about anything that's not 0.\n\n");
        printf( "Column A    Column B      Column C      Column D   Column E\n\n");

    for ( index = 0; index < EVENT_RING_BUFFER_SIZE; index++ )
        {
        next_print++;
        next_print = next_print % EVENT_RING_BUFFER_SIZE;

        printf( "%7d    %7d    %8d     %8d    %8d\n", 
                event_ring_buffer[next_print].time_of_request,
                event_ring_buffer[next_print].expected_time_of_event,
                event_ring_buffer[next_print].real_time_of_event,
                event_ring_buffer[next_print].event_type,
                event_ring_buffer[next_print].event_error );
    }
    if ( ReleaseLock(EventLock,"print_ring_buffer") == FALSE )
        printf( "Took error on ReleaseLock in print_ring_buffer\n");
}                               /* End of print_ring_buffer         */


    /*****************************************************************

        get_sector_struct()

    Determine if the requested sector exists, and if so hand back the 
    location in memory where we've stashed data for this sector.

    Actions include:
        o Hunt along the sector data until we find the
          appropriate sector.
        o Return the address of the sector data.

    Error not 0 means the structure wasn't found.  This means that
    noone has used this particular sector before and thus it hasn't
    been written to.
    *****************************************************************/

void    get_sector_struct( INT16         disk_id, 
                           INT16         sector, 
                           char          **sector_ptr, 
                           INT32         *error )
    {
    SECTOR              *temp_ptr;

    *error      = 0;
    temp_ptr    = (SECTOR *)sector_queue[disk_id].queue;
    while (1)
        {
        if ( temp_ptr == NULL )
            {
            *error = 1;
            break;
        }

        if ( temp_ptr->structure_id != SECTOR_STRUCTURE_ID )
            {
            printf( "Bad structure id read in get_sector_structure.\n" );
            z502_internal_panic( ERR_Z502_INTERNAL_BUG );
        }

        if ( temp_ptr->sector   == sector )
            {
            *sector_ptr = (temp_ptr->sector_data);
            break;
        }
        temp_ptr = (SECTOR *)temp_ptr->queue;
    }                                   /* End while                */ 
}                                       /* End get_sector_struct*/ 

    /*****************************************************************

        create_sector_struct()

    This is the routine that will create a sector structure and add it 
    to the list of valid sectors.

    Actions include:
                o Allocate a structure for the event.
                o Fill in the structure.
                o Enqueue it.
                o Pass back the pointer to the sector data.

    WARNING: NO CHECK is made to ensure a structure for this sector 
    doesn't exist.  The assumption is that the caller has previously 
    failed a call to get_sector_struct.
    *****************************************************************/

void    create_sector_struct( INT16   disk_id, 
                              INT16   sector, 
                              char    **returned_sector_ptr )
    {
    SECTOR              *ssp;

    ssp = ( SECTOR *)malloc ( sizeof( SECTOR ) );
    if ( ssp == NULL )
        {
        printf( "We didn't complete the malloc in create_sector_struct.\n" );
        printf( "A malloc returned with a NULL pointer.\n" );
        z502_internal_panic( ERR_OS502_GENERATED_BUG      );
    }
    ssp->structure_id   = SECTOR_STRUCTURE_ID;
    ssp->disk_id        = disk_id;
    ssp->sector         = sector;
    *returned_sector_ptr = (ssp->sector_data); 

    /* Enqueue this new structure on the HEAD of the queue for this disk   */
    ssp->queue                   = sector_queue[disk_id].queue;   
    sector_queue[disk_id].queue  = (INT32 *)ssp;

}                       /* End of create_sector_struct              */



/**************************************************************************
 **************************************************************************
                     THREADS AND LOCKS MANAGER
    What follows is a series of routines that manage the threads and
    synchronization for both Windows and LINUX.

    GetLock     - Can handle locks for either Windows or LINUX.
                     Assumes the lock has been established by the caller.
    ReleaseLock - Can handle locks for either Windows or LINUX.
                     Assumes the lock has been established by the caller.

 **************************************************************************
**************************************************************************/


/**************************************************************************
           CreateAThread
    There are Linux and Windows dependencies here.  Set up the threads
    for the two systems.
    We return the Thread Handle to the caller.
**************************************************************************/

int    CreateAThread( void *ThreadStartAddress, INT32 *data )
{
#ifdef  NT
    DWORD       ThreadID;
    HANDLE      ThreadHandle;
    if ( (ThreadHandle = CreateThread( NULL,       0,
                 (LPTHREAD_START_ROUTINE) ThreadStartAddress,
                 (LPVOID) data, (DWORD) 0, &ThreadID ) ) == NULL )  
    {
        printf( "Unable to create thread in CreateAThread\n" );
        GoToExit(0);
    }
    return( (int)ThreadHandle );
#endif

#if defined LINUX || defined MAC
    int                  ReturnCode;
    // int                  SchedPolicy = SCHED_FIFO;   // not used
    int                  policy;
    struct sched_param   param;
    pthread_t            Thread;
    pthread_attr_t       Attribute;

    ReturnCode = pthread_attr_init( &Attribute );
    if ( ReturnCode != FALSE )
        printf( "Error in pthread_attr_init in CreateAThread\n" );
    ReturnCode = pthread_attr_setdetachstate( &Attribute, PTHREAD_CREATE_JOINABLE );
    if ( ReturnCode != FALSE )
        printf( "Error in pthread_attr_setdetachstate in CreateAThread\n" );
    ReturnCode = pthread_create( &Thread, &Attribute, ThreadStartAddress, data );
    if ( ReturnCode == EINVAL )                        /* Will return 0 if successful */
        printf( "ERROR doing pthread_create - The Thread, attr or sched param is wrong\n");
    if ( ReturnCode == EAGAIN )                        /* Will return 0 if successful */
        printf( "ERROR doing pthread_create - Resources not available\n");
    if ( ReturnCode == EPERM )                        /* Will return 0 if successful */
        printf( "ERROR doing pthread_create - No privileges to do this sched type & prior.\n");

    ReturnCode = pthread_attr_destroy( &Attribute );
    if ( ReturnCode )                                    /* Will return 0 if successful */
        printf( "Error in pthread_mutexattr_destroy in CreateAThread\n" );
    ReturnCode = pthread_getschedparam( Thread, &policy, &param);
    return( (int)Thread );
#endif
}                            /* End of CreateAThread */
/**************************************************************************
           DestroyThread
**************************************************************************/

void     DestroyThread( INT32  ExitCode )
{
#ifdef   NT
    ExitThread( (DWORD) ExitCode );
#endif

#if defined LINUX || defined MAC
//    struct  thr_rtn  msg;
//    strcpy( msg.out_msg, "");
//    msg.completed = ExitCode;
//    pthread_exit( (void *)msg );
#endif
}                                                /* End of DestroyThread   */

/**************************************************************************
           ChangeThreadPriority
    On LINUX, there are two kinds of priority.  The static priority 
    requires root privileges so we aren't doing that here.
    The dynamic priority changes based on various scheduling needs -
    we have modist control over the dynamic priority.
    On LINUX, the dynamic priority ranges from -20 (most favorable) to
    +20 (least favorable) so we add and subtract here appropriately.
    Again, in our lab without priviliges, this thread can only make
    itself less favorable.
    For Windows, we will be using two classifications here:
    THREAD_PRIORITY_ABOVE_NORMAL 
       Indicates 1 point above normal priority for the priority class. 
    THREAD_PRIORITY_BELOW_NORMAL 
       Indicates 1 point below normal priority for the priority class. 

**************************************************************************/

void     ChangeThreadPriority( INT32  PriorityDirection )    {
#ifdef   NT
        INT32    ReturnValue;
        HANDLE    MyThreadID;
        MyThreadID = GetCurrentThread();
        if ( PriorityDirection == MORE_FAVORABLE_PRIORITY )
            ReturnValue = (INT32)SetThreadPriority(
                          MyThreadID, THREAD_PRIORITY_ABOVE_NORMAL  );
        if ( PriorityDirection == LESS_FAVORABLE_PRIORITY )
            ReturnValue = (INT32)SetThreadPriority(
                          MyThreadID, THREAD_PRIORITY_BELOW_NORMAL  );
        if ( ReturnValue == 0 )
        {
            printf( "ERROR:  SetThreadPriority failed in ChangeThreadPriority\n");
                        HandleWindowsError();
    }

#endif
#if defined LINUX || defined MAC
    // 09/2011 - I have attempted to make the interrupt thread a higher priority
    // than the base thread but have not been successful.  It's possible to change
    // the "nice" value for the whole process, but not for individual threads.
    //int                  policy;
    //struct sched_param   param;
    //int                  CurrentPriority;
    //CurrentPriority = getpriority( PRIO_PROCESS, 0 );
    //ReturnValue = setpriority( PRIO_PROCESS, 0, CurrentPriority - PriorityDirection );
    //ReturnValue = setpriority( PRIO_PROCESS, 0, 15 );
    //CurrentPriority = getpriority( PRIO_PROCESS, 0 );
    //ReturnValue = pthread_getschedparam( GetMyTid(), &policy, &param);
    
    //if ( ReturnValue == ESRCH || ReturnValue == EINVAL || ReturnValue == EPERM )
    //    printf( "ERROR in ChangeThreadPriority - Input parameters are wrong\n");
    //if ( ReturnValue == EACCES )
    //    printf( "ERROR in ChangeThreadPriority - Not privileged to do this!!\n");

#endif
}                                   /* End of ChangeThreadPriority   */

/**************************************************************************
           GetMyTid
	   Returns the current Thread ID
**************************************************************************/
int    GetMyTid( )    {
#ifdef   NT
    return( (int)GetCurrentThreadId() );
#endif
#ifdef   LINUX
    return( (int)pthread_self() );
#endif
#ifdef   MAC
    return( (unsigned long int)pthread_self() );
#endif
}                                   // End of GetMyTid

/**************************************************************************
           BaseThread
    Returns TRUE if the caller is the base thread, 
    FALSE if not (for instance if it's the interrupt thread).
**************************************************************************/
int  BaseThread()   {
    if ( GetMyTid() == BaseTid )
        return( TRUE );
    return( FALSE );
}                                    // End of BaseThread
/**************************************************************************
           CreateLock
**************************************************************************/
void    CreateLock( INT32 *RequestedMutex )
{
    int   ErrorFound = FALSE;
#ifdef NT
    HANDLE   MemoryMutex;
      // Create with no security, no initial ownership, and no name
    if ( ( MemoryMutex = CreateMutex( NULL, FALSE, NULL)) == NULL ) 
        ErrorFound = TRUE;
    *RequestedMutex = (UINT32)MemoryMutex;
#endif
#if defined LINUX || defined MAC

    pthread_mutexattr_t  Attribute;

    ErrorFound = pthread_mutexattr_init( &Attribute );
    if ( ErrorFound != FALSE )
        printf( "Error in pthread_mutexattr_init in CreateLock\n" );
    ErrorFound = pthread_mutexattr_settype( &Attribute, PTHREAD_MUTEX_ERRORCHECK_NP );
    if ( ErrorFound != FALSE )
        printf( "Error in pthread_mutexattr_settype in CreateLock\n" );
    ErrorFound = pthread_mutex_init( &(LocalMutex[NextMutexToAllocate]), &Attribute );
    if ( ErrorFound )                                    /* Will return 0 if successful */
        printf( "Error in pthread_mutex_init in CreateLock\n" );
    ErrorFound = pthread_mutexattr_destroy( &Attribute );
    if ( ErrorFound )                                    /* Will return 0 if successful */
        printf( "Error in pthread_mutexattr_destroy in CreateLock\n" );
    *RequestedMutex = NextMutexToAllocate;
    NextMutexToAllocate++;
#endif
    if ( ErrorFound == TRUE )
    {
          printf( "We were unable to create a mutex in CreateLock\n" );
          GoToExit(0);
    }  
    PrintLockDebug( " CreLock", 0, "", *RequestedMutex, -1 );
}                               // End of CreateLock

/**************************************************************************
           GetTryLock
GetTryLock tries to get a lock.  If it is successful, it returns 1.
If it is not successful, and the lock is held by someone else, 
a 0 is returned.

The pthread_mutex_trylock() function tries to lock the specified mutex. 
If the mutex is already locked, an error is returned. 
Otherwise, this operation returns with the mutex in the locked state 
with the calling thread as its owner. 
**************************************************************************/
int    GetTryLock( UINT32 RequestedMutex )  {
    int   ReturnValue = FALSE;
    int   LockReturn;
#ifdef   NT
    HANDLE   MemoryMutex;
#endif

    PrintLockDebug( " TryLock", 1, "", RequestedMutex, -1 );
#ifdef   NT
    MemoryMutex = (HANDLE)RequestedMutex;
    LockReturn = (int)WaitForSingleObject(MemoryMutex, 1);
//      printf( "Code Returned in GetTryLock is %d\n", LockReturn );
      if ( LockReturn == WAIT_FAILED )
    {
          printf( "Internal error in GetTryLock\n");
          HandleWindowsError();
          GoToExit(0);
    }
    if ( LockReturn == WAIT_TIMEOUT )   // Timeout occurred with no lock
        ReturnValue = FALSE;
    if ( LockReturn == WAIT_OBJECT_0 )  // Lock was obtained
        ReturnValue = TRUE;
#endif
#if defined LINUX || defined MAC
    LockReturn = pthread_mutex_trylock( &(LocalMutex[RequestedMutex])  );
//    printf( "Code Returned in GetTRyLock is %d\n", LockReturn );

    if ( LockReturn == EINVAL )
        printf( "PANIC in GetTryLock - mutex isn't initialized\n");
    if ( LockReturn == EFAULT )
        printf( "PANIC in GetTryLock - illegal address for mutex\n");
    if ( LockReturn == EBUSY )      //  Already locked by another thread
        ReturnValue = FALSE;
    if ( LockReturn == EDEADLK )    //  Already locked by this thread
        ReturnValue = FALSE;
    if ( LockReturn == 0 )          //  Not previously locked - all OK
        ReturnValue = TRUE;
#endif
    PrintLockDebug( " TryLock", 1, "", RequestedMutex, ReturnValue );
    return( ReturnValue );
}                               /* End of GetTryLock     */


/**************************************************************************
           GetLock
   This routine should normally only return when the lock has been gotten.
   It should normally return TRUE - and returns FALSE only on an error.
   Note that Windows and Linux operate differently here.  Windows does
   NOT take an error if a lock attempt is made by a thread already holding
   the lock.
**************************************************************************/

int    GetLock( UINT32 RequestedMutex, char *CallingRoutine )   {
    INT32    LockReturn;
    int      ReturnValue = FALSE;
#ifdef   NT
    HANDLE   MemoryMutex = (HANDLE)RequestedMutex;
#endif
    PrintLockDebug( " GetLock", 2, CallingRoutine, RequestedMutex, -1 );
#ifdef   NT
    LockReturn = WaitForSingleObject(MemoryMutex, INFINITE);
    if ( LockReturn != 0 )  {
          printf( "Internal error waiting for a lock in GetLock\n");
          HandleWindowsError();
          GoToExit(0);
    }
    if ( LockReturn == 0 )    //  Not previously locked - all OK
        ReturnValue = TRUE;
#endif

#if defined LINUX || defined MAC
    LockReturn = pthread_mutex_lock( &(LocalMutex[RequestedMutex])  );
    if ( LockReturn == EINVAL )
        printf( "PANIC in GetLock - mutex isn't initialized\n");
    if ( LockReturn == EFAULT )
        printf( "PANIC in GetLock - illegal address for mutex\n");
    if ( LockReturn == EDEADLK )    //  Already locked by this thread
        printf( "ERROR - Already locked by this thread\n");
    if ( LockReturn == 0 )          //  Not previously locked - all OK
        ReturnValue = TRUE;
#endif
    PrintLockDebug( " GetLock", 2, CallingRoutine, RequestedMutex, ReturnValue );
    return( ReturnValue );
}                               /* End of GetLock     */

/**************************************************************************
            ReleaseLock
    If the function succeeds, the return value is TRUE.
    If the function fails, and the Mutex is NOT unlocked,
        then FALSE is returned.
**************************************************************************/
int    ReleaseLock( UINT32 RequestedMutex, char* CallingRoutine )  {
    int    ReturnValue = FALSE;
    int    LockReturn;
#ifdef   NT
    HANDLE   MemoryMutex = (HANDLE)RequestedMutex;
#endif
    PrintLockDebug( " RelLock", 3, CallingRoutine, RequestedMutex, -1 );

#ifdef   NT
    LockReturn = ReleaseMutex(MemoryMutex);

    if ( LockReturn != 0 )    // Lock was released
        ReturnValue = TRUE;
#endif
#if defined LINUX || defined MAC
    LockReturn = pthread_mutex_unlock( &(LocalMutex[RequestedMutex]) );
//    printf( "Return Code in Release Lock = %d\n", LockReturn );

    if ( LockReturn == EINVAL )
        printf( "PANIC in ReleaseLock - mutex isn't initialized\n");
    if ( LockReturn == EFAULT )
        printf( "PANIC in ReleaseLock - illegal address for mutex\n");
    if ( LockReturn == EPERM )      //  Not owned by this thread
        printf( "ERROR - Lock is not currently locked by this thread.\n");
    if ( LockReturn == 0 )          //  Successfully unlocked - all OK
        ReturnValue = TRUE;
#endif
    PrintLockDebug( " RelLock", 3, CallingRoutine, RequestedMutex, ReturnValue );
    return( ReturnValue );
}                               /* End of ReleaseLock     */
/**************************************************************************
           PrintLockDebug
    Print out message indicating what's happening with locks
**************************************************************************/
#define     LOCKING_DB_SIZE    30
typedef struct  {
    int      ThreadID[5];
    int      NumberOfLocks;
    int      LockID[LOCKING_DB_SIZE];
    int      LockCount[5][LOCKING_DB_SIZE];
} LOCKING_DB;

LOCKING_DB    LockDB;
void    PrintLockDebug( char *Text, int Action, char *LockCaller, int Mutex, int Return )
{
#ifdef  DEBUG_LOCKS
    int         MyTid, i, DBIndex, LockID;
    char        TaskID[120], WhichLock[120];
    char        Output[120], Output2[120];
    static int  FirstTime = TRUE;

    if ( FirstTime )
    {
        for ( i = 0; i < LOCKING_DB_SIZE; i++ ) {
            LockDB.LockCount[0][i] = 0;
            LockDB.LockCount[1][i] = 0;
            LockDB.LockID[i]       = 0;
         }
         LockDB.NumberOfLocks = 0;
         FirstTime = FALSE;
    }
    MyTid = GetMyTid();
    strcpy( TaskID, "Other" );
    if ( MyTid == BaseTid )  {
        strcpy( TaskID, "Base " );
        DBIndex = 0;
    }
    if ( MyTid == InterruptTid )  {
        strcpy( TaskID, "Int  " );
        DBIndex = 1;
    }
    sprintf( WhichLock, "Oth%d   ", Mutex );
    if ( Mutex == EventLock )     strcpy( WhichLock, "Event  " );
    if ( Mutex == InterruptLock ) strcpy( WhichLock, "Int    " );
    if ( Mutex == HardwareLock )  strcpy( WhichLock, "Hard   " );
    LockID = -1;
    for ( i = 0; i < LockDB.NumberOfLocks; i++ ) {
        if ( LockDB.LockID[i] == Mutex )
            LockID = i;
    }
    if ( LockID == -1 )  {
        LockDB.LockID[LockDB.NumberOfLocks] = Mutex; 
        LockDB.NumberOfLocks++;
    }
    if (  ( Return == -1 && Action == 2 ) // Going to get a lock
       || ( Return == TRUE && Action == 1 ) )
        LockDB.LockCount[DBIndex][LockID]++;
    if ( Return == -1 && Action == 3 ) // Going to release a lock
        LockDB.LockCount[DBIndex][LockID]--;
    strcpy( Output,"" );
    if ( Return == -1 )  {
        if ( Action == 2 || Action == 3 ) // Going to get a lock
                strcpy( Output, LockCaller );
        sprintf( Output2, "%s  Count = %d", Output, LockDB.LockCount[DBIndex][LockID] );
    }
    else
        sprintf( Output2, "Returned = %d   POP = %d", Return, POP_THE_STACK );
    printf( "LOCKS: %s: %s  %s   %d    %s\n", 
            Text, TaskID, WhichLock, current_simulation_time, Output2 ); 
    // Guard against a thread trying to lock an already locked thread
    // or releasing a free thread.
    if (    LockDB.LockCount[DBIndex][LockID] < 0 
         || LockDB.LockCount[DBIndex][LockID]  > 1 )
        printf( "Locks shouldn't be released or locked more than once by a thread\n");
#endif
}                                 // End of PrintLockDebug
/**************************************************************************
           CreateCondition
**************************************************************************/
void    CreateCondition( UINT32 *RequestedCondition )
{
    int     ConditionReturn;
#ifdef NT
        LocalEvent[NextConditionToAllocate] 
                = CreateEvent( 
                        NULL,     // no security attributes
                        FALSE,    // auto-reset event
                        FALSE,     // initial state is NOT signaled
                        NULL);    // object not named
        ConditionReturn = 0;
        if (LocalEvent[NextConditionToAllocate] == NULL) 
    { 
          printf( "Internal error Creating an Event in CreateCondition\n");
          HandleWindowsError();
          GoToExit(0);
    }
#endif

#if defined LINUX || defined MAC
    *RequestedCondition = -1;
    ConditionReturn 
        = pthread_cond_init( &(LocalCondition[NextConditionToAllocate]), NULL );

    if ( ConditionReturn == EAGAIN || ConditionReturn == ENOMEM )
        printf( "PANIC in CreateCondition - No System Resources\n");
    if ( ConditionReturn == EINVAL || ConditionReturn == EFAULT)
        printf( "PANIC in CreateCondition - illegal input\n");
    if ( ConditionReturn == EBUSY )      //  Already locked by another thread
        printf( "PANIC in CreateCondition - Already initialized\n");
#endif
    if ( ConditionReturn == 0 )  
    {
        *RequestedCondition = NextConditionToAllocate;
        NextConditionToAllocate++;
    }
#ifdef  DEBUG_CONDITION
    printf( "CreateCondition # %d\n", *RequestedCondition );
#endif
}                               // End of CreateCondition

/**************************************************************************
           WaitForCondition
    It is assumed that the caller enters here with the mutex locked.
    The result of this call is that the mutex is unlocked.  The caller
    doesn't return from this call until the condition is signaled.
**************************************************************************/
int    WaitForCondition( UINT32 Condition, UINT32 Mutex, INT32 WaitTime )
{
    int        ReturnValue = 0;
    int        ConditionReturn;
#ifdef DEBUG_CONDITION
    printf("WaitForCondition - Enter - %d\n", current_simulation_time );
#endif
#ifdef NT
    //ConditionReturn = (int)WaitForSingleObject(LocalEvent[ Condition ], INFINITE);
    ConditionReturn = (int)WaitForSingleObject(LocalEvent[ Condition ], WaitTime);
    if ( ConditionReturn == WAIT_FAILED )
    {
        printf( "Internal error waiting for an event in WaitForCondition\n");
        HandleWindowsError();
        GoToExit(0);
    }
    ReturnValue = 0;
    
#endif
#if defined LINUX || defined MAC
    ConditionReturn 
        = pthread_cond_wait( &(LocalCondition[Condition]), 
                             &(LocalMutex[Mutex]) );
    if ( ConditionReturn == EINVAL || ConditionReturn == EFAULT )
        printf( "In WaitForCondition, An illegal value or status was found\n");
    if ( ConditionReturn == 0 )
        ReturnValue = TRUE;          // Success
#endif
#ifdef DEBUG_CONDITION
    printf("WaitForCondition - Exit - %d %d\n", 
                 current_simulation_time, ConditionReturn );
#endif
    return( ReturnValue );
}                               // End of WaitForCondition

/**************************************************************************
           SignalCondition
    Used to wake up a thread that's waiting on a condition.
**************************************************************************/
int    SignalCondition( UINT32 Condition, char* CallingRoutine )
{
    int        ReturnValue = 0;
#ifdef NT
    static int NumberOfSignals = 0;
#endif
#if defined LINUX || defined MAC
    int        ConditionReturn;
#endif

#ifdef DEBUG_CONDITION
    printf("SignalCondition - Enter - %d %s\n", 
                 current_simulation_time, CallingRoutine );
#endif
    if ( InterruptTid == GetMyTid() )  // We don't want to signal ourselves
    {
        ReturnValue = TRUE;
        return( ReturnValue );
    }
#ifdef NT
    if ( ! SetEvent( LocalEvent[ Condition ] ) )
    {
          printf( "Internal error signalling  an event in SignalCondition\n");
          HandleWindowsError();
          GoToExit(0);
    }
    ReturnValue = TRUE;
    if ( NumberOfSignals % 3 == 0 )
        DoSleep(1);
    NumberOfSignals++;
#endif
#if defined LINUX || defined MAC

    ConditionReturn 
        = pthread_cond_signal( &(LocalCondition[Condition]) );
    if ( ConditionReturn == EINVAL || ConditionReturn == EFAULT )
        printf( "In SignalCondition, An illegal value or status was found\n");
    if ( ConditionReturn == 0 )
        ReturnValue = TRUE;          // Success
    ConditionReturn = sched_yield();
#endif
#ifdef DEBUG_CONDITION
    printf("SignalCondition - Exit - %d %s\n", 
                 current_simulation_time, CallingRoutine );
#endif
    return( ReturnValue );
}                               // End of SignalCondition

/**************************************************************************
                     DoSleep
    The argument is the sleep time in milliseconds.
**************************************************************************/

void    DoSleep( INT32 millisecs )
    {

#ifdef NT
        Sleep( millisecs );
#endif
#ifndef NT
    usleep( (unsigned long) ( millisecs * 1000 ) );
#endif
}                              // End of DoSleep
#ifdef  NT
/**************************************************************************
                     HandleWindowsError
**************************************************************************/

void    HandleWindowsError( )
    {
    LPVOID      lpMsgBuf;
    char        OutputString[256];

    FormatMessage(     FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                       FORMAT_MESSAGE_FROM_SYSTEM |     
                       FORMAT_MESSAGE_IGNORE_INSERTS,    
                       NULL, GetLastError(),
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                       (LPTSTR) &lpMsgBuf,    0,    NULL );
    sprintf( OutputString, "%s\n", (char *)lpMsgBuf );
    printf( OutputString );
}                                     // End HandleWindowsError
#endif

/**************************************************************************
                     GoToExit
    This is a good place to put a breakpoint.  It helps you find
    places where the code is diving.
**************************************************************************/
void    GoToExit( int Value )
{
    printf( "Exiting the program\n");
    exit( Value );
}



    /*****************************************************************

        main()

            This is the routine that will start running when the
            simulator is invoked.

    *****************************************************************/

int    main( int argc, char  *argv[] )
                                        /* WARNING - argc is intentially
                                           made "int" since PC's will try
                                           to make this short.          */
    {
    void        *starting_context_ptr;
    INT16       i;

    printf( "This is Simulation Version %s and Hardware Version %s.\n\n", 
            CURRENT_REL, HARDWARE_VERSION );
    event_queue.queue   = NULL;
    BaseTid = GetMyTid();
    CreateLock( &EventLock );
    CreateLock( &InterruptLock );
    CreateLock( &HardwareLock );
    CreateCondition( &InterruptCondition );
    for ( i = 1; i < MAX_NUMBER_OF_DISKS; i++ )
        {
        sector_queue[i].queue           = NULL;
        disk_state[i].last_sector       = 0;
        disk_state[i].disk_in_use       = FALSE;
        disk_state[i].event_ptr         = NULL;
        hardware_stats.disk_reads[i]    = 0;
        hardware_stats.disk_writes[i]   = 0;
        hardware_stats.time_disk_busy[i]= 0;
    }
    hardware_stats.context_switches     = 0;
    hardware_stats.number_charge_times  = 0;
    hardware_stats.number_faults        = 0;
    hardware_stats.number_mask_set_seen = 0;

    for ( i = 0; i <= LARGEST_STAT_VECTOR_INDEX; i++ )
    {
        STAT_VECTOR[SV_ACTIVE][i]       = 0;
        STAT_VECTOR[SV_VALUE ][i]       = 0;
    }
    for ( i = 0; i < MEMORY_INTERLOCK_SIZE; i++ )
        InterlockRecord[i] = -1;

    for ( i = 0; i < sizeof(MEMORY); i++ )
        MEMORY[i] = i % 256;

    CALLING_ARGC                        = ( INT32 )argc;/* make global  */
    CALLING_ARGV                        = argv;

    timer_state.timer_in_use            = 0;
    timer_state.event_ptr               = NULL;

    Z502_MODE                       = KERNEL_MODE;

    Z502_MAKE_CONTEXT( &starting_context_ptr,  
                                        ( void *)os_init, KERNEL_MODE );
    Z502_CURRENT_CONTEXT            = NULL;
    z502_machine_next_context_ptr       = starting_context_ptr;
    POP_THE_STACK                       = TRUE;

    CreateAThread( (int *)hardware_interrupt, &EventLock );
    DoSleep(100);
    ChangeThreadPriority( LESS_FAVORABLE_PRIORITY );


    while( 1 )          /* This is base level - always come here*/
        {
        /*      If popping the stack, we're just trying to get 
                back to change_context.                             */

        while ( POP_THE_STACK == TRUE )
            change_context();

        if ( SYS_CALL_CALL_TYPE == SYSNUM_MEM_READ )
            Z502_MEM_READ( Z502_ARG1.VAL, (INT32 *)Z502_ARG2.PTR );
        if ( SYS_CALL_CALL_TYPE == SYSNUM_MEM_WRITE )
            Z502_MEM_WRITE( Z502_ARG1.VAL, (INT32 *)Z502_ARG2.PTR );
        if ( SYS_CALL_CALL_TYPE == SYSNUM_READ_MODIFY )
            Z502_READ_MODIFY( Z502_ARG1.VAL, Z502_ARG2.VAL,
                              Z502_ARG3.VAL, (INT32 *)Z502_ARG4.PTR );

        if (   SYS_CALL_CALL_TYPE != SYSNUM_MEM_WRITE 
            && SYS_CALL_CALL_TYPE != SYSNUM_MEM_READ 
            && SYS_CALL_CALL_TYPE != SYSNUM_READ_MODIFY )
            software_trap();

    }                                           /* End of while(1)  */
}                                               /* End of main      */
