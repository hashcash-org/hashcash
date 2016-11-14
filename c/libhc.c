/* -*- Mode: C; c-file-style: "bsd" -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashcash.h"
#include "sha1.h"
#include "random.h"
#include "timer.h"

time_t round_off( time_t now_time, int digits );

#define GROUP_SIZE 0xFFFFFFFFU
#define GROUP_DIGITS 8
#define GFORMAT "%08x"

#if 0
/* smaller base for debugging */
#define GROUP_SIZE 255
#define GROUP_DIGITS 2
#define GFORMAT "%02x"
#endif

word32 find_collision( char utct[ MAX_UTCTIME+1 ], const char* resource, 
		       int bits, char* token, word32 tries, char* counter );

int hashcash_mint( time_t now_time, int time_width, 
		   const char* resource, unsigned bits, 
		   time_t validity_period, long anon_period, 
		   char* token, int tok_len, long* anon_random,
		   double* tries_taken )
{
    word32 i0;
    word32 ran0, ran1;    
    char counter[ MAX_CTR+1 ];
    char* counter_ptr = counter;
    int found = 0;
    long rnd;
    char now_utime[ MAX_UTCTIME+1 ]; /* current time */
    double tries;

    if ( resource == NULL || token == NULL )
    {
	return HASHCASH_NULL_ARG;
    }

    if ( anon_random == NULL ) { anon_random = &rnd; }
    if ( tries_taken == NULL ) { tries_taken = &tries; }

    *anon_random = 0;

    if ( bits > SHA1_DIGEST_BYTES * 8 )
    {
	return HASHCASH_INVALID_TOK_LEN;
    }

    if ( time_width == 0 ) { time_width = 6; } /* default YYMMDD */

    if ( !random_getbytes( &ran0, sizeof( word32 ) ) ||
	 !random_getbytes( &ran1, sizeof( word32 ) ) )
    {
	return HASHCASH_RNG_FAILED;
    }
    
    if ( now_time < 0 )
    {
	return HASHCASH_INVALID_TIME;
    }

    if ( anon_period != 0 )
    {
	if ( !random_rectangular( (long)anon_period, anon_random ) )
	{
	    return HASHCASH_RNG_FAILED;
	}
    }

    now_time += *anon_random;

    if ( validity_period < 0 )
    {
	return HASHCASH_INVALID_VALIDITY_PERIOD;
    }

    if ( time_width != 12 && time_width != 10 && time_width != 8 &&
	 time_width != 6 && time_width != 4 && time_width != 2 )
    {
	return HASHCASH_INVALID_TIME_WIDTH;
    }

    now_time = round_off( now_time, 12-time_width );
    to_utctimestr( now_utime, time_width, now_time );

    for ( i0 = 0; !found; )
    {
	sprintf( counter_ptr, GFORMAT GFORMAT, ( i0 + ran0 ) & GROUP_SIZE,
		 ran1 & GROUP_SIZE );
	found = find_collision( now_utime, resource, bits, token,
				GROUP_SIZE, counter_ptr );
	if ( !found )
	{
	    i0 = ( i0 + 1 ) & GROUP_SIZE;
	    if ( i0 == 0 )	/* ie if wrapped */
	    { 
		return HASHCASH_TOO_MANY_TRIES;
		/* shouldn't get here without trying  */
		/* for a very long time */
	    }
	}
    }

    counter_ptr = counter;
    
    *tries_taken = ( (double) i0 ) * ULONG_MAX + ( (double) found );
    
    return HASHCASH_OK;
}

word32 find_collision( char utct[ MAX_UTCTIME+1 ], const char* resource, 
		       int bits, char* token, word32 tries, char* counter )
{
    char* hex = "0123456789abcdef";
    char ctry[ MAX_TOK+1 ];
    char* changing_part_of_try;
    SHA1_ctx ctx;
    SHA1_ctx precomputed_ctx;
    word32 i;
    int j;
    word32 trial;
    word32 tries2;
    int counter_len;
    int try_len;
    int try_strlen;
    byte target_digest[ SHA1_DIGEST_BYTES ];
    byte try_digest[ SHA1_DIGEST_BYTES ];
    int partial_byte = bits & 7;
    int check_bytes;
    int partial_byte_index = 0;	/* suppress dumb warning */
    int partial_byte_mask = 0xFF; /* suppress dumb warning */
    char last_char;
   
    ctry[0] = '0';		/* hardcode to version 0 */
    ctry[1] = '\0';
    strcat( ctry, ":" );
    strncat( ctry, utct, MAX_TOK );
    strcat( ctry, ":" );
    strncat( ctry, resource, MAX_RES );

    counter_len = (int)(strlen( counter ) - GROUP_DIGITS);
    sscanf( counter + counter_len, "%08x", &trial );
    trial -= trial % 16;		/* lop off last digit */

    memset( target_digest, 0, SHA1_DIGEST_BYTES );

    if ( partial_byte )
    {
	partial_byte_index = bits / 8;
	partial_byte_mask = ~ (( 1 << (8 - (bits & 7))) -1 );
	check_bytes = partial_byte_index + 1;
	target_digest[ partial_byte_index ] &= partial_byte_mask;
    }
    else
    {
	check_bytes = bits / 8;
    }

    strcat( ctry, ":" );
    strncat( ctry, counter, counter_len );
    try_len = (int)strlen( ctry );

/* length of try is fixed, GFORMAT is %08x, so move strlen outside loop */

    changing_part_of_try = ctry + try_len;
    sprintf( changing_part_of_try, GFORMAT, 0 );
    try_strlen = (int)strlen( ctry );

/* part of the ctx context can be precomputed as not all of the
   message is changing
*/

    tries2 = (int) ( (double) tries / 16.0 + 0.5 );
    for ( i = 0; i < tries2; i++, trial = (trial + 16) & GROUP_SIZE )
    {
/* move precompute closer to the inner loop to precompute more */

	SHA1_Init( &precomputed_ctx );

	sprintf( changing_part_of_try, GFORMAT, trial );

	SHA1_Update( &precomputed_ctx, ctry, try_strlen - 1 );

	for ( j = 0; j < 16; j++ )
	{
#if defined( DEBUG )
	    fprintf( stderr, "try: %s\n", ctry );
#endif
	    memcpy( &ctx, &precomputed_ctx, sizeof( SHA1_ctx ) );
	    last_char = hex[ j ];
	    SHA1_Update( &ctx, &last_char, 1 );
	    SHA1_Final( &ctx, try_digest );

	    if ( bits > 7 )
	    {
		if ( try_digest[ 0 ] != target_digest[ 0 ] )
		{
		    continue;
		}
	    }
	    if ( partial_byte )
	    {
		try_digest[ partial_byte_index ] &= partial_byte_mask;
	    }
	    if ( memcmp( target_digest, try_digest, check_bytes ) == 0 )
	    {
		changing_part_of_try[ GROUP_DIGITS - 1 ] = hex[ j ];
		sstrncpy( token, ctry, MAX_TOK );
		return i * 16 + j + 1;
	    }
	}
    }
    return 0;
}

time_t round_off( time_t now_time, int digits )
{
    struct tm* now;

    if ( digits != 2 && digits != 4 && digits != 6 && 
	 digits != 8 && digits != 10 )
    {
	return now_time;
    }
    now_time = utctime_to_local( now_time ); /* to counter act gmtime */
    now = gmtime( &now_time );	/* converts local -> utct as side effect */

    switch ( digits )
    {
    case 10: now->tm_mon = 0;
    case 8: now->tm_mday = 1;
    case 6: now->tm_hour = 0;
    case 4: now->tm_min = 0;
    case 2: now->tm_sec = 0;
    }
    return mktime( now );
}

int validity_to_width( time_t validity_period )
{
    int time_width = 6;		/* default YYMMDD */
    if ( validity_period < 0 ) { return HASHCASH_INVALID_VALIDITY_PERIOD; }
    if ( validity_period != 0 )
    {  /* YYMMDDhhmmss or YYMMDDhhmm or YYMMDDhh or YYMMDD or YYMM or YY */
	if ( validity_period < 2*TIME_MINUTE ) { time_width = 12; } 
	else if ( validity_period < 2*TIME_HOUR ) { time_width = 10; }
	else if ( validity_period < 2*TIME_DAY ) { time_width = 8; }
	else if ( validity_period < 2*TIME_MONTH ) { time_width = 6; }
	else if ( validity_period < 2*TIME_YEAR ) { time_width = 4; }
	else { time_width = 2; }
    }
    return time_width;
}

/* all chars from ascii(32) to ascii(126) inclusive */

#define VALID_STR_CHARS "!\"#$%&'()*+,-./0123456789:;<=>?@" \
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"

int hashcash_parse( const char* token, int* vers, char* utct, int utct_max,
		    char* token_resource, int res_max ) 
{
    char ver[MAX_VER+1];
    char* first_colon;
    char* second_colon;
    char* last_colon;
    char* str;
    int ver_len;
    int utct_len;
    int res_len;

    first_colon = strchr( token, ':' );

    if ( first_colon == NULL ) { return 0; }
    ver_len = (int)(first_colon - token);
    if ( ver_len > MAX_VER ) { return 0; }
    sstrncpy( ver, token, ver_len );
    *vers = atoi( ver );
    if ( *vers < 0 ) { return 0; }
    second_colon = strchr( first_colon+1, ':' );
    if ( second_colon == NULL ) { return 0; }
    utct_len = (int)(second_colon - (first_colon+1));
    if ( utct_len > utct_max ) { return 0; }

    /* parse out the resource name component 
     * format:   ver:utctime:resource:counter
     * where utctime is [YY[MM[DD[hh[mm[ss]]]]]] 
     * note the resource may itself include :s if it is a URL
     * for example
     *
     * the resource name is part between second colon and last colon */

    last_colon = strrchr( second_colon+1, ':' );
    if ( last_colon == NULL ) { return 0; }
    res_len = (int)(last_colon-1 - second_colon);
    if ( res_len > res_max ) { return 0; }
    memcpy( utct, first_colon+1, utct_len ); utct[utct_len] = '\0';
    sstrncpy( token_resource, second_colon+1, res_len );

    str = last_colon+1;
    if ( strlen( str ) != strspn( str, VALID_STR_CHARS ) )
    {
	return 0;
    }

    return 1;
}

unsigned hashcash_count( const char* resource, const char* token )
{
    SHA1_ctx ctx;
    byte target_digest[ SHA1_DIGEST_BYTES ];
    byte token_digest[ SHA1_DIGEST_BYTES ];
    char ver[MAX_VER+1];
    int vers;
    char* first_colon;
    char* second_colon;
    int ver_len;
    int i;
    int last;
    int collision_bits;

    first_colon = strchr( token, ':' );
    if ( first_colon == NULL ) { return 0; } /* should really fail */
    ver_len = (int)(first_colon - token);
    if ( ver_len > MAX_VER ) { return 0; }
    sstrncpy( ver, token, ver_len );
    vers = atoi( ver );
    if ( vers < 0 ) { return 0; }
    if ( vers != 0 ) { return 0; } /* unsupported version number */
    second_colon = strchr( first_colon+1, ':' );
    if ( second_colon == NULL ) { return 0; } /* should really fail */

    memset( target_digest, 0, SHA1_DIGEST_BYTES );

    SHA1_Init( &ctx );
    SHA1_Update( &ctx, token, strlen( token ) );
    SHA1_Final( &ctx, token_digest );
   
    for ( i = 0; 
	  i < SHA1_DIGEST_BYTES && token_digest[ i ] == target_digest[ i ]; 
	  i++ )
    {
    }

    last = i;
    collision_bits = 8 * i;

#define bit( n, c ) (((c) >> (7 - (n))) & 1)

    for ( i = 0; i < 8; i++ )
    {
	if ( bit( i, token_digest[ last ] ) == 
	     bit( i, target_digest[ last ] ) )
	{
	    collision_bits++;
	}
	else
	{
	    break;
	}
    }
    return collision_bits;
}

long hashcash_valid_for( time_t token_time, time_t validity_period, 
			 time_t now_time )
{
    long expiry_time;

    /* for ever -- return infinity */
    if ( validity_period == 0 )	{ return HASHCASH_VALID_FOREVER; }

    /* future date in token */
    if ( token_time > now_time ) { return HASHCASH_VALID_IN_FUTURE; }; 

    expiry_time = token_time + validity_period;
    if ( expiry_time > now_time )
    {				/* valid return seconds left */
	return expiry_time - now_time;
    }
    return HASHCASH_EXPIRED;	/* otherwise expired */
}

int hashcash_check( const char* token, const char* resource, time_t now_time, 
		    time_t validity_period, int required_bits ) {
    time_t token_time;
    char token_utime[ MAX_UTC+1 ];
    char token_resource[ MAX_RES+1 ];
    int bits = 0;
    int vers = 0;
    
    if ( !hashcash_parse( token, &vers, token_utime, MAX_UTC, 
			  token_resource, MAX_RES ) )
    {
	return HASHCASH_INVALID;
    }

    if ( vers != 0 ) {
	return HASHCASH_UNSUPPORTED_VERSION;
    }

    token_time = from_utctimestr( token_utime );
    if ( token_time == -1 )
    {
	return HASHCASH_INVALID;
    }
    if ( strcmp( token_resource, resource ) != 0 )
    {
	return HASHCASH_WRONG_RESOURCE;
    }
    bits = hashcash_count( token, token_resource );
    if ( bits < required_bits )
    {
	return HASHCASH_INSUFFICIENT_BITS;
    }
    return hashcash_valid_for( token_time, validity_period, now_time );
}

long hashcash_per_sec( void )
{
    timer t1, t2;
    double elapsed;
    unsigned long n_collisions = 0;
    char token[ MAX_TOK+1 ];
    char counter[ MAX_CTR+1 ];
    word32 step = 100;

    /* wait for start of tick */

    sprintf( counter, GFORMAT, 0 );

    timer_read( &t2 );
    do 
    {
	timer_read( &t1 );
    }
    while ( timer_usecs( &t1 ) == timer_usecs( &t2 ) &&
	    timer_secs( &t1 ) == timer_secs( &t2 ) );

    /* do computations for next tick */

    do
    {
	n_collisions += step;
	find_collision( "000101", "flame", 25, token, step, counter );
	timer_read( &t2 );
    }
    while ( timer_usecs( &t1 ) == timer_usecs( &t2 ) &&
	    timer_secs( &t1 ) == timer_secs( &t2 ) );

/* see how many us the tick took */
    elapsed = timer_interval( &t1, &t2 );
    return (word32) ( 1000000.0 / elapsed * (double)n_collisions
		      + 0.499999999 );
}

double hashcash_estimate_time( int b )
{
    return hashcash_expected_tries( b ) / (double)hashcash_per_sec();
}

double hashcash_expected_tries( int b )
{
    double expected_tests = 1;
    #define CHUNK ( sizeof( unsigned long ) - 1 )
    for ( ; b > CHUNK; b -= CHUNK )
    {
	expected_tests *= ((unsigned long)1) << CHUNK;
    }
    expected_tests *= ((unsigned long)1) << b;
    return expected_tests;
}
