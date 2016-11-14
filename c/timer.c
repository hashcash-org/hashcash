/* -*- Mode: C; c-file-style: "bsd" -*- */

#include <time.h>
#include "timer.h"

#if defined( THINK_C )

#elif defined( DOS )
    #include <conio.h>
    #include <dos.h>
#elif defined( unix )
    #include <sys/time.h>
#endif

#include <time.h>

word32 tick;
timer timer_at_start;
timer timer_at_end;

word32 find_tick_us( void )
{
#define delta 0.00000001
    timer t1, t2;
    double elapsed;

    timer_read( &t1 );

/* tight loop until a tick happens */
/* if it is very high resolution, this may be inaccurate */
/* but if it is very high resolution, we haven't got a problem anyway */

    do
    {
	timer_read( &t2 );
    } 
    while ( timer_usecs( &t1 ) == timer_usecs( &t2 ) &&
	    timer_secs( &t1 ) == timer_secs( &t2 ) );
    elapsed = timer_interval( &t1, &t2 );

/* see how many us the tick took */

    return (word32) ( elapsed + 0.5 - delta );
}

void timer_read( timer* su )
{
#if defined( THINK_C )
    struct tm t; 

    time( &t );
    su->secs = t.tm_sec; 
    su->usecs = 0;

/* 780000 ticks/sec resolution timer goes here */

/* #elif DOS */
/* anyone care to write the code for DOS? */

#elif defined( __linux__ )
    struct timeval tv; 

    gettimeofday( &tv, 0 ); 
    su->secs = (word32)tv.tv_sec; 
    su->usecs = (word32)tv.tv_usec;
#else
/* fall back */
    time_t t; 
    
    t = time( 0 ); 
    su->secs = (word32)t; 
    su->usecs = 0;
#endif
}

/* return difference in usecs */

void timer_diff( timer* s, timer* e, timer* diff )
{
    timer* t;
    double interval;
    
    if ( timer_gt( s, e ) )	/* s > e */
    {
	t = s; s = e; e = t;	/* swap( s, e ) */
    }
    
    interval = ( ( (double) e->secs ) - ( (double) s->secs ) )
	* 1000000.0 + ( (double) e->usecs ) - ( (double) s->usecs );

/* if diff is not NULL, store the difference between end and start in it */
    if ( diff )
    {
	diff->secs = (word32)(interval / 1000000.0);
	diff->usecs = (word32)(interval - diff->secs);
    }
}

double timer_interval( timer* s, timer* e )
{
    timer* t;
    double interval;
    
    if ( timer_gt( s, e ) )	/* s > e */
    {
	t = s; s = e; e = t;	/* swap( s, e ) */
    }
    
    interval = ( ( (double) e->secs ) - ( (double) s->secs ) )
	* 1000000.0 + ( (double) e->usecs ) - ( (double) s->usecs );
    
/* return interval in usecs */
    return interval;
}
