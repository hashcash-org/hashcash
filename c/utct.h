/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#if !defined( _utct_h )
#define _utct_h

#include <time.h>

#if defined( __cplusplus )
extern "C" {
#endif

#if !defined(HCEXPORT)
    #if !defined(WIN32) || defined(MONOLITHIC)
        #define HCEXPORT
    #elif defined(BUILD_DLL)
        #define HCEXPORT __declspec(dllexport)
    #else /* USE_DLL */
        #define HCEXPORT extern __declspec(dllimport)
    #endif
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

#define MAX_UTC 13

HCEXPORT
time_t hashcash_from_utctimestr( const char utct[MAX_UTC+1], int utc );

HCEXPORT
int hashcash_to_utctimestr( char utct[MAX_UTC+1], int len, time_t t );

#if defined( __cplusplus )
}
#endif

#endif
