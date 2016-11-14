/* -*- Mode: C; c-file-style: "bsd" -*- */

#if !defined( _hashcash_h )
#define _hashcash_h

#if defined( __cplusplus )
extern "C" {
#endif

#include <time.h>
#include "utct.h"
#include "types.h"

extern int verbose_flag;
extern int no_purge_flag;

#define MAX_UTCTIME 13
#define MAX_CTR 64
#define MAX_RES 256
#define MAX_STR 256
#define MAX_TOK ((MAX_RES)+1+(MAX_UTCTIME)+1+(MAX_STR))

#define TIME_MINUTE 60
#define TIME_HOUR (TIME_MINUTE*60)
#define TIME_DAY (TIME_HOUR*24)
#define TIME_YEAR (TIME_DAY*365)
#define TIME_MONTH (TIME_YEAR/12)
#define TIME_AEON (TIME_YEAR*1000000000.0)
#define TIME_MILLI_SECOND (1.0/1000)
#define TIME_MICRO_SECOND (TIME_MILLI_SECOND/1000)
#define TIME_NANO_SECOND (TIME_MICRO_SECOND/1000)

#define HASHCASH_OK 1
#define HASHCASH_INVALID_TOK_LEN -1
#define HASHCASH_RNG_FAILED -2
#define HASHCASH_INVALID_TIME -3
#define HASHCASH_TOO_MANY_TRIES -4
#define HASHCASH_EXPIRED_ON_CREATION -5
#define HASHCASH_INVALID_VALIDITY_PERIOD -6
#define HASHCASH_NULL_ARG -7

/* function hashcash_mint calling info:
 *
 * returns HASHCASH_OK on success or one of the error codes above on
 * failure (values < 0)
 * 
 * arguments are:
 *
 * now_time        -- should be time in UTC, use time(2) and local_to_utctime
 *                    from "utct.h"
 *
 * resource        -- resource name, unique descriptor for resource you're
 *                    trying to protect
 *
 * bits            -- bits of collision the resource demands
 * 
 * validity_period -- default to 0 (forever), only effect on minting
 *                    is to ensure the time field is expressed in
 *                    sufficiently high resolution that it won't be
 *                    consider expired at time of minting
 * 
 * anon_period     -- add (or subtract if negative) a random period
 *                    in seconds between this value and 0
 *                    to get same as mixmaster does set this to -3days
 *                    (default 0)
 * 
 * token           -- the token output
 *
 * tok_len         -- max length of token (excluding trailing 0)
 * 
 * anon_random     -- default NULL, if set to pointer to long
 *                    returns actuall random offset added to time
 *
 * tries_atken     -- default NULL, if set to pointer to double
 *                    returns number of tokens tested before finding
 *                    returned token.
 */

int hashcash_mint( time_t now_time, const char* resource, unsigned bits, 
		   time_t validity_period, long anon_period, 
		   char* token, int tok_len, long* anon_random,
		   double* tries_taken );

time_t from_utctime( const char[MAX_UTCTIME+1] );
int to_utctime( char[MAX_UTCTIME+1], int len, time_t );

#define sstrncpy(d,s,l) (d[l]='\0',strncpy(d,s,l))

word32 find_collision( char utct[ MAX_UTCTIME+1 ], const char* resource, 
		       int bits, char* token, word32 tries, char* counter );

#if defined( __cplusplus )
}
#endif

#endif
