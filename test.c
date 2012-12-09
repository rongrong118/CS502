/************************************************************************

    test.c

    These programs are designed to test the OS502 functionality     

    Read Appendix B about test programs and Appendix C concerning
    system calls when attempting to understand these programs.      

    Revision History:       
        1.0 August 1990
        1.1 December 1990: Tests cleaned up; 1b, 1e - 1k added
                           Portability attempted.
        1.2 December 1991: Still working on portabililty.
        1.3 July     1992: tests1i/1j made re-entrant.
        1.4 December 1992: Minor changes - portability
                           tests2c/2d added.  2f/2g rewritten
        1.5 August   1993: Produced new test2g to replace old
                           2g that did a swapping test.
        1.6 June     1995: Test Cleanup.
        1.7 June     1999: Add test0, minor fixes.
        2.0 January  2000: Lots of small changes & bug fixes
        2.1 March    2001: Minor Bugfixes.  
                           Rewrote get_skewed_random_number
        2.2 July     2002: Make appropriate for undergrads
        3.0 August   2004: Modified to support memory mapped IO 
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking
        3.13 November 2004: Minor fix defining USER
        3.41 August  2009: Additional work for multiprocessor + 64 bit
        3.53 November 2011: Changed test2c so data structure used
                           ints (4 bytes) rather than longs.
************************************************************************/

#define          USER
#include         "global.h"
#include         "syscalls.h"
#include         "z502.h"
#include         "protos.h"

#include         "stdio.h"
#include         "string.h"
#include         "stdlib.h"
#include         "math.h"

extern INT16    Z502_PROGRAM_COUNTER;
extern INT32    SYS_CALL_CALL_TYPE;
extern Z502_ARG Z502_ARG1;
extern Z502_ARG Z502_ARG2;
extern Z502_ARG Z502_ARG3;
extern Z502_ARG Z502_ARG4;
extern Z502_ARG Z502_ARG5;
extern Z502_ARG Z502_ARG6;
extern long     Z502_REG_1;
extern long     Z502_REG_2;
extern long     Z502_REG_3;
extern long     Z502_REG_4;
extern long     Z502_REG_5;
extern long     Z502_REG_6;
extern long     Z502_REG_7;
extern long     Z502_REG_8;
extern long     Z502_REG_9;
extern INT16    Z502_MODE;

/*      Prototypes for internally called routines.                  */

void                    test1x( void );
void                    test1j_echo( void );
void                    test2gx( void );
void                    error_expected( INT32, char[] );
void                    success_expected( INT32, char[] );



/**************************************************************************

        Test0
        
        Exercises GET_TIME_OF_DAY and TERMINATE_PROCESS

        Z502_REG_1              Time returned from call
        Z502_REG_9              Error returned

**************************************************************************/

void    test0( void )     {

    SELECT_STEP   {
       STEP( 0 )
                printf( "This is Release %s:  Test 0\n", CURRENT_REL );
                GET_TIME_OF_DAY( &Z502_REG_1 );
       STEP( 1 )
                printf( "Time of day is %ld\n", Z502_REG_1 );
                TERMINATE_PROCESS( -1, &Z502_REG_9 );
       STEP( 2 )
                printf( "ERROR: Test should be terminated but isn't.\n");
                break;
    }                                           // End of SELECT 
}                                               // End of test0 


/**************************************************************************

        Test1a 
        
        Exercises GET_TIME_OF_DAY and SLEEP and TERMINATE_PROCESS

        Z502_REG_9              Error returned

**************************************************************************/

void    test1a( void )  {
    static INT32          sleep_time = 100;
    static INT32          time1, time2;

    SELECT_STEP   {
       STEP( 0 )
                printf( "This is Release %s:  Test 1a\n", CURRENT_REL );
                GET_TIME_OF_DAY( &time1 );
       STEP( 1 )
                SLEEP ( sleep_time );
       STEP( 2 )
                GET_TIME_OF_DAY( &time2 );
       STEP( 3 )
                printf( "sleep time= %d, elapsed time= %d\n",
                        sleep_time, time2 - time1 );
                TERMINATE_PROCESS( -1, &Z502_REG_9 );
       STEP( 4 )
                printf( "ERROR: Test should be terminated but isn't.\n");
                break;
    }                                           /* End of SELECT    */
}                                               /* End of test1a    */

/**************************************************************************
        Test1b 
        
        Exercises the CREATE_PROCESS and GET_PROCESS_ID  commands. 

        This test tries lots of different inputs for create_process.
        In particular, there are tests for each of the following:

                1. use of illegal priorities
                2. use of a process name of an already existing process.
                3. creation of a LARGE number of processes, showing that 
                   there is a limit somewhere ( you run out of some
                   resource ) in which case you take appropriate action.

        Test the following items for get_process_id:

                1. Various legal process id inputs.
                2. An illegal/non-existant name.

        Z502_REG_1, _2          Used as return of process id's.
        Z502_REG_3              Cntr of # of processes created.
        Z502_REG_9              Used as return of error code.
**************************************************************************/

#define         ILLEGAL_PRIORITY                -3
#define         LEGAL_PRIORITY                  10

void    test1b( void) {
    static char         process_name[16];

    while (1) {
        SELECT_STEP {
        /* Try to create a process with an illegal priority.    */

           STEP( 0 )
                printf( "This is Release %s:  Test 1b\n", CURRENT_REL );
                CREATE_PROCESS( "test1b_a", test1x, ILLEGAL_PRIORITY, 
                                            &Z502_REG_1, &Z502_REG_9 );

           STEP( 1 )
                error_expected( Z502_REG_9, "CREATE_PROCESS" );

        /* Create two processes with same name - 1st succeeds, 2nd fails */

                CREATE_PROCESS( "two_the_same", test1x, LEGAL_PRIORITY, 
                                            &Z502_REG_2, &Z502_REG_9 );

           STEP( 2 )
                success_expected( Z502_REG_9, "CREATE_PROCESS" );
                CREATE_PROCESS( "two_the_same", test1x, LEGAL_PRIORITY, 
                                            &Z502_REG_1, &Z502_REG_9 );
           STEP( 3 )
                error_expected( Z502_REG_9, "CREATE_PROCESS" );
                TERMINATE_PROCESS( Z502_REG_2, &Z502_REG_9 );
           STEP( 4 )
                success_expected( Z502_REG_9, "TERMINATE_PROCESS" );
                break;
        /*      Loop until an error is found on the create_process.  
                Since the call itself is legal, we must get an error 
                because we exceed some limit.                           */

           STEP( 5 )
                Z502_REG_3++;                   /* Generate next prog name*/
                sprintf( process_name, "Test1b_%ld", Z502_REG_3 );
                printf( "Creating process \"%s\"\n", process_name );
                CREATE_PROCESS( process_name, test1x, LEGAL_PRIORITY, 
                                            &Z502_REG_1, &Z502_REG_9 );

           STEP( 6 )
                if ( Z502_REG_9 == ERR_SUCCESS )
                    {
                    GO_NEXT_TO( 5 )                        /* LOOP BACK */
                }
                break;

        /*      When we get here, we've created all the processes we can.*/

           STEP( 7 )
                error_expected( Z502_REG_9, "CREATE_PROCESS" );
                printf( "%ld processes were created in all.\n", Z502_REG_3 );

        /*      Now test the call GET_PROCESS_ID                        */

                GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 );  /* Legal */

           STEP( 8 )
                success_expected( Z502_REG_9, "GET_PROCESS_ID" );
                printf( "The PID of this process is %ld\n", Z502_REG_2 );
                strcpy( process_name, "Test1b_1" );
                GET_PROCESS_ID( process_name, &Z502_REG_1, 
                                                &Z502_REG_9 );  /* Legal */

           STEP( 9 )
                success_expected( Z502_REG_9, "GET_PROCESS_ID" );
                printf( "The PID of target process is %ld\n", Z502_REG_1 );
                GET_PROCESS_ID( "bogus_name", &Z502_REG_1, 
                                                &Z502_REG_9 );  /* Illegal */

           STEP( 10 )
                error_expected( Z502_REG_9, "GET_PROCESS_ID" );
                GET_TIME_OF_DAY( &Z502_REG_4 );

           STEP( 11 )
                printf( "Test1b, PID %ld, Ends at Time %ld\n", 
                         Z502_REG_2, Z502_REG_4);
                TERMINATE_PROCESS( -2, &Z502_REG_9 )

        }                                       /* End of SELECT     */
    }                                           /* End of while      */
}                                               /* End of test1b     */


/**************************************************************************

        Test1c

        Tests multiple copies of test1x running simultaneously.
        Test1c runs these with the same priority in order to show
        FCFS scheduling behavior; Test1d uses different priorities
        in order to show priority scheduling.                     

        WARNING:  This test assumes tests 1a - 1b run successfully 

        Z502_REG_1, 2, 3, 4, 5  Used as return of process id's.
        Z502_REG_6              Return of PID on GET_PROCESS_ID
        Z502_REG_9              Used as return of error code.

**************************************************************************/

#define         PRIORITY1C              10

void    test1c( void)
    {
    static INT32      sleep_time      = 1000;

    while( 1 )
        {
        SELECT_STEP
            {
           STEP( 0 )
                printf( "This is Release %s:  Test 1c\n", CURRENT_REL );
                CREATE_PROCESS( "test1c_a", test1x, PRIORITY1C, 
                                                &Z502_REG_1, &Z502_REG_9 );
                
           STEP( 1 )
                success_expected( Z502_REG_9, "CREATE_PROCESS" );
                CREATE_PROCESS( "test1c_b", test1x, PRIORITY1C, 
                                                &Z502_REG_2, &Z502_REG_9 );
                
           STEP( 2 )
                CREATE_PROCESS( "test1c_c", test1x, PRIORITY1C, 
                                                &Z502_REG_3, &Z502_REG_9 );
                
           STEP( 3 )
                CREATE_PROCESS( "test1c_d", test1x, PRIORITY1C, 
                                                &Z502_REG_4, &Z502_REG_9 );
        
           STEP( 4 )
                CREATE_PROCESS( "test1c_e", test1x, PRIORITY1C, 
                                                &Z502_REG_5, &Z502_REG_9 );
 

        /*      In these next three cases, we will loop until the target
                process ( test1c_e ) has terminated.  We know it
                terminated because for a while we get success on the call
                GET_PROCESS_ID, and then we get failure when the process
                no longer exists.                                       */

           STEP( 5 )
                SLEEP ( sleep_time );
                
           STEP( 6 )
                GET_PROCESS_ID( "test1c_e", &Z502_REG_6, &Z502_REG_9 );
                
           STEP( 7 )
                if ( Z502_REG_9 == ERR_SUCCESS )
                    GO_NEXT_TO( 5 )                /* Loop back */
                break;

           STEP( 8 )
                TERMINATE_PROCESS( -2, &Z502_REG_9 ); /* Terminate all */
                
        }                                       /* End switch           */
    }                                           /* End while            */
}                                               /* End test1c           */


/**************************************************************************


        Test 1d   
        
        Tests multiple copies of test1x running simultaneously.
        Test1c runs these with the same priority in order to show
        FCFS scheduling behavior; Test1d uses different priorities
        in order to show priority scheduling.                       

        WARNING:  This test assumes tests 1a - 1b run successfully 

        Z502_REG_1, 2, 3, 4, 5  Used as return of process id's.
        Z502_REG_6              Return of PID on GET_PROCESS_ID
        Z502_REG_9              Used as return of error code.

**************************************************************************/

#define         PRIORITY1               10
#define         PRIORITY2               11
#define         PRIORITY3               11
#define         PRIORITY4               90
#define         PRIORITY5               40

void    test1d( void)
    {
    static INT32      sleep_time      = 1000;

    while( 1 )
        {
        SELECT_STEP
            {
           STEP( 0 )
                printf( "This is Release %s:  Test 1d\n", CURRENT_REL );
                CREATE_PROCESS( "test1d_1", test1x, PRIORITY1, 
                                                &Z502_REG_1, &Z502_REG_9 );
        
           STEP( 1 )
                success_expected( Z502_REG_9, "CREATE_PROCESS" );
                CREATE_PROCESS( "test1d_2", test1x, PRIORITY2, 
                                                &Z502_REG_2, &Z502_REG_9 );
                
           STEP( 2 )
                CREATE_PROCESS( "test1d_3", test1x, PRIORITY3, 
                                                &Z502_REG_3, &Z502_REG_9 );

           STEP( 3 )
                CREATE_PROCESS( "test1d_4", test1x, PRIORITY4, 
                                                &Z502_REG_4, &Z502_REG_9 );
                
           STEP( 4 )
                CREATE_PROCESS( "test1d_5", test1x, PRIORITY5, 
                                                &Z502_REG_5, &Z502_REG_9 );
                

/*  We will loop until the target process ( test1d_4 ) has terminated.  
    We know it terminated because for a while we get success on the call
    GET_PROCESS_ID, and then failure when the process no longer exists. */

           STEP( 5 )
                SLEEP ( sleep_time );
                
           STEP( 6 )
                GET_PROCESS_ID( "test1d_4", &Z502_REG_6, &Z502_REG_9 );
                
           STEP( 7 )
                if ( Z502_REG_9 == ERR_SUCCESS )
                    GO_NEXT_TO( 5 )                    /* Loop back */
                break;

           STEP( 8 )
                TERMINATE_PROCESS( -2, &Z502_REG_9 );
                
        }                                       /* End switch           */
    }                                           /* End while            */
}                                               /* End test1d           */

/**************************************************************************

      Test 1e exercises the SUSPEND_PROCESS and RESUME_PROCESS commands

    
        This test should try lots of different inputs for suspend and resume.
        In particular, there should be tests for each of the following:

        1. use of illegal process id.
        2. what happens when you suspend yourself - is it legal?  The answer
           to this depends on the OS architecture and is up to the developer.
        3. suspending an already suspended process.
        4. resuming a process that's not suspended.

        there are probably lots of other conditions possible.
    
        Z502_REG_1              Target process ID
        Z502_REG_2              OUR process ID
        Z502_REG_9              Error returned

**************************************************************************/

#define         LEGAL_PRIORITY_1E               10

void    test1e( void)
    {
    SELECT_STEP
       {
       STEP( 0 )                            /* Get OUR PID          */
           GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 ); 
 
       STEP( 1 )                            /* Make legal target    */
           printf( "Release %s:Test 1e: Pid %ld\n", CURRENT_REL, Z502_REG_2 );
           CREATE_PROCESS( "test1e_a", test1x, LEGAL_PRIORITY_1E,
                                &Z502_REG_1, &Z502_REG_9);
                
       STEP( 2 )
           success_expected( Z502_REG_9, "CREATE_PROCESS" );
                                                /* Suspend Illegal PID  */
           SUSPEND_PROCESS( (INT32)9999, &Z502_REG_9);
                
       STEP( 3 )
           error_expected( Z502_REG_9, "SUSPEND_PROCESS" );
                                                /* Resume Illegal PID   */
           RESUME_PROCESS( (INT32)9999, &Z502_REG_9);
                
       STEP( 4 )
           error_expected( Z502_REG_9, "RESUME_PROCESS" );
                                                /* Suspend legal PID    */ 
           SUSPEND_PROCESS(Z502_REG_1, &Z502_REG_9); 
                
       STEP( 5 ) 
           success_expected( Z502_REG_9, "SUSPEND_PROCESS" );
                                        /* Suspend already suspended PID */
           SUSPEND_PROCESS(Z502_REG_1, &Z502_REG_9);
                
       STEP( 6 )
           error_expected( Z502_REG_9, "SUSPEND_PROCESS" );
                                                /* Do a legal resume    */      
           RESUME_PROCESS(Z502_REG_1, &Z502_REG_9);
                
       STEP( 7 )
           success_expected( Z502_REG_9, "RESUME_PROCESS" );
                                /* Resume an already resumed process */
           RESUME_PROCESS(Z502_REG_1, &Z502_REG_9);
                
       STEP( 8 )
           error_expected( Z502_REG_9, "RESUME_PROCESS" );
                                        /* Try to resume ourselves      */
           RESUME_PROCESS(Z502_REG_2, &Z502_REG_9);

       STEP( 9 )
           error_expected( Z502_REG_9, "RESUME_PROCESS" );
                 /* It may or may not be legal to suspend ourselves;
                    architectural decision.                    */
           SUSPEND_PROCESS(-1, &Z502_REG_9);
                
       STEP( 10 )
           /* If we returned "SUCCESS" here, then there is an inconsistancy; 
            * success implies that the process was suspended.  But if we 
            * get here, then we obviously weren't suspended.  Therefore
            * this must be an error.                                    */

           error_expected( Z502_REG_9, "SUSPEND_PROCESS" );
           GET_TIME_OF_DAY( &Z502_REG_4 );

       STEP( 11 )
           printf( "Test1e, PID %ld, Ends at Time %ld\n", Z502_REG_2, Z502_REG_4);
           TERMINATE_PROCESS(-2, &Z502_REG_9);
    }                                             /* End of SELECT     */
}                                                 /* End of test1e     */

/**************************************************************************

      Test1f  Successfully suspend and resume processes.          
    
      In particular, show what happens to scheduling when processes
      are temporarily suspended.

      This test works by starting up a number of processes at different
      priorities.  Then some of them are suspended.  Then some are resumed.

        Z502_REG_1              Loop counter
        Z502_REG_2              OUR process ID
        Z502_REG_3,4,5,6,7      Target process ID
        Z502_REG_9              Error returned

**************************************************************************/


#define         PRIORITY_1F1             5
#define         PRIORITY_1F2            10
#define         PRIORITY_1F3            15
#define         PRIORITY_1F4            20
#define         PRIORITY_1F5            25


void    test1f( void)
    {

    static INT32          sleep_time = 300;

    SELECT_STEP
        {
           STEP( 0 )                            /* Get OUR PID          */
                 Z502_REG_1 = 0;                /* Initialize */
                 GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 ); 
 
           STEP( 1 )                            /* Make legal target    */
                 printf( "Release %s:Test 1f: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_2 );
                 CREATE_PROCESS( "test1f_a", test1x, PRIORITY_1F1,
                                        &Z502_REG_3, &Z502_REG_9);
                
           STEP( 2 )                            /* Make legal target    */
                 CREATE_PROCESS( "test1f_b", test1x, PRIORITY_1F2,
                                        &Z502_REG_4, &Z502_REG_9);
                
           STEP( 3 )                            /* Make legal target    */
                 CREATE_PROCESS( "test1f_c", test1x, PRIORITY_1F3,
                                        &Z502_REG_5, &Z502_REG_9);
                
           STEP( 4 )                            /* Make legal target    */
                 CREATE_PROCESS( "test1f_d", test1x, PRIORITY_1F4,
                                        &Z502_REG_6, &Z502_REG_9);
                
           STEP( 5 )    /* Make legal target    */
                 CREATE_PROCESS( "test1f_e", test1x, PRIORITY_1F5,
                                        &Z502_REG_7, &Z502_REG_9);
                
                /* Let the 5 pids go for a bit                          */

           STEP( 6 )     
                SLEEP ( sleep_time );
                
                /* Suspend 3 of the pids and see what happens - we 
                   should see scheduling behavior where the processes
                   are yanked out of the ready and the waiting states,
                   and placed into the suspended state.                 */

           STEP( 7 )
                SUSPEND_PROCESS(Z502_REG_3, &Z502_REG_9);

           STEP( 8 )
                SUSPEND_PROCESS(Z502_REG_5, &Z502_REG_9);

           STEP( 9 )
                SUSPEND_PROCESS(Z502_REG_7, &Z502_REG_9);

           STEP( 10 )    
                SLEEP ( sleep_time );
                
           STEP( 11 )
                RESUME_PROCESS(Z502_REG_3, &Z502_REG_9);
                
           STEP( 12 )
                RESUME_PROCESS(Z502_REG_5, &Z502_REG_9);
                
           STEP( 13 )
                if ( Z502_REG_1 < 4 )
                    GO_NEXT_TO( 6 )             
                Z502_REG_1++;                   /* Inc the loop counter */
                RESUME_PROCESS(Z502_REG_7, &Z502_REG_9);

        /*      Wait for children to finish, then quit                  */

           STEP( 14 )    
                SLEEP ( (INT32)10000 );

           STEP( 15 )    
                TERMINATE_PROCESS(-1, &Z502_REG_9);
                

    }                                           /* End of SELECT        */
}                                               /* End of test1f     */


/**************************************************************************

      Test1g  Generate lots of errors for CHANGE_PRIORITY    

      Try lots of different inputs: In particular, some of the possible
        inputs include:

                1. use of illegal priorities
                2. use of an illegal process id.

    
        Z502_REG_1              Target process ID
        Z502_REG_2              OUR process ID
        Z502_REG_9              Error returned

**************************************************************************/

#define         LEGAL_PRIORITY_1G               10
#define         ILLEGAL_PRIORITY_1G            999

void    test1g( void)
    {
    SELECT_STEP
       {
       STEP( 0 )                            /* Get OUR PID          */
       GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 ); 
 
       STEP( 1 )                            /* Make legal target    */
       printf( "Release %s:Test 1g: Pid %ld\n",  CURRENT_REL, Z502_REG_2 );
       CREATE_PROCESS( "test1g_a", test1x, LEGAL_PRIORITY_1G,
                                        &Z502_REG_1, &Z502_REG_9);
                
       STEP( 2 )
       success_expected( Z502_REG_9, "CREATE_PROCESS" );
                                                /* Target Illegal PID  */
       CHANGE_PRIORITY( (INT32)9999, LEGAL_PRIORITY_1G, &Z502_REG_9);
                
       STEP( 3 )
       error_expected( Z502_REG_9, "CHANGE_PRIORITY" );
                                                /* Use illegal priority */
       CHANGE_PRIORITY( Z502_REG_1, ILLEGAL_PRIORITY_1G, &Z502_REG_9);
                
       STEP( 4 )
       error_expected( Z502_REG_9, "CHANGE_PRIORITY" );
       // Change made - I assume both proce
       TERMINATE_PROCESS(-2, &Z502_REG_9);
                
    }                                             /* End of SELECT     */
}                                                 /* End of test1g     */

/**************************************************************************

        Test1h  Successfully change the priority of a process       

        When you change the priority, it should be possible to see
        the scheduling behaviour of the system change; processes
        that used to be scheduled first are no longer first.

    
        Z502_REG_2              OUR process ID
        Z502_REG_3 - 5          Target process IDs
        Z502_REG_9              Error returned

**************************************************************************/

#define         MOST_FAVORABLE_PRIORITY         1
#define         FAVORABLE_PRIORITY             10
#define         NORMAL_PRIORITY                20
#define         LEAST_FAVORABLE_PRIORITY       30

void    test1h( void)
    {
    INT32       ourself;
    SELECT_STEP
        {
           STEP( 0 )    /* Get OUR PID          */
                 GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 ); 
 
           STEP( 1 )    /* Make our priority high */
                 printf( "Release %s:Test 1h: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_2 );
                 ourself = -1;
                 CHANGE_PRIORITY( ourself, MOST_FAVORABLE_PRIORITY,
                                                        &Z502_REG_9);

           STEP( 2 )    /* Make legal targets   */
                 CREATE_PROCESS( "test1h_a", test1x, NORMAL_PRIORITY,
                                        &Z502_REG_3, &Z502_REG_9);
                
           STEP( 3 )    /* Make legal targets   */
                 CREATE_PROCESS( "test1h_b", test1x, NORMAL_PRIORITY,
                                        &Z502_REG_4, &Z502_REG_9);
                
           STEP( 4 )    /* Make legal targets   */
                 CREATE_PROCESS( "test1h_c", test1x, NORMAL_PRIORITY,
                                        &Z502_REG_5, &Z502_REG_9);

        /*      Sleep awhile to watch the scheduling                    */

           STEP( 5 )    
                 SLEEP( 200 );

        /*      Now change the priority - it should be possible to see
                that the priorities have been changed for processes that
                are ready and for processes that are sleeping.          */

           STEP( 6 )    
                 CHANGE_PRIORITY( Z502_REG_3, FAVORABLE_PRIORITY, 
                                        &Z502_REG_9);

           STEP( 7 )    
                 CHANGE_PRIORITY( Z502_REG_5, LEAST_FAVORABLE_PRIORITY, 
                                        &Z502_REG_9);

        /*      Sleep awhile to watch the scheduling                    */

           STEP( 8 )    
                 SLEEP( 200 );

        /*      Now change the priority - it should be possible to see
                that the priorities have been changed for processes that
                are ready and for processes that are sleeping.          */

           STEP( 9 )    
                 CHANGE_PRIORITY( Z502_REG_3, LEAST_FAVORABLE_PRIORITY, 
                                        &Z502_REG_9);

           STEP( 10 )    
                 CHANGE_PRIORITY( Z502_REG_4, FAVORABLE_PRIORITY, 
                                        &Z502_REG_9);

        /*      Sleep awhile to watch the scheduling                    */

           STEP( 11 )    
                 SLEEP( 600 );

           STEP( 12 )    
                 TERMINATE_PROCESS(-2, &Z502_REG_9);

    }                                             /* End of SELECT     */
}                                                 /* End of test1h     */
/**************************************************************************

        Test1i   SEND_MESSAGE and RECEIVE_MESSAGE with errors.      

        This has the same kind of error conditions that previous tests did;
        bad PIDs, bad message lengths, illegal buffer addresses, etc.  
        Your imagination can go WILD on this one.

        This is a good time to mention os_switch_context_complete:::::
        As you know, after doing a switch_context, the hardware passes 
        control to the code in this test.  What hasn't been obvious thus
        far, is that control passes from swich_context, THEN to routine,
        os_switch_context_complete, and THEN to this test code.

        What it does:     This function allows you to gain control in the
                          OS of a scheduled-in process before it goes to test.

        Where to find it: The code is in base.c - right now it does nothing.

        Why use it:        Suppose process A has sent a message to 
                           process B.  It so happens that you may well want
                           to do some preparation in process B once it's
                           registers are in memory, but BEFORE it executes
                           the test.  In other words, it allows you to 
                           complete the work for the send to process B.
    
        Z502_REG_1         Pointer to data private to each process
                                   running this routine.
        Z502_REG_2         OUR process ID
        Z502_REG_3         Target process IDs
        Z502_REG_9         Error returned

**************************************************************************/

#define         LEGAL_MESSAGE_LENGTH           (INT16)64
#define         ILLEGAL_MESSAGE_LENGTH         (INT16)1000

#define         MOST_FAVORABLE_PRIORITY         1
#define         NORMAL_PRIORITY                20

typedef struct
    {
    INT32    target_pid;
    INT32    source_pid;
    INT32    actual_source_pid;
    INT32    send_length;
    INT32    receive_length;
    INT32    actual_send_length;
    INT32    loop_count;
    char     msg_buffer[LEGAL_MESSAGE_LENGTH];
} TEST1I_DATA;

void    test1i( void)
    {
    TEST1I_DATA *td;                            /* Use as ptr to data */

    /* Here we maintain the data to be used by this process when running
       on this routine.  This code should be re-entrant.                */

    if ( Z502_REG_1 == 0 )
        {
        Z502_REG_1 = (long)calloc( 1, sizeof ( TEST1I_DATA ));
        if ( Z502_REG_1 == 0 )
            {
            printf( "Something screwed up allocating space in test1i\n" );
        }
        td = ( TEST1I_DATA *)Z502_REG_1;
        td->loop_count = 0;
    }
    td = ( TEST1I_DATA *)Z502_REG_1;

while ( 1 )
    {
    SELECT_STEP
    {
    STEP( 0 )    /* Get OUR PID         */
    GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 ); 
 
    STEP( 1 )    /* Make our prior high */
    printf( "Release %s:Test 1i: Pid %ld\n", CURRENT_REL, Z502_REG_2 );
    CHANGE_PRIORITY( -1, MOST_FAVORABLE_PRIORITY,   &Z502_REG_9);

    STEP( 2 )    /* Make legal targets  */
    CREATE_PROCESS( "test1i_a", test1x, NORMAL_PRIORITY,
                                        &Z502_REG_3, &Z502_REG_9);
                
    STEP( 3 )         /*      Send to illegal process            */
    td->target_pid = 9999;
    td->send_length= 8;
    SEND_MESSAGE( td->target_pid, "message", td->send_length, &Z502_REG_9 );
                
    STEP( 4 )    
    error_expected( Z502_REG_9, "SEND_MESSAGE" );

    /* Try an illegal message length                        */

    td->target_pid = Z502_REG_3;
    td->send_length= ILLEGAL_MESSAGE_LENGTH;
    SEND_MESSAGE( td->target_pid, "message", td->send_length, &Z502_REG_9 );
                
    STEP( 5 )    
    error_expected( Z502_REG_9, "SEND_MESSAGE" );

    /*      Receive from illegal process                    */

    td->source_pid = 9999;
    td->receive_length = LEGAL_MESSAGE_LENGTH;
    RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                     td->receive_length, &(td->actual_send_length), 
                     &(td->actual_source_pid), &Z502_REG_9 );

    STEP( 6 )    
    error_expected( Z502_REG_9, "RECEIVE_MESSAGE" );

    /*      Receive with illegal buffer size                */

    td->source_pid = Z502_REG_3;
    td->receive_length = ILLEGAL_MESSAGE_LENGTH;
    RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                     td->receive_length, &(td->actual_send_length), 
                     &(td->actual_source_pid), &Z502_REG_9 );

    STEP( 7 )    
    error_expected( Z502_REG_9, "RECEIVE_MESSAGE" );

    /*      Send a legal ( but long ) message to self       */

    td->target_pid = Z502_REG_2;
    td->send_length= LEGAL_MESSAGE_LENGTH;
    SEND_MESSAGE( td->target_pid, "a long but legal message", 
                  td->send_length, &Z502_REG_9 );

    STEP( 8 )    
    success_expected( Z502_REG_9, "SEND_MESSAGE" );
    td->loop_count++;

    /*      Receive this long message, which should error
            because the receive buffer is too small         */

    td->source_pid = Z502_REG_2;
    td->receive_length = 10;
    RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                     td->receive_length, &(td->actual_send_length), 
                     &(td->actual_source_pid), &Z502_REG_9 );

    STEP( 9 )    
    error_expected( Z502_REG_9, "RECEIVE_MESSAGE" );
    break;

    /*      Keep sending legal messages until the architectural 
            limit for buffer space is exhausted.  In order to pass
            the  test1j, this number should be at least EIGHT     */

    STEP( 10 )    
    td->target_pid = Z502_REG_3;
    td->send_length= LEGAL_MESSAGE_LENGTH;
    SEND_MESSAGE( td->target_pid, "message", td->send_length, &Z502_REG_9 );
                
    STEP( 11 )    
    if (Z502_REG_9 == ERR_SUCCESS)
           GO_NEXT_TO( 10 )      /* Loop back    */
    td->loop_count++;
    break;

    STEP( 12 )    
    printf( "A total of %d messages were enqueued.\n", td->loop_count - 1 );
    TERMINATE_PROCESS(-2, &Z502_REG_9);

    }                                             /* End of SELECT     */
    }                                             /* End of while      */
}                                                 /* End of test1i     */
                
/**************************************************************************

        Test1j   SEND_MESSAGE and RECEIVE_MESSAGE Successfully.

        Creates three other processes, each running their own code.
        RECEIVE and SEND messages are winged back and forth at them.
    
        Z502_REG_1              Pointer to data private to each process
                                   running this routine.
        Z502_REG_2              OUR process ID
        Z502_REG_3 - 5          Target process IDs
        Z502_REG_9              Error returned

        Again, as mentioned in detail on Test1i, use of the code in
        os_switch_context_complete could be beneficial here.  In fact 
	it will be difficult to make the test work successfully UNLESS
	you use the  os_switch_context_complete().

        The SEND and RECEIVE system calls as implemented by this test
        imply the following behavior:

          SENDER = PID A          RECEIVER = PID B, 

                               Designates source_pid =
           target_pid =          A            C             -1
          ----------------+------------+------------+--------------+
                          |            |            |              |
               B          |  Message   |     X      |   Message    |
                          |Transmitted |            | Transmitted  |
          ----------------+------------+------------+--------------+
                          |            |            |              |
               C          |     X      |     X      |       X      |
                          |            |            |              |
          ----------------+------------+------------+--------------+
                          |            |            |              |
              -1          |   Message  |     X      |   Message    |
                          | Transmitted|            | Transmitted  |
          ----------------+------------+------------+--------------+
        A broadcast ( target_pid = -1 ) means send to everyone BUT yourself.
        ANY of the receiving processes can handle a broadcast message.
        A receive ( source_pid = -1 ) means receive from anyone.
**************************************************************************/

#define         LEGAL_MESSAGE_LENGTH            (INT16)64
#define         ILLEGAL_MESSAGE_LENGTH          (INT16)1000
#define         MOST_FAVORABLE_PRIORITY         1
#define         NORMAL_PRIORITY                20

typedef struct
    {
    INT32    target_pid;
    INT32    source_pid;
    INT32    actual_source_pid;
    INT32    send_length;
    INT32    receive_length;
    INT32    actual_send_length;
    INT32    send_loop_count;
    INT32    receive_loop_count; 
    char     msg_buffer[LEGAL_MESSAGE_LENGTH];
    char     msg_sent[LEGAL_MESSAGE_LENGTH];
} TEST1J_DATA;

void    test1j( void)
    {
    TEST1J_DATA *td;                            /* Use as ptr to data */

    /* Here we maintain the data to be used by this process when running
       on this routine.  This code should be re-entrant.                */

    if ( Z502_REG_1 == 0 )
        {
        Z502_REG_1 = (long)calloc( 1, sizeof ( TEST1J_DATA ));
        if ( Z502_REG_1 == 0 )
            {
            printf( "Something screwed up allocating space in test1j\n" );
        }
        td = ( TEST1J_DATA *)Z502_REG_1;
        td->send_loop_count = 0;
        td->receive_loop_count = 0;
    }
    td = ( TEST1J_DATA *)Z502_REG_1;


while ( 1 )
    {
    SELECT_STEP
        {
           STEP( 0 )    /* Get OUR PID         */
                GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 ); 
 
           STEP( 1 )    /* Make our prior high */
                printf( "Release %s:Test 1j: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_2 );
                CHANGE_PRIORITY( -1, MOST_FAVORABLE_PRIORITY,
                                                        &Z502_REG_9);

           STEP( 2 )    /* Make legal targets  */
                success_expected( Z502_REG_9, "CHANGE_PRIORITY" );
                CREATE_PROCESS( "test1j_1", test1j_echo, NORMAL_PRIORITY,
                                        &Z502_REG_3, &Z502_REG_9);
                
           STEP( 3 )    /* Make legal targets  */
                success_expected( Z502_REG_9, "CREATE_PROCESS" );
                CREATE_PROCESS( "test1j_2", test1j_echo, NORMAL_PRIORITY,
                                        &Z502_REG_4, &Z502_REG_9);
                
           STEP( 4 )    /* Make legal targets  */
                success_expected( Z502_REG_9, "CREATE_PROCESS" );
                CREATE_PROCESS( "test1j_3", test1j_echo, NORMAL_PRIORITY,
                                        &Z502_REG_5, &Z502_REG_9);

           STEP( 5 )    
                success_expected( Z502_REG_9, "CREATE_PROCESS" );

                /*      Send/receive a legal message to each child    */

                td->target_pid = Z502_REG_3;
                td->send_length= 20;
                strcpy( td->msg_sent, "message to #3" );
                SEND_MESSAGE( td->target_pid, td->msg_sent, 
                              td->send_length, &Z502_REG_9 );

           STEP( 6 )    
                success_expected( Z502_REG_9, "SEND_MESSAGE" );
                td->source_pid = -1;
                td->receive_length = LEGAL_MESSAGE_LENGTH;

                RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                        td->receive_length, &(td->actual_send_length), 
                        &(td->actual_source_pid), &Z502_REG_9 );

           STEP( 7 )    
                success_expected( Z502_REG_9, "RECEIVE_MESSAGE" );

                if ( strcmp( td->msg_buffer, td->msg_sent ) != 0 )
                    printf("ERROR - msg sent != msg received.\n");

                if ( td->actual_source_pid != Z502_REG_3 )
                    printf( "ERROR - source PID not correct.\n" );

                if ( td->actual_send_length != td->send_length )
                    printf( "ERROR - send length not sent correctly.\n" );

                td->target_pid = Z502_REG_4;
                td->send_length= 20;
                strcpy( td->msg_sent, "message to #4" );
                SEND_MESSAGE( td->target_pid, td->msg_sent, 
                              td->send_length, &Z502_REG_9 );

           STEP( 8 )    
                success_expected( Z502_REG_9, "SEND_MESSAGE" );
                td->source_pid = -1;
                td->receive_length = LEGAL_MESSAGE_LENGTH;

                RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                        td->receive_length, &(td->actual_send_length), 
                        &(td->actual_source_pid), &Z502_REG_9 );

           STEP( 9 )    
                success_expected( Z502_REG_9, "RECEIVE_MESSAGE" );
                if ( strcmp( td->msg_buffer, td->msg_sent ) != 0 )
                    printf("ERROR - msg sent != msg received.\n");
                if ( td->actual_source_pid != Z502_REG_4 )
                    printf( "ERROR - source PID not correct.\n" );
                if ( td->actual_send_length != td->send_length )
                    printf( "ERROR - send length not sent correctly.\n" );

                td->target_pid = Z502_REG_5;
                td->send_length= 20;
                strcpy( td->msg_sent, "message to #5" );
                SEND_MESSAGE( td->target_pid, td->msg_sent, 
                              td->send_length, &Z502_REG_9 );

           STEP( 10 )    
                success_expected( Z502_REG_9, "SEND_MESSAGE" );
                td->source_pid = -1;
                td->receive_length = LEGAL_MESSAGE_LENGTH;
                RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                        td->receive_length, &(td->actual_send_length), 
                        &(td->actual_source_pid), &Z502_REG_9 );

           STEP( 11 )    
                success_expected( Z502_REG_9, "RECEIVE_MESSAGE" );
                if ( strcmp( td->msg_buffer, td->msg_sent ) != 0 )
                    printf("ERROR - msg sent != msg received.\n");
                if ( td->actual_source_pid != Z502_REG_5 )
                    printf( "ERROR - source PID not correct.\n" );
                if ( td->actual_send_length != td->send_length )
                    printf( "ERROR - send length not sent correctly.\n" );
                break;       // Bugfix 08/2012 - Rel 3.60 so we explicitly
		             // go to the next step
        /*      Keep sending legal messages until the architectural (OS)
                limit for buffer space is exhausted.                    */

           STEP( 12 )    
                td->target_pid = -1;
                sprintf( td->msg_sent, "This is message %d", 
                                                td->send_loop_count );
                td->send_length= 20;
                SEND_MESSAGE( td->target_pid, td->msg_sent, 
                              td->send_length, &Z502_REG_9 );
              
           STEP( 13 )    
                if (Z502_REG_9 == ERR_SUCCESS)
                    GO_NEXT_TO( 12 )      /* Loop back    */
                td->send_loop_count++;
                break;

           STEP( 14 )    
                td->send_loop_count--;
                printf( "A total of %d messages were enqueued.\n",
                                                td->send_loop_count );
                break;

           STEP( 15 )    
                td->source_pid = -1;
                td->receive_length = LEGAL_MESSAGE_LENGTH;
                RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                        td->receive_length, &(td->actual_send_length), 
                        &(td->actual_source_pid), &Z502_REG_9 );

           STEP( 16 )    
                success_expected( Z502_REG_9, "RECEIVE_MESSAGE" );

                printf( "Receive from PID = %d: length = %d: msg = %s:\n",
                        td->actual_source_pid, td->actual_send_length, 
                        td->msg_buffer );
                td->receive_loop_count++; 

                if ( td->receive_loop_count < td->send_loop_count )
                    GO_NEXT_TO( 15 )          /* Loop back    */
                break;

           STEP( 17 )    
                printf( "A total of %d messages were received.\n",
                                        td->receive_loop_count - 1 );
                TERMINATE_PROCESS(-2, &Z502_REG_9);


        }                                         /* End of SELECT     */
    }                                             /* End of while      */
}                                                 /* End of test1j     */

/**************************************************************************

      Test1k  Test other oddities in your system.   


        There are many other strange effects, not taken into account
        by the previous tests.  One of these is:

         1. Executing a privileged instruction from a user program
        
          Registers Used:
        Z502_REG_2              OUR process ID
        Z502_REG_9              Error returned
    
**************************************************************************/

void    test1k( void)
    {
    INT32  Result;

    SELECT_STEP
        {
        STEP( 0 )
        GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 );
             
        STEP( 1 )
        printf( "Release %s:Test 1k: Pid %ld\n", CURRENT_REL, Z502_REG_2 );

        /*      Do an illegal hardware instruction - we will 
                not return from this.                                   */

        MEM_READ( Z502TimerStatus, &Result);
    }                                             /* End of SELECT     */
}                                                 /* End of test1k     */

/**************************************************************************

        Test1l   SEND_MESSAGE and RECEIVE_MESSAGE with SUSPEND/RESUME

        Explores how message handling is done in the midst of SUSPEND/RESUME
        system calls,
 
        The SEND and RECEIVE system calls as implemented by this test
        imply the following behavior:

        Case 1:
         - a process waiting to recieve a message can be suspended
         - a process waiting to recieve a message can be resumed
         - after being resumed, it can receive a message

        Case 2: 
         - when a process waiting for a message is suspended, it is out of 
           circulation and cannot recieve any message
         - once it is unsuspended, it may recieve a message and go on the ready 
           queue

        Case 3:
         - a process that waited for and found a message is now the ready queue
         - this process can be suspended before handling the message
         - the message and process remain paired up, no other process can have 
           that message
         - when resumed, the process will handle the message

**************************************************************************/

#define         LEGAL_MESSAGE_LENGTH            (INT16)64
#define         MOST_FAVORABLE_PRIORITY         1
#define         NORMAL_PRIORITY                20

typedef struct
    {
    INT32    target_pid;
    INT32    source_pid;
    INT32    actual_source_pid;
    INT32    send_length;
    INT32    receive_length;
    INT32    actual_send_length;
    INT32    send_loop_count;
    INT32    receive_loop_count; 
    char     msg_buffer[LEGAL_MESSAGE_LENGTH];
    char     msg_sent[LEGAL_MESSAGE_LENGTH];
} TEST1L_DATA;


void    test1l( void)
    {
    TEST1L_DATA *td;                            /* Use as ptr to data */

    /* Here we maintain the data to be used by this process when running
       on this routine.  This code should be re-entrant.                */

    if ( Z502_REG_1 == 0 )
        {
        Z502_REG_1 = (long)calloc( 1, sizeof ( TEST1L_DATA ));
        if ( Z502_REG_1 == 0 )
            {
            printf( "Something screwed up allocating space in test1j\n" );
        }
        td = ( TEST1L_DATA *)Z502_REG_1;
        td->send_loop_count = 0;
        td->receive_loop_count = 0;
    }
    td = ( TEST1L_DATA *)Z502_REG_1;


while ( 1 )
    {
    SELECT_STEP
        {
       STEP( 0 )    /* Get OUR PID         */
            GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 ); 

       STEP( 1 )    /* Make our prior high */
            printf( "Release %s:Test 1l: Pid %ld\n", 
                                            CURRENT_REL, Z502_REG_2 );
            CHANGE_PRIORITY( -1, MOST_FAVORABLE_PRIORITY,
                                                &Z502_REG_9);

       STEP( 2 )    /* Make process to test with  */
            success_expected( Z502_REG_9, "CHANGE_PRIORITY" );
            CREATE_PROCESS( "test1l_1", test1j_echo, NORMAL_PRIORITY,
                                    &Z502_REG_3, &Z502_REG_9);
                            
       STEP( 3 )    
            success_expected( Z502_REG_9, "CREATE_PROCESS" );

            /* BEGIN CASE 1 ------------------------------------------*/
            printf("\n\nBegin Case 1:\n\n");

            /*      Sleep so that first process will wake and receive  */
            SLEEP ( 200 );

       STEP( 4 )
            GET_PROCESS_ID( "test1l_1", &Z502_REG_4, &Z502_REG_9 ); 

       STEP( 5 )    
            success_expected( Z502_REG_9, "Get Receiving Process ID" );
            if ( Z502_REG_3 != Z502_REG_4 )
              printf("ERROR!  The process ids should match! New process ID is: %ld", Z502_REG_4);
            
            /*      Suspend the receiving process */
            SUSPEND_PROCESS(Z502_REG_3, &Z502_REG_9);

       STEP( 6 )    
            success_expected( Z502_REG_9, "SUSPEND" );

            /* Resume the recieving process */
            RESUME_PROCESS(Z502_REG_3, &Z502_REG_9);

       STEP( 7 )    
            success_expected( Z502_REG_9, "RESUME" );

            /* Send it a message */
            td->target_pid = Z502_REG_3;
            td->send_length= 30;
            strcpy( td->msg_sent, "Resume first echo" );
            SEND_MESSAGE( td->target_pid, td->msg_sent, 
                          td->send_length, &Z502_REG_9 );

       STEP( 8 )    
            success_expected( Z502_REG_9, "SEND_MESSAGE" );
            /* Receive it's response (process is now back in recieving mode) */
            td->source_pid = -1;
            td->receive_length = LEGAL_MESSAGE_LENGTH;
            RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                        td->receive_length, &(td->actual_send_length), 
                        &(td->actual_source_pid), &Z502_REG_9 );

       STEP( 9 )    
            success_expected( Z502_REG_9, "RECEIVE_MESSAGE" );
            if ( strcmp( td->msg_buffer, td->msg_sent ) != 0 )
                printf("ERROR - msg sent != msg received.\n");
            if ( td->actual_source_pid != Z502_REG_3 )
                printf( "ERROR - source PID not correct.\n" );
            if ( td->actual_send_length != td->send_length )
                printf( "ERROR - send length not sent correctly.\n" );

        /* BEGIN CASE 2 ------------------------------------------*/
            printf("\n\nBegin Case 2:\n\n");

        /* create a competitor to show suspend works with incoming messages */
            CREATE_PROCESS( "test1l_2", test1j_echo, NORMAL_PRIORITY,
                                        &Z502_REG_5, &Z502_REG_9);

       STEP( 10 )    
            success_expected( Z502_REG_9, "CREATE_PROCESS" );
 
        /*      Sleep so that the new process will wake and receive  */
            SLEEP ( 200 );

       STEP( 11 )
            GET_PROCESS_ID( "test1l_2", &Z502_REG_6, &Z502_REG_9 ); 

       STEP( 12 )    
            success_expected( Z502_REG_9, "Get Receiving Process ID" );
            if ( Z502_REG_5 != Z502_REG_6 )
              printf("ERROR!  The process ids should match! New process ID is: %ld", Z502_REG_4);

        /*      Suspend the first process */
            SUSPEND_PROCESS(Z502_REG_3, &Z502_REG_9);

       STEP( 13 )    
            success_expected( Z502_REG_9, "SUSPEND" );

            /* Send anyone a message */
            td->target_pid = -1;
            td->send_length= 30;
            strcpy( td->msg_sent, "Going to second process" );
            SEND_MESSAGE( td->target_pid, td->msg_sent, 
                          td->send_length, &Z502_REG_9 );
       STEP( 14 )    
            success_expected( Z502_REG_9, "SEND_MESSAGE" );
 
        /* Resume the first process */
            RESUME_PROCESS(Z502_REG_3, &Z502_REG_9);

       STEP( 15 )    
            success_expected( Z502_REG_9, "RESUME" );

        /* Receive the second process' response */
            td->source_pid = Z502_REG_5;
            td->receive_length = LEGAL_MESSAGE_LENGTH;
            RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                    td->receive_length, &(td->actual_send_length), 
                    &(td->actual_source_pid), &Z502_REG_9 );

       STEP( 16 )    
            success_expected( Z502_REG_9, "RECEIVE_MESSAGE" );

            if ( strcmp( td->msg_buffer, td->msg_sent ) != 0 )
                printf("ERROR - msg sent != msg received.\n");
            if ( td->actual_source_pid != Z502_REG_5 )
                printf( "ERROR - source PID not correct.\n" );
            if ( td->actual_send_length != td->send_length )
                printf( "ERROR - send length not sent correctly.\n" );


        /* BEGIN CASE 3 ------------------------------------------*/
            printf("\n\nBegin Case 3:\n\n");

       /*      Suspend the first process */
            SUSPEND_PROCESS(Z502_REG_3, &Z502_REG_9);

       STEP( 17 )    
            success_expected( Z502_REG_9, "SUSPEND" );

        /* Send it, specifically, a message */
            td->target_pid = Z502_REG_3;
            td->send_length= 30;
            strcpy( td->msg_sent, "Going to suspended" );
            SEND_MESSAGE( td->target_pid, td->msg_sent, 
                          td->send_length, &Z502_REG_9 );

       STEP( 18 )    
            success_expected( Z502_REG_9, "SEND_MESSAGE" );
 
        /* Resume the first process */
            RESUME_PROCESS(Z502_REG_3, &Z502_REG_9);

       STEP( 19 )    
            success_expected( Z502_REG_9, "RESUME" );

            /* Receive the process' response */
            td->source_pid = Z502_REG_3;
            td->receive_length = LEGAL_MESSAGE_LENGTH;
            RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                    td->receive_length, &(td->actual_send_length), 
                    &(td->actual_source_pid), &Z502_REG_9 );

       STEP( 20 )    
            success_expected( Z502_REG_9, "RECEIVE_MESSAGE" );

            if ( strcmp( td->msg_buffer, td->msg_sent ) != 0 )
                printf("ERROR - msg sent != msg received.\n");
            if ( td->actual_source_pid != Z502_REG_3 )
                printf( "ERROR - source PID not correct.\n" );
            if ( td->actual_send_length != td->send_length )
                printf( "ERROR - send length not sent correctly.\n" );

            TERMINATE_PROCESS(-2, &Z502_REG_9);

        }                                         // End of SELECT 
    }                                             // End of while 
}                                                 // End of test1l

/**************************************************************************


        Test 1m   
        
        Simulates starvation by running multiple copies of test1x simultaneously.
        Test1c runs these with the same priority in order to show
        FCFS scheduling behavior; Test1d uses different priorities
        in order to show priority scheduling.  Test 1m, by comparison, uses 
        more processes of low priority to show the rising needs of processes 9 and 10              

        Z502_REG_1, 2, 3, 4, 5  Used as return of process id's.
        Z502_REG_6              Return of PID on GET_PROCESS_ID
        Z502_REG_9              Used as return of error code.

**************************************************************************/

#define         PRIORITY1               10
#define         PRIORITY2               11
#define         PRIORITY3               11
#define         PRIORITY4               90
#define         PRIORITY5               40

void    test1m( void)
    {
    static INT32      sleep_time      = 1000;

    while( 1 )
        {
        SELECT_STEP
            {
           STEP( 0 )
                printf( "This is Release %s:  Test 1m\n", CURRENT_REL );
                CREATE_PROCESS( "test1m_1", test1x, PRIORITY1, 
                                                &Z502_REG_1, &Z502_REG_9 );
        
           STEP( 1 )
                success_expected( Z502_REG_9, "CREATE_PROCESS" );
                CREATE_PROCESS( "test1m_2", test1x, PRIORITY1, 
                                                &Z502_REG_2, &Z502_REG_9 );
                
           STEP( 2 )
                CREATE_PROCESS( "test1m_3", test1x, PRIORITY2, 
                                                &Z502_REG_3, &Z502_REG_9 );

           STEP( 3 )
                CREATE_PROCESS( "test1m_4", test1x, PRIORITY2, 
                                                &Z502_REG_4, &Z502_REG_9 );
        
           STEP( 4 )
                CREATE_PROCESS( "test1m_5", test1x, PRIORITY3, 
                                                &Z502_REG_2, &Z502_REG_9 );
                
           STEP( 5 )
                CREATE_PROCESS( "test1m_6", test1x, PRIORITY3, 
                                                &Z502_REG_3, &Z502_REG_9 );

           STEP( 6 )
                CREATE_PROCESS( "test1m_7", test1x, PRIORITY4, 
                                                &Z502_REG_4, &Z502_REG_9 );
                
                


/*  We will loop until the target process ( test1m_9 ) has terminated.  
    We know it terminated because for a while we get success on the call
    GET_PROCESS_ID, and then failure when the process no longer exists. */

           STEP( 7 )
                SLEEP ( sleep_time );
                
           STEP( 8 )
                GET_PROCESS_ID( "test1m_9", &Z502_REG_6, &Z502_REG_9 );
                
           STEP( 9 )
                if ( Z502_REG_9 == ERR_SUCCESS )
                    GO_NEXT_TO( 5 )                    /* Loop back */
                break;

           STEP( 10 )
                TERMINATE_PROCESS( -1, &Z502_REG_9 );
                
        }                                       // End switch   
    }                                           // End while   
}                                               // End test1m

/**************************************************************************

      Test1x 
      
        is used as a target by the process creation programs.
        It has the virtue of causing lots of rescheduling activity in
        a relatively random way.                                         

    
        Z502_REG_1              Loop counter
        Z502_REG_2              OUR process ID
        Z502_REG_3              Starting time
        Z502_REG_4              Ending time
        Z502_REG_9              Error returned

**************************************************************************/


#define         NUMBER_OF_TEST1X_ITERATIONS     10

void    test1x( void )
    {
    static INT32          sleep_time = 17;

    while( 1 )
        {
        SELECT_STEP
            {
           STEP( 0 )
                GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 );
             
           STEP( 1 )
                printf( "Release %s:Test 1x: Pid %ld\n", 
                                        CURRENT_REL, Z502_REG_2 );
                break;
           STEP( 2 )
                GET_TIME_OF_DAY( &Z502_REG_3 );
                    
           STEP( 3 )
                SLEEP ( ( sleep_time * Z502_REG_3 ) % 143 );  /* random*/
                    
           STEP( 4 )
                GET_TIME_OF_DAY( &Z502_REG_4 );
                    
           STEP( 5 )
                if ( Z502_REG_1 <= NUMBER_OF_TEST1X_ITERATIONS )
                    GO_NEXT_TO( 2 )
                else
                    {
                    printf( "Test1x, PID %ld, Ends at Time %ld\n",
                                 Z502_REG_2, Z502_REG_4 );
                    TERMINATE_PROCESS( -1, &Z502_REG_9 );
                }
                Z502_REG_1++;                       /* Inc loop cntr */
                break; 

           STEP( 6 )
                printf( "ERROR: Test should be terminated but isn't.\n");
                break;
        }                                       /* End of while     */
    }                                           /* End of SELECT    */
}                                               /* End of test1x    */

/**************************************************************************

      Test1j_echo
      
        is used as a target by the message send/receive programs.
        All it does is send back the same message it received to the
        same sender.
    
        Z502_REG_1              Pointer to data private to each process
                                   running this routine.
        Z502_REG_2              OUR process ID
        Z502_REG_3              Starting time
        Z502_REG_4              Ending time
        Z502_REG_9              Error returned
**************************************************************************/

typedef struct
    {
    INT32    target_pid;
    INT32    source_pid;
    INT32    actual_source_pid;
    INT32    send_length;
    INT32    receive_length;
    INT32    actual_senders_length;
    char     msg_buffer[LEGAL_MESSAGE_LENGTH];
    char     msg_sent[LEGAL_MESSAGE_LENGTH];
} TEST1J_ECHO_DATA;

void    test1j_echo( void)
    {
    TEST1J_ECHO_DATA *td;                               /* Use as ptr to data */

    /* Here we maintain the data to be used by this process when running
       on this routine.  This code should be re-entrant.                */

    if ( Z502_REG_1 == 0 )
        {
        Z502_REG_1 = (long)calloc( 1, sizeof ( TEST1J_ECHO_DATA ));
        if ( Z502_REG_1 == 0 )
            {
            printf( "Something screwed up allocating space in test1j_echo\n" );
        }
    }
    td = ( TEST1J_ECHO_DATA *)Z502_REG_1;


    while( 1 )
        {
        SELECT_STEP
            {
           STEP( 0 )
                GET_PROCESS_ID( "", &Z502_REG_2, &Z502_REG_9 );
             
           STEP( 1 )
                success_expected( Z502_REG_9, "GET_PROCESS_ID" );
                printf( "Release %s:Test 1j_echo: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_2 );
           STEP( 2 )
                td->source_pid = -1;
                td->receive_length = LEGAL_MESSAGE_LENGTH;
                RECEIVE_MESSAGE( td->source_pid, td->msg_buffer, 
                        td->receive_length, &(td->actual_senders_length), 
                        &(td->actual_source_pid), &Z502_REG_9 );
             
           STEP( 3 )
                success_expected( Z502_REG_9, "RECEIVE_MESSAGE" );
                printf( "Receive from PID = %d: length = %d: msg = %s:\n",
                        td->actual_source_pid, td->actual_senders_length, 
                        td->msg_buffer );
                                                        
                td->target_pid = td->actual_source_pid;
                strcpy( td->msg_sent, td->msg_buffer );
                td->send_length= td->actual_senders_length;
                SEND_MESSAGE( td->target_pid, td->msg_sent, 
                              td->send_length, &Z502_REG_9 );

           STEP( 4 )
                success_expected( Z502_REG_9, "SEND_MESSAGE" );
                GO_NEXT_TO( 2 )
                break;
        }                                       /* End of while     */
    }                                           /* End of SELECT    */
}                                               /* End of test1j_echo*/

/**************************************************************************

      error_expected    and    success_expected

        These routines simply handle the display of success/error data.

**************************************************************************/

void    error_expected( INT32 error_code, char sys_call[] )
    {
    if ( error_code == ERR_SUCCESS)
        {
        printf( "An Error SHOULD have occurred.\n" );
        printf("????: Error( %d ) occurred in case %d (%s)\n",
              error_code, Z502_PROGRAM_COUNTER-2, sys_call );
    }
    else
        printf( "Program correctly returned an error: %d\n", error_code );

}                                               /* End of error_expected */



void    success_expected( INT32 error_code, char sys_call[] )
    {
    if ( error_code != ERR_SUCCESS)
        {
        printf( "An Error should NOT have occurred.\n" );
        printf("????: Error( %d ) occurred in case %d (%s)\n",
              error_code, Z502_PROGRAM_COUNTER-2, sys_call );
    }
    else
        printf( "Program correctly returned success.\n" );

}                                            /* End of success_expected */


/**************************************************************************

      Test2a exercises a simple memory write and read             

        Use:  Z502_REG_1                data_written
              Z502_REG_2                data_read
              Z502_REG_3                address
              Z502_REG_4                process_id
              Z502_REG_9                error                    

      In global.h, there's a variable  DO_MEMORY_DEBUG.   Switching it to
      TRUE will allow you to see what the memory system thinks is happening.
      WARNING - it's verbose -- and I don't want to see such output - it's
      strictly for your debugging pleasure.
**************************************************************************/

void    test2a( void )
    {

    SELECT_STEP
        {
       STEP( 0 )
            GET_PROCESS_ID( "", &Z502_REG_4, &Z502_REG_9 );
           
       STEP( 1 )
            printf( "Release %s:Test 2a: Pid %ld\n", 
                    CURRENT_REL, Z502_REG_4 );
            Z502_REG_3 = 412;
            Z502_REG_1 = Z502_REG_3 + Z502_REG_4;
            MEM_WRITE( Z502_REG_3, &Z502_REG_1 );
            
       STEP( 2 )
            MEM_READ( Z502_REG_3, &Z502_REG_2 );
            
       STEP( 3 ) 
            printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                        Z502_REG_4, Z502_REG_3, Z502_REG_1, Z502_REG_2);
            if ( Z502_REG_2 != Z502_REG_1 )
                    printf( "AN ERROR HAS OCCURRED.\n" );
            TERMINATE_PROCESS( -1, &Z502_REG_9 );
            
    }                                           /* End of SELECT    */
}                                               /* End of test2a    */


/**************************************************************************

        Test2b 
        
        exercises simple memory writes and reads.  Watch out,
        the addresses used are diabolical and are designed to show
        unusual features of your memory management system.          

        Use:  Z502_REG_1                data_written
              Z502_REG_2                data_read
              Z502_REG_3                address
              Z502_REG_4                process_id
              Z502_REG_5                test_data_index             
              Z502_REG_9                error

        The following registers are used for sanity checks - after each
        read/write pair, we will read back the first set of data to make
        sure it's still there.

              Z502_REG_6                First data written
              Z502_REG_7                First data read
              Z502_REG_8                First address                   

**************************************************************************/

#define         TEST_DATA_SIZE          (INT16)7

void    test2b( void )
    {
    static INT32        test_data[TEST_DATA_SIZE] = {
                        0, 4, PGSIZE - 2, PGSIZE, 3 * PGSIZE - 2,
                        (VIRTUAL_MEM_PGS - 1) * PGSIZE,
                        VIRTUAL_MEM_PGS * PGSIZE - 2};

    while(1)
        {
        SELECT_STEP
            {
           STEP( 0 )
                GET_PROCESS_ID( "", &Z502_REG_4, &Z502_REG_9 );
                
           STEP( 1 )
                Z502_REG_8 = 5 * PGSIZE;
                Z502_REG_6 = Z502_REG_8 + Z502_REG_4 + 7;
                MEM_WRITE( Z502_REG_8, &Z502_REG_6 );
                printf( "\n\nRelease %s:Test 2b: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_4 );
                break;

           STEP( 2 )
                Z502_REG_3 = test_data[ (INT16)Z502_REG_5 ];
                Z502_REG_1 = Z502_REG_3 + Z502_REG_4 + 27;
                MEM_WRITE( Z502_REG_3, &Z502_REG_1 );
                
           STEP( 3 )
                MEM_READ( Z502_REG_3, &Z502_REG_2 );


           STEP( 4 )
                printf("PID= %ld  address= %ld  written= %ld   read= %ld\n",
                        Z502_REG_4, Z502_REG_3, Z502_REG_1, Z502_REG_2);
                if (Z502_REG_2 != Z502_REG_1 )
                    printf( "AN ERROR HAS OCCURRED.\n" );

                /*      Go back and check earlier write                 */

                MEM_READ( Z502_REG_8, &Z502_REG_7 );
                
           STEP( 5 )
                GO_NEXT_TO( 2 )
                printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                        Z502_REG_4, Z502_REG_8, Z502_REG_6, Z502_REG_7);
                if (Z502_REG_6 != Z502_REG_7 )
                    printf( "AN ERROR HAS OCCURRED.\n" );
                Z502_REG_5++;
                break;
        }                                       /* End of SELECT    */
    }                                           /* End of while     */
}                                               /* End of test2b    */


/**************************************************************************

        Test2c causes usage of disks.  The test is designed to give
        you a chance to develop a mechanism for handling disk requests.

    Z502_REG_1  - data that was written.
    Z502_REG_2  - data that was read from memory.
    Z502_REG_3  - address where data was written/read.
    Z502_REG_4  - process id of this process.
    Z502_REG_6  - number of iterations/loops through the code.
    Z502_REG_7  - which page will the write/read be on. start at 0  
    Z502_REG_9  - returned error code.

    Many people find it helpful to use os_switch_context_complete in order
    to wrap-up disk requests before returning to the test code.  See the
    description at test1i.
**************************************************************************/

#define         DISPLAY_GRANULARITY2c           10

typedef         union
    {
    char        char_data[ PGSIZE ];
    UINT32      int_data[ PGSIZE/ sizeof(int) ];
} DISK_DATA;

void    test2c( void )
    {
    DISK_DATA   *data_written;
    DISK_DATA   *data_read;
    INT16       disk_id;
    INT16       sanity  = 1234;
    INT16       sector;

    if ( Z502_REG_1 == 0 )
        {
        Z502_REG_1 = ( long )calloc( 1, sizeof( DISK_DATA ) );
        Z502_REG_2 = ( long )calloc( 1, sizeof( DISK_DATA ) );
        if ( Z502_REG_2 == 0 )
            printf( "Something screwed up allocating space in test2c\n");
    }
    data_written = (DISK_DATA *)Z502_REG_1;
    data_read    = (DISK_DATA *)Z502_REG_2;


    while(1)
        {
        SELECT_STEP
           {
           STEP( 0 )
                GET_PROCESS_ID( "", &Z502_REG_4, &Z502_REG_9 );
                
           STEP( 1 )
                Z502_REG_6 = Z502_REG_4;
                printf( "\n\nRelease %s:Test 2c: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_4 );
                break;
           STEP( 2 )
                disk_id  = ( Z502_REG_4 / 2 ) % MAX_NUMBER_OF_DISKS + 1;
                sector   = Z502_REG_6 % NUM_LOGICAL_SECTORS;
                data_written->int_data[0] = disk_id;
                data_written->int_data[1] = sanity;
                data_written->int_data[2] = sector;
                data_written->int_data[3] = (int)Z502_REG_4;

                DISK_WRITE( disk_id, sector, (char*)(data_written->char_data ) );
                
           STEP( 3 )
                disk_id  = ( Z502_REG_4 / 2 ) % MAX_NUMBER_OF_DISKS + 1;
                sector   = Z502_REG_6 % NUM_LOGICAL_SECTORS;
                DISK_READ( disk_id, sector, (char*)(data_read->char_data ) );
                
           STEP( 4 )
                disk_id  = ( Z502_REG_4 / 2 ) % MAX_NUMBER_OF_DISKS + 1;
                sector   = Z502_REG_6 % NUM_LOGICAL_SECTORS;

                if ( Z502_REG_6 % DISPLAY_GRANULARITY2c == 0 )
                     printf("PID= %ld  disk_id =%d, sector = %d\n",
                             Z502_REG_4, disk_id, sector );

                if (  ( data_read->int_data[0] != data_written->int_data[0] )
                   || ( data_read->int_data[1] != data_written->int_data[1] )
                   || ( data_read->int_data[2] != data_written->int_data[2] )
                   || ( data_read->int_data[3] != data_written->int_data[3]))
                    printf( "AN ERROR HAS OCCURRED.\n" );

                Z502_REG_6 += 2;
                if ( Z502_REG_6 < 50 )
                    GO_NEXT_TO( 2 )                     /* Go write/read */
                break;


                /* Now read back the data we've written and paged */

           STEP( 5 )
                printf( "Reading back data: test 2c, PID %ld.\n", 
                                                        Z502_REG_4 );
                Z502_REG_6 = Z502_REG_4;
                break;
           STEP( 6 )
                disk_id  = ( Z502_REG_4 / 2 ) % MAX_NUMBER_OF_DISKS + 1;
                sector   = Z502_REG_6 % NUM_LOGICAL_SECTORS;
                data_written->int_data[0] = disk_id;
                data_written->int_data[1] = sanity;
                data_written->int_data[2] = sector;
                data_written->int_data[3] = Z502_REG_4;

                DISK_READ( disk_id, sector, (char*)(data_read->char_data ) );
                
           STEP( 7 )
                disk_id  = ( Z502_REG_4 / 2 ) % MAX_NUMBER_OF_DISKS + 1;
                sector   = Z502_REG_6 % NUM_LOGICAL_SECTORS;

                if ( Z502_REG_6 % DISPLAY_GRANULARITY2c == 0 )
                    printf("PID= %ld  disk_id =%d, sector = %d\n",
                            Z502_REG_4, disk_id, sector );

                if (  ( data_read->int_data[0] != data_written->int_data[0] )
                   || ( data_read->int_data[1] != data_written->int_data[1] )
                   || ( data_read->int_data[2] != data_written->int_data[2] )
                   || ( data_read->int_data[3] != data_written->int_data[3]))
                    printf( "AN ERROR HAS OCCURRED.\n" );

                Z502_REG_6 += 2;
                if ( Z502_REG_6 < 50 )
                    GO_NEXT_TO( 6 )                     /* Go write/read */
                break;
            STEP( 8 )
                GET_TIME_OF_DAY( &Z502_REG_8 );

            STEP( 9 )
                printf( "Test2c, PID %ld, Ends at Time %ld\n", 
                         Z502_REG_4, Z502_REG_8);
                TERMINATE_PROCESS( -1, &Z502_REG_9 );

        }                                       /* End of SELECT    */
    }                                           /* End of while     */
}                                               /* End of test2c    */



/**************************************************************************

        Test2d runs several disk programs at a time.  The purpose here
               is to watch the scheduling that goes on for these
               various disk processes.  The behavior that should be seen
               is that the processes alternately run and do disk
               activity - there should always be someone running unless
               ALL processes happen to be waiting on the disk at some
               point.
               This program will terminate when all the test2c routines
               have finished.

    Z502_REG_4  - process id of this process.
    Z502_REG_5  - returned error code.
    Z502_REG_6  - pid of target process.
    Z502_REG_8  - returned error code from the GET_PROCESS_ID call.

**************************************************************************/

#define           MOST_FAVORABLE_PRIORITY                       1

void    test2d( void )
    {
    static INT32         trash;

    while(1)
        {
        SELECT_STEP
            {
           STEP( 0 )
                GET_PROCESS_ID( "", &Z502_REG_4, &Z502_REG_5 );
                
           STEP( 1 )
                printf( "\n\nRelease %s:Test 2d: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_4 );
                CHANGE_PRIORITY( -1, MOST_FAVORABLE_PRIORITY,
                                                        &Z502_REG_9);
           STEP( 2 )
                CREATE_PROCESS( "first", test2c, 5, &trash, &Z502_REG_5 );
                
           STEP( 3 )
                CREATE_PROCESS( "second", test2c, 5, &trash, &Z502_REG_5 );
                
           STEP( 4 )
                CREATE_PROCESS( "third", test2c, 7, &trash, &Z502_REG_5 );
     

           STEP( 5 )
                CREATE_PROCESS( "fourth", test2c, 7, &trash, &Z502_REG_5 );
                
           STEP( 6 )
                CREATE_PROCESS( "fifth", test2c, 7, &trash, &Z502_REG_5 );
                
           STEP( 7 )
                SLEEP ( 50000 );
                
           STEP( 8 )
                TERMINATE_PROCESS( -1, &Z502_REG_5 );
                

        }                                       /* End of SELECT    */
    }                                           /* End of while     */
}                                               /* End of test2d    */


/**************************************************************************

        Test2e causes extensive page replacement.  It simply advances
        through virtual memory.  It will eventually end because
        using an illegal virtual address will cause this process to
        be terminated by the operating system.                                                  

    Z502_REG_1  - data that was written.
    Z502_REG_2  - data that was read from memory.
    Z502_REG_3  - address where data was written/read.
    Z502_REG_4  - process id of this process.
    Z502_REG_6  - number of iterations/loops through the code.
    Z502_REG_7  - which page will the write/read be on. start at 0  
    Z502_REG_9  - returned error code.

**************************************************************************/

#define         STEP_SIZE               VIRTUAL_MEM_PGS/(2 * PHYS_MEM_PGS )
#define         DISPLAY_GRANULARITY2e     16 * STEP_SIZE
void    test2e( void )
    {

    while(1)
        {
        SELECT_STEP
            {
           STEP( 0 )
                GET_PROCESS_ID( "", &Z502_REG_4, &Z502_REG_9 );
                
           STEP( 1 )
                printf( "\n\nRelease %s:Test 2e: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_4 );
                break;
           STEP( 2 )
                Z502_REG_3 = PGSIZE * Z502_REG_7;       /* Generate address*/
                Z502_REG_1 = Z502_REG_3 + Z502_REG_4;   /* Generate data */
                MEM_WRITE( Z502_REG_3, &Z502_REG_1 );   /* Write the data */
                
           STEP( 3 )
                MEM_READ( Z502_REG_3, &Z502_REG_2 );    /* Read back data */
                
           STEP( 4 )
                if ( Z502_REG_7 % DISPLAY_GRANULARITY2e == 0 )
                  printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                        Z502_REG_4, Z502_REG_3, Z502_REG_1, Z502_REG_2 );
                if (Z502_REG_2 != Z502_REG_1 )          /* Written = read? */
                    printf( "AN ERROR HAS OCCURRED.\n" );
                Z502_REG_7 += STEP_SIZE;
                if ( Z502_REG_7 < VIRTUAL_MEM_PGS )
                    GO_NEXT_TO( 2 )                     /* Go write/read */
                break;


                /* Now read back the data we've written and paged */

           STEP( 5 )
                printf( "Reading back data: test 2e, PID %ld.\n", 
                                                        Z502_REG_4 );
                Z502_REG_7 = 0;
                break;
           STEP( 6 )
                Z502_REG_3 = PGSIZE * Z502_REG_7;       /* Generate address*/
                Z502_REG_1 = Z502_REG_3 + Z502_REG_4;   /* Data expected  */
                MEM_READ( Z502_REG_3, &Z502_REG_2 );    /* Read back data */
                
           STEP( 7 )
                if ( Z502_REG_7 % DISPLAY_GRANULARITY2e == 0 )
                  printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                        Z502_REG_4, Z502_REG_3, Z502_REG_1, Z502_REG_2 );
                if (Z502_REG_2 != Z502_REG_1 )          /* Written = read? */
                    printf( "AN ERROR HAS OCCURRED.\n" );
                Z502_REG_7 += STEP_SIZE;
                GO_NEXT_TO( 6 )                         /* Go write/read */
                break;
        }                                       /* End of SELECT    */
    }                                           /* End of while     */
}                                               /* End of test2e    */



/**************************************************************************

        Test2f causes extensive page replacement, but reuses pages. 
                This program will terminate, but it might take a while.

    Z502_REG_1  - data that was written.
    Z502_REG_2  - data that was read from memory.
    Z502_REG_3  - address where data was written/read.
    Z502_REG_4  - process id of this process.
    Z502_REG_5  - holds the pointer to the record of page touches
    Z502_REG_6  - counter/index - starts at 0.
    Z502_REG_8  - iteration count
    Z502_REG_9  - returned error code.

**************************************************************************/

#define                 NUMBER_OF_ITERATIONS            3
#define                 LOOP_COUNT                    400
#define                 DISPLAY_GRANULARITY2          100
#define                 LOGICAL_PAGES_TO_TOUCH       2 * PHYS_MEM_PGS

typedef struct
    {
    INT16       page_touched[LOGICAL_PAGES_TO_TOUCH];
} MEMORY_TOUCHED_RECORD;

void    test2f( void )
    {
    MEMORY_TOUCHED_RECORD       *mtr;
    short                       index;

    if ( Z502_REG_5 == 0 )
        {
        Z502_REG_5 = ( long )calloc( 1, sizeof(MEMORY_TOUCHED_RECORD) );
        if ( Z502_REG_5 == 0 )
            {
            printf("Something screwed up allocating space in test2f\n");
        }
        mtr = ( MEMORY_TOUCHED_RECORD *)Z502_REG_5;
        for ( index = 0; index < LOGICAL_PAGES_TO_TOUCH; index++ )
            mtr->page_touched[index] = 0;
    }
    mtr = ( MEMORY_TOUCHED_RECORD *)Z502_REG_5;


    while(1)
        {
        SELECT_STEP
            {
           STEP( 0 )
                GET_PROCESS_ID( "", &Z502_REG_4, &Z502_REG_9 );
                

           STEP( 1 )
                printf( "\n\nRelease %s:Test 2f: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_4 );
                break;

           STEP( 2 )
                get_skewed_random_number( &Z502_REG_7, LOGICAL_PAGES_TO_TOUCH );
                Z502_REG_3 = PGSIZE * Z502_REG_7;  /* Convert page to addr.*/
                Z502_REG_1 = Z502_REG_3 + Z502_REG_4;
                MEM_WRITE( Z502_REG_3, &Z502_REG_1 );
                
           STEP( 3 )
                MEM_READ( Z502_REG_3, &Z502_REG_2 );
                
           STEP( 4 )
                if ( Z502_REG_6 % DISPLAY_GRANULARITY2 == 0 )
                  printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                        Z502_REG_4, Z502_REG_3, Z502_REG_1, Z502_REG_2 );
                Z502_REG_6++;
                if (Z502_REG_2 != Z502_REG_1 )
                    printf( "AN ERROR HAS OCCURRED: READ NOT EQUAL WRITE.\n" );
                mtr->page_touched[ (short)Z502_REG_7 ]++;
                if ( Z502_REG_6 <= LOOP_COUNT )
                    GO_NEXT_TO( 2 )    /* There are more pages to read/write */
                else
                    {
                    printf( "PID %ld starting re-read.\n", Z502_REG_4 );
                    Z502_REG_6 = 0;
                }
                break;


           STEP( 5 )
                /* We can only read back from pages we've previously
                   written to, so find out which pages those are.           */

                while( mtr->page_touched[(short)Z502_REG_6] == 0 )
                     {
                     Z502_REG_6++;
                     if ( Z502_REG_6 >= LOGICAL_PAGES_TO_TOUCH ) 
                         {
                         GO_NEXT_TO( 7 )
                         break;
                    }
                }
                Z502_REG_3 = PGSIZE * Z502_REG_6;  /* Convert page to addr.*/   
                Z502_REG_1 = Z502_REG_3 + Z502_REG_4;   /* Expected read*/
                MEM_READ( Z502_REG_3, &Z502_REG_2 );

           STEP( 6 )
                if ( Z502_REG_6 % DISPLAY_GRANULARITY2 == 0 )
                  printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                        Z502_REG_4, Z502_REG_3, Z502_REG_1, Z502_REG_2 );
                Z502_REG_6++;
                if (Z502_REG_2 != Z502_REG_1 )
                    printf( "ERROR HAS OCCURRED: READ NOT SAME AS WRITE.\n" );
                if ( Z502_REG_6 < LOGICAL_PAGES_TO_TOUCH )
                    GO_NEXT_TO( 5 )     /* There's more to read back    */
                break;

            STEP( 7 )
                /* We've completed reading back everything              */

                Z502_REG_8++;
                printf( "TEST 2f, PID %ld, HAS COMPLETED %ld ITERATIONS\n",
                                Z502_REG_4, Z502_REG_8 );
                if ( Z502_REG_8 >= NUMBER_OF_ITERATIONS )
                    GO_NEXT_TO( 8 )                 /* All done     */
                else
                    GO_NEXT_TO( 2 )     /* Set up to start ALL over     */
                printf( "PID %ld starting read/write.\n", Z502_REG_4 );
                Z502_REG_6 = 0;
                break;

           STEP( 8 )
                TERMINATE_PROCESS( -1, &Z502_REG_9 );

        }                                       /* End of SELECT    */
    }                                           /* End of while     */
}                                               /* End of test2f    */


/**************************************************************************

      Test2g starts up a number of processes who do tests of shared area.

    Z502_REG_4  - process id of this process.
    Z502_REG_5  - returned error code.
    Z502_REG_6  - pid of target process.
    Z502_REG_8  - returned error code from the GET_PROCESS_ID call.

**************************************************************************/

#define           MOST_FAVORABLE_PRIORITY                       1

void    test2g( void )
    {
    INT32         trash;

    while(1)
        {
        SELECT_STEP
            {
           STEP( 0 )
                GET_PROCESS_ID( "", &Z502_REG_4, &Z502_REG_5 );
                
           STEP( 1 )
                printf( "\n\nRelease %s:Test 2g: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_4 );
                CHANGE_PRIORITY( -1, MOST_FAVORABLE_PRIORITY,
                                                        &Z502_REG_5);
           STEP( 2 )
                CREATE_PROCESS( "first", test2gx, 5, &trash, &Z502_REG_5 );
                
           STEP( 3 )
                CREATE_PROCESS( "second", test2gx, 6, &trash, &Z502_REG_5 );
                
           STEP( 4 )
                Z502_REG_8 = 0;
                CREATE_PROCESS( "third", test2gx, 7, &trash, &Z502_REG_5 );
                
           STEP( 5 )
                Z502_REG_8 = 0;
                CREATE_PROCESS( "fourth", test2gx, 8, &trash, &Z502_REG_5 );
                
           STEP( 6 )
                Z502_REG_8 = 0;
                CREATE_PROCESS( "fifth", test2gx, 9, &trash, &Z502_REG_5 );



                        /* Loop here until the "2gx" processes terminate. */
           STEP( 7 )
                GET_PROCESS_ID( "fifth", &Z502_REG_6, &Z502_REG_8 );
                
           STEP( 8 )
                if ( Z502_REG_8 != 0 )
                    GO_NEXT_TO( 9 )
                else
                    {
                    GO_NEXT_TO( 7 )
                    SLEEP ( 10000 );
                }
                break;
           STEP( 9 )
                TERMINATE_PROCESS( -1, &Z502_REG_5 );

        }                                       /* End of SELECT    */
    }                                           /* End of while     */
}                                               /* End of test2g    */


/**************************************************************************

      Test2gx - test shared memory usage.
      
      This test runs as multiple instances of processes; there are several
      processes who in turn manipulate shared memory.

      NOTE:  This test is new in Release 1.5 and has not been tested very
             thoroughly - be careful and trust nothing!!

      The algorithm used here flows as follows:

      o Get our PID and print it out.
      o Use our PID to determine the address at which to start shared
        area - every process will have a different starting address.
      o Define the shared area.
      o Fill in initial portions of the shared area by:
        + Locking the shared area
        + Determine which location in shared area is ours by using the
          number of processes that are already holding the region.
          For this discussion, call it the shared_index.
        + Fill in portions of the shared area.
        + Unlock the shared area.
      o Sleep to let all 2gx PIDs start up.
      o If (shared_index > 0) goto INSIDE_LOOP   *** NOT first DEFINER  ***

      o LOOP forever doing the following steps:
          + Lock shared area
          + Determine the "next" process, where 
             next = ( my_shared_index + 1 ) mod number_of_2gx_processes.
          + Put N + 1 ( initially N = 0 ) into mailbox of next process
          + Put sender's PID into Target's mailbox.
          + Get PID of "next" process.
          + Unlock shared area.
          + SEND_MESSAGE( "next", ..... );
   
      o INSIDE_LOOP 
        + RECEIVE_MESSAGE( "-1", ..... )
        + Lock shared area
        + Read my mailbox
        + Print out lots of stuff
        + Do lots of sanity checks
        + If  N < MAX_ITERATIONS then go to LOOP.

      o If (shared_index == 0)   ***** the first DEFINER   *****
        + sleep                      ***** let others finish   *****
        + print the whole shared structure

      o Terminate the process.
      
**************************************************************************/


#define           MAX_NUM_2GX_ITERATIONS      26
#define           MAX_NUM_2GX_PROCS           10
#define           PROC_INFO_STRUCT_TAG        1234
#define           SHARED_MEM_NAME             "almost_done!!\0"

/*      The following structure will be laid on shared memory by using
        the MEM_ADJUST   macro                                          */

typedef  struct
   {
   INT32        structure_tag;
   INT32        pid;
   INT32        mailbox;
   INT32        writer_of_mailbox;
} PROC_INFO;

typedef  struct
    {
    INT32       number_2gx_procs;
    INT32       lock_word;
    PROC_INFO   proc_info[ MAX_NUM_2GX_PROCS ];
} SHARED_DATA;

typedef struct
    {
    INT32          starting_address_of_shared_area;
    INT32          pages_in_shared_area; 
    char           area_tag[32];
    INT32          number_previous_sharers; 
    INT32          error_returned;
    INT32          successful_action;
    INT32          memory_info;
    INT32          our_index;
    INT32          next_index;
    INT32          next_pid;
    INT32          data_being_passed;

    INT32          source_pid;
    char           receive_buffer[20];
    INT32          message_receive_length;
    INT32          message_send_length;
    INT32          message_sender_pid;
} LOCAL_DATA;

/*  This MEM_ADJUST macro allows us to overlay the SHARED_DATA structure
    onto the shared memory we've defined.  It generates an address
    appropriate for use by READ and MEM_WRITE.                          */

#define         MEM_ADJUST( arg )                                       \
(long)&(shared_ptr->arg) - (long)(shared_ptr)                             \
                      + (long)ld->starting_address_of_shared_area


void    test2gx( void )
    {
    /* The declaration of shared_ptr is only for use by MEM_ADJUST macro.
       It points to a bogus location - but that's ok because we never
       actually use the result of the pointer.                      */

    SHARED_DATA           *shared_ptr = 0;       
    LOCAL_DATA            *ld;

    if ( Z502_REG_1 == 0 )
        {
        Z502_REG_1 = (long)calloc( 1, sizeof(LOCAL_DATA) );
        if ( Z502_REG_1 == 0 )
            {
            printf( "Unable to allocate memory in test2gx\n" );
        }
        ld = (LOCAL_DATA *)Z502_REG_1;
        ld->data_being_passed = 0;
        strcpy( ld->area_tag, SHARED_MEM_NAME );
    }
    ld = (LOCAL_DATA *)Z502_REG_1;


    while(1)
        {
        SELECT_STEP
            {
           STEP( 0 )
                GET_PROCESS_ID( "", &Z502_REG_4, &Z502_REG_5 );
                
           STEP( 1 )
                printf( "\n\nRelease %s:Test 2gx: Pid %ld\n", 
                                                CURRENT_REL, Z502_REG_4 );
                /* As an interesting wrinkle, each process should start
                   its shared region at a somewhat different address;
                   determine that here.                               */

                ld->starting_address_of_shared_area = 
                                            ( Z502_REG_4 % 17 ) * PGSIZE;
                ld->pages_in_shared_area = sizeof( SHARED_DATA )/PGSIZE + 1;

                DEFINE_SHARED_AREA( ld->starting_address_of_shared_area, 
                                    ld->pages_in_shared_area, 
                                    ld->area_tag, 
                                    &ld->number_previous_sharers, 
                                    &ld->error_returned );

           STEP( 2 )
                success_expected( ld->error_returned, "DEFINE_SHARED_AREA" );

                /*  Put stuff in shared area - lock it first        */

 //               READ_MODIFY( MEM_ADJUST( lock_word ), &ld->successful_action );
// BETH NOTES - without a memory lock, this code creates an infinite loop, and so it has
//              been removed.
//           STEP( 3 )
//                if ( ld->successful_action == FALSE )
//                    GO_NEXT_TO( 2 )   /* Couldn't get lock - try again */
//                SLEEP( 10 );

           STEP( 4 )
                /* We now have the lock and so own the shared area  */

                /* Increment the number of users of shared area     */

                MEM_READ( MEM_ADJUST( number_2gx_procs ), &ld->memory_info );

           STEP( 5 )
                ld->memory_info++;
                MEM_WRITE( MEM_ADJUST( number_2gx_procs ), &ld->memory_info );

           STEP( 6 )
                ld->memory_info = PROC_INFO_STRUCT_TAG;  /* Sanity data */
                ld->our_index = ld->number_previous_sharers;
                MEM_WRITE( MEM_ADJUST(proc_info[ld->our_index].structure_tag), 
                                                       &ld->memory_info );

           STEP( 7 )
                ld->memory_info = Z502_REG_4;  /* Store PID in our slot */
                MEM_WRITE( MEM_ADJUST(proc_info[ld->our_index].pid), 
                                                       &ld->memory_info );
           STEP( 8 )
                ld->memory_info = 0;                    /* Free lock */
                MEM_WRITE( MEM_ADJUST( lock_word ), &ld->memory_info );

           STEP( 9 )
                if ( ld->our_index == 0 )
                    {
                    GO_NEXT_TO( 12 );         /* This is the master */
                    SLEEP( 1000 );             /* Wait for slaves to start */
                }
                else
                    GO_NEXT_TO( 30 );         /* These are the slaves */
                break;

           STEP( 12 )
               /* This is the top of the loop - return here for each
                  send - receive pair                                */

                /*  Put stuff in shared area - lock it first        */

//                READ_MODIFY( MEM_ADJUST( lock_word ), &ld->successful_action );

// BETH NOTES - without a memory lock, this code creates an infinite loop, and so it has
//              been removed.
//           STEP( 13 )
//                if ( ld->successful_action == FALSE )
//                    GO_NEXT_TO( 12 )   /* Couldn't get lock - try again */
//                SLEEP( 10 );

           STEP( 14 )
                MEM_READ( MEM_ADJUST( number_2gx_procs ), &ld->memory_info );

           STEP( 15 )
                ld->next_index = ( ld->our_index + 1 ) % ld->memory_info;

                ld->our_index = ld->number_previous_sharers;
                MEM_READ( MEM_ADJUST(proc_info[ld->next_index].structure_tag), 
                                                       &ld->memory_info );


           STEP( 16 )
                if ( ld->memory_info != PROC_INFO_STRUCT_TAG )
                {
                    printf( "We should see a structure tag, but did not\n");
                    printf( "This means that this memory is not mapped \n");
                    printf( "consistent with the memory used by the writer\n");
                    printf( "of this structure.  It's a page table problem.\n");
                }
                MEM_WRITE( MEM_ADJUST(proc_info[ld->next_index].mailbox), 
                                                  &ld->data_being_passed );
        
           STEP( 17 )
                MEM_WRITE( MEM_ADJUST(proc_info[ld->next_index].
                                 writer_of_mailbox), &Z502_REG_4 );


           STEP( 18 )
                MEM_READ( MEM_ADJUST(proc_info[ld->next_index].pid), 
                                 &ld->next_pid );

           STEP( 19 )
                ld->memory_info = 0;                    /* Free lock */
                MEM_WRITE( MEM_ADJUST( lock_word ), &ld->memory_info );

           STEP( 20 )
                printf( "Sender %ld to Receiver %d passing data %d\n",
                            Z502_REG_4, ld->next_pid, ld->data_being_passed );
                SEND_MESSAGE( ld->next_pid, " ", 0, &ld->error_returned );

           STEP( 21 )
                success_expected( ld->error_returned, "SEND_MESSAGE" );
                if ( ld->data_being_passed < MAX_NUM_2GX_ITERATIONS )
                    {
                    GO_NEXT_TO( 30 );
                }
                else
                    {
                    GO_NEXT_TO( 40 );
                }
                break;

           STEP( 30 )
                ld->source_pid = -1;                /* From anyone  */
                ld->message_receive_length = 20;
                RECEIVE_MESSAGE( ld->source_pid, 
                                 ld->receive_buffer,
                                 ld->message_receive_length,
                                 &ld->message_send_length,
                                 &ld->message_sender_pid,
                                 &ld->error_returned );

           STEP( 31 )
                success_expected( ld->error_returned, "RECEIVE_MESSAGE" );

//                READ_MODIFY( MEM_ADJUST( lock_word ), &ld->successful_action );

// BETH NOTES - without a memory lock, this code creates an infinite loop, and so it has
//              been removed.
//           STEP( 32 )
//                if ( ld->successful_action == FALSE )
//                    GO_NEXT_TO( 31 )   /* Couldn't get lock - try again */
//                SLEEP( 10 );

           STEP( 33 )
                MEM_READ( MEM_ADJUST(proc_info[ld->our_index].structure_tag), 
                                                       &ld->memory_info );

           STEP( 34 )
                if ( ld->memory_info != PROC_INFO_STRUCT_TAG )
                {
                    printf( "We should see a structure tag, but did not.\n");
                    printf( "This means that this memory is not mapped \n");
                    printf( "consistent with the memory used when WE wrote\n");
                    printf( "this structure.  It's a page table problem.\n");
                }

                MEM_READ( MEM_ADJUST(proc_info[ld->our_index].mailbox), 
                                                  &ld->data_being_passed );

           STEP( 35 )
                printf( "\t\t\tReceiver %ld from Sender %d got data %d\n",
                                Z502_REG_4, ld->message_sender_pid, 
                                ld->data_being_passed );
                MEM_READ( MEM_ADJUST(proc_info[ld->our_index].
                                writer_of_mailbox), &ld->memory_info );

           STEP( 36 )
                if ( ld->memory_info != ld->message_sender_pid )
                    {
                    printf( "ERROR: ERROR: The sender PID, given by the \n");
                    printf( "RECIEVE_MESSAGE and the mailbox, don't match\n" );
                }
                ld->data_being_passed++;

           STEP( 37 )                             /* Free the lock    */
                ld->memory_info = 0;
                MEM_WRITE( MEM_ADJUST(lock_word), &ld->memory_info );

           STEP( 38 )
                GO_NEXT_TO( 12 );
                break;

           /* The code comes here when it's finished with all the messages. */

           STEP( 40 )
                if ( ld->our_index > 0 )
                    GO_NEXT_TO( 60 );
                break;

           /* The first sharer prints out the entire shared area    */

           STEP( 41 )
                SLEEP( 5000 );           /* Wait for msgs to finish  */

           STEP( 42 )
                MEM_READ( MEM_ADJUST( number_2gx_procs ), &ld->memory_info );

           STEP( 43 )
                printf( "Overview of shared area at completion of Test2g\n");
                printf( "number_2gx_processes = %d\n", ld->memory_info );
                Z502_REG_7 = 0;
                break;

           STEP( 44 )
                MEM_READ( MEM_ADJUST(proc_info[ Z502_REG_7].structure_tag), 
                                                       &ld->memory_info );

           STEP( 45 )
                MEM_READ( MEM_ADJUST( proc_info[ Z502_REG_7].pid ), 
                                                        &Z502_REG_6 );

           STEP( 46 )
                MEM_READ( MEM_ADJUST( proc_info[ Z502_REG_7].mailbox ), 
                                                        &Z502_REG_2 );

           STEP( 47 )
               MEM_READ( MEM_ADJUST( proc_info[Z502_REG_7].writer_of_mailbox),
                                                      &Z502_REG_3 );

           STEP( 48 )
                printf( "Mailbox info for index %ld:\n", Z502_REG_7 );
                printf( "\t\t\t%d   %ld   %ld   %ld\n", 
                         ld->memory_info, Z502_REG_6, Z502_REG_2, Z502_REG_3 );
                Z502_REG_7++;
                if ( Z502_REG_7 < ld->memory_info )
                    {
                    GO_NEXT_TO( 44 );
                }
                else
                    {
                    GO_NEXT_TO( 60 );
                }
                break;

           STEP( 60 )
                TERMINATE_PROCESS( -1, &Z502_REG_9 );

        }                                       /* End of SELECT    */
    }                                           /* End of while     */
}                                               /* End of test2gx   */

/**************************************************************************

      get_skewed_random_number   Is a homegrown deterministic random
      number generator.  It produces  numbers that are NOT uniform across
      the allowed range.
      This is useful in picking page locations so that pages
      get reused and makes a LRU algorithm meaningful.
      This algorithm is VERY good for developing page replacement tests.

**************************************************************************/

#define                 SKEWING_FACTOR          0.60
void    get_skewed_random_number( long *random_number, long range )
    {
    double  temp;
    long    extended_range = (long)pow( range, (double)(1/SKEWING_FACTOR) );

    temp = (double)rand();
    if ( temp < 0 ) 
        temp = -temp;
    temp = (double)((long)temp % extended_range);
    temp = pow(temp, (double)SKEWING_FACTOR);
    *random_number = (long)temp;
}                               /* End get_skewed_random_number */
