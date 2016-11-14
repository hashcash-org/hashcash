/* -*- Mode: C; c-file-style: "bsd" -*- */

#include <stdio.h>
#include <string.h>
#include "hashcash.h"
#include "sha1.h"
#include "random.h"

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

int hashcash_mint( time_t now_time, const char* resource, unsigned bits, 
		   time_t validity_period, long anon_period, 
		   char* token, int tok_len, long* anon_random,
		   double* tries_taken )
{
    word32 i0;
    word32 ran0, ran1;    
    int time_width = 6;		/* default YYMMDD */
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
	if ( anon_period > validity_period )
	{
	    return HASHCASH_EXPIRED_ON_CREATION;
	}
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

    if ( validity_period != 0 )
    {  /* YYMMDDhhmmss or YYMMDDhhmm or YYMMDDhh or YYMMDD or YYMM or YY */
	if ( validity_period < 2*TIME_MINUTE ) { time_width = 12; } 
	else if ( validity_period < 2*TIME_HOUR ) { time_width = 10; }
	else if ( validity_period < 2*TIME_DAY ) { time_width = 8; }
	else if ( validity_period < 2*TIME_MONTH ) { time_width = 6; }
	else if ( validity_period < 2*TIME_YEAR ) { time_width = 4; }
	else { time_width = 2; }
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
    char target[ MAX_TOK+1 ];
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
    byte target_digest[ 20 ];
    byte try_digest[ 20 ];
    int partial_byte = bits & 7;
    int check_bytes;
    int partial_byte_index = 0;	/* suppress dumb warning */
    int partial_byte_mask = 0xFF; /* suppress dumb warning */
    char last_char;
   
    sstrncpy( target, utct, MAX_TOK );
    strcat( target, ":" );
    strncat( target, resource, MAX_RES );

    counter_len = (int)(strlen( counter ) - GROUP_DIGITS);
    sscanf( counter + counter_len, "%08x", &trial );
    trial -= trial % 16;		/* lop off last digit */

    SHA1_Init( &ctx );
    SHA1_Update( &ctx, target, strlen( target ) );
    SHA1_Final( &ctx, target_digest );

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

    sstrncpy( ctry, target, MAX_TOK ); 
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
