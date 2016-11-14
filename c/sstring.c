/* -*- Mode: C; c-file-style: "stroustrup" -*- */
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "sstring.h"


/* strtok/strtok_r is a disaster, so here's a more sane one */

/* if *tok is NULL space is allocated; if the *tok is not NULL, and
 * the token is too large it is truncated; tok_len returns the length,
 * a pointer to **tok is also returned
 * if tok is NULL do not return the parsed value
 */

char* sstrtok( const char* str, const char* delim, char** tok, int tok_max, 
	       int* tok_len, char** s )
{
    char *end = NULL ;
    int use = 0 ;

    if ( delim == NULL ) { return NULL; }
    if ( str != NULL ) { *s = (char*)str; }
    if ( **s == '\0' ) { return NULL; } /* consumed last token */
    end = strpbrk( *s, delim );
    if ( end == NULL ) { 
	*tok_len = strlen(*s);
    } else {
	*tok_len = end - *s;
    }

    if ( tok ) {
	if ( tok_max > 0 ) {
	    use = tok_max < *tok_len ? tok_max : *tok_len;
	} else {
	    use = *tok_len; 
	}
	if ( !*tok ) { *tok = malloc( use+1 ); }
	sstrncpy( *tok, *s, use ); 
    }
    *s += *tok_len + ( end == NULL ? 0 : 1 );
    return tok ? *tok : "";
}

void stolower( char* str ) {
    if ( !str ) { return; }
    for ( ; *str; str++ ) {
        *str = tolower( *str );
    }
}

