/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "sstring.h"

#define BUILD_DLL
#include "utct.h"

#if defined( WIN32 )
#define snprintf _snprintf
#endif

static int char_pair_atoi( const char* pair )
{
    char str[3] = {0};
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

#define MAX_DATE 50		/* Sun Mar 10 19:25:06 2002 (EST) */

/* more logical time_t to string conversion */

const char* strtime( time_t* timep, int utc )
{
    static char str[MAX_DATE+1] = {0};
    struct tm* isdst = NULL;
    char date[MAX_DATE+1] = {0};
    char* timestr = NULL ;
    char* zone = NULL ;
    if ( utc ) {
	timestr = asctime( gmtime( timep ) );
	zone = "UTC";
    } else {
        isdst = localtime( timep );
	timestr = asctime( isdst );
	zone = tzname[isdst->tm_isdst];
    }
    sstrncpy( date, timestr, MAX_DATE );
    date[strlen(date)-1]='\0';	/* remove trailing \n */
    snprintf( str, MAX_DATE, "%s (%s)", date, zone );
    return str;
}

/* alternate form of mktime, this tm struct is in UTC time */

time_t mk_utctime( struct tm* tms ) {
    char* tz = getenv( "TZ" );
    time_t res = 0 ;
    char *set_tz = NULL ;

    putenv( "TZ=UTC+0" );
    res = mktime( tms );
    if ( tz ) { 
        set_tz = malloc( strlen( tz ) + 3 + 1 );
        sprintf( set_tz, "TZ=%s", tz );
	putenv( set_tz );
	free( set_tz );
    } else { putenv("TZ="); putenv( "TZ" ); }
    return res;
}

time_t hashcash_from_utctimestr( const char utct[MAX_UTC+1], int utc )
{
    time_t failed = -1;
    time_t res = 0 ;
    struct tm tms;
    int tms_hour = 0;
    struct tm* dst = NULL;
    int utct_len = strlen( utct );
    int century_offset = 0;

    if ( utct_len > MAX_UTC || utct_len < 2 || ( utct_len % 2 == 1 ) ) {
	return failed;
    }

/* defaults */
    tms.tm_mon = 0; tms.tm_mday = 1;
    tms.tm_hour = 0; tms.tm_min = 0; tms.tm_sec = 0;
    tms.tm_isdst = 0;	/* daylight saving on */
    tms_hour = tms.tm_hour;

/* year */
    century_offset = char_pair_atoi( utct );
    if ( century_offset < 0 ) { return failed; }
    tms.tm_year = century_offset_to_year( century_offset ) - 1900;
/* month -- optional */
    if ( utct_len <= 2 ) { goto convert; }
    tms.tm_mon = char_pair_atoi( utct+2 ) - 1;
    if ( tms.tm_mon < 0 ) { return failed; }
/* day */
    if ( utct_len <= 4 ) { goto convert; }
    tms.tm_mday = char_pair_atoi( utct+4 );
    if ( tms.tm_mday < 0 ) { return failed; }
/* hour -- optional */
    if ( utct_len <= 6 ) { goto convert; }
    tms.tm_hour = char_pair_atoi( utct+6 );
    if ( tms.tm_hour < 0 ) { return failed; }
/* minute -- optional */
    if ( utct_len <= 8 ) { goto convert; }
    tms.tm_min = char_pair_atoi( utct+8 );
    if ( tms.tm_min < 0 ) { return failed; }
    if ( utct_len <= 10 ) { goto convert; }
/* second -- optional */
    tms.tm_sec = char_pair_atoi( utct+10 );
    if ( tms.tm_sec < 0 ) { return failed; }

 convert:
    if ( utc ) {
        return mk_utctime( &tms );
    } else {
    /* note when switching from daylight to standard the last daylight
       hour(s) are ambiguous with the first hour(s) of standard time.  The
       system calls give you the first hour(s) of standard time which is
       as good a choice as any. */

    /* note also the undefined hour(s) between the last hour of
       standard time and the first hour of daylight time (when
       expressed in localtime) are illegal values and have undefined
       conversions, this code will do whatever the system calls do */

        tms_hour = tms.tm_hour;
        res = mktime( &tms );	/* get time without DST adjust */
 	dst = localtime( &res ); /* convert back to get DST adjusted  */
	dst->tm_hour = tms_hour; /* put back in hour to convert */
 	res = mktime( dst ); /* redo conversion with DST adjustment  */
 	return res;
    }
}

int hashcash_to_utctimestr( char utct[MAX_UTC+1], int len, time_t t  )
{
    struct tm* tms = gmtime( &t );

    if ( tms == NULL || len > MAX_UTC || len < 2 ) { return 0; }
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
