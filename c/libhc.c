/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined( REGEXP_BSD )
    #define _REGEX_RE_COMP
    #include <regex.h>
#elif defined( REGEXP_POSIX )
    #include <sys/types.h>
    #include <regex.h>
#else
/* no regular expression support */
#endif

#include "hashcash.h"
#include "libfastmint.h"
#include "sha1.h"
#include "random.h"
#include "sstring.h"

#if defined( WIN32 )
#include "mydll.h"
#else
#define EXPORT
#endif


time_t round_off( time_t now_time, int digits );

#if DEBUG
/* smaller base for debugging */
#define GROUP_SIZE 255
#define GROUP_DIGITS 2
#define GFFORMAT "%x"
#define GFORMAT "%02x"
#else
#define GROUP_SIZE 0xFFFFFFFFU
#define GROUP_DIGITS 8
#define GFFORMAT "%x"
#define GFORMAT "%08x"
#endif

long per_sec = 0;		/* cache calculation */

char *strrstr(char *s1,char *s2) 
{
    char *sc2 = NULL , *psc1 = NULL , *ps1 = NULL ;
 
    if ( *s2 == '\0' ) { return s1; }
    ps1 = s1 + strlen(s1);

    while( ps1 != s1 ) {
	--ps1;
	for ( psc1 = ps1, sc2 = s2; ; ) {
	    if (*(psc1++) != *(sc2++)) { break; }
	    else if ( *sc2 == '\0' ) { return ps1; }
	}
    }
    return NULL;
}

int wild_match( char* pat, char* str )
{
    int num = 1, last = 0, first = 1;
    char* term = NULL , *ptr = pat, *pos = str;

    do {
	term = ptr; ptr = strchr( ptr, '*' );
	if ( ptr ) { *ptr = '\0'; ptr++; } 
	else { last = 1; }
	
	if ( *term != '\0' ) {
	    if ( first ) {	/* begin */
		if ( strncmp( pos, term, strlen( term ) ) != 0 ) {
		    return 0;
		}
		pos += strlen( term );
	    } else if ( !first ) { /* middle */
		if ( last ) {
		    pos = strrstr( pos, term );
		} else {
		    pos = strstr( pos, term );
		}
		if ( pos == 0 ) { return 0; }
		pos += strlen( term );
	    }
	    if ( last && *pos != '\0' ) {
		return 0; 
	    }
	}

	num++; first = 0;
    } while ( term && !last );
    
    return 1;
}

int email_match( const char* email, const char* pattern )
{
    int len = 0 , ret = 0;
    char *pat_user = NULL, *pat_dom = NULL;
    char *em_user = NULL, *em_dom = NULL;
    char *pat_sub = NULL , *em_sub = NULL , *pat_next = NULL , *em_next = NULL , *state = NULL ;
    
    sstrtok( pattern, "@", &pat_user, 0, &len, &state );
    sstrtok( NULL, "@", &pat_dom, 0, &len, &state );

    sstrtok( email, "@", &em_user, 0, &len, &state );
    sstrtok( NULL, "@", &em_dom, 0, &len, &state );

    /* if @ in pattern, must have @ sign in email too */
    if ( pat_dom && em_dom == NULL ) { goto done; } 

    if ( !wild_match( pat_user, em_user ) ) { goto done; }

    if ( !pat_dom && !em_dom ) { ret = 1; goto done; } /* no @ in either, ok */

    pat_next = pat_dom; em_next = em_dom;
    do {
	pat_sub = pat_next; em_sub = em_next;
	pat_next = strchr( pat_next, '.' ); 
	if ( pat_next ) { *pat_next = '\0'; pat_next++; }
	em_next = strchr( em_next, '.' ); 
	if ( em_next ) { *em_next = '\0'; em_next++; }

	if ( !wild_match( pat_sub, em_sub ) ) { goto done; }
	
    } while ( pat_next && em_next );

    /* different numbers of subdomains, fail */
    if ( ( pat_next == NULL && em_next != NULL ) ||
	 ( pat_next != NULL && em_next == NULL ) ) { goto done; }
    
    ret = 1;
 done:
    if ( pat_user ) { free( pat_user ); }
    if ( pat_dom ) { free( pat_dom ); }
    if ( em_user ) { free( em_user ); }
    if ( em_dom ) { free( em_dom ); }
    return ret;
}

EXPORT
int hashcash_mint( time_t now_time, int time_width, 
		   const char* resource, unsigned bits, 
		   long anon_period, char** new_token, int tok_len, 
		   long* anon_random, double* tries_taken, char* ext )
{
    long rnd = 0 ;
    char now_utime[ MAX_UTCTIME+1 ] = {0}; /* current time */
    char* token = 0;
    volatile clock_t begin = 0 , end = 0 , tmp = 0 ;
    double elapsed = 0;

    if ( resource == NULL ) {
	return HASHCASH_INTERNAL_ERROR;
    }

    if ( anon_random == NULL ) { anon_random = &rnd; }

    *anon_random = 0;

    if ( bits > SHA1_DIGEST_BYTES * 8 ) {
	return HASHCASH_INVALID_TOK_LEN;
    }

    if ( time_width == 0 ) { time_width = 6; } /* default YYMMDD */

    if ( now_time < 0 ) {
	return HASHCASH_INVALID_TIME;
    }

    if ( anon_period != 0 ) {
	if ( !random_rectangular( (long)anon_period, anon_random ) ) {
	    return HASHCASH_RNG_FAILED;
	}
    }

    now_time += *anon_random;

    if ( time_width != 12 && time_width != 10 && time_width != 6 ) {
	return HASHCASH_INVALID_TIME_WIDTH;
    }

    now_time = round_off( now_time, 12-time_width );
    to_utctimestr( now_utime, time_width, now_time );

    if ( !ext ) { ext = ""; }
    token = malloc( MAX_TOK+strlen(ext)+1 );
    sprintf( token, "%d:%d:%s:%s:%s:", 
	     HASHCASH_FORMAT_VERSION, bits, now_utime, resource, ext );

    end = clock();
    while((begin = clock()) == end) {}
    
    hashcash_fastmint( bits, token, new_token );
    end = clock();

    if ( end < begin ) { tmp = end; end = begin; begin = tmp; }

    elapsed = (end-begin) / (double) CLOCKS_PER_SEC;

    /* hashcash_fastmint does not track iterations, 
       work backwards from time for now -- avoid work if caller
       does not care about tries_taken */

    if ( tries_taken ) { *tries_taken = elapsed * hashcash_per_sec(); }

    return HASHCASH_OK;
}

time_t round_off( time_t now_time, int digits )
{
    struct tm* now = NULL ;

    if ( digits != 2 && digits != 4 && 
	 digits != 6 && digits != 8 && digits != 10 ) {
	return now_time;
    }
    now = gmtime( &now_time );	/* still in UTC */

    switch ( digits ) {
    case 10: now->tm_mon = 0;
    case 8: now->tm_mday = 1;
    case 6: now->tm_hour = 0;
    case 4: now->tm_min = 0;
    case 2: now->tm_sec = 0;
    }
    return mk_utctime( now );
}

EXPORT
int hashcash_validity_to_width( time_t validity_period )
{
    int time_width = 6;		/* default YYMMDD */
    if ( validity_period < 0 ) { return 0; }
    if ( validity_period != 0 ) {
/* YYMMDDhhmmss or YYMMDDhhmm or YYMMDD */
	if ( validity_period < 2*TIME_MINUTE ) { time_width = 12; } 
	else if ( validity_period < 2*TIME_HOUR ) { time_width = 10; }
	else { time_width = 6; }
    }
    return time_width;
}

/* all chars from ascii(33) to ascii(126) inclusive, minus : */

/* limited v0 backwards compatibility: impose this valid random string
 * alphabet limitation on v0 also retroactively even tho the internet
 * draft did not require it.  All widely used clients generate hex /
 * base64 anyway.
 */

#define VALID_STR_CHARS "/+0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"\
			"abcdefghijklmnopqrstuvwxyz="

EXPORT
int hashcash_parse( const char* token, int* vers, int* bits, char* utct,
		    int utct_max, char* token_resource, int res_max, 
		    char** ext, int ext_max ) 
{
    char ver_arr[MAX_VER+1] = {0};
    char bits_arr[3+1] = {0};
    char *bits_str = bits_arr, *ver = ver_arr;
    char *rnd = NULL, *cnt = NULL;
    char *state = NULL ;
    int ver_len = 0 , utct_len = 0 , res_len = 0 , bit_len = 0 , rnd_len = 0 , cnt_len = 0 ;

    /* parse out the resource name component 
     * v1 format:   ver:bits:utctime:resource:ext:rand:counter
     * where utctime is [YYMMDD[hhmm[ss]]] 
     *
     * v0 format:   ver:utctime:resource:rand
     *
     * in v0 & v1 the resource may NOT include :s, if it needs to include
     * :s some encoding such as URL encoding must be used
     */

    if ( ext != NULL ) { *ext = NULL; }
    if ( !sstrtok( token, ":", &ver, MAX_VER, &ver_len, &state ) ) {
	return 0;
    }
    *vers = atoi( ver ); if ( *vers < 0 ) { return 0; }
    if ( *vers == 0 ) {
	*bits = -1;
	if ( !sstrtok( NULL, ":", &utct, utct_max, &utct_len, &state ) ||
	     !sstrtok( NULL, ":", &token_resource, res_max,&res_len,&state ) ||
	     !sstrtok( NULL, ":", &rnd, 0, &rnd_len, &state ) ) {
	    return 0;
	}
    } else if ( *vers == 1 ) {
	if ( !sstrtok( NULL, ":", &bits_str, 3, &bit_len, &state ) ||
	     !sstrtok( NULL, ":", &utct, utct_max, &utct_len, &state ) ||
	     !sstrtok( NULL, ":", &token_resource, res_max,&res_len,&state ) ||
	     !sstrtok( NULL, ":", ext, 0, &ext_max, &state ) ||
	     !sstrtok( NULL, ":", &rnd, 0, &rnd_len, &state ) ||
	     !sstrtok( NULL, ":", &cnt, 0, &cnt_len, &state ) ) {
	    return 0; 
	}
	*bits = atoi( bits_str ); if ( *bits < 0 ) { return 0; }
	if ( strspn( cnt, VALID_STR_CHARS ) != cnt_len ) { return 0; }
    }
    if ( rnd == NULL || strspn( rnd, VALID_STR_CHARS ) != rnd_len ) { return 0; }
    return 1;
}

EXPORT
unsigned hashcash_count( const char* token )
{
    SHA1_ctx ctx;
    byte target_digest[ SHA1_DIGEST_BYTES ] = {0};
    byte token_digest[ SHA1_DIGEST_BYTES ] = {0};
    char ver[MAX_VER+1] = {0};
    int vers = 0 ;
    char* first_colon = NULL; 
    char* second_colon = NULL; 
    int ver_len = 0 ;
    int i = 0 ;
    int last = 0 ;
    int collision_bits = 0 ;

    first_colon = strchr( token, ':' );
    if ( first_colon == NULL ) { return 0; } /* should really fail */
    ver_len = (int)(first_colon - token);
    if ( ver_len > MAX_VER ) { return 0; }
    sstrncpy( ver, token, ver_len );
    vers = atoi( ver );
    if ( vers < 0 ) { return 0; }
    if ( vers > 1 ) { return 0; } /* unsupported version number */
    second_colon = strchr( first_colon+1, ':' );
    if ( second_colon == NULL ) { return 0; } /* should really fail */

    memset( target_digest, 0, SHA1_DIGEST_BYTES );

    SHA1_Init( &ctx );
    SHA1_Update( &ctx, token, strlen( token ) );
    SHA1_Final( &ctx, token_digest );
   
    for ( i = 0; 
	  i < SHA1_DIGEST_BYTES && token_digest[ i ] == target_digest[ i ]; 
	  i++ ) { 
    }
    
    last = i;
    collision_bits = 8 * i;

#define bit( n, c ) (((c) >> (7 - (n))) & 1)

    for ( i = 0; i < 8; i++ ) 
    {
	if ( bit( i, token_digest[ last ] ) == 
	     bit( i, target_digest[ last ] ) ) { 
	    collision_bits++; 
	} else { 
	    break; 
	}
    }
    return collision_bits;
}

EXPORT
long hashcash_valid_for( time_t token_time, time_t validity_period,
			 long grace_period, time_t now_time )
{
    long expiry_time = 0 ;

    /* for ever -- return infinity */
    if ( validity_period == 0 )	{ return HASHCASH_VALID_FOREVER; }

    /* future date in token */
    if ( token_time > now_time + grace_period ) { 
	return HASHCASH_VALID_IN_FUTURE; 
    }

    expiry_time = token_time + validity_period;
    if ( expiry_time + grace_period > now_time ) {
				/* valid return seconds left */
	return expiry_time + grace_period - now_time;
    }
    return HASHCASH_EXPIRED;	/* otherwise expired */
}

#define REGEXP_DIFF "(|)"
#define REGEXP_UNSUP "{}"
#define REGEXP_SAME "\\.?[]*+^$"

#define MAX_RE_ERR 256

int regexp_match( const char* str, const char* regexp, 
		  void** compile, char** err ) 
{
#if defined( REGEXP_BSD )
	char* q = NULL ;
	const char *r = NULL ;
	char* quoted_regexp = malloc( strlen( regexp ) * 2 + 3 );
	
	*err = NULL;
	
	if ( quoted_regexp == NULL ) { *err = "out of memory"; return 0; }

	q = quoted_regexp;
	r = regexp;

	if ( *r != '^' ) { *q++ = '^'; }
	
	for ( ; *r; *q++ = *r++ ) {
	    if ( *r == '\\' ) { 
		if ( strchr( REGEXP_SAME, *(r+1) ) ) {
		    *q++ = *r++; 	/* copy thru \\ unchanged */
		} else { 
		    r++; 		/* skip \c for any c other than \ */
		} 
	    } else if ( strchr( REGEXP_DIFF, *r ) ) {
		*q++ = '\\';
	    } else if ( strchr( REGEXP_UNSUP, *r ) ) {
		*err = "compiled with BSD regexp, {} not suppored";
		return 0;
	    }
	}
	if ( *(q-1) != '$' ) { *q++ = '$'; }
	*q = '\0';
	if ( ( *err = re_comp( quoted_regexp ) ) != NULL ) { return 0; }
	free( quoted_regexp );
	return re_exec( str );
#elif defined( REGEXP_POSIX )
	regex_t** comp = (regex_t**) compile;
	int re_code = 0 ;
	char* bound_regexp = NULL ;
	int re_len = 0 , bre_len = 0 ;
	static char re_err[ MAX_RE_ERR+1 ] = {0};
	re_err[0] = '\0';
	
	if ( *comp == NULL ) {
	    *comp = malloc( sizeof(regex_t) );
	    if ( *comp == NULL ) { *err = "out of memory"; return 0; }
	    bre_len = re_len = strlen(regexp);
	    if ( regexp[0] != '^' || regexp[re_len-1] != '$' ) {
		bound_regexp = malloc( re_len+3 );
		if ( regexp[0] != '^' ) { 
		    bound_regexp[0] = '^';
		    sstrncpy( (bound_regexp+1), regexp, re_len );
		    bre_len++;
		} else {
		    sstrncpy( bound_regexp, regexp, re_len );
		}
		if ( regexp[re_len-1] != '$' ) {
		    bound_regexp[bre_len] = '$';
		    bound_regexp[bre_len+1] = '\0';
		}
	    } else {
		bound_regexp = (char*)regexp;
	    }

	    if ( ( re_code = regcomp( *comp, regexp, 
				      REG_EXTENDED | REG_NOSUB ) ) != 0 ) {
		regerror( re_code, *comp, re_err, MAX_RE_ERR );
		*err = re_err;
		if ( bound_regexp != regexp ) { free( bound_regexp ); }
		return 0;
	    }
	    if ( bound_regexp != regexp ) { free( bound_regexp ); }
	}
	return regexec( *comp, str, 0, NULL, 0 ) == 0;
#else
	*err = "regexps not supported on your platform, used -W wildcards";
	return 0;
#endif
}

EXPORT
int resource_match( int type, const char* token_res, const char* res,
		    void** compile, char** err ) 
{
    switch ( type ) {
    case TYPE_STR: 
	if ( strcmp( token_res, res ) != 0 ) { return 0; }
	break;
    case TYPE_WILD:
	if ( !email_match( token_res, res  ) ) { return 0; }
	break;
    case TYPE_REGEXP:
	if ( !regexp_match( token_res, res, compile, err ) ) { return 0; }
	break;
    default:
	return 0;
    }
    return 1;
}

EXPORT
int hashcash_check( const char* token, int case_flag, const char* resource,
		    void **compile, char** re_err, int type, time_t now_time, 
		    time_t validity_period, long grace_period, 
		    int required_bits, time_t* token_time ) {
    time_t token_t = 0 ;
    char token_utime[ MAX_UTC+1 ] = {0};
    char token_res[ MAX_RES+1 ] = {0};
    int bits = 0, claimed_bits = 0, vers = 0;
    
    if ( token_time == NULL ) { token_time = &token_t; }

    if ( !hashcash_parse( token, &vers, &claimed_bits, token_utime, 
			  MAX_UTC, token_res, MAX_RES, NULL, 0 ) ) {
	return HASHCASH_INVALID;
    }

    if ( vers < 0 || vers > 1 ) {
	return HASHCASH_UNSUPPORTED_VERSION;
    }

    *token_time = from_utctimestr( token_utime, 1 );
    if ( *token_time == -1 ) {
	return HASHCASH_INVALID;
    }

    if ( !case_flag ) {
	stolower( token_res );
    }

    if ( resource && 
	 !resource_match( type, token_res, resource, compile, re_err ) ) {
	if ( *re_err != NULL ) { 
	    return HASHCASH_REGEXP_ERROR;
	} else {
	    return HASHCASH_WRONG_RESOURCE;
	}
    }
    bits = hashcash_count( token );
    if ( vers == 1 ) {
	bits = ( bits < claimed_bits ) ? 0 : claimed_bits;
    }

    if ( bits < required_bits ) {
	return HASHCASH_INSUFFICIENT_BITS;
    }
    return hashcash_valid_for( *token_time, validity_period, 
			       grace_period, now_time );
}

EXPORT
double hashcash_estimate_time( int b )
{
    return hashcash_expected_tries( b ) / (double)hashcash_per_sec();
}

EXPORT
double hashcash_expected_tries( int b )
{
    double expected_tests = 1;
    #define CHUNK ( sizeof( unsigned long )*8 - 1 )
    for ( ; b > CHUNK; b -= CHUNK ) {
	expected_tests *= ((unsigned long)1) << CHUNK;
    }
    expected_tests *= ((unsigned long)1) << b;
    return expected_tests;
}

