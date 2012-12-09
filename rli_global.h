#include        "stdio.h"
#include	"stdlib.h"


///these is state for PCBs
#define	                CREATE		               100
#define			READY			       101
#define			RUNNING			       102
#define                 SUSPEND                        103
#define			SLEEPING		       104
#define                 SWAPPED                        105                          
#define			TERMINATE		       106

// flag for some function to distint ready queue or timer queue
#define                 READY_Q                         1
#define                 TIMER_Q                         0

#define			TIMER_IR			4 
#define                 DISK_IR                         5

//PCB information and set
#define			MAX_PIDs		        10
#define			MAX_NAME		        16
#define			MAX_MSG				64
#define			MAX_MSG_COUNT			10

//priority set
#define                 PRIORITY                        10
#define                 Legal_Priority_Level            500

//flag for make process which create to run immediately or not
#define                 NOTRUN                          0
#define                 RUN                             1

//lock function
#define                 DO_LOCK                         1
#define                 DO_UNLOCK                       0

#define                 SUSPEND_UNTIL_LOCKED            TRUE
#define                 DO_NOT_SUSPEND                  FALSE

//page table
#define                 PG_TABLE_LEN                    1024
#define                 FRAME_LEN                       64

#define                 FRAME_AVAILABLE	                0x8000
#define                 FRAME_ALLOCATED                 0x4000

//about disk define
#define                 DISK_IO_BUF_SIZE                16

//switch of output printer
#define                 SWITCH_ON                       1
#define                 SWITCH_OFF                      0



//FrameTable structure
typedef 		struct {
        INT32					time;
	INT32					page;
	INT32					frame;
	void					*next;
} FRAME_TABLE;


//Full FrameTable structure
typedef 		struct {
	INT32					page;
	INT32					frame;
	INT32					disk;
	INT32					sector;
	void					*next;	
} FULL_FRAME_TABLE;

//disk io structure                           
typedef         struct{
        INT32                                   disk_id;
        INT32                                   sector;
        char                                    buf[DISK_IO_BUF_SIZE];
        char                                    buf2[DISK_IO_BUF_SIZE];
        INT32                                   op;
        INT32                                   flag;
}DISK_IO_str;

//PCB structure
typedef         struct {
	char					p_name[MAX_NAME+1];      //name
	INT32					p_id;                    //process id
	INT32					p_priority;              //process priority
        INT32					p_time;                  //wake up time
	INT32					p_parent;                // parent control
        INT32                                   p_state;                 //state
	void					*context; 
	void				        *next;                    //link to next node
	void                                    *sentBox;                 //to sent queue
        void                                    *recvBox;                 // for recieve 
        INT32					msg_count;
      
        UINT16					pagetable[VIRTUAL_MEM_PGS]; //page table 
        DISK_IO_str                             disk_io;                    //disk use information
}PCB_str;


//message structure
typedef		struct{
	INT32					target_ID;                  
	char 					message_buffer[MAX_MSG+1];
	void					*next;
} MSG_str;

/////////////////////////////////////functions prototype////////////////////////////////////////////////
//create pcb and context operation
INT32 OS_Create_Process(INT32 RUN_SWITCH, char * name, void * process_point, INT32 priority, INT32 *pid, INT32 *error);

int check_name ( PCB_str **ptrheader, char *name );

void make_context( PCB_str * PCB, void *procPTR);
void switch_context( PCB_str * PCB );

//queue operation
INT32 add_to_timer_Queue(PCB_str * entry);
INT32 add_to_ready_Queue(PCB_str * entry);
INT32 add_to_suspend_queue(PCB_str **ptrFirst, PCB_str * entry );

void drop_Queue( PCB_str **ptrFirst,INT32 flag);

PCB_str *out_one_from_suspend_Queue(PCB_str **ptrFirst, int remove_id);
PCB_str *out_one_from_ready_Queue(PCB_str **ptrFirst, int remove_id);


//get pcb ID functions
void get_PCB_ID(char *name, INT32 *process_ID, INT32 *error);
INT32 get_PCB_ID_from_ready_queue(char *name, INT32 *process_ID);
INT32 get_PCB_ID_from_timer_queue(char *name, INT32 *process_ID);

//terminate process operation
void terminate_Process ( INT32 process_ID, INT32 *error );
INT32 rm_children_from_ready ( INT32 process_ID );
INT32 rm_children_from_timer ( INT32 process_ID );
INT32 rm_from_ready_Queue(INT32 remove_id);
INT32 rm_from_timer_Queue(INT32 remove_id);

//start timer and dispatcher
void Start_Timer(INT32 Time);
void Dispatcher();

//time functions (no use now)
void time_lapse(void);

//suspend operation
void suspend_Process(INT32 process_ID, INT32 * error);
void resume_Process(INT32 process_ID, INT32 * error);

//INT32 check_suspend_in_timer_queue(INT32 process_ID);

//change priority operation
void change_Priority( INT32 process_ID, INT32 new_priority, INT32 *error );
INT32 new_Priority ( INT32 process_ID, INT32 new_priority );

INT32 check_priority_in_timer_queue(INT32 process_ID,INT32 priority);

//send and recieve message
void send_Message ( INT32 target_ID, char *message, INT32 msg_Length, INT32 *error );
INT32 add_message_Queue ( MSG_str *entry);

void receive_Message ( INT32 source_ID, char *message, INT32 msg_recvLength, INT32 *msg_sendLength, INT32 *sender_ID, INT32 *error);
MSG_str *get_message ( void );
INT32 check_pid ( INT32 check_ID );// use for send message

//memory page manage
INT32	find_empty_Frame( INT32 page_num );    //find empty frame
FRAME_TABLE* 	find_full_Frame( void );       // find victim frame
void 	find_empty_Disk( INT32 *disk, INT32 *sector);
void 	add_FullpageList( FULL_FRAME_TABLE *entry );
void 	page_update(INT32 pageRequest, INT32 frame);
void	memory_to_disk( FRAME_TABLE *return_page, INT32 page_num );
void	disk_to_memory( FRAME_TABLE *return_page, INT32 page_num );

//disk read and write
void read_disk(INT32 disk_id, INT32 sector, char DATA[PGSIZE]);
void write_disk(INT32 disk_id, INT32 sector, char DATA[PGSIZE]);

INT32 add_to_disk_queue(PCB_str **ptrFirst, PCB_str * entry );   // for op disk queue
PCB_str *out_one_from_disk_Queue(PCB_str **ptrFirst, int remove_id);

void os_disk_op(PCB_str *pcb);  // operation disk

//lock function
void lockTimer( void );
void unlockTimer( void );
void lockReady( void );
void unlockReady ( void );
void lockSuspend( void );
void unlockSuspend ( void );

//test & schedule printer
void schedule_print(char * str,INT32 target, INT32 create, INT32 done);
void memory_print (void);

void print_queues ( PCB_str **ptrFirst );  //only self test (no use now)
