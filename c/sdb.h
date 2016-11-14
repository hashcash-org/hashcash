/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#if !defined( _sdb_h )
#define _sdb_h

#include <limits.h>

#if !defined( PATH_MAX )
#define PATH_MAX 256            /* sane smallish value */
#endif

#if defined( __cplusplus )
extern "C" {
#endif

#if !defined(HCEXPORT)
    #if !defined(WIN32) || defined(MONOLITHIC)
        #define HCEXPORT
    #elif defined(BUILD_DLL)
        #define HCEXPORT __declspec(dllexport)
    #else /* USE_DLL */
        #define HCEXPORT extern __declspec(dllimport)
    #endif
#endif

typedef struct {
    FILE* file;
    char filename[PATH_MAX+1];
    long read_pos;
    long write_pos;
} DB;

#define MAX_KEY 10240+1024+1
#define MAX_VAL 1024

#define READ_MODE 0
#define WRITE_MODE 1

/* higher level functions */

#define PURGED_KEY "last_purged"

HCEXPORT
int hashcash_db_open( DB* db, const char* db_filename, int* err );

HCEXPORT
int hashcash_db_in( DB* db, char* token, char *period, int* err );

HCEXPORT
int hashcash_db_add( DB* db, char* token, char *period, int* err );

HCEXPORT
int hashcash_db_close( DB* db, int* err );


/* low level functions */

/* empty string matches any key for sdb_find, sdb_findnext */

#define ANY_KEY ""		

typedef int (*sdb_wcallback)( const char* key, char* val, 
			      void* arg, int* err );
typedef int (*sdb_rcallback)( const char* key, const char* val, 
			      void* arg, int* err );

/* NOTE: keys should not contain spaces */

HCEXPORT
int sdb_open( DB*, const char* filename, int* err );
HCEXPORT
int sdb_add( DB*, const char* key, const char* val, int* err );
HCEXPORT
int sdb_updateiterate( DB*, sdb_wcallback cb, void* arg, int* err );
HCEXPORT
int sdb_del( DB*, const char* key, int* err );
HCEXPORT
int sdb_findfirst( DB*, char* key, int klen, char* val, int vlen, int* err );
HCEXPORT
int sdb_findnext( DB*, char* key, int klen, char* val, int vlen, int* err );
HCEXPORT
int sdb_lookup( DB*, const char* key, char* val, int vlen, int* err );
HCEXPORT
int sdb_lookupnext( DB*, const char* key, char* val, int vlen, int* err );
HCEXPORT
int sdb_close( DB*, int* err );

HCEXPORT
int sdb_callbacklookup( DB*, sdb_rcallback cb, void* arg, 
			char* key, int klen, char* val, int vlen,
			int* err );

HCEXPORT
int sdb_callbacklookupnext( DB*, sdb_rcallback cb, void* arg, 
			    char* key, int klen, char* val, int vlen, 
			    int* err );


#if defined( __cplusplus )
}
#endif

#endif
