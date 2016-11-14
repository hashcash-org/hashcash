/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#include <stdio.h>
#include <string.h>
#include "sha1.h"

#define BUFFER_SIZE 1024

int SHA1_file( char* filename, byte md[ SHA1_DIGEST_BYTES ] )
{
    FILE* file = NULL ;
    byte buffer[ BUFFER_SIZE ] = {0};
    size_t bytes_read = 0 ;
    int opened = 0;
    SHA1_ctx ctx;
    
    if ( strcmp( filename, "-" ) == 0 ) {
	file = stdin;
    } else {
	file = fopen( filename, "rb" );
	if ( file == 0 ) { return -1; }
	opened = 1;
    }

    SHA1_Init( &ctx );
    while ( !feof( file ) ) {
	bytes_read = fread( buffer, 1, BUFFER_SIZE, file );
	if ( bytes_read < BUFFER_SIZE && ferror( file ) ) {
	    return -1;
	}
	SHA1_Update( &ctx, buffer, bytes_read );
    }
    SHA1_Final( &ctx, md );
    if ( opened ) {
	fclose( file );
    }
    return 0;
}

const char* hex_digest( byte md[ SHA1_DIGEST_BYTES ] )
{
    int i = 0 ;
    static char hex[ SHA1_DIGEST_BYTES * 2 + 1 ] = {0};

    for ( i = 0; i < SHA1_DIGEST_BYTES; i++ ) {
	sprintf( hex + 2 * i, "%02x", md[ i ] );
    }
    hex[ sizeof( hex ) - 1 ] = '\0';
    return hex;
}

int main( int argc, char* argv[] )
{
    int i = 0 ;
    byte digest[ SHA1_DIGEST_BYTES ] = {0};
    int status = 0 ;
    
    if ( argc == 1 ) {
	status = SHA1_file( "-", digest );
	if ( status < 0 ) {
	    perror( "(stdin)" );
	} else {
	    printf( "%s\n", hex_digest( digest ) );
	}
    } else {
	for ( i = 1; i < argc; i++ ) {
	    status = SHA1_file( argv[ i ], digest );
	    if ( status < 0 ) {
		perror( argv[ i ] );
		return 1;
	    } else {
		printf( "%s %s\n", hex_digest( digest ), argv[ i ] );
	    }
	}
    }
    return 0;
}
