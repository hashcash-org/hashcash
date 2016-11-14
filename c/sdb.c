/* -*- Mode: C; c-file-style: "bsd" -*- */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "sdb.h"
#include "types.h"
#include "utct.h"

/* simple though inefficient implementation of a database function */

int file_exists( const char* filename, int* err )
{
    FILE* sdb = fopen( filename, "r" );
    *err = 0;
    if ( sdb != NULL ) { fclose( sdb ); return 1; }
    if ( errno == ENOENT ) { return 0; }
    *err = errno; return 0;
}

int file_delete( const char* filename, int* err )
{
    if ( unlink( filename ) == -1 ) { *err = errno; return 0; }
    return 1;
}

int file_rename( const char* old_name, const char* new_name, int* err )
{
    if ( rename( old_name, new_name ) == -1 ) { *err = errno; return 0; }
    return 1;
}

int sdb_create( DB* h, const char* filename, int* err )
{
    return sdb_open( h, filename, err );
}

int sdb_open( DB* h, const char* filename, int* err )
{
    *err = 0;
    h->file = NULL;
    if ( filename == NULL ) { return 0; }
    h->file = fopen( filename, "a+" );
    if ( h->file == NULL ) { *err = errno; h->file = NULL; return 0; }
    strncpy( h->filename, filename, PATH_MAX ); h->filename[PATH_MAX] = '\0';
    return 1;
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

int sdb_findfirst( DB* h, char* key, int klen, char* val, int vlen, int* err )
{
    rewind( h->file );
    return sdb_findnext( h, key, klen, val, vlen, err );
}

int sdb_findnext( DB* h, char* key, int klen, char* val, int vlen, int* err )
{
    char line[MAX_LINE+1];
    int line_len;
    char* fkey = line;
    char* fval;
    char* eptr;

    *err = 0;
    if ( h->file == NULL ) { return 0; }
    if ( feof( h->file ) ) { return 0; }
    if ( fgets( line, MAX_LINE, h->file ) == NULL ) { return 0; }
    line_len = strlen( line );
    if ( line_len == 0 ) { return 0; }
    if ( ( eptr = strchr( line, '\n' ) ) ) { *eptr = '\0'; }
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
    char fkey[MAX_KEY+1];
    return sdb_callbacklookup( h, sdb_cb_keymatch, (void*)key, 
			       fkey, MAX_KEY, val, vlen, err );
}

int sdb_lookupnext( DB* h, const char* key, char* val, int vlen, int* err )
{
    char fkey[MAX_KEY+1];
    return sdb_callbacklookupnext( h, sdb_cb_keymatch, (void*)key,
				   fkey, MAX_KEY, val, vlen, err );
}

int sdb_callbacklookup( DB* h, sdb_rcallback cb, void* arg, 
			char* key, int klen, char* val, int vlen, 
			int* err )
{
    rewind( h->file );
    return sdb_callbacklookupnext( h, cb, arg, key, klen, val, vlen, err );
}

int sdb_callbacklookupnext( DB* h, sdb_rcallback cb, void* arg, 
			    char* key, int klen, char* val, int vlen, 
			    int* err )
{
    char fkey[MAX_KEY+1];

    while ( sdb_findnext( h, fkey, MAX_KEY, val, vlen, err ) )
    {
	if ( cb( fkey, val, arg, err ) )
	{
	    strncpy( key, fkey, klen ); key[klen] = '\0';
	    return 1;
	}
    }
    return 0;
}

int sdb_updateiterate( DB* h, sdb_wcallback cb, void* arg, int* err )
{
    DB tmp;
    int found;
    int dontcare;
    char fkey[MAX_KEY+1];
    char fval[MAX_VAL+1];

    tmp.file = NULL;
    strncpy( tmp.filename, h->filename, PATH_MAX ); 
    tmp.filename[PATH_MAX] = '\0';
    strncat( tmp.filename, "t", PATH_MAX ); tmp.filename[PATH_MAX] = '\0';

    if ( file_exists( tmp.filename, err ) ) 
    { 
	file_delete( tmp.filename, err ); 
    }
    if ( *err ) { return 0; }

    if ( !sdb_create( &tmp, tmp.filename, err ) ) { return 0; }
    for ( found = sdb_findfirst( h, fkey, MAX_KEY, fval, MAX_VAL, err );
	  found;
	  found = sdb_findnext( h, fkey, MAX_KEY, fval, MAX_VAL, err ) )
    {
	if ( cb( fkey, fval, arg, err ) ) 
	{
	    if ( *err ) { goto fail; }
	    if ( !sdb_add( &tmp, fkey, fval, err ) ) { goto fail; }
	}
	else if ( *err ) { goto fail; }
    }
    if ( !sdb_close( &tmp, err ) ) { goto fail; }
    tmp.file = NULL;
    if ( !sdb_close( h, err ) ) { goto fail; }
    h->file = NULL;
    if ( !file_rename( tmp.filename, h->filename, err ) ) { goto fail; }
    if ( !sdb_open( h, h->filename, err ) ) { return 0; }
    return 1;
 fail:
    if ( tmp.file != NULL ) { sdb_close( &tmp, &dontcare ); }
    file_delete( tmp.filename, &dontcare );
    if ( h->file == NULL ) { sdb_open( h, h->filename, &dontcare ); }
    return 0;
}

typedef struct {
    char* key;
    char* nval;
} sdb_cb_update_arg;

static int sdb_cb_update( const char* key, char* val, void* arg, int* err )
{
    sdb_cb_update_arg* argt  = (sdb_cb_update_arg*)arg;
    *err = 0;
    if ( strncmp( argt->key, key, MAX_KEY ) == 0 )
    {
	strncpy( val, argt->nval, MAX_VAL ); val[MAX_VAL] = '\0';
    }
    return 1;
}

int sdb_update( DB* h, char* key, char* nval, int* err )
{
    sdb_cb_update_arg argt;
    argt.key = key;
    argt.nval = nval;
    return sdb_updateiterate( h, sdb_cb_update, key, (void*)&argt );
}
