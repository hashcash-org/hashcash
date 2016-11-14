/* -*- Mode: C; c-file-style: "bsd" -*- */

#if !defined( _utct_h )
#define _utct_h

#include <time.h>

#if defined( __cplusplus )
extern "C" {
#endif

const char* strtime( time_t* timep, int utc ); /* ctime like with utc arg */
time_t mk_utctime( struct tm* tms ); /* mktime like with utc struct tm */

/* functions to parse and create UTCTime string which is:
 *    YYMMDDhhmm[ss]Z
 */

/* when creating utc string, if requested len is odd Z is appended, 
 * if even Z is omitted
 *
 * when parsing Z or missing Z is tolerated
 *
 * parsing has an extension to parse like:
 *
 *    YYMMDD[hh[mm[ss]]][Z]
 */

#define MAX_UTCTIME 13

time_t from_utctimestr( const char utct[MAX_UTCTIME+1], int utc );
int to_utctimestr( char utct[MAX_UTCTIME+1], int len, time_t t );

#define sstrncpy(d,s,l) (d[l]='\0',strncpy(d,s,l))

#if defined( __cplusplus )
}
#endif

#endif
