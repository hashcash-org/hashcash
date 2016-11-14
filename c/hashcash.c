/* -*- Mode: C; c-file-style: "stroustrup" -*- */

/*  hashcash mint and verification 
 * 
 *  hashcash is payment in burnt CPU cycles by calculating n-bit
 *  partial hash preimages of the all 0 string
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
#include "hashcash.h"
#include "libfastmint.h"
#include "sstring.h"
#include "getopt.h"

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
#define MAX_LINE 1024+MAX_EXT+1

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

#define HDR_LINE_LEN 80
char *read_header( FILE* f, char** s, int* slen, int* alloc, 
		   char* a, int alen );
int read_eof( FILE* fp, char* a );
void chomplf( char* token );
void trimspace( char* token );
void die( int err );
void die_msg( const char* );
void usage( const char* );
int parse_period( const char* aperiod, long* resp );
double report_speed( int bits, double* time_est, int display );

#if defined( WIN32 )
#define LINEFEED "\r\n"
#else
#define LINEFEED "\n"
#endif

/* avoid needing to export stolower from hashcash.dll */

void mystolower( char* str );
#define stolower mystolower

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

void db_open( DB* db, const char* db_filename );
void db_purge_arr( DB* db, ARRAY* purge_resource, int purge_all, 
		   long purge_period, time_t now_time, long validity_period,
		   long grace_period );
int db_purge( DB* db, ARRAY* purge_resource, int purge_all, 
	      long purge_period, time_t now_time, long validity_period,
	      long grace_period, int verbose, int* err );
int db_in( DB* db, char* token, char *period );
void db_add( DB* db, char* token, char *token_utime );
void db_close( DB* db ) ;

void array_alloc( ARRAY* array, int num );
void array_push( ARRAY* array, const char *str, int type, int case_flag, 
		 long validity, long grace, long anon, int width, int bits, 
		 int over );
#define array_num( a ) ( (a)->num )
int bit_cmp( const void* ap, const void* bp );
void array_sort( ARRAY* array, int(*cmp)(const void*, const void*) );


#define hc_est_time(b) ( hashcash_expected_tries(b) / \
        (double)hashcash_per_sec() )
int quiet_flag;
int verbose_flag;
int out_is_tty;
int in_is_tty;

#define PROGRESS_FMT "percent: %%.0lf/%%.0lf = %%.%dlf%%%% [%%d/%%d bits]%%c"
char progress_format[80] = { 0 };

int progress_callback(int percent, int largest, int target, 
		      double counter, double expected, void* user);

#define PURGED_KEY "last_purged"

int main( int argc, char* argv[] ) 
{
    long validity_period = 28*TIME_DAY; /* default validity period: 28 days */
    long purge_validity_period = 28*TIME_DAY; /* same default */
    long grace_period = 2*TIME_DAY; /* default grace period: 2 days */

    char period[ MAX_PERIOD+1 ], utcttime[ MAX_UTC+1 ];
    const char* db_filename = "hashcash.sdb";
    int boundary, err, anon_flag = 0, over_flag = 0, grace_flag = 0;
    long anon_period = 0, anon_random = 0 ;
    int default_bits = 20;	/* default 20 bits */
    int count_bits, claimed_bits = 0, bits = 0;
    int check_flag = 0, case_flag = 0, hdr_flag = 0;
    int width_flag = 0, left_flag = 0, speed_flag = 0, utc_flag = 0;
    int bits_flag = 0, str_type = TYPE_WILD; /* default to wildcard match */
    int validity_flag = 0, db_flag = 0, yes_flag = 0, purge_flag = 0;
    int mint_flag = 0, ignore_boundary_flag = 0, name_flag = 0, res_flag = 0;
    int auto_version = 0, version_flag = 0, checked = 0, comma = 0;
    char header[ MAX_HDR+1 ] = { 0 };
    char header2[ MAX_HDR+1 ] = { 0 };
    char* header_wrapped = NULL;
    int hdr_len, hdr2_len, token_found = 0, token2_found = 0, hdrs_found = 0;
    char token[ MAX_TOK+1 ] = { 0 }, token_resource[ MAX_RES+1 ] = { 0 };
    char *new_token = NULL ;
    char line_arr[ MAX_LINE+1 ] = { 0 }, *line = line_arr;
    int line_max = MAX_LINE, line_alloc = 0;
    char ahead[ MAX_LINE+1 ] = { 0 } , *ext = NULL, *junk = NULL;
    ARRAY purge_resource, resource, tokens, args;

    clock_t start = 0, end = 0, tmp = 0;

    long purge_period = 0, valid_for = HASHCASH_INVALID, time_period = 0 ;
    int purge_all = 0;
    int multiple_validity = 0, multiple_bits = 0, multiple_resources = 0;
    int multiple_grace = 0, accept = 0, parsed = 0;

    char token_utime[ MAX_UTC+1 ] = { 0 } ;	/* time token created */
    time_t real_time = time(0); /* now, in UTCTIME */
    time_t now_time = real_time;
    time_t token_time = 0, expiry_time = 0;
    int time_width_flag = 0;	/* -z option, default 6 YYMMDD */
    int compress = 0;		/* fast by default */
    int inferred_time_width = 0, time_width = 6; /* default YYMMDD */
    int core = 0, res = 0, core_flag = 0;

    double tries_taken = 0, taken = 0, tries_expected = 0, time_est = 0;
    int opt = 0, vers = 0, db_opened = 0, i = 0, j = 0, t = 0, tty_info = 0;
    int in_headers = 0, skip = 0, over = 0, precision = 0;
    char *re_err = NULL;
    ELEMENT* ent = NULL;
    DB db;
    hashcash_callback callback = NULL;

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
		"-a:b:cde:f:g:hij:klmnop:qr:st:uvwx:yz:CEMO:PSVXZ:")) >0 ) {
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
		usage( "error: max preimage with sha1 is 160 bits" );
	    }
	    break;
	case 'C': case_flag = 1; break;
	case 'c': check_flag = 1; break;
	case 'd': db_flag = 1; break;
	case 'e': 
	    if ( validity_flag ) { multiple_validity = 1; }
	    validity_flag = 1; 
	    if ( !parse_period( optarg, &validity_period ) ||
		 validity_period < 0 ) {
		usage( "error: -e invalid validity period" ); 
	    }
	    break;
	case 'E': str_type = TYPE_REGEXP; break;
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
	    array_push( &purge_resource, optarg, str_type, case_flag, 
			0, 0, 0, 0, 0, 0 );
	    over_flag = 0;
	    break;
	case 'k': purge_all = 1; break;
	case 'l': left_flag = 1; break;
	case 'n': name_flag = 1; break;
	case 'o': over_flag = 1; break;
	case 'O': 
	    core_flag = 1; 
	    core = atoi( optarg ); 
	    res = hashcash_use_core( core );
	    if ( res < 1 ) {
		usage( res == -1 ? "error: -O no such core\n" : 
		       "error: -O core does not work on this platform" );
	    }
	    break;
	case 'm': mint_flag = 1; 
	    if ( !bits_flag ) { 
		bits_flag = 1; 
		bits = default_bits;
	    }
	    break;
	case 'M': str_type = TYPE_WILD; break;
	case 'p': 
	    if ( purge_flag ) { usage("error: only one -p flag per call"); }
	    purge_flag = 1;
	    if ( strcmp( optarg, "now" ) == 0 ) { purge_period = 0; }
	    else if ( !parse_period( optarg, &purge_period ) ||
		      purge_period < 0 ) {
		usage( "error: -p invalid purge interval" ); 
	    }
	    purge_validity_period = validity_period;
	    break;
	case 'P': callback = progress_callback; break;
	case 'q': quiet_flag = 1; break;
	case 1:
	case 'r':
	    if ( opt == 'r' ) { 
		if ( res_flag ) { multiple_resources = 1; }
		res_flag = 1; 
	    }

	    /* infer appropriate time_width from validity period */
	    if ( validity_flag && !time_width_flag && !inferred_time_width ) {
                time_width = hashcash_validity_to_width( validity_period );
		if ( !time_width ) { 
		    die_msg( "error: -ve validity period" ); 
		}
		inferred_time_width = time_width;
	    }

	    array_push( &args, optarg, str_type, case_flag, validity_period, 
			grace_period, anon_period, time_width, bits, 0 );

	    if ( opt == 'r' ) {
		array_push( &resource, optarg, str_type, case_flag,
			    validity_period, grace_period, anon_period, 
			    time_width, bits, over_flag );
	    } else if ( opt == 1 ) {
		array_push( &tokens, optarg, str_type, case_flag, 
			    validity_period, grace_period, anon_period, 
			    time_width, bits, 0 );
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
		now_time = hashcash_from_utctimestr( optarg, utc_flag );
		if ( now_time == (time_t)-1 ) { 
		    usage( "error: -t invalid time format" ); 
		}
	    }
	    break;
	case 'u': utc_flag = 1; break;
	case 'v': verbose_flag = 1; break;
        case 'V': version_flag = 1; break;
	case 'w': width_flag = 1; break;
	case 'x':
	    ext = strdup( optarg ); 
/* deprecate old -x flag */
/*	    hdr_flag = 1; */
/*	    sstrncpy( header, optarg, MAX_HDR ); */
	    break;
	case 'X':
	    hdr_flag = 1;
	    sstrncpy( header, "X-Hashcash: ", MAX_HDR );
	    sstrncpy( header2, "Hashcash: ", MAX_HDR );
	    break;
	case 'y': yes_flag = 1; break;
	case 'z': 
	    time_width_flag = 1; 
	    time_width = atoi( optarg );
	    if ( time_width != 6 && time_width != 10 && time_width != 12 ) {
	        usage( "error: -z invalid time width: must be 6, 10 or 12" );
	    }
	    break;
	case 'Z': compress = atoi( optarg ); 
            /* temporary work around for bug discovered in 1.15, disable -Z2 */
/*	    if ( compress > 1 ) { compress = 1; } */
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
        time_width = hashcash_validity_to_width( resource.elt[0].validity );
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

    if ( quiet_flag ) {	verbose_flag = 0; } /* quiet overrides verbose */
    if ( speed_flag && check_flag ) { speed_flag = 0; }	/* ignore speed */

    if ( purge_flag ) {
	db_open( &db, db_filename );
	db_opened = 1;

	db_purge_arr( &db, &purge_resource, purge_all, purge_period, 
		  now_time, validity_flag ? purge_validity_period : 0,
		  grace_period );

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
		    junk = fgets( line, MAX_LINE, stdin );
		    chomplf( line );
		    trimspace( line );
		    if ( line[0] != '\0' ) {
			array_push( &args, line, 0, 0, validity_period, 
				    grace_period, anon_period, time_width, 
				    bits, 0 );
		    }
		}
	    }
	}

	/* integrate the bench test */

	if ( verbose_flag && speed_flag && !mint_flag ) {
	    if ( core_flag ) {
		hashcash_benchtest( 3, core );
	    } else {
		hashcash_benchtest( 3, -1 );
	    }
	    exit( EXIT_SUCCESS ); /* don't actually calculate it */
	}

	if ( !verbose_flag && speed_flag && mint_flag ) {
	    QPRINTF( stderr, "core: %s\n", 
		     hashcash_core_name( hashcash_core() ) );
	    QPRINTF( stderr, "speed: %ld preimage tests per second\n",
                     hashcash_per_sec() );
            PPRINTF( stdout, "%ld\n", hashcash_per_sec() );
	    QPRINTF( stderr, "compression: %d\n", compress );
	}

	if ( verbose_flag || ( speed_flag && !bits_flag ) ) {
	    QPRINTF( stderr, "core: %s\n",
                     hashcash_core_name( hashcash_core() ) );
	    QPRINTF( stderr, "speed: %ld preimage tests per second\n",
                     hashcash_per_sec() );
	    if ( speed_flag && !bits_flag && !mint_flag ) {
                PPRINTF( stdout, "%ld\n", hashcash_per_sec() );
	    }
	    QPRINTF( stderr, "compression: %d\n", compress );
	    if ( speed_flag && !bits_flag && !mint_flag ) {
		exit( EXIT_SUCCESS ); /* don't actually calculate it */
	    }
	}

	if ( speed_flag && bits_flag && !mint_flag ) {
	    tries_expected = report_speed( bits, (PPE) ? &time_est : 0,
					   !quiet_flag );
	    PPRINTF( stdout, "%.0f\n", time_est );
	    exit( EXIT_SUCCESS ); /* don't actually calculate it */
	}

	for ( i = 0; i < array_num( &args ); i++ ) {

	    start = clock();

	    ent = &(args.elt[i]);

	    tries_expected = report_speed( ent->bits, NULL, 
					   verbose_flag||speed_flag );

	    if ( !ent->case_flag ) { stolower( ent->str ); }

	    /* calculate precision to see responsive % progress */
	    /* aim to see progress min every 0.25 seconds */
	    time_est = hc_est_time( ent->bits ) / 100;
	    precision = 0;
	    for ( j = 0; j <= 12; j++ ) {
		if ( time_est > 0.25 ) { time_est /= 10; precision++; }
	    }
	    sprintf( progress_format, PROGRESS_FMT, precision );

	    err = hashcash_mint( now_time, ent->width, ent->str, ent->bits, 
				 ent->anon, &new_token, &anon_random, 
				 &tries_taken, ext, compress, 
				 callback, NULL );
	    end = clock();

	    switch ( err ) {
	    case HASHCASH_INVALID_TOK_LEN:
		die_msg( "error: maximum preimage with sha1 is 160 bits" );
	    case HASHCASH_RNG_FAILED:
		die_msg( "error: random number generator failed" );
	    case HASHCASH_INVALID_TIME:
		die_msg( "error: outside unix time Epoch" );
	    case HASHCASH_TOO_MANY_TRIES:
		die_msg( "error: failed to find preimage in 2^96 tries!" );
	    case HASHCASH_EXPIRED_ON_CREATION:
		die_msg( "error: -a stamp may expire on creation" );
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
	    VPRINTF( stderr, " %.0f%% of expected                         \n", 
		     tries_taken / tries_expected * 100 );

	    if ( end < start ) { tmp = end; end = start; start = tmp; }
	    taken = (end-start)/(double)CLOCKS_PER_SEC;
	    VPRINTF( stderr, "time: %.0f seconds\n", taken );
	    
	    if ( hdr_flag ) {
		header_wrapped = hashcash_make_header( new_token, HDR_LINE_LEN,
						       header, '\t', 
						       LINEFEED );
		if ( header_wrapped == NULL ) { 
		    /* don't expect this but for security fail closed */
		    fprintf(stderr, "out of memory\n");
		    exit( EXIT_FAILURE );
		}
		fprintf( stdout, "%s", header_wrapped );
		free( header_wrapped );
	    } else { 
		fprintf( stdout, "%s%s\n", 
			 ((quiet_flag||!out_is_tty)? "" : "hashcash stamp: "),
			 new_token );
	    }
	    free( new_token );
	}
	exit( EXIT_SUCCESS );

    } else if ( check_flag || name_flag || width_flag || 
		left_flag || bits_flag || res_flag || db_flag ) {

	/* check token is valid */
	trimspace( header );	/* be forgiving about missing space */
	hdr_len = strlen( header );

	if ( header2[0] ) {
	    trimspace( header2 );
	    hdr2_len = strlen( header2 );
	}

	/* if no resources given create a kind of NULL entry to carry */
	/* the other options along; hashcash_check knows to not check */
	/* resource name if resource given is NULL */

	if ( array_num( &resource ) == 0 ) {
	    array_push( &resource, NULL, str_type, case_flag, validity_period, 
			grace_period, anon_period, time_width, bits, 0 );
	}

	hdrs_found = 0;
	tty_info = 0;
	in_headers = 0;

	ahead[0] = '\0';
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
			if ( ignore_boundary_flag ) {
			    QPRINTF( stderr, "enter email headers followed by mail body:\n" );
			} else {
			    QPRINTF( stderr, "enter email headers:\n" );
			}
		    } else {
			QPRINTF( stderr, 
				 "enter stamps to check one per line:\n" );
		    }
		    tty_info = 1;
		}
		if ( read_eof( stdin, ahead ) ) { break; }
		read_header( stdin, &line, &line_max, &line_alloc,
			     ahead, MAX_LINE );
		if ( line[0] == '\0' ) { continue; }
		if ( hdr_flag )	{
		    token_found = (strncasecmp( line, header, 
						hdr_len ) == 0);
		    token2_found = header2[0] && 
			(strncasecmp( line, header2, hdr2_len ) == 0);
		    if ( token_found ) { 
			sstrncpy( token, line+hdr_len, MAX_TOK );
			hdrs_found = 1; 
		    } else if ( token2_found ) { 
			sstrncpy( token, line+hdr2_len, MAX_TOK );
			hdrs_found = 1;
			token_found = 1;
		    }
		} else {
		    token_found = 1; 
		    sstrncpy( token, line, MAX_TOK );
		}
		trimspace( token );
	    }

	    /* end of stamp finder which results in clean stamp in token */

	    if ( token_found ) {
		VPRINTF( stderr, "examining stamp: %s\n", token );

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

		    if ( !ent->case_flag ) { stolower( ent->str ); }
		    valid_for = hashcash_check( token, ent->case_flag, 
						ent->str, 
						&(ent->regexp), &re_err,
						ent->type, now_time, 
						ent->validity, ent->grace, 
						bits_flag ? ent->bits : 0,
						&token_time );

		    if ( valid_for < 0 ) {
			switch ( valid_for ) {
		    /* reasons token is broken, give up */

			case HASHCASH_INVALID:
			    QPRINTF( stderr, "skipped: stamp invalid\n" );
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
				QPRINTF( stderr, "skipped: stamp with wrong resource\n" );
				skip = 1; continue;
			    }
			    break;

			case HASHCASH_INSUFFICIENT_BITS:
			    if ( !multiple_bits && bits_flag ) {
			        QPRINTF( stderr, 
					 "skipped: stamp has insufficient bits\n" );
				skip = 1; continue;
			    }
			    break;

			case HASHCASH_EXPIRED:
			    if ( !multiple_validity && check_flag ) {
				QPRINTF( stderr, "skipped: stamp expired\n" );
				skip = 1; continue;
			    }
			    break;

			case HASHCASH_VALID_IN_FUTURE:
			    if ( !multiple_grace && check_flag ) {
				QPRINTF( stderr, "skipped: stamp valid in future\n" );
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
			    QPUTS( stderr, "no match: stamp has insufficient bits\n" );
			    over = 1; accept = 0; continue;
			}
			break;
		    case HASHCASH_VALID_IN_FUTURE:
			if ( check_flag ) {
			    QPRINTF( stderr, "no match: stamp valid in future\n" );
			    over = 1; accept = 0; continue;
			}
			break;
		    case HASHCASH_EXPIRED:
			if ( check_flag ) { 
			    QPUTS( stderr, "no match: stamp expired\n" );
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
			    QPRINTF( stderr, "skipped: spent stamp\n" );
			    valid_for = HASHCASH_SPENT;
			    continue; /* to next token */
			}
		    }

		    QPRINTF( stderr, "matched stamp: %s\n", token );

		    if ( name_flag || verbose_flag ) {
			if ( ext ) { free( ext ); ext = NULL; }
			hashcash_parse( token, &vers, &claimed_bits,
					utcttime, MAX_UTC,
					token_resource, MAX_RES, &ext, 0 );
			parsed = 1;
			QPRINTF( stderr, "stamp resource name: %s\n", 
				 token_resource );
			PPRINTF( stdout, "%s", token_resource );
		    }

		    if ( width_flag || verbose_flag ) {
			count_bits = hashcash_count( token );
			if ( !parsed ) {
			    hashcash_parse( token, &vers, &claimed_bits,
					    utcttime, MAX_UTC,
					    token_resource, MAX_RES, &ext, 0 );
			    parsed = 1;
			}
			if ( vers == 1 ) {
			    count_bits = ( count_bits < claimed_bits ) ? 
				0 : claimed_bits;
			}
			QPRINTF( stderr, "stamp value: %d\n", count_bits );
			if ( name_flag || verbose_flag ) { 
			    PPUTS( stdout, " " ); 
			}
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
			    case HASHCASH_VALID_FOREVER:
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
			    sprintf( period, "%ld", validity_period );
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
	    if ( hdr_flag && !hdrs_found ) {
		QPRINTF( stderr, "warning: no line matching %s found in input\n",
			 header );
	    } 
	    QPUTS( stderr, "rejected: no valid stamps found\n" );
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

int progress_callback(int ignore, int largest, int target, 
		      double counter, double expected, void* user)
{
    static int previous_percent = -1;
    static int previous_largest = -1;
    static double previous_counter = -1;
    double percent;

    if ( previous_counter != counter || previous_largest != largest ) {
	percent = counter*100/expected;
	fprintf( stderr, progress_format,
		 counter, expected, percent, largest, target,
		 (previous_largest == largest) ? '\r' : '\n' );
	previous_percent = percent;
	previous_largest = largest;
	previous_counter = counter;
    }
    return 1;
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
    fprintf( stderr, "check:\t\thashcash -c [opts] -d -r resource [-e period] [stamp]\n" );
    fprintf( stderr, "purge expired:\thashcash -p now [-k] [-j resource] [-t time] [-u]\n" );
    fprintf( stderr, "count bits:\thashcash -w [opts] [stamp]\n" );
    fprintf( stderr, "get resource:\thashcash -n [opts] [stamp]\n" );
    fprintf( stderr, "time remaining:\thashcash -l [opts] -e period [stamp]\n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "\t-b bits\t\tfind or check partial preimage of length bits\n" );
    fprintf( stderr, "\t-d\t\tuse database (for double spending detection)\n" );
    fprintf( stderr, "\t-r resource\tresource name for minting or checking stamp\n" );
    fprintf( stderr, "\t-o\t\tprevious resource overrides this resource\n" );
    fprintf( stderr, "\t-e period\ttime until stamp expires\n" );
    fprintf( stderr, "\t-g\t\tgrace period for clock skew\n" );
    fprintf( stderr, "\t-t time\t\tmodify current time stamp created at\n" );
    fprintf( stderr, "\t-a time\t\tmodify time by random amount in range given\n" );
    fprintf( stderr, "\t-u\t\tgive time in UTC instead of local time\n" );
    fprintf( stderr, "\t-q\t\tquiet -- suppress all informational output\n" );
    fprintf( stderr, "\t-v\t\tprint verbose informational output\n" );
    fprintf( stderr, "\t-h\t\tprint this usage info\n" );
    fprintf( stderr, "\t-f dbfile\tuse filename dbfile for database\n" );
    fprintf( stderr, "\t-j resource\twith -p delete just stamps matching the given resource\n" );
    fprintf( stderr, "\t-k\t\twith -p delete all not just expired\n" );
    fprintf( stderr, "\t-x ext\t\tput in extension field\n" );
    fprintf( stderr, "\t-X\t\toutput with header format 'X-Hashcash: '\n" );
    fprintf( stderr, "\t-i\t\twith -X and -c, check msg body as well\n" );
    fprintf( stderr, "\t-y\t\treturn success if stamps is valid but not fully checked\n" );
    fprintf( stderr, "\t-z width\twidth of time field 6,10 or 12 chars (default 6)\n" );
    fprintf( stderr, "\t-C\t\tmatch resources as case sensitive (default insensitive)\n" );
    fprintf( stderr, "\t-S\t\tmatch following resources as text strings\n" );
    fprintf( stderr, "\t-W\t\tmatch following resources with wildcards (default)\n" );
    fprintf( stderr, "\t-E\t\tmatch following resources as regular expression\n" );
    fprintf( stderr, "\t-P\t\tshow progress while searching\n");
    fprintf( stderr, "\t-O core\t\tuse specified minting core\n");
    fprintf( stderr, "\t-Z n\t\t0 = fast (default), 1 = medium, 2 = small/slow\n");
    fprintf( stderr, "examples:\n" );
    fprintf( stderr, "\thashcash -mb20 foo                               # mint 20 bit preimage\n" );
    fprintf( stderr, "\thashcash -cdb20 -r foo 1:20:040806:foo::831d0c6f22eb81ff:15eae4 # check preimage\n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "see hashcash (1) man page or http://www.hashcash.org/ for more details.\n" );
    fflush( stderr );
    exit( EXIT_ERROR );
}

typedef struct {
    time_t expires_before;
    char now_utime[ MAX_UTC+1 ];
    int all;
    long validity;
    long grace;
    ARRAY* resource;
} db_arg;

/* compile time assert */

#if MAX_UTC > MAX_VAL
#error "MAX_UTC must be less than MAX_VAL"
#endif

static int sdb_cb_token_matcher( const char* key, char* val,
				 void* argp, int* err ) {
    db_arg* arg = (db_arg*)argp;
    char token_utime[ MAX_UTC+1 ] = {0};
    char token_res[ MAX_RES+1 ] = {0}, lower_token_res[ MAX_RES+1 ] = {0};
    char *res = NULL;
    time_t expires = 0;
    time_t expiry_period = 0;
    time_t created = 0;
    int vers = 0, i = 0, bits = 0, matched = 0, type = 0, case_flag = 0;
    int lowered = 0 ;
    void** compile = NULL;
    char* re_err = NULL;

    *err = 0;
    if ( strcmp( key, PURGED_KEY ) == 0 ) {
	sstrncpy( val, arg->now_utime, MAX_UTC ); 
	return 1;		/* update purge time */
    }

    if ( !hashcash_parse( key, &vers, &bits, token_utime, MAX_UTC, 
			  token_res, MAX_RES, NULL, 0 ) ) {
	*err = EINPUT;		/* corrupted token in DB */
	return 0;
    }
    if ( vers != 0 && vers != 1 ) {
	*err = EINPUT; 		/* unsupported version number in DB */
	return 0; 
    }
    /* if purging only for given resource */
    if ( array_num( arg->resource ) > 0 ) {
	matched = 0;
	for ( i = 0; !matched && i < array_num( arg->resource ); i++ ) {
	    res = arg->resource->elt[i].str;
	    type = arg->resource->elt[i].type;
	    case_flag = arg->resource->elt[i].case_flag;
	    compile = &(arg->resource->elt[i].regexp);
	    if ( !case_flag && !lowered ) {
		strncpy( lower_token_res, token_res, MAX_RES );
		stolower( lower_token_res );
		lowered = 1;
	    }
	    matched = hashcash_resource_match( type, case_flag ? 
					       token_res : lower_token_res, 
					       res, compile, &re_err );
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
    created = hashcash_from_utctimestr( token_utime, 1 );
    if ( created < 0 ) { *err = EINPUT; return 0; } /* corrupted */
    expires = created 
	+ (arg->validity ? arg->validity : expiry_period) + arg->grace;
    if ( expires <= arg->expires_before ) { return 0; }	/* delete if expired */
    return 1;			/* otherwise keep */
}

int hashcash_db_purge( DB* db, const char* purge_resource, int type,
		       int case_flag, long validity_period, long grace_period,
		       int purge_all, long purge_period, time_t now_time,
		       int* err ) {
    ARRAY* purge_resource_arr = NULL;
    array_alloc( purge_resource_arr, 1 );
    array_push( purge_resource_arr, purge_resource, type, case_flag,
		validity_period, grace_period, 0, 0, 0, 0 );
    return db_purge( db, purge_resource_arr, purge_all, purge_period, 
		     now_time, validity_period, grace_period, 0, err );
}

void db_purge_arr( DB* db, ARRAY* purge_resource, int purge_all, 
	       long purge_period, time_t now_time, long validity_period,
	       long grace_period ) {
    int res, err = HASHCASH_FAIL;
    res = db_purge( db, purge_resource, purge_all, purge_period, now_time, 
		    validity_period, grace_period, verbose_flag, &err );
    switch ( res ) {
    case HASHCASH_INVALID_TIME:
	die_msg( "error: invalid time argument" ); break;
    case HASHCASH_FAIL: die( err ); break;
    case HASHCASH_OK: break;
    default: die( err );
    }
}

int db_purge( DB* db, ARRAY* purge_resource, int purge_all, 
	       long purge_period, time_t now_time, long validity_period,
	       long grace_period, int verbose, int* err ) {
    time_t last_time = 0 ;
    char purge_utime[ MAX_UTC+1 ] = {0}; /* time token created */
    int ret = 0;
    db_arg arg;

    if ( now_time < 0 ) { return HASHCASH_INVALID_TIME; }

    if ( !sdb_lookup( db, PURGED_KEY, purge_utime, MAX_UTC, err ) ) {
	return 0;
    }

    last_time = hashcash_from_utctimestr( purge_utime, 1 );
    if ( last_time < 0 ) { /* not first time, but corrupted */
	purge_period = 0; /* purge now */
    }

    if ( !hashcash_to_utctimestr( arg.now_utime, MAX_UTC, now_time ) ) {
	return HASHCASH_INVALID_TIME;
    }

    arg.expires_before = now_time;
    arg.resource = purge_resource;
    arg.all = purge_all;
    arg.validity = validity_period;
    arg.grace = grace_period;

    if ( purge_period == 0 || now_time >= last_time + purge_period ) {
	VPRINTF( stderr, "purging database: ..." );
	ret = sdb_updateiterate( db, sdb_cb_token_matcher, (void*)&arg, err );
	VPRINTF( stderr, ret ? "done\n" : "failed\n" ); 
    } else {
	ret = 1;
    }
    return ret;
}

void db_open( DB* db, const char* db_filename ) {
    int err;
    if (!hashcash_db_open( db, db_filename, &err )) { die(err); }
}

int db_in( DB* db, char* token, char *period ) {
    int err = 0;
    int res;

    res = hashcash_db_in( db, token, period, &err );
    if ( err ) { die( err ); }
    return res;
}

void db_add( DB* db, char* token, char *period ) {
    int err = 0;
    if ( !hashcash_db_add( db, token, period, &err ) ) {
	die( err );
    }
}

void db_close( DB* db ) {
    int err = 0;
    if ( !hashcash_db_close( db, &err ) ) {
	die( err );
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

void die_msg( const char* str ) 
{
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
	memmove( token, token+tok_begin, strlen(token+tok_begin)+1 );
    }
    while ( tok_len > 0 && isspace(token[tok_len-1]) ) {
	token[--tok_len] = '\0';
    }
}

double report_speed( int bits, double* time_est, int display ) 
{
    double te = 0;
    double tries_expected = hashcash_expected_tries( bits );
    
    VPRINTF( stderr, "mint: %d bit partial hash preimage\n", bits );
    VPRINTF( stderr, "expected: %.0f tries (= 2^%d tries)\n",
	     tries_expected, bits );

    if ( time_est ) { *time_est = hc_est_time( bits ); }

    if ( display ) {
	te = hc_est_time( bits );
	QPRINTF( stderr, "time estimate: %.0f seconds", te );
	if ( te > TIME_AEON ) {
	    QPRINTF( stderr, " (%.0f aeons)", te / TIME_AEON );
	} else if ( te > TIME_YEAR * 10 ) {
	    QPRINTF( stderr, " (%.0f years)", te / TIME_YEAR );
	} else if ( te > TIME_YEAR ) {
	    QPRINTF( stderr, " (%.1f years)", te / TIME_YEAR );
	} else if ( te > TIME_MONTH ) {
	    QPRINTF( stderr, " (%.1f months)", te / TIME_MONTH );
	} else if ( te > TIME_DAY * 10 ) {
	    QPRINTF( stderr, " (%.0f days)", te / TIME_DAY );
	} else if ( te > TIME_DAY ) {
	    QPRINTF( stderr, " (%.1f days)", te / TIME_DAY );
	} else if ( te > TIME_HOUR ) {
	    QPRINTF( stderr, " (%.0f hours)", te / TIME_HOUR );
	} else if ( te > TIME_MINUTE ) {
	    QPRINTF( stderr, " (%.0f minutes)", te / TIME_MINUTE );
	} else if ( te > 1 ) {
	    /* already printed seconds */
	} else if ( te > TIME_MILLI_SECOND ) {
	    QPRINTF( stderr, " (%.0f milli-seconds)", 
		     te / TIME_MILLI_SECOND );
	} else if ( te > TIME_MICRO_SECOND ) {
	    QPRINTF( stderr, " (%.0f micro-seconds)", 
		     te / TIME_MICRO_SECOND );
	} else if ( te > TIME_NANO_SECOND ) {
	    QPRINTF( stderr, " (%.0f nano-seconds)", 
		     te / TIME_NANO_SECOND );
	}
	QPUTS( stderr, "\n" );
    }
    return tries_expected;
}

int read_append( char** s, int* smax, int* alloc, char* append )
{
    int slen = strlen( *s );
    int alen = strlen( append );

    if ( slen + alen > *smax ) {
	if ( *alloc ) {
	    *s = realloc( *s, slen+alen+1 );
	} else {
	    *s = malloc( slen+alen+1 );
	    *alloc = 1;
	}
	if ( *s == NULL ) { return 0; } /* out of memory */
    }
    sstrncpy( (*s)+slen, append, alen );
    return 1;
}

char *read_header( FILE* f, char** s, int* smax, int* alloc, 
		   char* a, int amax )
{
    char* junk;

    (*s)[0] = '\0';
    if ( a[0] ) {
	sstrncpy( *s, a, *smax );
    } else {
	junk = fgets( *s, *smax, f );
	chomplf( *s );
    }

    do {
	a[0] = '\0';
	junk = fgets( a, amax, f );
	chomplf( a );
	if ( a[0] == '\t' || a[0] == ' ') {
	    read_append( s, smax, alloc, a+1 );
	}
    } while ( a[0] == '\t' || a[0] == ' ' );

    return *s;
}

int read_eof( FILE* fp, char* a )
{
    return feof(fp) && a[0] == '\0';
}

void mystolower( char* str ) {
    if ( !str ) { return; }
    for ( ; *str; str++ ) {
        *str = tolower( *str );
    }
}
