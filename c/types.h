/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#if !defined( _types_h )
#define _types_h

#include <limits.h>

#define word unsigned

#define byte unsigned char 

#define bool byte
#define true 1
#define false 0

#define word8 unsigned char
#define int8 signed char

#define int16 signed short
#define word16 unsigned short

#if ( ULONG_MAX > 0xFFFFFFFFUL )
    #define int32 signed int
    #define word32 unsigned int
    #define int64 signed long
    #define word64 unsigned long
#elif ( UINT_MAX == 0xFFFFFFFFUL )
    #define int32 signed int
    #define word32 unsigned int
#else 
    #define int32 signed long
    #define word32 unsigned long
#endif

#if defined( __GNUC__ ) && !defined( word32 )
    #define int64 signed long long
    #define word64 unsigned long long
#endif

#endif
