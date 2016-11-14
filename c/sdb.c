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
#include "array.h"
#include "hashcash.h"
#include "sstring.h"

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

/* compile time assert */

#if MAX_UTC > MAX_VAL
#error "MAX_UTC must be less than MAX_VAL"
#endif

static int sdb_cb_token_matcher( const char* key, char* val,
				 void* argp, int* err ) {
    db_arg* arg = (db_arg*)argp;
    char token_utime[ MAX_UTC+1 ] = {0};
    char token_res[ MAX_RES+1 ] = {0}, lower_token_res[ MAX_RES+1 ] = {0};
    char *res = NULL;
    time_t expires = 0;
    time_t expiry_period = 0;
    time_t created = 0;
    int vers = 0, i = 0, bits = 0, matched = 0, type = 0, case_flag = 0;
    int lowered = 0 ;
    void** compile = NULL;
    char* re_err = NULL;

    *err = 0;
    if ( strcmp( key, PURGED_KEY ) == 0 ) {
	sstrncpy( val, arg->now_utime, MAX_UTC ); 
	return 1;		/* update purge time */
    }

    if ( !hashcash_parse( key, &vers, &bits, token_utime, MAX_UTC, 
			  token_res, MAX_RES, NULL, 0 ) ) {
	*err = EINPUT;		/* corrupted token in DB */
	return 0;
    }
    if ( vers != 0 && vers != 1 ) {
	*err = EINPUT; 		/* unsupported version number in DB */
	return 0; 
    }
    /* if purging only for given resource */
    if ( array_num( arg->resource ) > 0 ) {
	matched = 0;
	for ( i = 0; !matched && i < array_num( arg->resource ); i++ ) {
	    res = arg->resource->elt[i].str;
	    type = arg->resource->elt[i].type;
	    case_flag = arg->resource->elt[i].case_flag;
	    compile = &(arg->resource->elt[i].regexp);
	    if ( !case_flag && !lowered ) {
		strncpy( lower_token_res, token_res, MAX_RES );
		stolower( lower_token_res );
		lowered = 1;
	    }
	    matched = hashcash_resource_match( type, case_flag ? 
					       token_res : lower_token_res, 
					       res, compile, &re_err );
	    if ( re_err != NULL ) { 
		fprintf( stderr, "regexp error: " ); 
		die_msg( re_err ); 
	    }
	}
	if ( !matched ) { return 1; } /* if it doesn't match, keep it */
    }
    if ( arg->all ) { return 0; } /* delete all regardless of expiry */
    if ( val[0] == '0' && val[1] == '\0' ) { return 1; } /* keep forever */
    if ( strlen( val ) != strspn( val, "0123456789" ) ) {
	*err = EINPUT; return 0;
    }
    expiry_period = atoi( val );
    if ( expiry_period < 0 ) { *err = EINPUT; return 0; } /* corrupted */
    created = hashcash_from_utctimestr( token_utime, 1 );
    if ( created < 0 ) { *err = EINPUT; return 0; } /* corrupted */
    expires = created 
	+ (arg->validity ? arg->validity : expiry_period) + arg->grace;
    if ( expires <= arg->expires_before ) { return 0; }	/* delete if expired */
    return 1;			/* otherwise keep */
}

int db_purge( DB* db, ARRAY* purge_resource, int purge_all, 
	       long purge_period, time_t now_time, long validity_period,
	       long grace_period, int verbose_flag, int* err ) {
    time_t last_time = 0 ;
    char purge_utime[ MAX_UTC+1 ] = {0}; /* time token created */
    int ret = 0;
    db_arg arg;

    if ( now_time < 0 ) { return HASHCASH_INVALID_TIME; }

    if ( !sdb_lookup( db, PURGED_KEY, purge_utime, MAX_UTC, err ) ) {
	return 0;
    }

    last_time = hashcash_from_utctimestr( purge_utime, 1 );
    if ( last_time < 0 ) { /* not first time, but corrupted */
	purge_period = 0; /* purge now */
    }

    if ( !hashcash_to_utctimestr( arg.now_utime, MAX_UTC, now_time ) ) {
	return HASHCASH_INVALID_TIME;
    }

    arg.expires_before = now_time;
    arg.resource = purge_resource;
    arg.all = purge_all;
    arg.validity = validity_period;
    arg.grace = grace_period;

    if ( purge_period == 0 || now_time >= last_time + purge_period ) {
	VPRINTF( stderr, "purging database: ..." );
	ret = sdb_updateiterate( db, sdb_cb_token_matcher, (void*)&arg, err );
	VPRINTF( stderr, ret ? "done\n" : "failed\n" ); 
    } else {
	ret = 1;
    }
    return ret;
}

int hashcash_db_purge( DB* db, const char* purge_resource, int type,
		       int case_flag, long validity_period, long grace_period,
		       int purge_all, long purge_period, time_t now_time,
		       int* err ) {
    ARRAY* purge_resource_arr = NULL;
    array_alloc( purge_resource_arr, 1 );
    array_push( purge_resource_arr, purge_resource, type, case_flag,
		validity_period, grace_period, 0, 0, 0, 0 );
    return db_purge( db, purge_resource_arr, purge_all, purge_period, 
		     now_time, validity_period, grace_period, 0, err );
}

int hashcash_db_close( DB* db, int* err ) {
    if ( !sdb_close( db, err ) ) { return 0; }
    return 1;
}
