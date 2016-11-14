/* -*- Mode: C; c-file-style: "bsd" -*- */

#if !defined( _hashcash_h )
#define _hashcash_h

#if defined( __cplusplus )
extern "C" {
#endif

extern int verbose_flag;
extern int no_purge_flag;

#define MAX_UTCTIME 13

time_t from_utctime( const char[MAX_UTCTIME+1] );
int to_utctime( char[MAX_UTCTIME+1], int len, time_t );

#if defined( __cplusplus )
}
#endif

#endif
