/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#include "lock.h"

int lock_write( FILE* f )
{
  return flock( fileno(f), LOCK_EX ) == 0;
}

int lock_read( FILE* f )
{
  return flock( fileno(f), LOCK_SH ) == 0;
}

int lock_unlock( FILE* f )
{
  return flock( fileno(f), LOCK_UN ) == 0;
}
