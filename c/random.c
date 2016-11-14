/* -*- Mode: C; c-file-style: "bsd" -*- */

#include <stdio.h>
#include <limits.h>
#include "random.h"

/* on machines that have /dev/urandom -- use it */

#if defined( __linux__ ) || defined( __FreeBSD__ ) || \
    defined( __OpenBSD__ ) || defined( DEV_URANDOM )

#define URANDOM_FILE "/dev/urandom"
FILE* urandom;

int random_init( void )
{
    urandom = fopen( URANDOM_FILE, "r" );
    return ( urandom != NULL );
}

int random_getbytes( void* data, size_t len )
{
    if ( !urandom && !random_init() ) { return 0; }
    return fread( data, len, 1, urandom );
}

#else

/* if no /dev/urandom fall back to a crappy rng who's only
 * entropy source is your high resolution timer
 */

/* WARNING: this is not of cryptographic quality */

#include <unistd.h>
#include "timer.h"
#if defined( OPENSSL )
    #include <openssl/sha.h>
    #define SHA1_ctx SHA_CTX
    #define SHA1_Final( x, digest ) SHA1_Final( digest, x )
    #define SHA1_DIGEST_BYTES SHA_DIGEST_LENGTH
#else
    #include "sha1.h"
#endif

byte state[ SHA1_DIGEST_BYTES ];
byte output[ SHA1_DIGEST_BYTES ];
long counter = 0;

/* output = SHA1( input || time || pid || counter++ ) */

static void random_stir( const byte input[SHA1_DIGEST_BYTES],
			 byte output[SHA1_DIGEST_BYTES] )
{
    SHA1_ctx sha1;
    pid_t pid = getpid();
    timer t;

    timer_read( &t );
    SHA1_Init( &sha1 );
    SHA1_Update( &sha1, input, SHA1_DIGEST_BYTES );
    SHA1_Update( &sha1, &t, sizeof( timer ) );
    SHA1_Update( &sha1, &pid, sizeof( pid_t ) );
    SHA1_Update( &sha1, &counter, sizeof( long ) );
    SHA1_Final( &sha1, output );
    counter++;
}

int random_init( void )
{
    random_stir( state, state );
    return 1;
}

#define CHUNK_LEN (SHA1_DIGEST_BYTES)

int random_getbytes( void* rnd, size_t len )
{
    byte* rndp = (byte*)rnd;
    int use = 0;

    random_stir( state, state ); /* mix in the time, pid */
    for ( ; len > 0; len -= use, rndp += CHUNK_LEN )
    {
	random_stir( state, output );
	use = len > CHUNK_LEN ? CHUNK_LEN : len;
	memcpy( rndp, output, use );
    }
    return 1;
}

#endif

static int count_bits( long val )
{
    int count;
    for ( count = 0; val; val >>= 1, count++ ) { }
    return count;
}

int random_rectangular( long top, long* resp )
{
    long mask;
    int neg = 1;
    long res;

    if ( top < 0 ) { neg = -1; top = -top; }
    mask = ~( LONG_MAX << count_bits( top ) );
    do
    {
	if ( !random_getbytes( &res, sizeof( long ) ) ) { return 0; }
	res &= mask;
    }
    while ( res > top );
    *resp = res * neg;
    return 1;
}
