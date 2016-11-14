/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#if defined( unix ) || defined(WIN32 )
    #include <unistd.h>
#elif defined( VMS )
    #include <unixio.h>
#endif
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "types.h"
#include "lock.h"

#define BUILD_DLL
#include "sdb.h"

#include "utct.h"

#if defined( VMS )
    #define unlink(x) delete(x)
#endif

#if defined( WIN32 )
    #define ftruncate chsize
#endif

#define MAX_UTC 13

/* simple though inefficient implementation of a database function */

static int sdb_insert( DB*, const char* key, const char* val, int* err );

int sdb_open( DB* h, const char* filename, int* err ) 
{
    int fd = 0 ;
    FILE* fp = NULL ;

    *err = 0;
    h->file = NULL;
    if ( filename == NULL ) { return 0; }
    fd = open( filename, O_RDWR | O_CREAT, S_IREAD | S_IWRITE );
    if ( fd == -1 ) { goto fail; }
    fp = fdopen( fd, "w+" );
    if ( fp == NULL ) { goto fail; }
    h->file = fp;
    if ( !lock_write( h->file ) ) { goto fail; }
    strncpy( h->filename, filename, PATH_MAX ); h->filename[PATH_MAX] = '\0';
    h->write_pos = 0;
    return 1;
 fail:
    *err = errno;
    return 0;
}

int sdb_close( DB* h, int* err )
{
    if ( h == NULL || h->file == NULL ) { return 0; }
    if ( fclose( h->file ) == EOF ) { *err = errno; return 0; }
    *err = 0; return 1;
}

int sdb_add( DB* h, const char* key, const char* val, int* err )
{
    *err = 0;

    if ( h == NULL || h->file == NULL ) { return 0; }
    if ( strlen( key ) > MAX_KEY ) { return 0; }
    if ( strlen( val ) > MAX_VAL ) { return 0; }
    if ( fseek( h->file, 0, SEEK_END ) == -1 ) { goto fail; }
    if ( fprintf( h->file, "%s %s\n", key, val ) == 0 ) { goto fail; }
    return 1;
 fail:
    *err = errno;
    return 0;
}

#define MAX_LINE (MAX_KEY+MAX_VAL) 

static void sdb_rewind( DB* h ) 
{
    rewind( h->file );
    h->write_pos = 0;
}

int sdb_findfirst( DB* h, char* key, int klen, char* val, int vlen, int* err )
{
    sdb_rewind( h );
    return sdb_findnext( h, key, klen, val, vlen, err );
}

int sdb_findnext( DB* h, char* key, int klen, char* val, int vlen, 
		  int* err ) 
{
    char line[MAX_LINE+1] = {0};
    int line_len = 0 ;
    char* fkey = line;
    char* fval = NULL ;

    *err = 0;
    if ( h->file == NULL ) { return 0; }
    if ( feof( h->file ) ) { return 0; }
    if ( fgets( line, MAX_LINE, h->file ) == NULL ) { return 0; }
    line_len = strlen( line );
    if ( line_len == 0 ) { return 0; }

    /* remove unix, DOS, and MAC linefeeds */

    if ( line[line_len-1] == '\n' ) { line[--line_len] = '\0'; }
    if ( line[line_len-1] == '\r' ) { line[--line_len] = '\0'; }
    if ( line[line_len-1] == '\n' ) { line[--line_len] = '\0'; }

    fval = strchr( line, ' ' );
    if ( fval != NULL ) { *fval = '\0'; fval++; } 
    else { fval = ""; }		/* empty */
    strncpy( key, fkey, klen ); key[klen] = '\0';
    strncpy( val, fval, vlen ); val[vlen] = '\0';
    return 1;
}

static int sdb_cb_notkeymatch( const char* key, const char* val, 
			       void* arg, int* err ) 
{
    *err = 0;
    if ( strncmp( key, arg, MAX_KEY ) != 0 ) { return 1; }
    return 0;
} 

static int sdb_cb_keymatch( const char* key, const char* val, 
			    void* arg, int* err ) 
{
    *err = 0;
    if ( strncmp( key, arg, MAX_KEY ) == 0 ) { return 1; }
    return 0;
} 

int sdb_del( DB* h, const char* key, int* err )
{
    return sdb_updateiterate( h, (sdb_wcallback)sdb_cb_notkeymatch, 
				  (void*)key, err );
}

int sdb_lookup( DB* h, const char* key, char* val, int vlen, int* err )
{
    char fkey[MAX_KEY+1] = {0};
    return sdb_callbacklookup( h, sdb_cb_keymatch, (void*)key, 
				   fkey, MAX_KEY, val, vlen, err );
}

int sdb_lookupnext( DB* h, const char* key, char* val, int vlen, int* err )
{
    char fkey[MAX_KEY+1] = {0};
    return sdb_callbacklookupnext( h, sdb_cb_keymatch, (void*)key,
				       fkey, MAX_KEY, val, vlen, err );
}

int sdb_callbacklookup( DB* h, sdb_rcallback cb, void* arg, char* key, 
			int klen, char* val, int vlen, int* err ) 
{
    rewind( h->file );
    return sdb_callbacklookupnext( h, cb, arg, key, klen, val, vlen, err );
}

int sdb_callbacklookupnext( DB* h, sdb_rcallback cb, void* arg, char* key, 
			    int klen, char* val, int vlen, int* err ) 
{
    char fkey[MAX_KEY+1] = {0};

    while ( sdb_findnext( h, fkey, MAX_KEY, val, vlen, err ) ) {
	if ( cb( fkey, val, arg, err ) ) {
	    strncpy( key, fkey, klen ); key[klen] = '\0';
	    return 1;
	}
    }
    return 0;
}

int sdb_insert( DB* db, const char* key, const char* val, int* err )
{
    int res = 0 ;
    *err = 0;

    res = ftell( db->file ); 
    if ( res < 0 ) { goto fail; }
    db->read_pos = res;

    if ( fseek( db->file, db->write_pos, SEEK_SET ) == -1 ) { goto fail; }
    if ( fprintf( db->file, "%s %s\n", key, val ) == 0 ) { goto fail; }
    
    res = ftell( db->file );
    if ( res < 0 ) { goto fail; }
    db->write_pos = res;

    if ( fseek( db->file, db->read_pos, SEEK_SET ) == -1 ) { goto fail; }
    return 1;
 fail:
    *err = errno;
    return 0;
}

int sdb_updateiterate( DB* h, sdb_wcallback cb, void* arg, int* err )
{
    int found = 0, res = 0;
    char fkey[MAX_KEY+1] = {0};
    char fval[MAX_VAL+1] = {0};

    for ( found = sdb_findfirst( h, fkey, MAX_KEY, fval, MAX_VAL, err );
	  found;
	  found = sdb_findnext( h, fkey, MAX_KEY, fval, MAX_VAL, err ) ) {
	if ( cb( fkey, fval, arg, err ) ) {
	    if ( *err ) { goto fail; }
	    if ( !sdb_insert( h, fkey, fval, err ) ) { goto fail; }
	}
	else if ( *err ) { goto fail; }
    }

    res = ftruncate( fileno( h->file ), h->write_pos );
    return 1;
 fail:
    return 0;
}

/* higher level functions */

int hashcash_db_open( DB* db, const char* db_filename, int* err ) {
    int my_err;

    if ( !err ) { err = &my_err; }
    if ( !sdb_open( db, db_filename, err ) ) { return 0; }
    fgetc( db->file );		/* try read to trigger EOF */
    if ( feof( db->file ) ) {
	if ( !sdb_add( db, PURGED_KEY, "700101000000", err ) ) { 
	    return 0;
	}
    }
    rewind( db->file );
    return 1;
}

int hashcash_db_in( DB* db, char* token, char *period, int* err ) {
    int in_db = 0;
    int my_err;

    if ( !err ) { err = &my_err; }
    *err = 0;

    in_db = sdb_lookup( db, token, period, MAX_UTC, err ); 
    if ( *err ) { return 0; }
    return in_db;
}

int hashcash_db_add( DB* db, char* token, char *period, int* err ) {
    if ( !sdb_add( db, token, period, err ) ) { 
	return 0; 
    }
    return 1;
}

int hashcash_db_close( DB* db, int* err ) {
    if ( !sdb_close( db, err ) ) { return 0; }
    return 1;
}
