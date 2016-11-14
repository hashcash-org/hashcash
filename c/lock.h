/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#if !defined( _lock_h )
#define _lock_h

#include <stdio.h>
#include <sys/file.h>

int lock_write( FILE* f );
int lock_read( FILE* f );
int lock_unlock( FILE* f );

#endif
