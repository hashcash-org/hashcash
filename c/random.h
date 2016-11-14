/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#if !defined( _random_h )
#define _random_h

#ifdef __UNIX__
#include <sys/types.h>
#endif

#if defined( __cplusplus )
extern "C" {
#endif

int random_init( void );
int random_addbytes( void*, size_t );
int random_getbytes( void*, size_t );
int random_rectangular( long top, long* resp );
int random_final( void );

#if defined( __cplusplus )
}
#endif

#endif
