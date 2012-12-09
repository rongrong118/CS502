/************************************************************************

        This code forms the base of the operating system you will
        build.  It has only the barest rudiments of what you will
        eventually construct; yet it contains the interfaces that
        allow test.c and z502.c to be successfully built together.      

        Revision History:       
        1.0 August 1990
        1.1 December 1990: Portability attempted.
        1.3 July     1992: More Portability enhancements.
                           Add call to sample_code.
        1.4 December 1992: Limit (temporarily) printout in
                           interrupt handler.  More portability.
        2.0 January  2000: A number of small changes.
        2.1 May      2001: Bug fixes and clear STAT_VECTOR
        2.2 July     2002: Make code appropriate for undergrads.
                           Default program start is in test0.
        3.0 August   2004: Modified to support memory mapped IO
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking
************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"

#include             "rli_global.h"   //this header file for PCB and other structure information, and functions prototype

extern char          MEMORY[];  
extern BOOL          POP_THE_STACK;
extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;
extern INT16         Z502_PROGRAM_COUNTER;
extern INT16         Z502_INTERRUPT_MASK;
extern INT32         SYS_CALL_CALL_TYPE;
extern INT16         Z502_MODE;
extern Z502_ARG      Z502_ARG1;
extern Z502_ARG      Z502_ARG2;
extern Z502_ARG      Z502_ARG3;
extern Z502_ARG      Z502_ARG4;
extern Z502_ARG      Z502_ARG5;
extern Z502_ARG      Z502_ARG6;

extern void          *TO_VECTOR [];
extern INT32         CALLING_ARGC;
extern char          **CALLING_ARGV;


char                 *call_names[] = { "mem_read ", "mem_write",
                            "read_mod ", "get_time ", "sleep    ", 
                            "get_pid  ", "create   ", "term_proc", 
                            "suspend  ", "resume   ", "ch_prior ", 
                            "send     ", "receive  ", "disk_read",
                            "disk_wrt ", "def_sh_ar" };


PCB_str		       *readyList = NULL;   //ready queue header
PCB_str		       *timerList = NULL;   //timer queue header
PCB_str                *suspendList =NULL;  //suspend queue header

FRAME_TABLE            *pageList = NULL;      //page table header
FULL_FRAME_TABLE       *full_pageList = NULL; //full page table header

PCB_str                *diskList =NULL;     //suspend queue header

PCB_str                *ready_tail = NULL;  //ready queue tail
PCB_str                *timer_tail = NULL;  //timer queue tail
PCB_str                *suspend_tail=NULL;  //suspend queue tail

PCB_str                *disk_tail =NULL;    //suspend queue tail

PCB_str	               *current_PCB = NULL;    // this is runing PCB

PCB_str                *drop_ready_node=NULL;  //drop from the ready queue and put it in to the timer queue
PCB_str                *drop_timer_node=NULL;  //drop from the timer queue and put it in to the ready queue

BOOL                   disk_lock=0;
BOOL                   interrupt_lock=0;
int 	               gen_pid=0;     // for generate process id

int	               total_ready_pid=0;   //count number pcb in ready queue
int	               total_timer_pid=0;   //count number pcb in timer queue


INT32                  temp=0;
INT16		       bitmap[12][1600] = {0};     //disk's bitmap for keep which disk use or not 
INT32                  count=0;                    // it count the frame num
INT32                  LRU_switch;                 // switch for LRU algorithm



/* Here is switch of printer, you can turn on the printer output by set SWITCH_ON,
if you want to close the output of printer, just set value by SWITCH_OFF*/

INT32                  memory_SWITCH = SWITCH_ON;    // SWITCH_ON or SWITCH_OFF
INT32                  schedule_SWITCH = SWITCH_ON;

/************************************************************************
    INTERRUPT_HANDLER
        When the Z502 gets a hardware interrupt, it transfers control to
        this routine in the OS.
************************************************************************/
void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index=0;
    INT32              Time;
    PCB_str*           ptrcheck;

    PCB_str*           disk_node;      // node remove from disk queue to ready queue
    
    int                Temp;
    
    INT32              disk_index1=1;   // disk number, until now we only use 3 disks
    INT32              disk_index2=2;
    INT32              disk_index3=3;

    INT32              temp;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

   // printf( "Interrupt_handler: Found device ID %d with status %d\n", device_id, status );

    switch(device_id){
        // timer interuput
    	case(TIMER_IR):
                interrupt_lock = TRUE;
                ZCALL(MEM_READ(Z502ClockStatus, &Time));   // get system time
                while(timerList!=NULL)
                {
                     ptrcheck=timerList;                     
                     
                     if(Time>(ptrcheck->p_time))            //this means this node should wake up!         
                     {
                        ZCALL(lockTimer());
                        ZCALL(drop_Queue(&timerList,TIMER_Q));  //drop from time queue
                        ZCALL(unlockTimer());
                      
                        // put back in ready queue
                        ZCALL(lockReady());
                        if(schedule_SWITCH) schedule_print("READY",drop_timer_node->p_id, -1, -1);
                        ZCALL(add_to_ready_Queue(drop_timer_node));   //put back into ready queue
                        ZCALL(unlockReady());
                     }
                     else
                     break;
                }
                 interrupt_lock = FALSE;
                break;
        // disk interuput
        case(DISK_IR):     //disk 1 interrupt
               while(interrupt_lock);   // it need check interrupt lock 
               interrupt_lock = TRUE;
               while(disk_lock);        // check disk lock
               disk_lock = TRUE;
                  
                    ptrcheck = diskList;

	         while (ptrcheck != NULL){
		    if( ptrcheck->disk_io.disk_id == 1){                    

                      MEM_WRITE( Z502DiskSetID, &disk_index1);
                      MEM_READ( Z502DiskStatus, &temp);   //check the disk status

                       if ( temp == DEVICE_FREE )
                       {
                      
                       ZCALL(lockSuspend());     //drop the proper node from disk queue
                       ZCALL(disk_node=out_one_from_disk_Queue( &diskList, ptrcheck->p_id));
		       ZCALL(unlockSuspend()); 
                           
                        
                        os_disk_op(disk_node);   // begin the op of disk

                         if(disk_node->disk_io.op == 0)
                          {
                           disk_node->disk_io.flag = 1;  //disk read flag, it will use in os switch_context_complete()
                          }

                        if(schedule_SWITCH) schedule_print("READY",disk_node->p_id, -1, -1);
                        ZCALL(lockReady());
		        ZCALL(add_to_ready_Queue(disk_node));  //put back into ready queue
                        ZCALL(unlockReady());

                       }
                      break;
                     }
                       ptrcheck=ptrcheck->next;
                    }    
                  
                disk_lock = FALSE;
                interrupt_lock = FALSE;
                 break;

        case(DISK_IR+1):     //disk 2 interrupt
                while(interrupt_lock);   // it need check interrupt lock 
                interrupt_lock = TRUE;
                while(disk_lock);         // check disk lock
                disk_lock = TRUE;

                ptrcheck = diskList;

	         while (ptrcheck != NULL){
		   if( ptrcheck->disk_io.disk_id == 2){                     

                   MEM_WRITE( Z502DiskSetID, &disk_index2);
                   MEM_READ( Z502DiskStatus, &temp);       //check the disk status

                       if ( temp == DEVICE_FREE )
                       {
                       
                         ZCALL(lockSuspend());    //drop the proper node from disk queue
                         ZCALL(disk_node=out_one_from_disk_Queue( &diskList, ptrcheck->p_id));
		         ZCALL(unlockSuspend()); 

                        os_disk_op(disk_node);

                         if(disk_node->disk_io.op == 0)
                          {
                           disk_node->disk_io.flag = 1;    //disk read flag, it will use in os switch_context_complete()
                           }
                         if(schedule_SWITCH) schedule_print("READY",disk_node->p_id, -1, -1);
                         ZCALL(lockReady());
		         ZCALL(add_to_ready_Queue(disk_node));  //put back into ready queue
                         ZCALL(unlockReady());

                      }
                     break;
                     }
                       ptrcheck=ptrcheck->next;
                    }   
                 disk_lock = FALSE;
                 interrupt_lock = FALSE;
                 break;

        case(DISK_IR+2):    //disk 3 interrupt
                 while(interrupt_lock);     // it need check interrupt lock 
                 interrupt_lock = TRUE;
                 while(disk_lock);           // check disk lock 
                 disk_lock = TRUE;
                
                     ptrcheck = diskList;

	         while (ptrcheck != NULL){
		  if( ptrcheck->disk_io.disk_id == 3){                    
                  
                      MEM_WRITE( Z502DiskSetID, &disk_index3);
                      MEM_READ( Z502DiskStatus, &temp);     //check the disk status

                       if ( temp == DEVICE_FREE )
                       {
                          
                         ZCALL(lockSuspend());     //drop the proper node from disk queue
                         ZCALL(disk_node=out_one_from_disk_Queue( &diskList, ptrcheck->p_id));
		         ZCALL(unlockSuspend());    
                       
                         os_disk_op(disk_node);

                        if(disk_node->disk_io.op == 0)
                          {
                           disk_node->disk_io.flag = 1;    //disk read flag, it will use in os switch_context_complete()
                          }

                          if(schedule_SWITCH) schedule_print("READY",disk_node->p_id, -1, -1);
                          ZCALL(lockReady());
		          ZCALL(add_to_ready_Queue(disk_node));  //put back into ready queue
                          ZCALL(unlockReady());

                      }  
                     break;
                    }
                       ptrcheck=ptrcheck->next;
                   }   
                  disk_lock = FALSE;
                  interrupt_lock = FALSE;
                break;

      /*case(DISK+3):   // until now these disks is no use.
        case(DISK+4):
        case(DISK+5):
        case(DISK+6):
        case(DISK+7):
        case(DISK+8):
        case(DISK+9):
        case(DISK+10):
        case(DISK+11):*/                
    }
  
    // Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );
}                                       /* End of interrupt_handler */

/************************************************************************
    FAULT_HANDLER

        The beginning of the OS502.  Used to receive hardware faults.
************************************************************************/
void    fault_handler( void ){

    INT32       device_id;
    INT32       status;
    INT32       Index = 0;
    INT32       frame = -1;
    FRAME_TABLE	*return_page;  //victim page 
    char	DATA[PGSIZE];

    INT16       call_type=-1;
    call_type = (INT16)SYS_CALL_CALL_TYPE;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );
    
    //printf( "Fault_handler: Found vector type %d with value %d\n",device_id, status );

    switch(device_id){
        case(CPU_ERROR):
             printf("ERROR:CPU_ERROR!!"); //this is kind of CPU error
             terminate_Process ( -1,  &Index );
             break;
        case(PRIVILEGED_INSTRUCTION):  //this is kind of privileged error
             printf("ERROR:PRIVILEGED_INSTRUCTION!!");
             terminate_Process ( -1,  &Index );
             break;
        case (INVALID_MEMORY) :  //this is kind of memory invalid error
             //set page table to PCB
             Z502_PAGE_TBL_ADDR = current_PCB->pagetable;  //link with PCB 
             Z502_PAGE_TBL_LENGTH = 1024;  
              
             if(status < 0 || status > 1023){       //check the page size
                printf("ERROR: The Page Size is Illegal! Terminate the process!\n");
		CALL(Z502_HALT());
	     }

             //Get a empty frame, return the frame number
	     CALL( frame = find_empty_Frame(status) );
	   
             //frame is -1 means no empty frame now, it need swap with disk
             if(frame == -1){ 
         	CALL(return_page = find_full_Frame( ) );  //set a victim page
	        frame = return_page->frame;
	        
                //now swap with disk
		if(call_type == SYSNUM_MEM_WRITE){                       
			CALL(memory_to_disk(return_page, status));   //swap into disk
		}
		else if(call_type == SYSNUM_MEM_READ){
			CALL(memory_to_disk(return_page, status));  //swap out from disk
			CALL(disk_to_memory(return_page, status));
		}
              }

              Z502_PAGE_TBL_ADDR[status] = frame;
              Z502_PAGE_TBL_ADDR[status] |= PTBL_VALID_BIT; 

              // memory read or write system call
              if(call_type == SYSNUM_MEM_READ ){
                ZCALL( MEM_READ( (INT32) Z502_ARG1.VAL, (INT32 *)Z502_ARG2.PTR ) );
              }
              else if( call_type == SYSNUM_MEM_WRITE ){	
                ZCALL( MEM_WRITE( (INT32) Z502_ARG1.VAL, (INT32 *)Z502_ARG2.PTR ) );
              }
            
              if(memory_SWITCH) memory_print();   //memory printer output

              break;
    }
        
    // Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );
}                                       /* End of fault_handler */

/************************************************************************
    OS_SWITCH_CONTEXT_COMPLETE
        The hardware, after completing a process switch, calls this routine
        to see if the OS wants to do anything before starting the user
        process.
************************************************************************/
void    os_switch_context_complete( void )
{
    static INT16        do_print = TRUE;
    INT16   call_type;

    call_type=(INT16)SYS_CALL_CALL_TYPE;
    
    if ( do_print == TRUE )
    {
        printf( "os_switch_context_complete  called before user code.\n");
        do_print = FALSE;
    }
       
   if(current_PCB!=NULL){
      if(current_PCB->disk_io.flag)  // it means it has read disk operation in interrupt handler
        {
            memcpy((char *)Z502_ARG3.PTR,current_PCB->disk_io.buf, DISK_IO_BUF_SIZE); //copy buffer into ARG
            
            memcpy(current_PCB->disk_io.buf, "abcdeabcdabcd", DISK_IO_BUF_SIZE);  // clear buffer
            current_PCB->disk_io.op = -1;
            current_PCB->disk_io.sector = -1;
            current_PCB->disk_io.flag = 0;    //clear flag
        } 
    }
   

}                               /* End of os_switch_context_complete */

/************************************************************************
    OS_INIT
        This is the first routine called after the simulation begins.  This
        is equivalent to boot code.  All the initial OS components can be
        defined and initialized here.
************************************************************************/
void    os_init( void )
    {
    void                *next_context;
    INT32               i;
    void		*process_point;

   /* Demonstrates how calling arguments are passed thru to here       */

    printf( "Program called with %d arguments:", CALLING_ARGC );
    printf( "\n" );
    for ( i = 0; i < CALLING_ARGC; i++ )
    printf( " %s", CALLING_ARGV[i] );
    printf( "\n" );
    printf( "Calling with argument 'sample' executes the sample program.\n" );

    /*          Setup so handlers will come to code in base.c           */

    TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR]   = (void *)interrupt_handler;
    TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)fault_handler;
    TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR]  = (void *)svc; 

    /* firstly, Initialize Page Table */
    INT32 frame = 0;
    while (frame <= 63){ 
        FRAME_TABLE *pagetable = (FRAME_TABLE *)(malloc(sizeof(FRAME_TABLE)));
        //pagetable->p_id = -1;
        pagetable->time = -1;
        pagetable->page = -1;
        pagetable->frame = frame;
        pagetable->next = NULL;

        FRAME_TABLE * ptrCheck = pageList;     // set page header
        if (ptrCheck == NULL){
            pageList = pagetable;      // input first page node
        }
        else{
            while (ptrCheck->next != NULL){
                ptrCheck = ptrCheck->next;   
            }
            ptrCheck->next = pagetable;    //input other page nodes 
        }
        frame++;
       }

    /*  Determine if the switch was set, and if so go to demo routine.  */

    if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "sample" ) == 0 ) )
        {
        ZCALL( Z502_MAKE_CONTEXT( &next_context,(void *)sample_code, KERNEL_MODE ));
        ZCALL( Z502_SWITCH_CONTEXT( SWITCH_CONTEXT_KILL_MODE, &next_context ));
    }                   /* This routine should never return!!           */
    
    /*here for change the run test1*/
    else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1a" ) == 0 ) )
        {
         process_point = test1a;
         CALL(OS_Create_Process( RUN,"test1a", process_point, PRIORITY, &i, &i));
        }
    else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1b" ) == 0 ) )
        {
          process_point = test1b;
         CALL(OS_Create_Process( RUN,"test1b", process_point, PRIORITY, &i, &i));
        }
    else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1c" ) == 0 ) )
        {
         process_point = test1c;
         CALL(OS_Create_Process( RUN,"test1c", process_point, PRIORITY, &i, &i));
        }
     else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1d" ) == 0 ) )
        {
         process_point = test1d;
         CALL(OS_Create_Process( RUN,"test1d", process_point, PRIORITY, &i, &i));
        }
     else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1e" ) == 0 ) )
        {
         process_point = test1e;
         CALL(OS_Create_Process( RUN,"test1e", process_point, PRIORITY, &i, &i));
        }
     else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1f" ) == 0 ) )
        {
         process_point = test1f;
         CALL(OS_Create_Process( RUN,"test1f", process_point, PRIORITY, &i, &i));
        }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1g" ) == 0 ) )
        {
         process_point = test1g;
         CALL(OS_Create_Process( RUN,"test1g", process_point, PRIORITY, &i, &i));
        }
     else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1h" ) == 0 ) )
        {
         process_point = test1h;
         CALL(OS_Create_Process( RUN,"test1h", process_point, PRIORITY, &i, &i));
        }
     else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1i" ) == 0 ) )
        {
         process_point = test1i;
         CALL(OS_Create_Process( RUN,"test1i", process_point, PRIORITY, &i, &i));
        }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1j" ) == 0 ) )      //TODO: test1j is not finish 
        {
         process_point = test1j;
         CALL(OS_Create_Process( RUN,"test1j", process_point, PRIORITY, &i, &i));
        }
     else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1k" ) == 0 ) )
        {
         process_point = test1k;
         CALL(OS_Create_Process( RUN,"test1k", process_point, PRIORITY, &i, &i));
        }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test1m" ) == 0 ) )
        {
         process_point = test1m;
         CALL(OS_Create_Process( RUN,"test1m", process_point, PRIORITY, &i, &i));
        }
  /* test2 begin here */
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test2a" ) == 0 ) )
              {
               process_point = test2a;
               CALL(OS_Create_Process( RUN,"test2a", process_point, PRIORITY, &i, &i));
              }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test2b" ) == 0 ) )
              {
               process_point = test2b;
               CALL(OS_Create_Process( RUN,"test2b", process_point, PRIORITY, &i, &i));
              }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test2c" ) == 0 ) )
              {
               process_point = test2c;
               CALL(OS_Create_Process( RUN,"test2c", process_point, PRIORITY, &i, &i));
              }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test2d" ) == 0 ) )
              {
               process_point = test2d;
               CALL(OS_Create_Process( RUN,"test2d", process_point, PRIORITY, &i, &i));
              }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test2e" ) == 0 ) )
              {
               LRU_switch=0;
               process_point = test2e;
               CALL(OS_Create_Process( RUN, "test2e", process_point, PRIORITY, &i, &i));
              }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test2f" ) == 0 ) )
              {
               LRU_switch=0;
               process_point = test2f;
               CALL(OS_Create_Process( RUN, "test2f", process_point, PRIORITY, &i, &i));
              }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test2g" ) == 0 ) )       //TODO: test2g is not finish
              {
               process_point = test2g;
               CALL(OS_Create_Process( RUN, "test2g", process_point, PRIORITY, &i, &i));
              }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test2e_LRU" ) == 0 ) )   //test2 extra part: use LRU to run test2e
              {
               LRU_switch=1;
               process_point = test2e;
               CALL(OS_Create_Process( RUN, "test2e", process_point, PRIORITY, &i, &i));
              }
      else if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "test2f_LRU" ) == 0 ) )   //test2 extra part: use LRU to run test2f
              {
               LRU_switch=1;
               process_point = test2f;
               CALL(OS_Create_Process( RUN, "test2f", process_point, PRIORITY, &i, &i));
              }
   
      else
             {
               process_point = test0;
	       CALL(OS_Create_Process( RUN,"test0", process_point, PRIORITY, &i, &i));
             }
}                                               /* End of os_init      */

/************************************************************************
    SVC
        The beginning of the OS502.  Used to receive software interrupts.
        All system calls come to this point in the code and are to be
        handled by the student written code here.


************************************************************************/
void    svc( void ) {
    INT16               call_type;
    static INT16        do_print = 1;
    INT32		Time;
    INT32 		sleepTime;
    INT32               i;
    
    call_type = (INT16)SYS_CALL_CALL_TYPE;
    if ( do_print==1 ) {                 /*always out printf SVC handler*/
        printf( "SVC handler: %s %8ld %8ld %8ld %8ld %8ld %8ld\n",
                call_names[call_type], Z502_ARG1.VAL, Z502_ARG2.VAL, 
                Z502_ARG3.VAL, Z502_ARG4.VAL, 
                Z502_ARG5.VAL, Z502_ARG6.VAL );
       //do_print--;                            
    }
 
    switch (call_type){
       //get time of day, added by test0
    	case SYSNUM_GET_TIME_OF_DAY: 
    		ZCALL(MEM_READ(Z502ClockStatus, &Time));
    		*(INT32 *)Z502_ARG1.PTR = Time;
    		break;
       //terminate a process, added by test0
    	case SYSNUM_TERMINATE_PROCESS:
                CALL(terminate_Process( (INT32)Z502_ARG1.VAL, (INT32*)Z502_ARG2.PTR ));  // need consider
                break;
        //sleep function, added by test1a, make evoluation in test1c & test1d
    	case SYSNUM_SLEEP:
                ZCALL( MEM_READ(Z502ClockStatus, &Time));
                ZCALL( sleepTime = Z502_ARG1.VAL );
                ZCALL(current_PCB->p_time= (Time+sleepTime)); //update time
                if(schedule_SWITCH) ZCALL(schedule_print("SLEEP",current_PCB->p_id, -1, -1));
                ZCALL(lockTimer());
                ZCALL(add_to_timer_Queue(current_PCB));
                ZCALL(unlockTimer());
                if(Time<(timerList->p_time))        //compare the wake time with current time 
                    {
                    ZCALL( Start_Timer(sleepTime ));
                    ZCALL( Dispatcher());
                    }
                else{                          //if wake time is smaller than now, it need wake up immedately
                    ZCALL(Start_Timer(1));
                    ZCALL(Dispatcher());
                    }
    		break;
    	//create a process, Added by Test1b
    	case SYSNUM_CREATE_PROCESS:
    		CALL(OS_Create_Process(NOTRUN, (char*)Z502_ARG1.PTR, (void *)Z502_ARG2.PTR,(INT32)Z502_ARG3.VAL,(INT32*) Z502_ARG4.PTR, (INT32*)Z502_ARG5.PTR));
    		break;
        //get a process ID, Added by Test1b
	case SYSNUM_GET_PROCESS_ID:
		CALL(get_PCB_ID((char *)Z502_ARG1.PTR,(INT32 *)Z502_ARG2.PTR,(INT32 *)Z502_ARG3.PTR));
                break;
        // suspend a process, Added by Test1e & test1f
        case SYSNUM_SUSPEND_PROCESS:
    		CALL(suspend_Process((INT32)Z502_ARG1.VAL, (INT32 *)Z502_ARG2.PTR) );
    		break;
        //Resume Process, Added by Test1e & test1f
    	case SYSNUM_RESUME_PROCESS:
    		CALL(resume_Process((INT32)Z502_ARG1.VAL, (INT32 *)Z502_ARG2.PTR) );
    		break;
        //change prrority, Added by Test1g & test1h
        case SYSNUM_CHANGE_PRIORITY:
    		CALL(change_Priority((INT32)Z502_ARG1.VAL,(INT32)Z502_ARG2.VAL,(INT32*)Z502_ARG3.PTR));
    		break;
        //Send Message, Added by test1i & test1j
    	case SYSNUM_SEND_MESSAGE:
    		CALL(send_Message((INT32)Z502_ARG1.VAL,(char *)Z502_ARG2.PTR,(INT32)Z502_ARG3.VAL,(INT32 *)Z502_ARG4.PTR) );
    		break;
    	//Receive Message,Added by test1i & test1j
    	case SYSNUM_RECEIVE_MESSAGE:
    		CALL(receive_Message((INT32)Z502_ARG1.VAL,(char *)Z502_ARG2.PTR,(INT32)Z502_ARG3.VAL,(INT32 *)Z502_ARG4.PTR,(INT32 *)Z502_ARG5.PTR,(INT32 *)Z502_ARG6.PTR));
                break;
        //disk read ,Add by test2c & test2d
        case SYSNUM_DISK_READ:
                CALL(read_disk(Z502_ARG1.VAL, Z502_ARG2.VAL, Z502_ARG3.PTR));
                break;
        //disk write ,Add by test2c & test2d
        case SYSNUM_DISK_WRITE:
                CALL( write_disk(Z502_ARG1.VAL, Z502_ARG2.VAL, Z502_ARG3.PTR));
                break;
         
    	default:
    		printf("this call_type is ERROR!!!\n");
                CALL(Z502_HALT());
    		break;
    }											
    // End of switch call_type
}                                              // End of svc

/************************************************************************
OS Create process:

this routine's function is create process, its function is for create process,and decide run or not.
when it create a parent process, it will directly make it run. 
other children processs will not running but put into the ready queue

input list instructions:
INT32 RUN_SWITCH: it is flag for distinct parent and children process, if it is RUN
the PCB will run immedately. other children process are NOTRUN.
char * name: process name;
void * process_point: this pointer will used in make context.
INT32 priority: process priority
INT32 *pid: process id
INT32 *error: flag for return error

*************************************************************************/
INT32	OS_Create_Process(INT32 RUN_SWITCH, char * name, void * process_point, INT32 priority, INT32 *pid, INT32 *error){

	if (priority < 0){
		//printf("ERROR:the Priority is illegal!\n");
		(*error) = ERR_BAD_PARAM;
                gen_pid;
		return -1;
	}

	if (readyList != NULL){
		if(check_name(&readyList, name) == 0){
		//printf("ERROR:this Name has already existed!\n");
		(*error) = ERR_BAD_PARAM;
                gen_pid;
		return -1;
		}	
	}

	if (total_ready_pid >= MAX_PIDs-2){
		//printf("ERROR:PIDs number reach the MAX!\n");
                gen_pid;
		(*error) = ERR_BAD_PARAM;
		return -1;
	}	

    	PCB_str *PCB = (PCB_str *)(malloc(sizeof(PCB_str)));    //allocate memory for PCB
       
	PCB->p_time = 0;                                        //now is zero
	PCB->p_id = gen_pid;                                    //assign pid
        gen_pid++;  
        PCB->p_state=CREATE;

        memset(PCB->p_name, 0, MAX_NAME+1);                    //assign process name
	strcpy(PCB->p_name,name);                              //enter name 
	PCB->p_priority = priority;                            //assign priority
        PCB->disk_io.flag=0;                                   //assign disk read flag
        memset(PCB->pagetable, 0, VIRTUAL_MEM_PGS+1);          //pagetable

	if (current_PCB != NULL) 
        PCB->p_parent = current_PCB->p_id;                //assign parent id
        else 
        PCB->p_parent = -1;                               //(update from pro1) -1 means this process is parent process
	
	(*error) = ERR_SUCCESS;                           //return error value
	(*pid) = PCB->p_id;                               //return pid

	if (RUN_SWITCH == 1) 
        {                                                 // there is create parent process
         make_context(PCB, process_point);
         add_to_ready_Queue(PCB);                         //now put this into ready queue, then call dispatcher
         Dispatcher();
        }
	else	                                          // there is create child process
        {
         make_context(PCB, process_point);      
         if(schedule_SWITCH) schedule_print("CREATE",PCB->p_id, PCB->p_id, -1);       
         add_to_ready_Queue(PCB);                         // only into ready queue
        }      
        
	return 0; 
}

///help for crate process, for check the same name in ready queue 
int check_name( PCB_str **ptrheader, char *name ){
        
	PCB_str *ptrCheck = *ptrheader;

	while (ptrCheck != NULL){
		if(strcmp(ptrCheck->p_name, name) == 0){
                       
			return 0;
		}
		ptrCheck = ptrCheck->next;
	}
       
	return 1;
}

/************************************************************************
context operation

there are two functions, make context, create context to a PCB.
and switch context make this PCB to run through assign current_PCB

they use Z502_MAKE_CONTEXT and Z502_SWITCH_CONTEXT

*************************************************************************/
void make_context ( PCB_str * PCB, void *procPTR ){
	ZCALL( Z502_MAKE_CONTEXT( &PCB->context, procPTR, USER_MODE ));
}

void switch_context ( PCB_str * PCB ){
	current_PCB = PCB;
        current_PCB -> p_state =RUN;      //update the PCB state to RUN
	ZCALL( Z502_SWITCH_CONTEXT( SWITCH_CONTEXT_SAVE_MODE, &current_PCB->context ));
}

/************************************************************************
add to queue functions

there are two functions:
add to timer queue, with sort nodes as wake up time (p_time in PCB);
add to ready queue, with sort nodes as priority (p_priority in PCB);

their inputs are all entry PCB_str * entry.

*************************************************************************/
INT32 add_to_timer_Queue(PCB_str * entry) //put a node into timer queue 
{
   PCB_str **ptrFirst= &timerList;                       // pointer to the timer queue header
   PCB_str *PCB = (PCB_str *)(malloc(sizeof(PCB_str)));  //set a memory for PCB
   memcpy(PCB, entry, sizeof(PCB_str));                  // copy entry PCB to this memory space 
   PCB_str *current=NULL;
   PCB_str *previous=NULL;
   
   entry->p_state=SLEEPING;                              //update state to sleep
   
   int flag=0;

	//First one into queue
	if ( *ptrFirst  == NULL)
        {
	        (*ptrFirst ) = entry;
		 timer_tail = entry;
                 total_timer_pid++;
	}

	//Add to from tail
	else{   
                
                current=(*ptrFirst);
                
                while(current!=NULL)
                { 
                  if(entry->p_time<current->p_time)        //time sort
                    {
                     if(current==(*ptrFirst))
                        {
                          (*ptrFirst)=entry;
                           entry->next=current;
                           total_timer_pid++;
                           flag=1;
                           break;
                         }
                      else
                         {
                           previous->next=entry;
                           entry->next=current;
                           total_timer_pid++;
                           flag=1;
                           break;
                         }
                      }
                   else
                      {
                      previous=current;
                      current=current->next;
                      }
                 }
                 
                  if(flag==0)
                  {
                    timer_tail->next = entry;
                    timer_tail=entry;
                    total_timer_pid++;
                  }

	     }
	return -1;
}


INT32 add_to_ready_Queue(PCB_str * entry)   //put a node into ready queue 
{  
   PCB_str **ptrFirst= &readyList;                        // pointer to the ready queue header
   PCB_str *PCB = (PCB_str *)(malloc(sizeof(PCB_str)));   // set a memory for PCB
   memcpy(PCB, entry, sizeof(PCB_str));                   // copy entry PCB to this memory space 

   PCB_str *current=NULL;
   PCB_str *previous=NULL;
 
   entry->p_state=READY;                            //update state

   int flag=0;
	//First One to queue
	if ( *ptrFirst  == NULL)
        {
	        (*ptrFirst ) = entry;
		 ready_tail = entry;
		 total_ready_pid++;
        return 1;
	}

	//Add to queue after
	else{   
                
                current=(*ptrFirst);
                
                while(current!=NULL)
                { 
                  if(entry->p_priority<current->p_priority)   //priority sort
                    {
                     if(current==(*ptrFirst))
                        {
                          (*ptrFirst)=entry;
                           entry->next=current;
                           total_ready_pid++;
                           flag=1;
                           return 1;
                         }
                      else
                         {
                           previous->next=entry;
                           entry->next=current;
                           total_ready_pid++;
                           flag=1;
                           return 1;
              
                         }
                      }
                   else
                      {
                      previous=current;
                      current=current->next;
                      }
                 }
                 
                  if(flag==0)
                  {
                    ready_tail->next = entry;
                    ready_tail=entry;
                    total_ready_pid++;
                    return 1;
                  }

	       }
	return -1;
}

/**********************************************************************************
remove a node from queue

there are two functions:
rm_from_ready_Queue
rm_from_timer_Queue

they will check the timer queue and ready queue, find target process, and remove it
from queue. 

they are used by terminate process

inputs list:
INT32 remove_id: target process's id, they find target process though their id

***********************************************************************************/

INT32 rm_from_ready_Queue(INT32 remove_id)
{
        PCB_str **ptrFirst=&readyList;
	PCB_str * ptrDel = *ptrFirst;
	PCB_str * ptrPrev = NULL;

	while ( ptrDel != NULL ){
		if (ptrDel->p_id == remove_id){
                    ptrDel->p_state=TERMINATE;
			//First one
			if ( ptrDel== *ptrFirst){
				(*ptrFirst) = ptrDel->next;
                                 ptrDel->next=NULL;
                                 total_ready_pid--;
				 return 1;
			}
			//Last one
			else if (ptrDel->next == NULL)
                         {
				ready_tail=ptrPrev ;
                                ready_tail->next=NULL;                                
				total_ready_pid--;
				return 1;

			}
                        // other one
			else{   
				ptrPrev->next= ptrDel->next;
                                ptrDel->next=NULL;                                
				total_ready_pid--;
				return 1;
			}
		}
		ptrPrev = ptrDel;
		ptrDel = ptrDel->next;
	}
	return 0;
}

INT32 rm_from_timer_Queue(INT32 remove_id)
{
        PCB_str **ptrFirst=&timerList;
	PCB_str * ptrDel = *ptrFirst;
	PCB_str * ptrPrev = NULL;

	while ( ptrDel != NULL ){
		if (ptrDel->p_id == remove_id){
                   ptrDel->p_state=TERMINATE;
			//First ID
			if ( ptrDel== *ptrFirst){
				(*ptrFirst) = ptrDel->next;
                                 ptrDel->next=NULL;                              
                                 total_timer_pid--;
				 return 1;
			}
			//Last ID
			else if (ptrDel->next == NULL)
                         {
				timer_tail=ptrPrev ;
                                timer_tail->next=NULL;                               
				total_timer_pid--;
				return 1;

			}
                        // middle ID
			else{    
				ptrPrev->next= ptrDel->next;
                                ptrDel->next=NULL;                               
				total_timer_pid--;
				return 1;
			}
		}
		ptrPrev = ptrDel;
		ptrDel = ptrDel->next;
	}
	return 0;
}


/************************************************************************
drop a node from queue, it always drop the first one(header) in queue

there are flag for switch timer queue and ready queue:
flag==1 (READY_Q)  it means for ready queue.
flag==0 (TIMER_Q)  it means for timer queue.

dispatcher and Interrupt handler will call and use them.

PCB_str **ptrFirst is mean the first one in queue, I use it point to queue's header 
*************************************************************************/
void drop_Queue( PCB_str **ptrFirst,INT32 flag)
{
       if(flag==1) //ready Queue
        {
         if(*ptrFirst==NULL)
          {
            drop_ready_node=NULL;          
          }
         else if((*ptrFirst)->next==NULL)  //only one 
          {
            drop_ready_node= (*ptrFirst);
            drop_ready_node->next=NULL;
            (*ptrFirst)=NULL;
            ready_tail=NULL;
            total_ready_pid--;           
          }
         else 
          {
           drop_ready_node= (*ptrFirst);
           (*ptrFirst)=(*ptrFirst)->next;
           drop_ready_node->next=NULL;
           total_ready_pid--;           
          }
         }
        else  //timer q
        {
          if(*ptrFirst==NULL)
          {
            drop_timer_node=NULL;          
          }
         else if((*ptrFirst)->next==NULL)  //only one 
          {
            drop_timer_node= (*ptrFirst);
            drop_timer_node->next=NULL;
            *ptrFirst=NULL;
            timer_tail=NULL;
            total_timer_pid--;           
          }
         else 
          {
           drop_timer_node= (*ptrFirst);
           *ptrFirst=(*ptrFirst)->next;
           drop_timer_node->next=NULL;
           total_timer_pid--;           
          }
        }  
return;
}

/************************************************************************
Get PCB ID :

it has two function for use,
get_PCB_ID_from_ready_queue(char *name, INT32 *process_ID)
get_PCB_ID_from_timer_queue(char *name, INT32 *process_ID)

they check the timer Queue and ready Queue. 

if the find the target process (name is same), it will return the 

process id value.

inputs List:
char *name: target process name;
INT32 *process_ID: return process ID
INT32 *error: flag for return error 
*************************************************************************/
void get_PCB_ID(char *name, INT32 *process_ID, INT32 *error)
{   
    int a,b;
    a=get_PCB_ID_from_ready_queue(name, process_ID);
    b=get_PCB_ID_from_timer_queue(name, process_ID);
    
    if((a==-1)&&(b==-1))
    (*error) = ERR_BAD_PARAM;
    else
    (*error) = ERR_SUCCESS;

}

INT32 get_PCB_ID_from_ready_queue(char *name, INT32 *process_ID){
	
        PCB_str ** ptrFirst=&readyList;
         
	        if (strcmp("", name) == 0){
		     (*process_ID) = current_PCB->p_id;
		     return 0;
	}
	PCB_str *ptrCheck = *ptrFirst;
	while (ptrCheck != NULL){
		if (strcmp(ptrCheck->p_name, name) == 0){
			(*process_ID) = ptrCheck->p_id;
			return 0;
		}
		ptrCheck = ptrCheck->next;
	}
	return -1;
}

INT32 get_PCB_ID_from_timer_queue(char *name, INT32 *process_ID)
{
	
        PCB_str ** ptrFirst=&timerList;     
   
	if (strcmp("", name) == 0){
		(*process_ID) = current_PCB->p_id;
		return 0;
	}
	PCB_str *ptrCheck = *ptrFirst;
	while (ptrCheck != NULL){
		if (strcmp(ptrCheck->p_name, name) == 0){
			(*process_ID) = ptrCheck->p_id;
			return 0;
		}
		ptrCheck = ptrCheck->next;
	}
	return -1;
}

/************************************************************************
terminate process:

it inputs process id, and this functions works in three kinds of mode.

1. process id=-1, it means terminate current_PCB(running one), 
if it has other PCB in ready Queue, it will call dispatcher to run it.
if ready queue is empty, but it has other process in timer queue it will generate
a new start timer for get process out from timer queue.
2. process id=-2, it needs terminate current_PCB and its children process.
3. process_id, it will check the timer Queue and ready Queue, 
find and remove target PCB.

INT32 *error is flag for return error
*************************************************************************/
void terminate_Process ( INT32 process_ID, INT32 *error ){
        INT32 Time;
        INT32 SleepTime;
        
        if(schedule_SWITCH) schedule_print("DONE",current_PCB->p_id, -1, current_PCB->p_id);
        
        ZCALL(MEM_READ(Z502ClockStatus, &Time));

	//if pid is -1; terminate self
	if ( process_ID == -1 ){
		(*error) = ERR_SUCCESS;
                
                if(current_PCB->p_parent==-1)  CALL(Z502_HALT());  // if only parent process, simply stop!
                
               if(readyList!=NULL)
                {
                 ZCALL(Dispatcher());
                }
                else if(timerList!=NULL)
                {
                 
                 SleepTime=timerList->p_time-Time;
                 if(SleepTime>0)
                 {CALL(Start_Timer(SleepTime+1));     // genreate another interrupt!
                 ZCALL(Dispatcher());}
                 else
                 {CALL(Start_Timer(1));               // genreate another interrupt immedatly!
                 ZCALL(Dispatcher());}
                }
                 
	}	
	//if pid -2; terminate self and any child
	else if ( process_ID == -2 ){

		rm_children_from_ready (current_PCB->p_id );
                rm_children_from_timer (current_PCB->p_id );
		
	        (*error) = ERR_SUCCESS;
		
		 CALL(Z502_HALT());
	}
	//remove pid from readyList
	else{
		if ((rm_from_ready_Queue(process_ID))||(rm_from_timer_Queue(process_ID))) 
			(*error) = ERR_SUCCESS;
		else 	(*error) = ERR_BAD_PARAM;     
	}

}

/* these functions use for help terminate children process. */
INT32 rm_children_from_ready (INT32 process_ID)
{       
       
        PCB_str ** ptrFirst= &readyList; 
	PCB_str * ptrCheck = *ptrFirst;
	while (ptrCheck != NULL)
        {
		if ( ptrCheck->p_parent == process_ID )
                {
	            rm_from_ready_Queue(ptrCheck->p_id);        
		}
           ptrCheck = readyList;  
                
              
	}
return 1;
}

INT32 rm_children_from_timer (INT32 process_ID)
{       
        PCB_str ** ptrFirst= &timerList; 
	PCB_str * ptrCheck = *ptrFirst;
	while (ptrCheck != NULL)
        {
		if ( ptrCheck->p_parent == process_ID )
                {
		     rm_from_timer_Queue(ptrCheck->p_id);
		}
		ptrCheck = timerList;
	}
return 1;
}

/************************************************************************
Dispatcher function:

this is infinite loop, it will call drop queue(), 
the loop will continue until the dispatcher get one node from ready queue, 
and switch context to it  (make it run!!)

if the ready queue is empty, which no node to drop out. Dispatcher will 
CALL(time_lapse()), this is my write a time function. it will make time move.

*************************************************************************/
void Dispatcher(){
    while(1){
       if(readyList!=NULL)
       {   
        ZCALL(lockReady());   
        ZCALL(drop_Queue(&readyList,READY_Q));
        ZCALL(unlockReady()); 
 
        current_PCB=drop_ready_node;

        ZCALL(switch_context(current_PCB));
        break;
       }
       else
       {
       CALL(time_lapse());     //make time move!!!
       //ZCALL(Z502_IDLE());    

       while(interrupt_lock);  // check interrupt lock, wait the interupt op finish
       }
    }

 return;
}

/*timer function */
void time_lapse()
{
 // use it as CALL(time_lapse()), since it can make time to run.
}

/************************************************************************
Start_Timer:

make Z502 delay timer to run!!

Time is pass into function, for set delay timer in Z502

*************************************************************************/
void Start_Timer(INT32 Time) 
{
        INT32 Status;
	MEM_WRITE( Z502TimerStart, &Time );
        MEM_READ( Z502TimerStatus, &Status );
}

/************************************************************************
suspend_process:

there has bulit a suspend queue, suspend operation is put a node into suspend queue
it also check seveal errors before move operation,if illegal happens, return error.

it has functions:

1,check_suspend_in_timer_queue: check timer queue, if find target node, change this node
state to SUSPEND,

2,out_one_from_Queue: out one node from ready queue.

3,add_to_suspend_queue: put a node into suspend queue.

input List:
INT32 process_ID, this is target process's id
INT32 * error, this is flag for return error
*************************************************************************/
void suspend_Process(INT32 process_ID, INT32 * error) 
{
	PCB_str **switchPtr=&suspendList;
        PCB_str  *suspend_node =NULL;   //temp PCB node to suspend queue 
	INT32	status;
        INT32   flag;
        INT32   Time;
        INT32   SleepTime;

        if(process_ID>MAX_PIDs)  //the pid is illegal!
        {
		(*error) = ERR_BAD_PARAM;
                return;
	}

        while((*switchPtr)!=NULL)
         {
            if((*switchPtr)->p_id==process_ID) //already suspend!!
             {
		(*error) = ERR_BAD_PARAM;
                return;
              }
           (*switchPtr)=(*switchPtr)->next;
          }
        
	//if pid -1, suspend self   
	if ( process_ID == -1 ){
                if(current_PCB->p_parent==-1)
                {   //if it is parent process, it canot suspend in here!!
                (*error) = ERR_BAD_PARAM;
                return;
                }
                else
                {
                 ZCALL(lockSuspend());
                 if(schedule_SWITCH) schedule_print("SUSPEND",current_PCB->p_id, -1, -1);
                 add_to_suspend_queue(&suspendList, current_PCB);
                 ZCALL(unlockSuspend());

                 ZCALL(MEM_READ(Z502ClockStatus, &Time));
                   if(readyList!=NULL)
                   {
                    CALL(Dispatcher());
                   }
                   else if(timerList!=NULL)
                   {
                    SleepTime=timerList->p_time-Time;
                      if(SleepTime>0)
                      {
                       CALL(Start_Timer(SleepTime+1));     // genreate another interrupt!
                       CALL(Dispatcher());
                      }
                      else
                      {
                       CALL(Start_Timer(1));               // genreate another interrupt immedatly!
                       CALL(Dispatcher());}
                      }
	            }
                  }
	else{   
               
                ZCALL(lockReady());   
                CALL(suspend_node=out_one_from_ready_Queue( &readyList, process_ID));   //target process if in ready queue, remove out
                ZCALL(unlockReady());                                                  
                
                if(suspend_node!=NULL)
                {  
                 ZCALL(lockSuspend());
                 if(schedule_SWITCH) schedule_print("SUSPEND",suspend_node->p_id, -1, -1);
                 ZCALL( status = add_to_suspend_queue(&suspendList, suspend_node ) );  //add into suspend queue
                 ZCALL(unlockSuspend());
		if (status) (*error) = ERR_SUCCESS;
		else (*error) = ERR_BAD_PARAM;
                }
                return;
              
	    }
}

//help suspend process, add into suspend queue
INT32 add_to_suspend_queue(PCB_str **ptrFirst, PCB_str * entry )
{
   // first case in suspend queue
   if ( *ptrFirst  == NULL)
        {
	        (*ptrFirst) = entry;
		 suspend_tail = entry;
                 entry->p_state=SUSPEND;  //update the state
               
                 return 1;
	}
	//Add to start of list
    else{   
                suspend_tail->next=entry;
                suspend_tail=entry;
                entry->p_state=SUSPEND;   //update the state
               
                return 1;
         }
        return -1;
}


/************************************************************************
out one node from ready queue:

it likes drop from queue, but it need find target node firstly.

it also return the remove PCB (ptrDelet), so it return one PCB pointer.

the suspend_process, resume_process, and change priority will call it 

*************************************************************************/
PCB_str *out_one_from_ready_Queue(PCB_str **ptrFirst, int remove_id)
{
        PCB_str * ptrDelet = *ptrFirst;
	PCB_str * ptrPrev = NULL;

	while ( ptrDelet != NULL ){
		if (ptrDelet->p_id == remove_id){
			//First ONE
			if ( ptrDelet== *ptrFirst){
				(*ptrFirst) = ptrDelet->next;
                                 ptrDelet->next=NULL;                             				        
                                 return ptrDelet;
			}
			//Last ONE
			else if (ptrDelet->next == NULL)
                         {
				ready_tail=ptrPrev ;
                                ready_tail->next=NULL;                          
				return ptrDelet;

			}
                        // middle one
			else{    
				ptrPrev->next= ptrDelet->next;
                                ptrDelet->next=NULL;                               
				return ptrDelet;
			}
		}
		ptrPrev = ptrDelet;
		ptrDelet = ptrDelet->next;
	}
}

/************************************************************************
resume_process:

resume process function is check suspend queue, find the target node, move 
it into ready queue. 

before check suspend queue, it will check the node in the ready queue, and 
current_PCB, if the PCB is not suspend, or is running, function will return 
with error. 

input List:
INT32 process_ID, this is target process's id
INT32 * error, this is flag for return error
*************************************************************************/
void resume_Process(INT32 process_ID, INT32 * error) 
{       
	PCB_str **switchPtr=&readyList;
	INT32	status;
        PCB_str  *resume_node=NULL;     //temp PCB node from suspend node
        
        if(process_ID>MAX_PIDs)         //the pid is illegal!
        {
		(*error) = ERR_BAD_PARAM;
                return;
	}

        while((*switchPtr)!=NULL)       //this process is not suspend
         {
            if((*switchPtr)->p_id==process_ID)
             {
		(*error) = ERR_BAD_PARAM;
                return;
              }
           (*switchPtr)=(*switchPtr)->next;
          }
        
	if (current_PCB->p_id== process_ID){   //here we cannot resume running PCB 
                (*error) = ERR_BAD_PARAM;
                return; 
	}
	else{
                ZCALL(lockSuspend());
                ZCALL(resume_node=out_one_from_suspend_Queue( &suspendList, process_ID)); //out from suspend queue
                ZCALL(unlockSuspend());
                if(resume_node!=NULL)
                {
                if(schedule_SWITCH) schedule_print("RESUME",resume_node->p_id, -1, -1);
                ZCALL(lockReady());   
                ZCALL( status = add_to_ready_Queue(resume_node ) );   //put into ready queue
                ZCALL(unlockReady());   
		if (status) (*error) = ERR_SUCCESS;
		else (*error) = ERR_BAD_PARAM;
                }
                return;
	}

}


/************************************************************************
out one node from suspend queue:

it likes drop from queue, but it need find target node firstly.

it also return the remove PCB (ptrDelet), so it return one PCB pointer.

the suspend_process, resume_process, and change priority will call it 

*************************************************************************/

PCB_str *out_one_from_suspend_Queue(PCB_str **ptrFirst, int remove_id)
{
        PCB_str * ptrDelet = *ptrFirst;
	PCB_str * ptrPrev = NULL;

	while ( ptrDelet != NULL ){
		if (ptrDelet->p_id == remove_id){
			//First ONE
			if ( ptrDelet== *ptrFirst){
				(*ptrFirst) = ptrDelet->next;
                                 ptrDelet->next=NULL;                             				        
                                 return ptrDelet;
			}
			//Last ONE
			else if (ptrDelet->next == NULL)
                         {
				suspend_tail=ptrPrev ;
                                suspend_tail->next=NULL;                          
				return ptrDelet;

			}
                        // Middle one
			else{    
				ptrPrev->next= ptrDelet->next;
                                ptrDelet->next=NULL;                               
				return ptrDelet;
			}
		}
		ptrPrev = ptrDelet;
		ptrDelet = ptrDelet->next;
	}
}

/***************************************************************************
change the priority:

change the priority will find the target node.

some error situation as process ID is illegal or update priority is illagal is 
considered. the function will return with error.

check_priority_in_timer_queue:
if the node in the timer queue(sleep), only change its' priority. 

new_Priority:
if the node in the ready queue, the node will be out, and 
change node's priority, and put back to ready queue again with resort priority.  

****************************************************************************/
void change_Priority( INT32 process_ID, INT32 new_priority, INT32 *error ){
	PCB_str * switchPCB;
	INT32	status;

        if(process_ID>MAX_PIDs)                //process id is illegal
        {
		(*error) = ERR_BAD_PARAM;
                return;
	}
        if(new_priority> Legal_Priority_Level)  //priority is illegal
        {
		(*error) = ERR_BAD_PARAM;
                return;
	}
	if ( process_ID == -1 )   //if pid -1, update current PCB's priority
        {       
                if(schedule_SWITCH) schedule_print("PRIORITY",current_PCB->p_id, -1, -1); 
		status=(current_PCB->p_priority=new_priority);
		if (status) (*error) = ERR_SUCCESS;
		else{
			(*error) = ERR_BAD_PARAM;
			return;
		}
	}
	//else, update target Process id
	else{
                if(schedule_SWITCH) schedule_print("PRIORITY",process_ID, -1, -1); 
		CALL(status = new_Priority(process_ID, new_priority));  //
		if (status) (*error) = ERR_SUCCESS;
		else{
			(*error) = ERR_BAD_PARAM;
			return;
		}
	}

}

/*help for change_Priority, change priority in ready queue or timer queue*/
INT32 new_Priority ( INT32 process_ID, INT32 new_priority )
{
	int        s;
        int        flag;
        PCB_str   *priority_node=NULL;   //temp PCB node for change priority

       flag=check_priority_in_timer_queue(process_ID,new_priority);
       
       if(flag==1)
       {
       return 1;
       }
       else
       {
       priority_node=out_one_from_ready_Queue( &readyList, process_ID);
       if(priority_node!=NULL){
       priority_node->p_priority= new_priority;
       CALL(add_to_ready_Queue(priority_node));
       return 1;}          
       }

       return -1;
}

//check timer queue
INT32 check_priority_in_timer_queue(INT32 process_ID,INT32 priority)
{
  PCB_str *ptrCheck;
  ptrCheck=timerList;
  while(ptrCheck!=NULL)
  {
   if(ptrCheck->p_id==process_ID)
     {
      ptrCheck->p_priority=priority;
      return 1;
     }
  ptrCheck=ptrCheck->next;
  }
 return 0;
}


/***************************************************************************
send message:

this function will check the target process ID, message lenth. if there have error
in it, it will sent error;
without error, then it can build message and put it into message queue.

Input List:
INT32 target_ID :target process id, if not exist or illegal, it will return error.
char *message : mesage context
INT32 msg_Length :message length
INT32 *error:flag for error return.
****************************************************************************/
void send_Message ( INT32 target_ID, char *message, INT32 msg_Length, INT32 *error ){

 	INT32 check;
        INT32 check_current;
         
        if(target_ID>MAX_PIDs){ // the ID is illegal	
 		(*error) = ERR_BAD_PARAM;
 		return;}

        CALL( check = check_pid(target_ID) );  // check id whether exist or not 
                                               //check =-1 not exist
        if(target_ID==current_PCB->p_id)       //check target process id whether is current_PCB or not
        {check_current=1;}
        else{check_current=-1;}
        
 	if (target_ID == -1) check = 1;   //update check
 	
 	if ((check == -1)&&(check_current==-1)){  // the target ID is not exist!
 		(*error) = ERR_BAD_PARAM;
 		return;
 	}
 	
 	if (msg_Length > MAX_MSG){  //Check the message length, error if too long 
 		(*error) = ERR_BAD_PARAM;
 		return;
 	}

 	MSG_str *MSG = (MSG_str *)(malloc(sizeof(MSG_str)));  //for here it means No errors, then it can create 
                                                               //message
 	MSG->target_ID = target_ID;
 	memset(MSG->message_buffer, 0, MAX_MSG+1);
	strcpy(MSG->message_buffer, message);
	MSG->next = NULL;

	
	CALL( check = add_message_Queue (MSG) );             //Add the message to message queue
	if (check == -1){
		
		(*error) = ERR_BAD_PARAM;                    //sentBox is MAX!!
		return;
	}	
	else (*error) = ERR_SUCCESS;
}         

//help for send message
INT32 add_message_Queue ( MSG_str *entry ){
	(current_PCB->msg_count)++;
	if(current_PCB->msg_count > MAX_MSG_COUNT){
		return -1;
	}

	MSG_str *toSend = current_PCB->sentBox;
	//if First one
	if (toSend == NULL){
		current_PCB->sentBox = entry;
		return 1;
	}
	//after one
	while (toSend->next != NULL){
		toSend = toSend->next;
	}
	toSend->next = entry;
	return 1;
}

INT32 check_pid ( INT32 check_ID ){
	
	PCB_str 	*ptrCheck = readyList;

	while(ptrCheck != NULL){
		if (ptrCheck->p_id == check_ID){
			
			return 1;
		}
		ptrCheck = ptrCheck->next;
	}
	
	return -1;
}

/***************************************************************************
recieve message
this function will check the target process ID, message lenth. if there have error
in it, it will sent error;
without error, then it can build message and put it into message queue.

Input List:
INT32 target_ID :target process id, if not exist or illegal, it will return error.
char *message : mesage context
INT32 msg_Length :message length
INT32 *error:flag for error return.
***************************************************************************/
void receive_Message ( INT32 source_ID, char *message, INT32 msg_recvLength, INT32 *msg_sendLength, INT32 *sender_ID, INT32 *error)
{
	
        MSG_str * msgRecv = NULL;   // a message structure for recieve message 

	
	INT32 check;
	CALL( check = check_pid(source_ID) );  //Check the source_ID whether exists
	if (source_ID == -1) check = 1;
	if (check == -1){                     // the soure ID is not exsit!
 		(*error) = ERR_BAD_PARAM;
 		return;
 	}
 	
 	if (msg_recvLength > MAX_MSG){        //Check the message size
 		(*error) = ERR_BAD_PARAM;     //message size is two long!
 		return;
 	}

 	if (source_ID == -1) CALL( msgRecv = get_message() ); //if source_ID=-1, then it just get this message 

 	//Need to further discuss this check
 	if (*msg_sendLength != msg_recvLength){  //Send Length != recieve Length
 		
 		(*error) = ERR_BAD_PARAM;
 		return;
 	}
}

//help for get message
MSG_str * get_message ( void ){
	//ZCALL( lockReady() );
	PCB_str * ptrCheck = readyList;

	while (ptrCheck != NULL){
            MSG_str * msgCheck = ptrCheck->sentBox;
	      while (msgCheck != NULL){
			if ( msgCheck->target_ID == -1 || msgCheck->target_ID == current_PCB->p_id ){
				
				return msgCheck;               // return this message
			}
			msgCheck = msgCheck->next;
		}
		ptrCheck = ptrCheck->next;
	}
	return NULL;
}


/***************************************************************************
disk read & disk write
this is disk read & write function provide to system call. 

when the process operate disk, it need check the disk status, if the disk is
not free, the process will enter the disk queue for waiting

after the disk interrupt happened, the interrupt handler will get node from disk 
queue and put back into ready queue.  

there are seveal routine below:
void write_disk(INT32 disk_id, INT32 sector, char DATA[PGSIZE])
void read_disk(INT32 disk_id, INT32 sector, char DATA[PGSIZE]) 
void os_disk_op(PCB_str* pcb)
INT32 add_to_disk_queue(PCB_str **ptrFirst, PCB_str * entry )
PCB_str *out_one_from_disk_Queue(PCB_str **ptrFirst, int remove_id)
***************************************************************************/
void write_disk(INT32 disk_id, INT32 sector, char DATA[PGSIZE])   //disk write,handle system call
{
    INT32 temp;
    
    current_PCB->disk_io.disk_id =  disk_id;
    current_PCB->disk_io.sector = sector;
    current_PCB->disk_io.op = 1;  // it means write
    memcpy(current_PCB->disk_io.buf, (INT32*)DATA, DISK_IO_BUF_SIZE);

    while(disk_lock);  //check the disklock
    disk_lock = TRUE;
   
  
        MEM_WRITE( Z502DiskSetID, &(current_PCB->disk_io.disk_id) );
        MEM_READ( Z502DiskStatus, &temp );  //check disk status
        if ( temp == DEVICE_FREE )
        {
            os_disk_op(current_PCB);   //begin work
      
            disk_lock = FALSE;
            return;
        }
   
     ZCALL(lockSuspend());
     add_to_disk_queue(&diskList, current_PCB);
     ZCALL(unlockSuspend());
     
     disk_lock = FALSE;
     Dispatcher();
     return;
}


void read_disk(INT32 disk_id, INT32 sector, char DATA[PGSIZE])   //disk read,handle system call
{
    INT32 temp;
    current_PCB->disk_io.disk_id = disk_id;
    current_PCB->disk_io.sector = sector;
    current_PCB->disk_io.op = 0;  //0 means read
     
    while(disk_lock);   // here need disklock
    disk_lock = TRUE;
   

        MEM_WRITE( Z502DiskSetID, &(current_PCB->disk_io.disk_id) );
        MEM_READ( Z502DiskStatus, &temp );  //check disk status
        if ( temp == DEVICE_FREE )
        {   //begin work
            os_disk_op(current_PCB);
            memcpy((INT32*)DATA, current_PCB->disk_io.buf, DISK_IO_BUF_SIZE);
          
            disk_lock = FALSE;
            return;
        }

    
    current_PCB->disk_io.flag = 1;
    
    //here we need suspend this process
     ZCALL(lockSuspend());
     add_to_disk_queue(&diskList, current_PCB);
     ZCALL(unlockSuspend());
     
    disk_lock = FALSE;
    Dispatcher();
    return;
}

/* help function, the operation of disk */
void os_disk_op(PCB_str* pcb)  // it come from simple.c
{
    INT32 temp;
    if(schedule_SWITCH) ZCALL(schedule_print("DISK",pcb->p_id, -1, -1));
    MEM_WRITE( Z502DiskSetID, &(pcb->disk_io.disk_id));
    MEM_READ( Z502DiskStatus, &temp );

    MEM_WRITE( Z502DiskSetSector, &(pcb->disk_io.sector));


    MEM_WRITE( Z502DiskSetBuffer, (INT32 *)(pcb->disk_io.buf));


    MEM_WRITE( Z502DiskSetAction, &(pcb->disk_io.op));

    temp = 0;
    MEM_WRITE( Z502DiskStart, &temp );
}

/* put the node into disk queue */
INT32 add_to_disk_queue(PCB_str **ptrFirst, PCB_str * entry )
{
   
   // first case in suspend queue
   if ( *ptrFirst  == NULL)
        {
	        (*ptrFirst) = entry;
		 disk_tail = entry;
                
        return 1;
	}

	//Add to start of list
	else{   
                disk_tail->next=entry;
                disk_tail=entry;
               
            return 1;
             }
        return -1;
}


/*out one node from disk queue*/
PCB_str *out_one_from_disk_Queue(PCB_str **ptrFirst, int remove_id)
{
        PCB_str * ptrDelet = *ptrFirst;
	PCB_str * ptrPrev = NULL;

	while ( ptrDelet != NULL ){
		if (ptrDelet->p_id == remove_id){
			//First ONE
			if ( ptrDelet== *ptrFirst){
				(*ptrFirst) = ptrDelet->next;
                                 ptrDelet->next=NULL;                             				        
                                 return ptrDelet;
			}
			//Last ONE
			else if (ptrDelet->next == NULL)
                         {
				disk_tail=ptrPrev ;
                                disk_tail->next=NULL;                          
				return ptrDelet;

			}
                        // middle one
			else{    
				ptrPrev->next= ptrDelet->next;
                                ptrDelet->next=NULL;                               
				return ptrDelet;
			}
		}
		ptrPrev = ptrDelet;
		ptrDelet = ptrDelet->next;
	}
}

/***************************************************************
//memory page manage
for support the memory paging and replacement, seveal routine 
are here.

INT32 find_empty_Frame(INT32 page_num )
FRAME_TABLE *find_full_Frame()
void add_FullpageList( FULL_FRAME_TABLE *entry )
void page_update(INT32 page_num, INT32 frame)
void find_empty_Disk( INT32 *disk, INT32 *sector)
void memory_to_disk( FRAME_TABLE *return_page, INT32 page_num)
void disk_to_memory( FRAME_TABLE *return_page, INT32 page_num)

****************************************************************/
//find a empty frame from frame table
INT32 find_empty_Frame(INT32 page_num ){  
    INT32 	currentTime;
    FRAME_TABLE *ptrCheck = pageList;

	while( ptrCheck != NULL ){
                //this frame is free
		if(ptrCheck->page == -1){
                        ZCALL( MEM_READ( Z502ClockStatus, &currentTime ) );
                        ptrCheck->time = currentTime;
			ptrCheck->page = page_num;
			return ptrCheck->frame;
		}
		ptrCheck = ptrCheck->next;
	}
	//else, no free frame
	return -1;
}


/* if no free frame can be found, we find a frame as victim, and then swap with disk*/
/* this routine will has two algorithm: FIFO and LRU, it will select by different test*/
/* it return the victim page*/
FRAME_TABLE *find_full_Frame(){
	FRAME_TABLE *ptrCheck = pageList;   
	FRAME_TABLE *return_page;

	INT32	small_time = -1;   //for keep small time
	INT32	frame = -1;
        
       if(LRU_switch==1){ // for extra test, use LRU, find small time page in frame table
	   while( ptrCheck != NULL ){          //check pageList
		if( small_time == -1){
		      small_time = ptrCheck->time;  //first compare
		      return_page = ptrCheck;
		}
		else{
			if( small_time < ptrCheck->time ){  //compare smalltime with nodes' time
		              small_time = ptrCheck->time;
			      return_page = ptrCheck;       //find vitcim page
                              return return_page;
			}
		}
		ptrCheck = ptrCheck->next;
	   }
        }
        else if(LRU_switch==0){ //here for test2e and test2f ,use FIFO
		while( ptrCheck != NULL ){
			if (ptrCheck->frame == count){ //FIND first in from pageList
				return_page = ptrCheck;
				count++;
				if(count==64) count=0; //count is full, return to 0
                                return return_page;
			}
			ptrCheck = ptrCheck->next;			
		}

        }
	
}

/* add entry node into full page table*/
void add_FullpageList( FULL_FRAME_TABLE *entry ){
	 FULL_FRAME_TABLE *ptrCheck = full_pageList;

	if( ptrCheck == NULL){      //First One
		full_pageList = entry;
		return;
	}

	while( ptrCheck->next != NULL){  //Add other one
		ptrCheck = ptrCheck->next;
	}
	ptrCheck->next = entry;
}

/*update the page information*/
/*most is time update*/
void page_update(INT32 page_num, INT32 frame){  
	FRAME_TABLE * ptrCheck = pageList;
        INT32 	time;
	while( ptrCheck != NULL){
		if (ptrCheck->frame == frame){
                        ZCALL( MEM_READ( Z502ClockStatus, &time ) );
			ptrCheck->page = page_num;
			ptrCheck->time = time;
		}
		ptrCheck = ptrCheck->next;
	}
}

/* find a empty disk from disk bitmap*/
void find_empty_Disk( INT32 *disk, INT32 *sector){
	INT32 disks, sectors;
	for( disks = 1; disks <= 12; disk++ ){
		for( sectors = 1; sectors <= 1600; sectors++ ){
			if( bitmap[disks][sectors] == 0 ){
				bitmap[disks][sectors] = 1;
				*disk = disks;
				*sector = sectors;
				return;
			}
		}
	}
	*disk = -1;
	*sector = -1;
	return;
}

// swap data from memory to disk. 
// first it find a empty disk.
// and generate a node, update disk information in it
// then put node into full page table.
// finally, swap data from memory to disk
void memory_to_disk( FRAME_TABLE *return_page, INT32 page_num ){
        INT32	temp = 0;
        INT32   Disk;
        INT32   Sector;
	
	INT32	disk_id, sector;
	
	CALL( find_empty_Disk(&disk_id, &sector) );  //find a free disk and sector
	
	FULL_FRAME_TABLE *entry = ( FULL_FRAME_TABLE *)(malloc(sizeof( FULL_FRAME_TABLE)));
	//generate a node and update information with victim frame
        entry->page = return_page->page;
	entry->frame = return_page->frame;
        // put disk information into this node
	entry->disk = disk_id;
	entry->sector = sector;
	entry->next = NULL;
	CALL( add_FullpageList(entry) );  // add into full page table

	char	DATA[PGSIZE];
	Z502_PAGE_TBL_ADDR[return_page->page] = return_page->frame;
	Z502_PAGE_TBL_ADDR[return_page->page] |= PTBL_VALID_BIT;

	ZCALL( MEM_READ( (return_page->page*PGSIZE), (INT32*)DATA) );
	//update the page table
	Z502_PAGE_TBL_ADDR[return_page->page] = 0;
	CALL( page_update(page_num, return_page->frame) );

        // begin write disk
	Disk = (INT32) disk_id;  //set the disk id 
	Sector = (INT32) sector;   //set sector
	
	ZCALL( MEM_WRITE( Z502DiskSetID, &Disk ) );
	
	ZCALL( MEM_READ( Z502DiskStatus, &temp ) );
        while( temp != DEVICE_FREE )  {
        	ZCALL( Z502_IDLE() );          //for here the process only wait the disk until it free
        	ZCALL( MEM_WRITE( Z502DiskSetID, &Disk ) );       
        	ZCALL( MEM_READ( Z502DiskStatus, &temp ) );
    	}
        ZCALL( MEM_WRITE( Z502DiskSetSector, &Sector ) );
        ZCALL( MEM_WRITE( Z502DiskSetBuffer, (INT32*)DATA ) );
       
        temp = 1;
        ZCALL( MEM_WRITE( Z502DiskSetAction, &temp ) );
       
        temp = 0;                        // Must be set to 0
        ZCALL( MEM_WRITE( Z502DiskStart, &temp ) );
        if(schedule_SWITCH) schedule_print("DISK",current_PCB->p_id, -1, -1);

}

// swap data from disk to memory. 
// it check full page table to find the page,
// finally, swap data from disk to memory
void disk_to_memory( FRAME_TABLE *return_page, INT32 page_num ){
	FULL_FRAME_TABLE *ptrCheck;
	INT32	disk_id, sector;
	char	DATA[PGSIZE];
        INT32 	temp = 0;
        INT32 	Disk;
        INT32	Sector;

	INT32 page = return_page->page;
       
	ptrCheck = full_pageList; 
	
        while(ptrCheck != NULL){
	       if( ptrCheck->page == page ){  //find the right node through page
                // read disk
	        Disk =ptrCheck->disk;
        	Sector = ptrCheck->sector;

	       ZCALL( MEM_WRITE( Z502DiskSetID, &Disk) );
               while( temp != DEVICE_FREE ){
                  ZCALL( Z502_IDLE() );            //for here the process only wait the disk until it free
                  ZCALL( MEM_WRITE( Z502DiskSetID, &Disk) );       
                  ZCALL( MEM_READ( Z502DiskStatus, &temp ) );
               }
               // begin read disk
               ZCALL( MEM_WRITE( Z502DiskSetSector, &Sector ) );
               ZCALL( MEM_WRITE( Z502DiskSetBuffer, (INT32*)DATA ) );
               
               temp = 0;
               ZCALL( MEM_WRITE( Z502DiskSetAction, &temp ) );
              
               temp = 0;                   // Must be set to 0
               ZCALL( MEM_WRITE( Z502DiskStart, &temp ) );
               if(schedule_SWITCH) schedule_print("DISK",current_PCB->p_id, -1, -1);
	       Z502_PAGE_TBL_ADDR[page_num] = return_page->frame;
	       Z502_PAGE_TBL_ADDR[page_num] |= PTBL_VALID_BIT;
	       ZCALL( MEM_WRITE(page*PGSIZE, (INT32*)DATA) );
	       return;
	       }	
	       ptrCheck = ptrCheck->next;
	}
	
}


/*******************************************************
lock function

comes from the sample.c, provide the lock queues

********************************************************/
//Timer Locks
void lockTimer( void ){
	INT32 LockResult;
	Z502_READ_MODIFY( MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult );
}
void unlockTimer ( void ){
	INT32 LockResult;
	Z502_READ_MODIFY( MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult );
}

//Ready Locks
void lockReady ( void ){
	INT32 LockResult;
	Z502_READ_MODIFY( MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult );
}
void unlockReady ( void ){
	INT32 LockResult;
	Z502_READ_MODIFY( MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult );
}

//Suspend Locks
void lockSuspend ( void ){
	INT32 LockResult;
	Z502_READ_MODIFY( MEMORY_INTERLOCK_BASE + 2, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult );
}
void unlockSuspend ( void ){
	INT32 LockResult;
	Z502_READ_MODIFY( MEMORY_INTERLOCK_BASE + 2, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult );
}


//////////////////////////////// other function ////////////////////////////////////////////////////
/************************************************************************
schedule_print:

mainly is for print output and use for test 
input list:
char * str: input for decide the ACTION MODE,I set the "create", "ready",
            "sleep","suspend","resume","Done"
INT32 target: the target process ID for doing this action 
INT32 create: if it is create action, show the create process id, if in other 
              action mode, just input -1, it will no print 
INT32 done:   if it is done action, show the terminate process id, if in other 
              action mode, just input -1, it will no print 
*************************************************************************/
void schedule_print(char * str,INT32 target, INT32 create, INT32 done)
{
    INT32 Time;
    INT32 r,s,t,u;
    INT32 new;
    PCB_str * ptrCheck0 = readyList;
    PCB_str * ptrCheck1 = timerList;
    PCB_str * ptrCheck2 = suspendList;
    PCB_str * ptrCheck3 = diskList;

    ZCALL(MEM_READ(Z502ClockStatus, &Time));

    ZCALL( SP_setup( SP_TIME_MODE, Time ) );
    ZCALL( SP_setup_action( SP_ACTION_MODE, str ));
    ZCALL( SP_setup( SP_TARGET_MODE, target) );
    if(create!=-1) ZCALL( SP_setup( SP_NEW_MODE,create));
    ZCALL( SP_setup( SP_RUNNING_MODE, current_PCB->p_id ) );
    if(done!=-1) ZCALL( SP_setup( SP_TERMINATED_MODE, done));
  
  while(ptrCheck0!=NULL)
    {
       r=ptrCheck0->p_id;
       CALL( SP_setup( SP_READY_MODE, r) );
       ptrCheck0=ptrCheck0->next;
    }
  while(ptrCheck1!=NULL)
    {
       s=ptrCheck1->p_id;
       CALL( SP_setup( SP_WAITING_MODE, s) );
       ptrCheck1=ptrCheck1->next;
    }
   while(ptrCheck2!=NULL)
    {
       t=ptrCheck2->p_id;
       CALL( SP_setup( SP_SUSPENDED_MODE, t) );
       ptrCheck2=ptrCheck2->next;
    }
   while(ptrCheck3!=NULL)
     {
       u=ptrCheck3->p_id;
       CALL( SP_setup( SP_SWAPPED_MODE, u) );
       ptrCheck3=ptrCheck3->next;
    }
    printf("\n==========================schedule print==============================\n");
    ZCALL( SP_print_header() );
    ZCALL( SP_print_line()   );
    printf("======================================================================\n\n");
}

/************************************************************************
memory_print:

mainly is for print output and use for test 

output the frame
*************************************************************************/
void memory_print (void){
    FRAME_TABLE     *ptrCheck;
    ptrCheck = pageList;

    while( ptrCheck != NULL ){
        if( ptrCheck->page != -1 ){
            MP_setup( ptrCheck->frame, current_PCB->p_id, 
            ptrCheck->page, 0 );
        }
        ptrCheck = ptrCheck->next;
    }
    printf("\n**********************************************************************");
    MP_print_line();
    printf("**********************************************************************\n");
}


/************************************************************************
print queue is for test and check

it can print the timer queue, ready queue. it was for using self test&debug

now it is no use................
*************************************************************************/
void print_queues ( PCB_str **ptrFirst ){
	if (*ptrFirst == NULL) 
        return;
        PCB_str * ptrCheck = *ptrFirst;
        
       if((*ptrFirst)==timerList){
	
          while (ptrCheck != NULL){
		printf("*********TIMER QUEUE***************\n");
		printf("Process Name: %s\t", ptrCheck->p_name);
		printf("ID: %d\t", ptrCheck->p_id);
                printf("Time: %d\t", ptrCheck->p_time); 
                printf("Prioity: %d\t",ptrCheck->p_priority);                                    
                printf("************************************\n");
		ptrCheck = ptrCheck->next;
	        }  // end while
        
        }  //end if
        else{
        while (ptrCheck != NULL){
		printf("*********READY QUEUE***************\n");
		printf("Process Name: %s\t", ptrCheck->p_name);
		printf("ID: %d\t", ptrCheck->p_id);
                printf("Time: %d\t", ptrCheck->p_time); 
                printf("Prioity: %d\t",ptrCheck->p_priority);                                
                printf("************************************\n");
		ptrCheck = ptrCheck->next;
	        }
        }
	return;
}

