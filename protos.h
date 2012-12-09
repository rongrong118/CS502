/*********************************************************************

 protos.h

     This include file contains prototypes needed by the various routines.
     It does NOT contain internal-only entries, but only those that are
     externally visible.


     Revision History:
        1.0    August 1990     Initial Release.
        2.1    May    2001     Add memory_printer.
        2.2    July   2002     Make code appropriate for undergrads.
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking

*********************************************************************/

/*                      ENTRIES in base.c                         */

void   interrupt_handler( void );
void   fault_handler( void );
void   svc( void );
void   os_init( void );
void   os_switch_context_complete( void );

/*                      ENTRIES in sample.c                       */

void   sample_code(void );

/*                      ENTRIES in state_printer.c                */

void   SP_setup( INT16, INT32 );
void   SP_setup_file( INT16, FILE * );
void   SP_setup_action( INT16, char * );
void   SP_print_header( void );
void   SP_print_line( void );
void   SP_do_output( char * );
void   MP_setup( INT32, INT32, INT32, INT32 );
void   MP_print_line( void );

/*                      ENTRIES in test.c                         */                     

void   test0( void );
void   test1a( void );
void   test1b( void );
void   test1c( void );
void   test1d( void );
void   test1e( void );
void   test1f( void );
void   test1g( void );
void   test1h( void );
void   test1i( void );
void   test1j( void );
void   test1k( void );
void   test1l( void );
void   test1m( void );
void   test2a( void );
void   test2b( void );
void   test2c( void );
void   test2d( void );
void   test2e( void );
void   test2f( void );
void   test2g( void );
void   get_skewed_random_number( long *, long );



/*                      ENTRIES in z502.c                       */

void   Z502_HALT( void );
void   Z502_MEM_READ(INT32, INT32 * );
void   Z502_MEM_WRITE(INT32, INT32 * );
void   Z502_READ_MODIFY( INT32, INT32, INT32, INT32 * );
void   Z502_HALT( void );
void   Z502_IDLE( void );
void   Z502_DESTROY_CONTEXT( void ** );
void   Z502_MAKE_CONTEXT( void **, void *, BOOL );
void   Z502_SWITCH_CONTEXT( BOOL, void ** );

int    CreateAThread( void *, INT32 * );
void   DestroyThread( INT32   );

void   CreateLock( INT32 * );
void   CreateCondition( UINT32 * );
int    GetLock( UINT32, char *  );
int    WaitForCondition( UINT32 , UINT32, INT32  );
int    GetTryLock( UINT32 );
int    ReleaseLock( UINT32, char *  );
int    SignalCondition( UINT32, char *  );
void   DoSleep( INT32 millisecs );
void   HandleWindowsError( );
void   GoToExit( int );
