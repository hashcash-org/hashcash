/* -*- Mode: C; c-file-style: "bsd" -*- */

#if !defined( _sdb_h )
#define _sdb_h

#include <limits.h>

#if !defined( PATH_MAX )
#define PATH_MAX 256            /* sane smallish value */
#endif

#if defined( __cplusplus )
extern "C" {
#endif

typedef struct {
    FILE* file;
    char filename[PATH_MAX+1];
} DB;

#define MAX_KEY 1024
#define MAX_VAL 1024

/* empty string matches any key for sdb_find, sdb_findnext */

#define ANY_KEY ""		

typedef int (*sdb_wcallback)( const char* key, char* val, 
			      void* arg, int* err );
typedef int (*sdb_rcallback)( const char* key, const char* val, 
			      void* arg, int* err );

/* NOTE: keys should not contain spaces */

int sdb_create( DB*, const char* filename, int* err );
int sdb_open( DB*, const char* filename, int* err );
int sdb_add( DB*, const char* key, const char* val, int* err );
int sdb_update( DB*, char* key, char* nval, int* err );
int sdb_updateiterate( DB*, sdb_wcallback cb, void* arg, int* err );
int sdb_del( DB*, const char* key, int* err );
int sdb_findfirst( DB*, char* key, int klen, char* val, int vlen, int* err );
int sdb_findnext( DB*, char* key, int klen, char* val, int vlen, int* err );
int sdb_lookup( DB*, const char* key, char* val, int vlen, int* err );
int sdb_lookupnext( DB*, const char* key, char* val, int vlen, int* err );
int sdb_close( DB*, int* err );

int sdb_callbacklookup( DB*, sdb_rcallback cb, void* arg, 
			char* key, int klen, char* val, int vlen,
			int* err );

int sdb_callbacklookupnext( DB*, sdb_rcallback cb, void* arg, 
			    char* key, int klen, char* val, int vlen, 
			    int* err );

int file_exists( const char* filename, int* err );
int file_delete( const char* filename, int* err );
int file_rename( const char* old_name, const char* new_name, int* err );

#if defined( __cplusplus )
}
#endif

#endif
