/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#include <stdlib.h>
#include <string.h>
#include "array.h"

/* num = start size, auto-grows */ 

void array_alloc( ARRAY* array, int num ) {
    array->num = 0;
    array->max = num;
    array->elt = malloc( sizeof( ELEMENT ) * num );
    if ( array->elt == NULL ) { die_msg( "out of memory" ); }
}

/* auto-grow array */

void array_push( ARRAY* array, const char *str, int type, int case_flag, 
		 long validity, long grace, long anon, int width, int bits, 
		 int over ) 
{
    if ( array->num >= array->max ) {
	array->elt = realloc( array->elt, sizeof( ELEMENT) * array->max * 2 );
	if ( array->elt == NULL ) { die_msg( "out of memory" ); }
	array->max *= 2;
    }
    array->elt[array->num].regexp = NULL; /* compiled regexp */
    array->elt[array->num].type = type;
    array->elt[array->num].case_flag = case_flag;
    array->elt[array->num].validity = validity;
    array->elt[array->num].grace = grace;
    array->elt[array->num].anon = anon;
    array->elt[array->num].width = width;
    array->elt[array->num].bits = bits;
    array->elt[array->num].over = over;
    if ( str ) {
	array->elt[array->num].str = strdup( str );
	if ( array->elt[array->num].str == NULL ) { 
	    die_msg( "out of memory" ); 
	}
    } else {
	array->elt[array->num].str = NULL;
    }
    array->num++;
}

int bit_cmp( const void* ap, const void* bp )
{
    ELEMENT* a = (ELEMENT*) ap;
    ELEMENT* b = (ELEMENT*) bp;

    return a->bits == b->bits ? 0 : ( a->bits < b->bits ? 1 : -1 );
}

void array_sort( ARRAY* array, int(*cmp)(const void*, const void*) )
{
    qsort( array->elt, array->num, sizeof( ELEMENT ), cmp );
}
