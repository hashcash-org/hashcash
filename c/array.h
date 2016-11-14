/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#if !defined( _array_h )
#define _array_h

#if defined( __cplusplus )
extern "C" {
#endif

typedef struct {
    char* str;
    void* regexp;		/* compiled regexp */
    int type;
    int case_flag;
    long validity;
    long grace;
    long anon;
    int width;
    int bits;
    int over;
} ELEMENT;

typedef struct {
    int num;
    int max;
    ELEMENT* elt;
} ARRAY;

#define MAX_UTC 13

typedef struct {
    time_t expires_before;
    char now_utime[ MAX_UTC+1 ];
    int all;
    long validity;
    long grace;
    ARRAY* resource;
} db_arg;

void array_alloc( ARRAY* array, int num );
void array_push( ARRAY* array, const char *str, int type, int case_flag, 
		 long validity, long grace, long anon, int width, int bits, 
		 int over );
#define array_num( a ) ( (a)->num )
int bit_cmp( const void* ap, const void* bp );
void array_sort( ARRAY* array, int(*cmp)(const void*, const void*) );

/* WARNING: the *PRINTF macros assume a coding style which always uses
 * braces {} around conditional code -- unfortunately there is no
 * portable way to do vararg macros or functions which call other
 * vararg functions
 */

#define QQE (!quiet_flag)
#define PPE (!out_is_tty||quiet_flag)
#define VVE (verbose_flag)

#define QQ if (QQE) 
#define PP if (PPE) 
#define VV if (VVE) 
#define QPRINTF QQ fprintf
#define PPRINTF PP fprintf
#define VPRINTF VV fprintf

#define QPUTS(f,str) QQ { fputs(str,f); }
#define PPUTS(f,str) PP { fputs(str,f); }
#define VPUTS(f,str) VV { fputs(str,f); }

#if defined( __cplusplus )
}
#endif

#endif
