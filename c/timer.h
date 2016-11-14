/* -*- Mode: C; c-file-style: "bsd" -*- */

#if !defined( _timer_h )
#define _timer_h

#include "types.h"

#if defined( __cplusplus )
extern "C" {
#endif

typedef struct timer
{
      word32 secs;
      word32 usecs;
} timer;

#if defined( THINK_C )
   #define tick_us() 1000000

/* 780000 ticks/sec resolution timer goes here */
/* tick_us() ( 1000000.0 / 780000.0 ) */

#else
extern word32 tick;
   #define tick_us() ( tick ? tick : (tick = find_tick_us()) )
#endif

void timer_read( timer* );
void timer_diff( timer*, timer*, timer* );
double timer_interval( timer*, timer* );
#define timer_secs( t ) ( (t)->secs )
#define timer_usecs( t ) ( (t)->usecs )
word32 find_tick_us( void );

/*************************** useful macros ***************************/

extern timer timer_at_start;
extern timer timer_at_end;

#define timer_start( ) ( timer_read( &timer_at_start ) )
#define timer_stop( ) ( timer_read( &timer_at_end ) )
#define timer_time( ) ( timer_interval( &timer_at_end, &timer_at_start ) )


#define timer_lt( t, u ) ((t)->secs < (u)->secs || \
			  ((t)->secs == (u)->secs && (t)->usecs < (u)->usecs))

#define timer_gt( t, u ) timer_lt( u, t )

#if defined( __cplusplus )
}
#endif

#endif
