/* -*- Mode: C; c-file-style: "bsd" -*- */

#if !defined( _utct_h )
#define _utct_h

#if defined( __cplusplus )
extern "C" {
#endif

/* function to return current time in utctime */

time_t utctime( time_t* utc_timep ); /* usage like time(2) */
time_t utctime_to_local( time_t utc_time ); /* convert to local from utc */
time_t local_to_utctime( time_t local_time ); /* convert to uct from local */

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

time_t from_utctimestr( const char utct[MAX_UTCTIME+1] );
int to_utctimestr( char utct[MAX_UTCTIME+1], int len, time_t t );

#if defined( __cplusplus )
}
#endif

#endif
