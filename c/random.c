/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/time.h>
#include "random.h"

int initialized = 0;

/* on machines that have /dev/urandom -- use it */

#if defined( __linux__ ) || defined( __FreeBSD__ ) || defined( __MACH__ ) || \
    defined( __OpenBSD__ ) || defined( DEV_URANDOM )

#define URANDOM_FILE "/dev/urandom"
FILE* urandom = NULL;

int random_init( void )
{
    int res = (urandom = fopen( URANDOM_FILE, "r" )) != NULL;
    if ( res ) { initialized = 1; }
    return res;
}

int random_getbytes( void* data, size_t len )
{
    if ( !initialized && !random_init() ) { return 0; }
    return fread( data, len, 1, urandom );
}

int random_final( void )
{
    int res = 0;
    if ( urandom ) { res = (fclose( urandom ) == 0); }
    return res;
}

#else

/* if no /dev/urandom fall back to a crappy rng who's only
 * entropy source is your high resolution timer
 */

/* on windows we are ok as we can use CAPI, but use CAPI combined with
   the below just to be sure! */

/* WARNING: on platforms other than windows this is not of
 * cryptographic quality 
 */

#include <stdlib.h>

#if defined( unix ) || defined( VMS )
    #include <unistd.h>
    #include <sys/time.h>
#elif defined( WIN32 )
    #include <process.h>
    #include <windows.h>
    #include <wincrypt.h>
    #include <sys/time.h>
#else
    #include <time.h>
#endif
#include <time.h>
#if defined( OPENSSL )
    #include <openssl/sha.h>
    #define SHA1_ctx SHA_CTX
    #define SHA1_Final( x, digest ) SHA1_Final( digest, x )
    #define SHA1_DIGEST_BYTES SHA_DIGEST_LENGTH
#else
    #include "sha1.h"
#endif

#if defined( WIN32 )
    #define pid_t int
    typedef BOOL (WINAPI *CRYPTACQUIRECONTEXT)(HCRYPTPROV *, LPCTSTR, LPCTSTR,
					       DWORD, DWORD);
    typedef BOOL (WINAPI *CRYPTGENRANDOM)(HCRYPTPROV, DWORD, BYTE *);
    typedef BOOL (WINAPI *CRYPTRELEASECONTEXT)(HCRYPTPROV, DWORD);
    HCRYPTPROV hProvider = 0;
    CRYPTRELEASECONTEXT release = 0;
    CRYPTGENRANDOM gen = 0;
#endif

byte state[ SHA1_DIGEST_BYTES ];
byte output[ SHA1_DIGEST_BYTES ];
long counter = 0;

/* output = SHA1( input || time || pid || counter++ ) */

static void random_stir( const byte input[SHA1_DIGEST_BYTES],
			 byte output[SHA1_DIGEST_BYTES] )
{
    SHA1_ctx sha1;
#if defined(__unix__) || defined(WIN32)
    pid_t pid = getpid();
#else
    unsigned long pid = rand();
#endif
#if defined(__unix__)
    struct timeval tv = {};
    struct timezone tz = {};
#endif
#if defined(WIN32)
    SYSTEMTIME tw;
    BYTE buf[64];
#endif
    clock_t t = clock();
    time_t t2 = time(0);

    SHA1_Init( &sha1 );
#if defined(__unix__)
    gettimeofday(&tv,&tz);
    SHA1_Update( &sha1, &tv, sizeof( tv ) );
    SHA1_Update( &sha1, &tz, sizeof( tz ) );
#endif
#if defined(WIN32)
    GetSystemTime(&tw);
    SHA1_Update( &sha1, &tw, sizeof( tw ) );    
    if ( gen ) {
	if (gen(hProvider, sizeof(buf), buf)) {
	    SHA1_Update( &sha1, buf, sizeof(buf) );
	}
    }
#endif
    SHA1_Update( &sha1, input, SHA1_DIGEST_BYTES );
    SHA1_Update( &sha1, &t, sizeof( clock_t ) );
    SHA1_Update( &sha1, &t2, sizeof( time_t ) );
    SHA1_Update( &sha1, &pid, sizeof( pid ) );
    SHA1_Update( &sha1, &counter, sizeof( long ) );

    SHA1_Final( &sha1, output );
    counter++;
}

int random_init( void )
{
#if defined(WIN32)
    HMODULE advapi = 0;
    CRYPTACQUIRECONTEXT acquire = 0;
#endif

#if defined(WIN32)
    advapi = LoadLibrary(TEXT("ADVAPI32.DLL"));
    if (advapi) {
	acquire = (CRYPTACQUIRECONTEXT) 
	    GetProcAddress(advapi, TEXT("CryptAcquireContextA"));
	gen = (CRYPTGENRANDOM) 
	    GetProcAddress(advapi, TEXT("CryptGenRandom"));
	release = (CRYPTRELEASECONTEXT)
	    GetProcAddress(advapi, TEXT("CryptReleaseContext"));
    }
    if ( acquire && gen ) {
	if (!acquire(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
	    gen = NULL;
	}
    }
#endif
    srand(clock());
    random_stir( state, state );
    
    initialized = 1;

    return 1;
}

int random_final( void )
{
#if defined(WIN32)
    if ( hProvider && release ) { release(hProvider,0); }
#endif
    return 1;
}

#define CHUNK_LEN (SHA1_DIGEST_BYTES)

int random_getbytes( void* rnd, size_t len )
{
    byte* rndp = (byte*)rnd;
    int use = 0;

    if ( !initialized && !random_init() ) { return 0; }

    random_stir( state, state ); /* mix in the time, pid */
    for ( ; len > 0; len -= use, rndp += CHUNK_LEN ) {
	random_stir( state, output );
	use = len > CHUNK_LEN ? CHUNK_LEN : len;
	memcpy( rndp, output, use );
    }
    return 1;
}

#endif

static int count_bits( long val )
{
    int count = 0 ;
    for ( count = 0; val; val >>= 1, count++ ) { }
    return count;
}

int random_rectangular( long top, long* resp )
{
    long mask = 0 ;
    int neg = 1;
    long res = 0 ;

    if ( top < 0 ) { neg = -1; top = -top; }
    mask = ~( LONG_MAX << count_bits( top ) );
    do {
	if ( !random_getbytes( &res, sizeof( long ) ) ) { return 0; }
	res &= mask;
    } while ( res > top );
    *resp = res * neg;
    return 1;
}
