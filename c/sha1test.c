/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "sha1.h"

byte b = 0 ;

#define BUF_SIZE 1024

int main( int argc, char* argv[] )
{
    SHA1_ctx ctx = {} ;
    int i = 0 , j = 0 ;
    byte digest[ SHA1_DIGEST_BYTES ] = {} ;
    clock_t start = 0 , end = 0 , tmp = 0 ;
    double elapsed = 0 ;

/* test 1 data */
    const char* test1 = "abc";

    byte result1[ SHA1_DIGEST_BYTES ] = {
	0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E, 
	0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D };
    
/* test 2 date */
    const char* test2 = 
	"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    byte result2[ SHA1_DIGEST_BYTES ] = {
	0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E, 0xBA, 0xAE, 
	0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5, 0xE5, 0x46, 0x70, 0xF1 };

/* test 3 data */
    /* test 3 is 1,000,000 letter "a"s. */
    /* fill test 3 with "a"s and hash this 5000 times */
    byte test3[ 200 ];
    byte result3[ SHA1_DIGEST_BYTES ] = {
	0x34, 0xAA, 0x97, 0x3C, 0xD4, 0xC4, 0xDA, 0xA4, 0xF6, 0x1E, 
	0xEB, 0x2B, 0xDB, 0xAD, 0x27, 0x31, 0x65, 0x34, 0x01, 0x6F };

/* test 1 */

    printf( "test 1\n\n" );
    SHA1_Init( &ctx );
    SHA1_Update( &ctx, test1, strlen( test1 ) );
    SHA1_Final( &ctx, digest );
    printf( "SHA1(\"%s\") = \n\t", test1 );
    
    for ( i = 0; i < 20 ; i++ ) {
	printf( "%02x", digest[ i ] );
    }
    
    if ( memcmp( digest, result1, SHA1_DIGEST_BYTES ) == 0 ) {
	printf( " test ok\n" );
    } else {
	printf( " test failed\n" );
    }
    printf( "\n" );
    
/* test 2 */
    
    printf( "test 2\n\n" );
    SHA1_Init( &ctx );
    SHA1_Update( &ctx, test2, strlen( test2 ) );
    SHA1_Final( &ctx, digest );
    
    printf( "SHA1(\"%s\") = \n\t", test2 );
    
    for ( i = 0; i < SHA1_DIGEST_BYTES ; i++ ) {
	printf( "%02x", digest[ i ] );
    }
    
    if ( memcmp( digest, result2, SHA1_DIGEST_BYTES ) == 0 ) {
	printf( " test ok\n" );
    } else {
	printf( " test failed\n" );
    }
    printf( "\n" );
    
/* test 3 */
    
    printf( "test 3\n\n" );
    
/* we'll time test3 to see how many Mb/s we can do */
    
    end = clock();
    do { start = clock(); } while ( start == end );

    printf( "start = %ld\n", start );

    for ( j = 0; j < 10; j++ ) {
	SHA1_Init( &ctx );
	memset( test3 , 'a', 200 );	/* fill test3 with "a"s */
	for( i = 0; i < 5000; i++ )	{ /* digest 5000 times */
	    SHA1_Update( &ctx, test3, 200 );
	}
	SHA1_Final( &ctx, digest );
    }
    end = clock();

    if ( end < start ) { tmp = end; end = start; start = tmp; }
    elapsed = ((end-start)/(double)CLOCKS_PER_SEC);
    printf( "SHA1(\"a\" x 1,000,000) = \n\t" );
    
    for ( i = 0; i < SHA1_DIGEST_BYTES ; i++ ) {
	printf( "%02x", digest[ i ] );
    }
    
    if ( memcmp( digest, result3, SHA1_DIGEST_BYTES ) == 0 ) {
	printf( " test ok\n" );
    } else {
	printf( " test failed\n" );
    }
    printf( "\n" );
    
/* report timing information */
    
    printf( "speed: %.2f Mb/s\n", 10.0 / elapsed );
    
    return 0;
}
