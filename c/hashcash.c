/* -*- Mode: C; c-file-style: "stroustrup" -*- */

/*  hashcash mint and verification 
 * 
 *  hashcash is payment in burnt CPU cycles by calculating n-bit
 *  partial hash collisions
 *
 *  see:         http://www.hashcash.org/
 *
 */

#if defined( THINK_C )
    #include <console.h>
    #include <unix.h>
#elif defined( unix ) || defined( VMS ) || defined( __APPLE__ )
    #include <unistd.h>
#elif defined ( WIN32 )
    #include <io.h>
    #include "getopt.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

#include "sdb.h"
#include "utct.h"
#include "random.h"
#include "timer.h"
#include "hashcash.h"

#if defined( OPENSSL )
#include <openssl/sha.h>
#define SHA1_ctx SHA_CTX
#define SHA1_Final( x, digest ) SHA1_Final( digest, x )
#define SHA1_DIGEST_BYTES SHA_DIGEST_LENGTH
#else
#include "sha1.h"
#endif

#define EOK 0			/* no error */
#define EINPUT -1		/* error invalid input */

#define EXIT_UNCHECKED 2	/* don't consider the token valid because */
                                /* there are aspects of the token that */
				/* were not checked */
#define EXIT_ERROR 3

#define MAX_PERIOD 11		/* max time_t = 10 plus 's' */
#define MAX_HDR 256
#define MAX_LINE 1024

/* WARNING: the *PRINTF macros assume a coding style which always uses
 * braces {} around conditional code -- unfortunately there is no
 * portable way to do vararg macros or functions which call other
 * vararg functions
 */

#define QPRINTF if (!quiet_flag) fprintf
#define PPRINTF if (!out_is_tty||quiet_flag) fprintf
#define VPRINTF if (verbose_flag) fprintf

#define QPUTS(f,str) if (!quiet_flag) { fputs(str,f); }
#define PPUTS(f,str) if (!out_is_tty||quiet_flag) { fputs(str,f); }
#define VPUTS(f,str) if (verbose_flag) { fputs(str,f); }

void chomplf( char* token );
void trimspace( char* token );
void die( int err );
void die_msg( const char* );
void usage( const char* );
int parse_period( const char* aperiod, long* resp );
double report_speed( int bits, double* time_est );

typedef struct {
    char* str;
    void* regexp;		/* compiled regexp */
    int type;
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

void db_open( DB* db, const char* db_filename );
void db_purge( DB* db, ARRAY* purge_resource, int purge_all, 
	       long purge_period, time_t now_time, time_t real_time );
int db_in( DB* db, char* token, char *period );
void db_add( DB* db, char* token, char *token_utime );
void db_close( DB* db ) ;

void array_alloc( ARRAY* array, int num );
void array_push( ARRAY* array, char *str, int type, long validity, 
		 long grace, long anon, int width, int bits, int over );
#define array_num( a ) ( (a)->num )
int bit_cmp( const void* ap, const void* bp );
void array_sort( ARRAY* array, int(*cmp)(const void*, const void*) );


void stolower( char* str );

long per_sec = 0;		/* cache calculation */
#define hc_per_sec() ( per_sec ? per_sec : ( per_sec = hashcash_per_sec() ) )
#define hc_est_time(b) ( hashcash_expected_tries(b) / (double)hc_per_sec() )
int quiet_flag;
int verbose_flag;
int out_is_tty;
int in_is_tty;

#define PURGED_KEY "last_purged"

int main( int argc, char* argv[] ) 
{
    long validity_period = 28*TIME_DAY; /* default validity period: 28 days */
    long grace_period = 2*TIME_DAY; /* default grace period: 2 days */

    char period[ MAX_PERIOD+1 ], utcttime[ MAX_UTC+1 ];
    const char* db_filename = "hashcash.sdb";
    int boundary, err, anon_flag = 0, over_flag = 0, grace_flag = 0;
    long anon_period = 0, anon_random;
    int default_bits = 20;	/* default 20 bits */
    int count_bits, bits = 0;
    int collision_bits, check_flag = 0, case_flag = 0, hdr_flag = 0;
    int width_flag = 0, left_flag = 0, speed_flag = 0, utc_flag = 0;
    int bits_flag = 0, str_type = TYPE_WILD; /* default to wildcard match */
    int validity_flag = 0, db_flag = 0, yes_flag = 0, purge_flag = 0;
    int mint_flag = 0, ignore_boundary_flag = 0, name_flag = 0, res_flag = 0;
    int auto_version = 0, version_flag = 0, checked = 0, comma = 0;
    char header[ MAX_HDR+1 ];
    int header_len, token_found = 0, headers_found = 0;
    char token[ MAX_TOK+1 ], line[ MAX_LINE+1 ], token_resource[ MAX_RES+1 ];
    ARRAY purge_resource, resource, tokens, args;

    long purge_period = 0, valid_for = HASHCASH_INVALID, time_period;
    int purge_all = 0, just_flag = 0, re_flag = 0, wild_flag = 0;
    int multiple_validity = 0, multiple_bits = 0, multiple_resources = 0;
    int multiple_grace = 0, accept = 0;

    char token_utime[ MAX_UTC+1 ];	/* time token created */
    time_t real_time = time(0); /* now, in UTCTIME */
    time_t now_time = real_time;
    time_t token_time = 0, expiry_time = 0;
    int time_width_flag = 0;	/* -z option, default 6 YYMMDD */
    int inferred_time_width, time_width = 6; /* default YYMMDD */

    double tries_taken, taken, tries_expected, time_est;
    int opt, vers, db_opened = 0, i, t, tty_info, in_headers, skip, over = 0;
    char* re_err = NULL;
    ELEMENT* ent;
    DB db;

#if defined( THINK_C )
    argc = ccommand(&argv);
#endif

    out_is_tty = isatty( fileno( stdout ) );
    in_is_tty = isatty( fileno( stdin ) ) && !feof( stdin );
    quiet_flag = 0;
    verbose_flag = 0;

    token_resource[0] = '\0';
    token[0] = '\0';
    opterr = 0;

    array_alloc( &resource, 32 );
    array_alloc( &purge_resource, 32 );
    array_alloc( &tokens, 32 );
    array_alloc( &args, 32 );

    while ( (opt=getopt(argc, argv, 
			"-a:b:cde:Ef:g:hij:klmnop:qr:sSt:uvwx:yz:CVWX")) >0 ) {
	switch ( opt ) {
	case 'a': anon_flag = 1; 
	    if ( !parse_period( optarg, &anon_period ) ) {
		usage( "error: -a invalid period arg" );
	    }
	    break;
	case 'b':
	    if ( bits_flag ) { multiple_bits = 1; }
	    bits_flag = 1;

	    if ( strcmp( optarg, "default" ) == 0 ) { 
		bits = default_bits; 
	    } else if ( optarg[0] == '-' || optarg[0] == '+' ) {
		bits = atoi( optarg+1 );
		if ( bits < 0 ) { usage( "error: -b invalid bits arg" ); }
		if ( optarg[0] == '-' ) { bits = -bits; }
		bits = default_bits + bits;
		if ( bits < 0 ) { bits = 0; }
	    } else {		/* absolute */
		bits = atoi( optarg );
		if ( bits < 0 ) { usage( "error: -b invalid bits arg" ); }
	    }
	    if ( bits > SHA1_DIGEST_BYTES * 8 ) {
		usage( "error: max collision with sha1 is 160 bits" );
	    }
	    break;
	case 'C': case_flag = 1; break;
	case 'c': check_flag = 1; 
	    break;
	case 'd': db_flag = 1; break;
	case 'e': 
	    if ( validity_flag ) { multiple_validity = 1; }
	    validity_flag = 1; 
	    if ( !parse_period( optarg, &validity_period ) ||
		 validity_period < 0 ) {
		usage( "error: -e invalid validity period" ); 
	    }
	    break;
	case 'E': 
	    re_flag = 1;
	    str_type = TYPE_REGEXP; 
	    break;
	case 'f': db_filename = strdup( optarg ); break;
	case 'g': 
	    if ( grace_flag ) { multiple_grace = 1; }
	    grace_flag = 1;
	    if ( optarg[0] == '-' ) {
		usage( "error: -g -ve grace period not valid" );
	    }
	    if ( !parse_period( optarg, &grace_period ) ) {
		usage( "error: -g invalid grace period format" );
	    }
	    break;
	case 'h': usage( "" ); break;
	case 'i': ignore_boundary_flag = 1; break;
	case 'j': 
	    just_flag = 1;
	    array_push( &purge_resource, optarg, str_type, 0, 0, 0, 0, 0, 0 );
	    over_flag = 0;
	    break;
	case 'k': purge_all = 1; break;
	case 'l': left_flag = 1; break;
	case 'n': name_flag = 1; break;
	case 'o': over_flag = 1; break;
	case 'm': mint_flag = 1; 
	    if ( !bits_flag ) { 
		bits_flag = 1; 
		bits = default_bits;
	    }
	    break;
	case 'p': 
	    purge_flag = 1;
	    if ( strcmp( optarg, "now" ) == 0 ) { purge_period = 0; }
	    else if ( !parse_period( optarg, &purge_period ) ||
		      purge_period < 0 ) {
		usage( "error: -p invalid purge interval" ); 
	    }
	    break;
	case 'q': quiet_flag = 1; break;
	case 1:
	case 'r':
	    if ( opt == 'r' ) { 
		if ( res_flag ) { multiple_resources = 1; }
		res_flag = 1; 
	    }

	    /* infer appropriate time_width from validity period */
	    if ( validity_flag && !time_width_flag && !inferred_time_width ) {
		time_width = validity_to_width( validity_period );
		if ( !time_width ) { 
		    die_msg( "error: -ve validity period" ); 
		}
		inferred_time_width = time_width;
	    }

	    array_push( &args, optarg, str_type, validity_period, 
			grace_period, anon_period, time_width, bits, 0 );

	    if ( opt == 'r' ) {
		array_push( &resource, optarg, str_type, validity_period, 
			    grace_period, anon_period, time_width, bits, 
			    over_flag );
	    } else if ( opt == 1 ) {
		array_push( &tokens, optarg, str_type, validity_period, 
			    grace_period, anon_period, time_width, bits, 0 );
	    }
	    over_flag = 0;

	    break;
	case 's': speed_flag = 1; break;
	case 'S': str_type = TYPE_STR; break;
	case 't':
	    if ( optarg[0] == '-' || optarg[0] == '+' ) {
		if ( !parse_period( optarg, &time_period ) ) {
		    usage( "error: -t invalid relative time format" );
		}
		now_time += time_period;
	    } else {
		now_time = from_utctimestr( optarg, utc_flag );
		if ( now_time == (time_t)-1 ) { 
		    usage( "error: -t invalid time format" ); 
		}
	    }
	    break;
	case 'u': utc_flag = 1; break;
	case 'v': verbose_flag = 1; break;
        case 'V': version_flag = 1; break;
	case 'w': width_flag = 1; break;
	case 'W': 
	    wild_flag = 1;
	    str_type = TYPE_WILD; 
	    break;
	case 'x':
	    hdr_flag = 1;
	    sstrncpy( header, optarg, MAX_HDR );
	    break;
	case 'X':
	    hdr_flag = 1;
	    sstrncpy( header, "X-Hashcash: ", MAX_HDR );
	    break;
	case 'y': yes_flag = 1; break;
	case 'z': 
	    time_width_flag = 1; 
	    time_width = atoi( optarg );
	    if ( time_width <= 0 || time_width % 2 || time_width > 12) {
	        usage( "error: -z invalid time width: must be 2,4,6,8,10 or 12" );
	    }
	    break;
	case '?': 
	    fprintf( stderr, "error: unrecognized option -%c", optopt );
	    usage( "" );
	    break;
	case ':':
	    fprintf( stderr, "error: option -%c missing argument", optopt );
	    usage( "" );
	    break;
	default: 
	    usage( "error with argument processing" );
	    break;
	}
    }

    if ( validity_flag && !time_width_flag && !inferred_time_width ) {
	time_width = validity_to_width( resource.elt[i].validity );
	if ( !time_width ) { 
	    die_msg( "error: -ve validity period" ); 
	}
	inferred_time_width = time_width;
    }

    if ( array_num( &args ) == 0 && verbose_flag && 
	 name_flag + left_flag + width_flag + check_flag +
	 mint_flag + purge_flag + speed_flag < 1 ) {
	auto_version = 1;
        version_flag = 1; verbose_flag = 0;
    }

    if ( version_flag ) {
        QPRINTF( stderr, "Version: hashcash-%s\n", 
		 HASHCASH_VERSION_STRING );
	PPRINTF( stdout, "%s\n", HASHCASH_VERSION_STRING );
	if ( auto_version ) { exit( EXIT_FAILURE ); }
    }

    if ( mint_flag + check_flag > 1 ) {
	usage( "can only specify one of -m, -c" );
    }

    if ( mint_flag && ( name_flag || width_flag || left_flag ) ) {
	usage( "can not use -n, -w or -l with -m" );
    }

    if ( mint_flag + check_flag + name_flag + left_flag + width_flag + db_flag+
	 bits_flag + res_flag + purge_flag + speed_flag + version_flag == 0 ) {
	usage( "must specify at least one of -m, -c, -d, -n, -l-, -w, -b, -r, -p or -s");
    }

//    if ( check_flag && hdr_flag && name_flag + left_flag + width_flag > 0 ) {
//	usage( "must not specify -n, -l, or -w with -cx or -cX" );
//    }

//    if ( check_flag && hdr_flag && !res_flag ) {
//	usage( "must give -r resource with -cx or -cX" );
//    }

    if ( quiet_flag ) {	verbose_flag = 0; } /* quiet overrides verbose */
    if ( speed_flag && check_flag ) { speed_flag = 0; }	/* ignore speed */
    if ( speed_flag && mint_flag ) { mint_flag = 0; } /* ignore mint */

    if ( purge_flag ) {
	db_open( &db, db_filename );
	db_opened = 1;

	if ( !case_flag ) {
	    for ( i = 0; i < array_num( &purge_resource ); i++ ) {
		stolower( purge_resource.elt[i].str );
	    }
	}

	db_purge( &db, &purge_resource, purge_all, purge_period, 
		  now_time, real_time );

	if ( mint_flag + check_flag + name_flag + left_flag + 
	     width_flag + bits_flag + res_flag + speed_flag == 0 ) { 
	    db_close( &db );	/* just -p we're done */
	    exit( EXIT_SUCCESS );
	}
    }

    if ( mint_flag || speed_flag ) { /* mint stamp */

	if ( mint_flag ) {

	    /* no resources given, read them from stdin */
	    if ( array_num( &args ) == 0 ) {
		if ( in_is_tty ) {
		    fprintf( stderr, "enter resources to mint stamps for one per line:\n" );
		}
		while ( !feof(stdin) ) {
		    line[0] = '\0';
		    fgets( line, MAX_LINE, stdin );
		    chomplf( line );
		    trimspace( line );
		    if ( line[0] != '\0' ) {
			array_push( &resource, line, 0,validity_period, 
				    grace_period, anon_period, time_width, 
				    bits, 0 );
		    }
		}
	    }

	    if ( !case_flag ) { 
		for ( i = 0; i < array_num( &args ); i++ ) {
		    stolower( args.elt[i].str );
		}
	    }
	}

	if ( verbose_flag || ( speed_flag && !bits_flag ) ) {
	    QPRINTF( stderr, "speed: %ld collision tests per second\n",
		     hc_per_sec() );
	    if ( speed_flag && !bits_flag ) {
		PPRINTF( stdout, "%ld\n", hc_per_sec() );
		exit( EXIT_SUCCESS ); /* don't actually calculate it */
	    }
	}

	if ( speed_flag && bits_flag ) {
	    tries_expected = report_speed( bits, &time_est );
	    PPRINTF( stdout, "%.0f\n", time_est );
	    exit( EXIT_SUCCESS ); /* don't actually calculate it */

	}

	timer_start();

	for ( i = 0; i < array_num( &args ); i++ ) {

	    ent = &(args.elt[i]);

	    tries_expected = report_speed( ent->bits, &time_est );

	    err = hashcash_mint( now_time, ent->width, ent->str, ent->bits, 
				 ent->anon, token, MAX_TOK, &anon_random, 
				 &tries_taken );

	    switch ( err ) {
	    case HASHCASH_INVALID_TOK_LEN:
		die_msg( "error: maximum collision with sha1 is 160 bits" );
	    case HASHCASH_RNG_FAILED:
		die_msg( "error: random number generator failed" );
	    case HASHCASH_INVALID_TIME:
		die_msg( "error: outside unix time Epoch" );
	    case HASHCASH_TOO_MANY_TRIES:
		die_msg( "error: failed to find collision in 2^96 tries!" );
	    case HASHCASH_EXPIRED_ON_CREATION:
		die_msg( "error: -a token may expire on creation" );
	    case HASHCASH_INVALID_VALIDITY_PERIOD:
		die_msg( "error: negative validity period" );
	    case HASHCASH_INVALID_TIME_WIDTH:
		die_msg( "error: invalid time width" );
	    case HASHCASH_INTERNAL_ERROR:
		die_msg( "error: internal error" );
	    case HASHCASH_OK:
		break;
	    default:
		die_msg( "error: unknown failure" );
	    }
   
	    if ( anon_flag ) {
		VPRINTF( stderr, "anon period: %ld seconds\n", 
			 (long)anon_period );
		VPRINTF( stderr, "adding: %ld seconds\n", anon_random );
	    }

	    VPRINTF( stderr, "tries: %.0f", tries_taken );
	    VPRINTF( stderr, " %.2f x expected\n", 
		     tries_taken / tries_expected );

	    timer_stop();
	    taken = timer_time() / 1000000.0;
	    VPRINTF( stderr, "time: %.0f seconds\n", taken );
	    if ( hdr_flag ) { QPRINTF( stderr, "%s%s\n", header, token ); }
	    else { QPRINTF( stderr, "hashcash token: %s\n", token ); }
	    if ( hdr_flag ) { PPUTS( stdout, header ); } 
	    PPRINTF( stdout, "%s\n", token );
	}
	exit( EXIT_SUCCESS );

    } else if ( check_flag || name_flag || width_flag || 
		left_flag || bits_flag || res_flag || db_flag ) {

	/* check token is valid */
	trimspace(header);	/* be forgiving about missing space */
	header_len = strlen( header );

	/* if no resources given create a kind of NULL entry to carry */
	/* the other options along; hashcash_check knows to not check */
	/* resource name if resource given is NULL */

	if ( array_num( &resource ) == 0 ) {
	    array_push( &resource, NULL, str_type, validity_period, 
			grace_period, anon_period, time_width, bits, 0 );
	}

	headers_found = 0;
	tty_info = 0;
	in_headers = 0;

	for ( t = 0, token_found = 0, boundary = 0; !boundary; ) {
	    /* read tokens from cmd line args 1st */
	    if ( t < array_num( &tokens ) ) {
		sstrncpy( token, tokens.elt[t].str, MAX_TOK );
		t++;
		token_found = 1;
	    }
	    /* if no hdr flag, and were cmd line tokens, we're done */
	    else if ( !hdr_flag && array_num( &tokens ) > 0 ) { break; }
	    else {
		if ( !in_headers ) { in_headers = 1; }
		if ( in_is_tty && !tty_info ) {
		    if ( hdr_flag ) {
			QPRINTF( stderr, "enter email headers:\n" );
		    } else {
			QPRINTF( stderr, 
				 "enter stamps to check one per line:\n" );
		    }
		    tty_info = 1;
		}
		if ( feof( stdin ) ) { break; } 
		line[0] = '\0';
		fgets( line, MAX_LINE, stdin );
		chomplf( line );
		if ( line[0] == '\0' ) { continue; }
		if ( hdr_flag )	{
		    token_found = (strncmp( line, header, header_len ) == 0);
		    sstrncpy( token, line+header_len, MAX_TOK );
		    if ( token_found ) { headers_found = 1; }
		} else { 
		    token_found = 1; 
		    sstrncpy( token, line, MAX_TOK );
		}
		trimspace( token );
	    }

	    /* end of stamp finder which results in clean stamp in token */

	    if ( token_found ) {
		VPRINTF( stderr, "examining token: %s\n", token );

		skip = 0;
		over = 0;
		accept = 0;
		for ( i = 0; i < array_num( &resource ) && !skip; i++ ) {
		    ent = &resource.elt[i];

		    /* keep going until find non overridden */

		    if ( over && ent->over ) { 
			if ( multiple_resources ) {
			    VPRINTF( stderr, "checking against resource: %s\n", 
				     ent->str );
			}
			QPRINTF( stderr, "no match: overridden resource\n" );
			continue; 
		    }

		    over = 0;

		    valid_for = hashcash_check( token, ent->str, 
						&(ent->regexp), &re_err,
						ent->type, now_time, 
						ent->validity, ent->grace, 
						bits_flag ? ent->bits : 0,
						&token_time );

		    if ( valid_for < 0 ) {
			switch ( valid_for ) {
		    /* reasons token is broken, give up */

			case HASHCASH_INVALID:
			    QPRINTF( stderr, "skipped: token invalid\n" );
			    skip = 1; continue;

			case HASHCASH_UNSUPPORTED_VERSION:
			    QPRINTF( stderr, "skipped: unsupported version\n" );
			    skip = 1; continue;

			    /* avoid plural reporting if one of something */
			    /* not possible for match with */
			    /* other resources in these situations */
			    /* as will just fail for same reason again */

			case HASHCASH_WRONG_RESOURCE:
			    if ( !multiple_resources ) {
				QPRINTF( stderr, "skipped: token with wrong resource\n" );
				skip = 1; continue;
			    }
			    break;

			case HASHCASH_INSUFFICIENT_BITS:
			    if ( !multiple_bits && bits_flag ) {
			        QPRINTF( stderr, 
					 "skipped: token has insufficient bits\n" );
				skip = 1; continue;
			    }
			    break;

			case HASHCASH_EXPIRED:
			    if ( !multiple_validity && check_flag ) {
				QPRINTF( stderr, "skipped: token expired\n" );
				skip = 1; continue;
			    }
			    break;

			case HASHCASH_VALID_IN_FUTURE:
			    if ( !multiple_grace && check_flag ) {
				QPRINTF( stderr, "skipped: valid in future\n" );
				skip = 1; continue;
			    }
			    break;
			}
		    }

		    if ( multiple_resources ) {
			VPRINTF( stderr, "checking against resource: %s\n", 
				 ent->str );
		    }

		    if ( valid_for >= 0 ) { break; }

		    if ( width_flag || left_flag || name_flag ) {
			accept = 1;
		    }

		    /* reason token failed for this resource  */
		    /* but may succeed with diff resource */

		    switch ( valid_for ) {

			/* need non override to continue after this */
			/* kind of failure */

		    case HASHCASH_INSUFFICIENT_BITS:
			if ( bits_flag ) {
			    QPUTS( stderr, "no match: token has insufficient bits\n" );
			    over = 1; accept = 0; continue;
			}
			break;
		    case HASHCASH_VALID_IN_FUTURE:
			if ( check_flag ) {
			    QPRINTF( stderr, "no match: valid in future\n" );
			    over = 1; accept = 0; continue;
			}
			break;
		    case HASHCASH_EXPIRED:
			if ( check_flag ) { 
			    QPUTS( stderr, "no match: token expired\n" );
			    over = 1; accept = 0; continue;
			}
			break;
		    case HASHCASH_WRONG_RESOURCE:
			QPRINTF( stderr, "no match: wrong resource\n" );
			accept = 0;
			continue;

		     /* fatal error stop */

		    case HASHCASH_REGEXP_ERROR:
			fprintf( stderr, "regexp error: " );
			die_msg( re_err );
			break;
			
		    default:
			die_msg( "internal error" );
			break;
		    }
		}

		if ( valid_for >= 0 || accept ) {
		    if ( db_flag ) {
			if ( !db_opened ) {
			    db_open( &db, db_filename );
			    db_opened = 1;
			}
			if ( db_in( &db, token, token_utime ) ) {
			    QPRINTF( stderr, "skipped: spent token\n" );
			    valid_for = HASHCASH_SPENT;
			    continue; /* to next token */
			}
		    }

		    QPRINTF( stderr, "matched token: %s\n", token );

		    if ( name_flag || verbose_flag ) {
			hashcash_parse( token, &vers, utcttime, MAX_UTC,
					token_resource, MAX_RES );
			QPRINTF( stderr, "token resource name: %s\n", 
				 token_resource );
			PPRINTF( stdout, "%s", token_resource );
		    }

		    if ( width_flag || verbose_flag ) {
			count_bits = hashcash_count( token );
			QPRINTF( stderr, "token value: %d\n", count_bits );
			if ( name_flag ) { PPUTS( stdout, " " ); }
			PPRINTF( stdout, "%d", count_bits );
		    } 

		    if ( left_flag || verbose_flag ) {
			QPRINTF( stderr, "valid: " );
			if ( valid_for > 0 ) {
			    QPRINTF( stderr, "for %ld seconds\n",
				     (long)valid_for );
			    if ( name_flag || width_flag ) {
				PPUTS( stdout, " " );
			    }
			    PPRINTF( stdout, "%ld", (long)valid_for );
			} else { 
			    expiry_time = token_time + validity_period;
			    switch ( valid_for ) {
			    case 0:
				QPUTS( stderr, "forever\n" );
				if ( name_flag || width_flag ) {
				    PPUTS( stdout, " " );
				}
				PPUTS( stdout, "0" );
				break;
			    case HASHCASH_VALID_IN_FUTURE:
				QPRINTF( stderr, "in %ld seconds\n",
					 token_time-(now_time+grace_period) );
				if ( name_flag || width_flag ) {
				    PPUTS( stdout, " " );
				}
				PPRINTF( stdout, "+%ld",
					 token_time-(now_time+grace_period) );
				break;
			    case HASHCASH_EXPIRED:
				QPRINTF( stderr, "expired %ld seconds ago\n",
					 now_time-(expiry_time+grace_period) );
				if ( name_flag || width_flag ) {
				    PPUTS( stdout, " " );
				}
				PPRINTF( stdout, "-%ld",
					 now_time-(expiry_time+grace_period) );
				break;
			    default:
				QPRINTF( stderr, "not valid\n" );
			    }
			}
		    }

		    if ( name_flag || width_flag || left_flag ) {
			PPUTS( stdout, "\n" );
			continue; /* to next token */
		    }

		    if ( db_flag ) {
			VPUTS( stderr, 
			       "database: not double spent\n" );
			checked = yes_flag || ( res_flag && bits_flag);
			if ( checked ) {
			    sprintf( period, "%ld", (long)validity_period );
			    db_add( &db, token, period );
			}
		    } else {
			checked = yes_flag;
		    }
		    goto leave;	/* matched so leave */
		}
	    } else if ( hdr_flag && in_headers && !ignore_boundary_flag  ) {
		if ( line[0] == '\0' ) { boundary = 1; }
	    }
	}
	if ( !width_flag && !left_flag && !name_flag ) {
	    if ( hdr_flag && !headers_found ) {
		QPRINTF( stderr, "warning: no line matching %s found in input\n",
			 header );
	    } 
	    QPUTS( stderr, "rejected: no valid tokens found\n" );
	    valid_for = HASHCASH_NO_TOKEN;
	}

    leave:
	if ( width_flag || left_flag || name_flag ) {
	    exit( yes_flag ? EXIT_SUCCESS : EXIT_UNCHECKED );
	}
	QPRINTF( stderr, "check: %s", 
		 ( valid_for >= 0 ) ? 
		 ( checked ? "ok" : "ok but not fully checked as" ) : 
		 "failed" );
	if ( valid_for >= 0 ) {
	    if ( !checked ) {
		if ( !bits_flag ) {
		    comma = 1; QPUTS( stderr, " bits" );
		}
		if ( !check_flag ) {
		    if ( comma ) { QPUTS( stderr, "," ); } 
		    comma = 1; QPUTS( stderr, " expiry" );
		}
		if ( !res_flag ) {
		    if ( comma ) { QPUTS( stderr, "," ); } 
		    comma = 1; QPUTS( stderr, " resource" );
		}
		if ( !db_flag ) {
		    if ( comma ) { QPUTS( stderr, "," ); } 
		    comma = 1; QPUTS( stderr, " database" );
		}
		if ( comma ) { QPUTS( stderr, " not specified" ); }
	    }
	}
	QPUTS( stderr, "\n" );
	if ( valid_for < 0 ) { exit( EXIT_FAILURE ); }
	exit( checked ? EXIT_SUCCESS : 
	      ( yes_flag ? EXIT_SUCCESS : EXIT_UNCHECKED ) );
    }
    exit( EXIT_ERROR );		/* get here if no functional flags */
    return 0;
}

int parse_period( const char* aperiod, long* resp )
{
    int period_len;
    char last_char;
    long res = 1;
    char period_array[MAX_PERIOD+1];
    char* period = period_array;

    if ( period == NULL ) { return 0; }
    period_len = strlen( aperiod );
    if ( period_len == 0 ) { return 0; }
    last_char = aperiod[period_len-1];
    if ( ! isdigit( last_char ) && ! strchr( "YyMdhmsw", last_char ) ) {
	return 0;
    }

    sstrncpy( period, aperiod, MAX_PERIOD );

    if ( ! isdigit( last_char ) ) { period[--period_len] = '\0'; }
    if ( period[0] == '+' || period[0] == '-' ) {
	if ( period[0] == '-' ) { res = -1; }
	period++; period_len--;
    }
    if ( period_len > 0 ) { res *= atoi( period ); }
    switch ( last_char )
    {
    case 's': break;
    case 'm': res *= TIME_MINUTE; break;
    case 'h': res *= TIME_HOUR; break;
    case 'd': res *= TIME_DAY; break;
    case 'w': res *= TIME_DAY*7; break;
    case 'M': res *= TIME_MONTH; break;
    case 'y': case 'Y': res *= TIME_YEAR; break;
    case '0': case '1': case '2': case '3': case '4': 
    case '5': case '6': case '7': case '8': case '9': break;
    default: return 0;
    }
    *resp = res;
    return 1;
}

void usage( const char* msg )
{
    if ( msg ) { fputs( msg, stderr ); fputs( "\n\n", stderr ); }
    fprintf( stderr, "mint:\t\thashcash [-m] [opts] resource\n" );
    fprintf( stderr, "measure speed:\thashcash -s [-b bits]\n" );
    fprintf( stderr, "check:\t\thashcash -c [opts] -d -r resource [-e period] [token]\n" );
    fprintf( stderr, "purge expired:\thashcash -p now [-k] [-j resource] [-t time] [-u]\n" );
    fprintf( stderr, "count bits:\thashcash -w [opts] [token]\n" );
    fprintf( stderr, "get resource:\thashcash -n [opts] [token]\n" );
    fprintf( stderr, "time remaining:\thashcash -l [opts] -e period [token]\n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "\t-b bits\t\tfind or check partial hash collision of length bits\n" );
    fprintf( stderr, "\t-d\t\tuse database (for double spending detection)\n" );
    fprintf( stderr, "\t-r resource\tresource name for minting or checking token\n" );
    fprintf( stderr, "\t-o\t\tprevious resource overrides this resource\n" );
    fprintf( stderr, "\t-e period\ttime until token expires\n" );
    fprintf( stderr, "\t-g\t\tgrace period for clock skew\n" );
    fprintf( stderr, "\t-t time\t\tmodify current time token created at\n" );
    fprintf( stderr, "\t-a time\t\tmodify time by random amount in range given\n" );
    fprintf( stderr, "\t-u\t\tgive time in UTC instead of local time\n" );
    fprintf( stderr, "\t-q\t\tquiet -- suppress all informational output\n" );
    fprintf( stderr, "\t-v\t\tprint verbose informational output\n" );
    fprintf( stderr, "\t-h\t\tprint this usage info\n" );
    fprintf( stderr, "\t-f dbfile\tuse filename dbfile for database\n" );
    fprintf( stderr, "\t-j resource\twith -p delete just tokens matching the given resource\n" );
    fprintf( stderr, "\t-k\t\twith -p delete all not just expired\n" );
    fprintf( stderr, "\t-x string\tprepend token output to string (or input match string)\n" );
    fprintf( stderr, "\t-X\t\tshorthand for -x 'X-Hashcash: '\n" );
    fprintf( stderr, "\t-i\t\twith -x/-X and -c, check msg body as well\n" );
    fprintf( stderr, "\t-y\t\treturn success if token is valid but not fully checked\n" );
    fprintf( stderr, "\t-z width\twidth of time field 2,4,6,8,10 or 12 chars (default 6)\n" );
    fprintf( stderr, "\t-C\t\tmatch resources as case sensitive (default insensitive)\n" );
    fprintf( stderr, "\t-S\t\tmatch following resources as text strings\n" );
    fprintf( stderr, "\t-W\t\tmatch following resources with wildcards (default)\n" );
    fprintf( stderr, "\t-E\t\tmatch following resources as regular expression\n" );
    fprintf( stderr, "examples:\n" );
    fprintf( stderr, "\thashcash -mb20 foo                               # mint 20 bit collision\n" );
    fprintf( stderr, "\thashcash -cdb20 -r foo 0:020814:foo:55f4316f29cd98f2  # check collision\n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "see hashcash (1) man page or http://www.hashcash.org/ for more details.\n" );
    exit( EXIT_ERROR );
}

typedef struct {
    time_t expires_before;
    char now_utime[ MAX_UTC+1 ];
    int all;
    ARRAY* resource;
} db_arg;

/* compile time assert */

#if MAX_UTC > MAX_VAL
#error "MAX_UTC must be less than MAX_VAL"
#endif

static int sdb_cb_token_matcher( const char* key, char* val,
				 void* argp, int* err ) {
    db_arg* arg = (db_arg*)argp;
    char token_utime[ MAX_UTC+1 ];
    char token_res[ MAX_RES+1 ], *res;
    time_t expires;
    time_t expiry_period;
    time_t created;
    int vers, i, matched, type;
    void** compile;
    char* re_err;

    *err = 0;
    if ( strcmp( key, PURGED_KEY ) == 0 ) {
	sstrncpy( val, arg->now_utime, MAX_UTC ); 
	return 1;		/* update purge time */
    }

    if ( !hashcash_parse( key, &vers, token_utime, MAX_UTC, token_res, 
			  MAX_RES ) ) {
	*err = EINPUT;		/* corrupted token in DB */
	return 0;
    }
    if ( vers != 0 ) {
	*err = EINPUT; 		/* unsupported version number in DB */
	return 0; 
    }
    /* if purging only for given resource */
    if ( array_num( arg->resource ) > 0 ) {
	matched = 0;
	for ( i = 0; !matched && i < array_num( arg->resource ); i++ ) {
	    res = arg->resource->elt[i].str;
	    type = arg->resource->elt[i].type;
	    compile = &(arg->resource->elt[i].regexp);
	    matched = resource_match( type, token_res, res, compile, &re_err );
	    if ( re_err != NULL ) { 
		fprintf( stderr, "regexp error: " ); 
		die_msg( re_err ); 
	    }
	}
	if ( !matched ) { return 1; } /* if it doesn't match, keep it */
    }
    if ( arg->all ) { return 0; } /* delete all regardless of expiry */
    if ( val[0] == '0' && val[1] == '\0' ) { return 1; } /* keep forever */
    if ( strlen( val ) != strspn( val, "0123456789" ) ) {
	*err = EINPUT; return 0;
    }
    expiry_period = atoi( val );
    if ( expiry_period < 0 ) { *err = EINPUT; return 0; } /* corrupted */
    created = from_utctimestr( token_utime, 1 );
    if ( created < 0 ) { *err = EINPUT; return 0; } /* corrupted */
    expires = created + expiry_period;
    if ( expires <= arg->expires_before ) { return 0; }	/* delete if expired */
    return 1;			/* otherwise keep */
}

void db_purge( DB* db, ARRAY* purge_resource, int purge_all, 
	       long purge_period, time_t now_time, time_t real_time ) {
    int err;
    time_t last_time;
    char purge_utime[ MAX_UTC+1 ];	/* time token created */
    int ret = 0;
    db_arg arg;

    if ( now_time < 0 ) { die_msg( "error: outside unix time Epoch" ); }

    if ( !sdb_lookup( db, PURGED_KEY, purge_utime, MAX_UTC, &err ) ) {
	die( err );
    }

    last_time = from_utctimestr( purge_utime, 1 );
    if ( last_time < 0 ) { /* not first time, but corrupted */
	purge_period = 0; /* purge now */
    }

    if ( !to_utctimestr( arg.now_utime, MAX_UTC, now_time ) ) {
	die_msg( "error: converting time" );
    }

    arg.expires_before = now_time;
    arg.resource = purge_resource;
    arg.all = purge_all;

    if ( purge_period == 0 || now_time >= last_time + purge_period ) {
	VPRINTF( stderr, "purging database: ..." );
	ret = sdb_updateiterate( db, sdb_cb_token_matcher, (void*)&arg, &err );
	VPRINTF( stderr, ret ? "done\n" : "failed\n" ); 
    }
}

void db_open( DB* db, const char* db_filename ) {
    int err;

    if ( !sdb_open( db, db_filename, &err ) ) { die( err ); }
    fgetc( db->file );		/* try read to trigger EOF */
    if ( feof( db->file ) ) {
	if ( !sdb_add( db, PURGED_KEY, "700101000000", &err ) ) { 
	    die( err ); 
	}
    }
    rewind( db->file );
}

int db_in( DB* db, char* token, char *period ) {
    int err;
    int in_db;

    in_db = sdb_lookup( db, token, period, MAX_UTC, &err ); 
    if ( err ) { die( err ); }
    return in_db;
}

void db_add( DB* db, char* token, char *period ) {
    int err;
    if ( !sdb_add( db, token, period, &err ) ) { die( err ); }
}

void db_close( DB* db ) {
    int err;
    if ( !sdb_close( db, &err ) ) { die( err ); }
}

void stolower( char* str ) {
    if ( !str ) { return; }
    for ( ; *str; str++ ) {
	*str = tolower( *str );
    }
}

/* num = start size, auto-grows */ 

void array_alloc( ARRAY* array, int num ) {
    array->num = 0;
    array->max = num;
    array->elt = malloc( sizeof( ELEMENT ) * num );
    if ( array->elt == NULL ) { die_msg( "out of memory" ); }
}

/* auto-grow array */

void array_push( ARRAY* array, char *str, int type, long validity, 
		 long grace, long anon, int width, int bits, int over ) 
{
    if ( array->num >= array->max ) {
	array->elt = realloc( array->elt, array->max * 2 );
	if ( array->elt == NULL ) { die_msg( "out of memory" ); }
	array->max *= 2;
    }
    array->elt[array->num].regexp = NULL; /* compiled regexp */
    array->elt[array->num].type = type;
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

void die( int err ) 
{
    const char* str = "";

    switch ( err ) {
    case EOK:
	exit( EXIT_SUCCESS );
	break;
    case EINPUT:
	str = "invalid inputs";
	break;
    default:
	str = strerror( err );
	break;
    }
    QPRINTF( stderr, "error: %s\n", str );
    exit( EXIT_ERROR );
}

void die_msg( const char* str ) {
    QPUTS( stderr, str );
    QPUTS( stderr, "\n" );
    exit( EXIT_ERROR );
}

/* remove unix, DOS and MAC linefeeds from line ending */

void chomplf( char* token ) {
    int tok_len = strlen(token);
    if ( token[tok_len-1] == '\n' ) { token[--tok_len] = '\0'; }
    if ( token[tok_len-1] == '\r' ) { token[--tok_len] = '\0'; }
    if ( token[tok_len-1] == '\n' ) { token[--tok_len] = '\0'; }
}

void trimspace( char* token ) {
    int tok_len = strlen(token);
    int tok_begin = 0;
    while ( tok_begin < tok_len && isspace(token[tok_begin]) ) {
	tok_begin++;
    }
    if ( tok_begin > 0 ) {
	tok_len -= tok_begin;
	strcpy( token, token+tok_begin );
    }
    while ( tok_len > 0 && isspace(token[tok_len-1]) ) {
	token[--tok_len] = '\0';
    }
}

double report_speed( int bits, double* time_est ) 
{
    double tries_expected = hashcash_expected_tries( bits );
    
    VPRINTF( stderr, "mint: %d bit partial hash collision\n", bits );
    VPRINTF( stderr, "expected: %.0f tries (= 2^%d tries)\n",
	     tries_expected, bits );

    if ( !quiet_flag ) {
	*time_est = hc_est_time( bits );
	QPRINTF( stderr, "time estimate: %.0f seconds", *time_est );
	if ( *time_est > TIME_AEON ) {
	    QPRINTF( stderr, " (%.0f aeons)", *time_est / TIME_AEON );
	} else if ( *time_est > TIME_YEAR * 10 ) {
	    QPRINTF( stderr, " (%.0f years)", *time_est / TIME_YEAR );
	} else if ( *time_est > TIME_YEAR ) {
	    QPRINTF( stderr, " (%.1f years)", *time_est / TIME_YEAR );
	} else if ( *time_est > TIME_MONTH ) {
	    QPRINTF( stderr, " (%.1f months)", *time_est / TIME_MONTH );
	} else if ( *time_est > TIME_DAY * 10 ) {
	    QPRINTF( stderr, " (%.0f days)", *time_est / TIME_DAY );
	} else if ( *time_est > TIME_DAY ) {
	    QPRINTF( stderr, " (%.1f days)", *time_est / TIME_DAY );
	} else if ( *time_est > TIME_HOUR ) {
	    QPRINTF( stderr, " (%.0f hours)", *time_est / TIME_HOUR );
	} else if ( *time_est > TIME_MINUTE ) {
	    QPRINTF( stderr, " (%.0f minutes)", *time_est / TIME_MINUTE );
	} else if ( *time_est > 1 ) {
	    /* already printed seconds */
	} else if ( *time_est > TIME_MILLI_SECOND ) {
	    QPRINTF( stderr, " (%.0f milli-seconds)", 
		     *time_est / TIME_MILLI_SECOND );
	} else if ( *time_est > TIME_MICRO_SECOND ) {
	    QPRINTF( stderr, " (%.0f micro-seconds)", 
		     *time_est / TIME_MICRO_SECOND );
	} else if ( *time_est > TIME_NANO_SECOND ) {
	    QPRINTF( stderr, " (%.0f nano-seconds)", 
		     *time_est / TIME_NANO_SECOND );
	}
	QPUTS( stderr, "\n" );
    }
    return tries_expected;
}
