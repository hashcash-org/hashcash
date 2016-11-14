/* -*- Mode: C; c-file-style: "bsd" -*- */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "utct.h"

static int char_pair_atoi( const char* pair )
{
    char str[3];
    str[0] = pair[0]; str[1] = pair[1]; str[2] = '\0';
    if ( !isdigit( str[0] ) || !isdigit( str[1] ) ) { return -1; }
    return atoi( str );
}

/* deal with 2 char year issue */
static int century_offset_to_year( int century_offset )
{
    time_t time_now = time( 0 ); /* local time */
    struct tm* now = gmtime( &time_now ); /* gmt broken down time */
    int current_year = now->tm_year + 1900;
    int current_century_offset = current_year % 100;
    int current_century = (current_year - current_century_offset) / 100;
    int year = current_century * 100 + century_offset;

    /* assume year is in current century */
    /* unless that leads to very old or very new, then adjust */ 
    if ( year - current_year > 50 ) { year -= 100; }
    else if ( year - current_year < -50 ) { year += 100; }
    return year;
}

/* the portable C library functionality for time manipulation is a bit
 * poor.  We actually have to convert to structured time just to find
 * out what UCT is.
 */

time_t local_to_utctime( time_t local_time )
{
    struct tm* tms;
    tms = gmtime( &local_time ); /* convert to UTCtime */
    return mktime( tms ); /* no conversion, so stays in UTC */
}

/* converting back from utctime to local time is even worse
 * have to guess and correct! due to day light time startin
 * at different absolute times depending on time zones
 */

time_t utctime_to_local( time_t utc_time )
{
    time_t try_time;
    time_t diff_time;
    time_t local_time;
    time_t check_utc_time;
/* try converting time utc_time into UTCTIME by pretending it is already
 * in local time, calculate the difference between the utc_time and 
 * converted time and adjust in the other direction
 */
    try_time = local_to_utctime( utc_time ); 
    diff_time = try_time - utc_time;
/* and adjust in the other direction */
    local_time = utc_time - diff_time;

/* if we're on a day that is switching from or to daylight savings
 * that may not work, so check and if it is wrong adjust accordingly
 */
    check_utc_time = local_to_utctime( local_time );
    if ( check_utc_time != utc_time ) 
    { 
	diff_time = check_utc_time - utc_time;
	local_time -= diff_time;
	check_utc_time = local_to_utctime( local_time );
    }
    assert( check_utc_time == utc_time );
    return local_time;
}

time_t utctime( time_t* utc_timep )
{
    time_t utc_time = local_to_utctime( time(0) );
    if ( utc_timep != NULL ) { *utc_timep = utc_time; }
    return utc_time;
}

time_t from_utctimestr( const char utct[MAX_UTCTIME+1] )
{
    time_t failed = -1;
    struct tm tms;
    int utct_len = strlen( utct );
    int century_offset;

    if ( utct_len > MAX_UTCTIME || utct_len < 2 || ( utct_len % 2 == 1 ) )
    {
	return failed;
    }

/* defaults */
    tms.tm_mon = 0; tms.tm_mday = 1;
    tms.tm_hour = 0; tms.tm_min = 0; tms.tm_sec = 0;
    tms.tm_isdst = 0;		/* ignore daylight savings time */

/* year */
    century_offset = char_pair_atoi( utct );
    if ( century_offset < 0 ) { return failed; }
    tms.tm_year = century_offset_to_year( century_offset ) - 1900;
/* month -- optional */
    if ( utct_len <= 2 ) { return mktime( &tms ); }
    tms.tm_mon = char_pair_atoi( utct+2 ) - 1;
    if ( tms.tm_mon < 0 ) { return failed; }
/* day */
    if ( utct_len <= 4 ) { return mktime( &tms ); }
    tms.tm_mday = char_pair_atoi( utct+4 );
    if ( tms.tm_mday < 0 ) { return failed; }
/* hour -- optional */
    if ( utct_len <= 6 ) { return mktime( &tms ); }
    tms.tm_hour = char_pair_atoi( utct+6 );
    if ( tms.tm_mday < 0 ) { return failed; }
/* minute -- optional */
    if ( utct_len <= 8 ) { return mktime( &tms ); }
    tms.tm_min = char_pair_atoi( utct+8 );
    if ( tms.tm_min < 0 ) { return failed; }
    if ( utct_len <= 10 ) { return mktime( &tms ); }
/* second -- optional */
    tms.tm_sec = char_pair_atoi( utct+10 );
    if ( tms.tm_sec < 0 ) { return failed; }

    return mktime( &tms );
}

int to_utctimestr( char utct[MAX_UTCTIME+1], int len, time_t t  )
{
    struct tm* tms = localtime( &t );

    if ( tms == NULL || len > MAX_UTCTIME || len < 2 ) { return 0; }
    sprintf( utct, "%02d", tms->tm_year % 100 );
    if ( len == 2 ) { goto leave; }
    sprintf( utct+2, "%02d", tms->tm_mon+1 );
    if ( len == 4 ) { goto leave; }
    sprintf( utct+4, "%02d", tms->tm_mday );
    if ( len == 6 ) { goto leave; }
    sprintf( utct+6, "%02d", tms->tm_hour );
    if ( len == 8 ) { goto leave; }
    sprintf( utct+8, "%02d", tms->tm_min ); 
    if ( len == 10 ) { goto leave; }
    sprintf( utct+10, "%02d", tms->tm_sec );

 leave:
    return 1;
}
