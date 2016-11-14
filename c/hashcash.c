/* -*- Mode: C; c-file-style: "bsd" -*- */

/*  "hash cash" mint and verification 
 * 
 *  hash cash is payment in burnt CPU cycles by calculating n-bit
 *  partial hash collisions
 *
 *  see:         http://www.cypherspace.org/hashcash/
 *
 */

#if defined( THINK_C )
    #include <console.h>
    #include <unix.h>
#elif defined( unix ) || defined( VMS )
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

#define MAX_DATE 50		/* Sun Mar 10 19:25:06 2002 (EST) */
#define MAX_PERIOD 11		/* max time_t = 10 plus 's' */
#define MAX_HDR 256
#define MAX_LINE 1024

/* WARNING: the *PRINTF macros assume a coding style which always uses
 * braces {} around conditional code -- unfortunately there is no
 * portable way to do vararg macros or functions which call other
 * vararg functions
 */

#define QPRINTF if (!quiet_flag) fprintf
#define PPRINTF if (!out_is_tty) fprintf
#define VPRINTF if (verbose_flag) fprintf

#define QPUTS(f,str) if (!quiet_flag) { fputs(str,f); }
#define PPUTS(f,str) if (!out_is_tty) { fputs(str,f); }
#define VPUTS(f,str) if (verbose_flag) { fputs(str,f); }

void chomplf( char* token );
void die( int err );
void die_msg( const char* );
void usage( const char* );
int parse_period( const char* aperiod, long* resp );

int quiet_flag;
int verbose_flag;
int in_is_tty;

int do_purge( DB* db, time_t expires_before, time_t now, int all, 
	      char* purge_resource, int* err );

#define PURGED_KEY "last_purged"

int main( int argc, char* argv[] )
{
    const char* db_filename = "hashcash.db";
    int db_exists = 0;
    int boundary;
    int found;
    int err;
    int non_opt_argc = 0;
    int anon_flag = 0;
    long anon_period = 0;
    long anon_random;
    int bits = 0;
    int collision_bits;
    int check_flag = 0;
    int hdr_flag = 0;
    int left_flag = 0;
    int speed_flag = 0;
    int utc_flag = 0;
    int bits_flag = 0;
    int validity_flag = 0;
    int database_flag = 0;
    int yes_flag = 0;
    int width_flag = 0;
    int purge_flag = 0;
    int mint_flag = 0;
    int ignore_boundary_flag = 0;
    int name_flag = 0;
    int res_flag = 0;
    int checked = 0;
    int comma = 0;
    int in_db;
    char header[ MAX_HDR+1 ];
    int header_len;
    char token[ MAX_TOK+1 ];
    char line[ MAX_LINE+1 ];
    char token_resource[ MAX_RES+1 ];
    char resource[ MAX_RES+1 ];
    char* zone = "UTC";
    char date[ MAX_DATE+1 ];

    long purge_period = 0;
    char purge_utime[ MAX_UTC+1 ];	/* time token created */
    int purge_all = 0;
    int just_flag = 0;
    char purge_resource[ MAX_RES+1 ];
    time_t last_time;
    long valid_for = 0;

    long validity_period = 0;	/* default validity period: forever */
    char period[ MAX_PERIOD+1 ];

    char token_utime[ MAX_UTC+1 ];	/* time token created */
    time_t token_time;
    time_t local_time;
    time_t real_time = utctime(0); /* now, in UTCTIME */
    time_t now_time = real_time;
    int time_width = 6;		/* default YYMMDD */
    long time_period;

    long per_sec;
    double tries_expected = 1;	/* suppress dumb warning */
    double tries_taken;
    double time_estimated;
    double taken;
    int isopen;
    int out_is_tty = isatty( fileno( stdout ) );
    int opt;
    DB db;

#if defined( THINK_C )
    argc = ccommand(&argv);
#endif

    in_is_tty = isatty( fileno( stdin ) ) && !feof( stdin );
    quiet_flag = 0;
    verbose_flag = 0;

    purge_resource[0] = '\0';
    token_resource[0] = '\0';
    opterr = 0;

    while ( ( opt = getopt( argc, argv, 
			    "a:b:cde:f:hij:klmnp:qr:st:uvwx:y" ) ) > 0 )
    {
	switch ( opt )
	{
	case 'a': anon_flag = 1; 
	    if ( !parse_period( optarg, &anon_period ) )
	    {
		usage( "error: -a invalid period arg\n\n" );
	    }
	    break;
	case 'b':
	    bits_flag = 1;
	    bits = atoi( optarg );
	    if ( bits < 0 ) { usage( "error: -b invalid bits arg\n\n" ); }
	    break;
	case 'c': check_flag = 1; break;
	case 'd': database_flag = 1; break;
	case 'e': 
	    validity_flag = 1; 
	    if ( !parse_period( optarg, &validity_period ) ||
		 validity_period < 0 )
	    {
		usage( "error: -e invalid validity period\n\n" ); 
	    }
	    break;
	case 'f': db_filename = optarg; break;
	case 'h': usage( "" ); break;
	case 'i': ignore_boundary_flag = 1; break;
	case 'j': 
	    just_flag = 1;
	    sstrncpy( purge_resource, optarg, MAX_RES );
	    break;
	case 'k': purge_all = 1; break;
	case 'l': left_flag = 1; break;
	case 'n': name_flag = 1; break;
	case 'm': mint_flag = 1; break;
	case 'p': 
	    purge_flag = 1;
	    if ( strcmp( optarg, "now" ) == 0 ) { purge_period = 0; }
	    else if ( !parse_period( optarg, &purge_period ) ||
		      purge_period < 0 )
	    {
		usage( "error: -p invalid purge interval\n\n" ); 
	    }
	    break;
	case 'q': quiet_flag = 1; break;
	case 'r':
	    res_flag = 1;
	    sstrncpy( resource, optarg, MAX_RES );
	    break;
	case 's': speed_flag = 1; break;
	case 't':
	    if ( optarg[0] == '-' || optarg[0] == '+' ) 
	    {
		if ( !parse_period( optarg, &time_period ) )
		{
		    usage( "error: -t invalid relative time format\n\n" );
		}
		now_time += time_period;
	    }
	    else 
	    {
		now_time = from_utctimestr( optarg );
		if ( now_time == (time_t)-1 ) 
		{ 
		    usage( "error: -t invalid time format\n\n" ); 
		}
		if ( !utc_flag ) { now_time = local_to_utctime( now_time ); }
	    }
	    break;
	case 'u': utc_flag = 1; break;
	case 'v': verbose_flag = 1;	break;
	case 'w': width_flag = 1; break;
	case 'x':
	    hdr_flag = 1;
	    sstrncpy( header, optarg, MAX_HDR );
	    break;
	case 'y': yes_flag = 1; break;
	case '?': 
	    fprintf( stderr, "error: unrecognized option -%c\n\n", optopt );
	    usage( "" );
	    break;
	case ':':
	    fprintf( stderr, "error: option -%c missing argument\n\n", optopt );
	    usage( "" );
	    break;
	default: 
	    usage( "error with argument processing\n\n" );
	    break;
	}
    }

    if ( name_flag + left_flag + width_flag + check_flag > 1 )
    {
	usage( "can only specify one of -n, -l, -w and -c\n\n" );
    }

    if ( check_flag & mint_flag )
    {
	usage( "can only specify one of -c -r -m\n\n" );
    }

    db_exists = file_exists( db_filename, &err );
    if ( err ) { die( err ); }
    if ( purge_flag && db_exists )
    {
	if ( !( isopen = sdb_open( &db, db_filename, &err ) &&
		sdb_lookup( &db, PURGED_KEY, purge_utime, 
			    MAX_UTC, &err ) ) )
	{
	    goto dbleave;	/* should always be there */
	}

	last_time = from_utctimestr( purge_utime );
	if ( last_time < 0 ) /* not first time, but corrupted */
	{ 
	    purge_period = 0; /* purge now */
	}

	if ( now_time < 0 )
	{
	    die_msg( "error: outside unix time Epoch\n" );
	}

	if ( purge_period == 0 || now_time >= last_time + purge_period )
	{
	    if ( !do_purge( &db, now_time, real_time, purge_all, 
			    just_flag ? purge_resource : NULL , &err ) ) 
	    { 
		goto dbleave;
	    }
	}
	if ( !sdb_close( &db, &err ) ) { isopen = 0; goto dbleave; }
    dbleave:
	if ( err )
	{
	    if ( isopen ) { sdb_close( &db, &err ); }
	    die( err );
	}
    }

    if ( purge_flag && !mint_flag && !check_flag ) /* just -p we're done */
    {
	exit( EXIT_SUCCESS );
    }

    non_opt_argc = argc - optind; /* how many args */

    if ( quiet_flag ) {	verbose_flag = 0; } /* quiet overrides verbose */

    if ( !out_is_tty ) { quiet_flag = 1; } /* !out_is_tty => quiet */

    if ( name_flag && !check_flag ) { check_flag = 1; }
    if ( left_flag && !check_flag ) { check_flag = 1; }
    if ( width_flag && !check_flag ) { check_flag = 1; }
    if ( speed_flag && check_flag ) { speed_flag = 0; }	/* ignore speed */
    if ( width_flag && bits_flag ) 
    {
	usage( "error: only one of -b and -w can be used\n\n" );
    }
    if ( res_flag && name_flag ) { 
	usage( "error: only one of -r and -n can be used\n\n" );
    }
    if ( !check_flag ) /* mint token */
    {
	if ( non_opt_argc > 1 ) 
	{ 
	    usage( "error: only one argument should be given\n\n" ); 
	}
	if ( res_flag && non_opt_argc > 0 )
	{
	    usage( "error: two resource names given\n\n" );
	}
	if ( !speed_flag )
	{
	    if ( !res_flag )
	    {
		if ( non_opt_argc == 1 ) 
		{
		    sstrncpy( token, argv[optind++], MAX_TOK ); 
		} 
		else 
		{
		    if ( in_is_tty )
		    {
			QPRINTF( stderr, "resource name to mint: " );
		    }
		    fgets( token, MAX_TOK, stdin );
		    chomplf( token );
		}
	    }

	    if ( in_is_tty && out_is_tty && !bits_flag )
	    {
		QPRINTF( stderr, "required token value (bits): " );
		scanf( "%3d", &bits );
		bits_flag = 1;
	    }

	    if ( !bits_flag ) 
	    {
		usage( "must use -b option when minting a token\n\n" ); 
	    }

	    if ( !res_flag )
	    {
		sstrncpy( resource, token, MAX_RES );
	    }
	}

	if ( bits_flag && bits > SHA1_DIGEST_BYTES * 8 ) 
	{
	    usage( "error: maximum collision with sha1 is 160 bits\n\n" );
	}

	if ( ( speed_flag && !bits_flag ) || verbose_flag )
	{
	    per_sec = hashcash_per_sec();
	    QPRINTF( stderr, "speed: %ld collision tests per second\n",
		     per_sec );
	    if ( speed_flag && !bits_flag )
	    {
		PPRINTF( stdout, "%ld\n", per_sec ); fflush( stdout );
	    }
	}

	if ( bits_flag || verbose_flag || speed_flag )
	{
	    time_estimated = hashcash_estimate_time( bits );
	    tries_expected = hashcash_expected_tries( bits );
	    VPRINTF( stderr, "mint: %d bit partial hash collision\n", 
		     bits );
	    VPRINTF( stderr, "expected: %.0f tries (= 2^%d tries)\n",
		     tries_expected, bits );
	    if ( bits_flag && ( verbose_flag || speed_flag ) )
	    {
		QPRINTF( stderr, "time estimate: %.0f seconds", 
			 time_estimated ); 
		if ( time_estimated > TIME_AEON ) 
		{
		    QPRINTF( stderr, " (%.0f aeons)", 
			     time_estimated / TIME_AEON );
		} 
		else if ( time_estimated > TIME_YEAR * 10 ) 
		{
		    QPRINTF( stderr, " (%.0f years)", 
			     time_estimated / TIME_YEAR );
		}
		else if ( time_estimated > TIME_YEAR ) 
		{
		    QPRINTF( stderr, " (%.1f years)", 
			     time_estimated / TIME_YEAR );
		}
		else if ( time_estimated > TIME_MONTH )
		{
		    QPRINTF( stderr, " (%.1f months)", 
			     time_estimated / TIME_MONTH );
		}
		else if ( time_estimated > TIME_DAY * 10 )
		{
		    QPRINTF( stderr, " (%.0f days)", 
			     time_estimated / TIME_DAY );
		}
		else if ( time_estimated > TIME_DAY )
		{
		    QPRINTF( stderr, " (%.1f days)", 
			     time_estimated / TIME_DAY );
		}
		else if ( time_estimated > TIME_HOUR ) 
		{
		    QPRINTF( stderr, " (%.0f hours)", 
			     time_estimated / TIME_HOUR );
		}
		else if ( time_estimated > TIME_MINUTE )
		{
		    QPRINTF( stderr, " (%.0f minutes)", 
			     time_estimated / TIME_MINUTE );
		}
		else if ( time_estimated > 1 )
		{
		    /* already printed seconds */
		}
		else if ( time_estimated > TIME_MILLI_SECOND )
		{
		    QPRINTF( stderr, " (%.0f milli-seconds)", 
			     time_estimated / TIME_MILLI_SECOND );
		}
		else if ( time_estimated > TIME_MICRO_SECOND )
		{
		    QPRINTF( stderr, " (%.0f micro-seconds)", 
			     time_estimated / TIME_MICRO_SECOND );
		}
		else if ( time_estimated > TIME_NANO_SECOND )
		{
		    QPRINTF( stderr, " (%.0f nano-seconds)", 
			     time_estimated / TIME_NANO_SECOND );
		}
		QPUTS( stderr, "\n" );
	    }
	    if ( speed_flag )
	    {
		PPRINTF( stdout, "%.0f", time_estimated ); fflush( stdout );
		exit( EXIT_SUCCESS ); /* don't actually calculate it */
	    }
	}

	timer_start();
	time_width = validity_to_width( validity_period );
	switch ( time_width )
	{
	case HASHCASH_INVALID_VALIDITY_PERIOD:
	    die_msg( "error: negative validity period\n" );
	}
	if ( time_width < 0 ) { die_msg( "error: unknown failure\n" ); }
	err = hashcash_mint( now_time, time_width, resource, bits, 
			     validity_period, anon_period, 
			     token, MAX_TOK, &anon_random, &tries_taken );
	switch ( err )
	{
	case HASHCASH_INVALID_TOK_LEN:
	    die_msg( "error: maximum collision with sha1 is 160 bits\n" );
	case HASHCASH_RNG_FAILED:
	    die_msg( "error: random number generator failed\n" );
	case HASHCASH_INVALID_TIME:
	    die_msg( "error: outside unix time Epoch\n" );
	case HASHCASH_TOO_MANY_TRIES:
	    die_msg( "error: failed to find collision in 2^64 tries!\n" );
	case HASHCASH_EXPIRED_ON_CREATION:
	    die_msg( "error: -a token may expire on creation\n" );
	case HASHCASH_INVALID_VALIDITY_PERIOD:
	    die_msg( "error: negative validity period\n" );
	case HASHCASH_INVALID_TIME_WIDTH:
	case HASHCASH_NULL_ARG:
	    die_msg( "error: internal error\n" );
	case HASHCASH_OK:
	    break;
	default:
	    die_msg( "error: unknown failure\n" );
	}
   
	if ( anon_flag )
	{
	    VPRINTF( stderr, "anon period: %ld seconds\n", (long)anon_period );
	    VPRINTF( stderr, "adding: %ld seconds\n", anon_random );
	}

	VPRINTF( stderr, "tries: %.0f", tries_taken );
	VPRINTF( stderr, " %.2f x expected\n", tries_taken / tries_expected );

	timer_stop();
	taken = timer_time() / 1000000.0;
	VPRINTF( stderr, "time: %.0f seconds\n", taken );
	if ( hdr_flag ) { QPRINTF( stderr, "%s%s\n", header, token ); }
	else { QPRINTF( stderr, "hashcash token: %s\n", token ); }
	if ( hdr_flag ) { PPUTS( stdout, header ); } 
	PPRINTF( stdout, "%s\n", token ); fflush( stdout );
	exit( EXIT_SUCCESS );
    }
    else /* if check_flag -- check token is valid */
    {
	if ( non_opt_argc > 0 ) 
	{
	    sstrncpy( token, argv[optind++], MAX_TOK );
	}
	else
	{
	    if ( hdr_flag )
	    {
		for ( found = 0, boundary = 0, header_len = strlen( header );
		      !found && !boundary && !feof( stdin ) && 
			  fgets( line, MAX_LINE, stdin ); )
		{
		    chomplf( line );
		    found = (strncmp( line, header, header_len ) == 0);
		    if ( !found && !ignore_boundary_flag )
		    {
			if ( line[0] == '\0' ) { boundary = 1; }
		    }
		}
		sstrncpy( token, line+header_len, MAX_TOK );
		if ( !found )
		{
		    QPRINTF( stderr, 
			     "error: no line matching %s found in input\n",
			     header );
		    exit( EXIT_ERROR );
		}
		else
		{ 
		    QPRINTF( stderr, "input text contained:\n%s%s\n", 
			     header, token );
		}
	    }
	    else 
	    {
		if ( in_is_tty )
		{ 
		    fputs ( "input token to check: ", stderr ); 
		}
		fgets( token, MAX_TOK, stdin );
		chomplf( token );
	    }
	    if ( hdr_flag ) { VPRINTF( stderr, "token: %s\n", token ); }
	}

	if ( !hashcash_parse( token, token_utime, MAX_UTC,
			      token_resource, MAX_RES ) ) 
	{ 
	    QPUTS( stderr, "rejected: malformed token\n" );
	    valid_for = HASHCASH_INVALID;
	    goto leave;
	}

	if ( res_flag ) 
	{
	    if ( strcmp( resource, token_resource ) != 0 ) 
	    {
		VPRINTF( stderr, "required resource name: %s\n", 
			 token_resource );
		QPUTS( stderr, "rejected: wrong resource name\n" );
		valid_for = HASHCASH_INVALID;
		goto leave;
	    }
	} 
	
	if ( utc_flag ) { local_time = now_time; zone = "UTC"; }
	else { local_time = utctime_to_local( now_time ); zone = tzname[0]; }
	sstrncpy( date, ctime( &local_time ), MAX_DATE );
	date[strlen(date)-1] = '\0'; /* remove trailing \n */
	VPRINTF( stderr, "current time: %s (%s)\n", date, zone );

	token_time = from_utctimestr( token_utime );
	if ( token_time == -1 ) 
	{ 
	    QPUTS( stderr, "rejected: malformed token\n" );
	    valid_for = HASHCASH_INVALID;
	    goto leave;
	}
	if ( utc_flag ) { local_time = token_time; zone = "UTC"; }
	else { local_time = utctime_to_local( token_time ); zone = tzname[0]; }
	sstrncpy( date, ctime( &local_time ), MAX_DATE );
	date[strlen(date)-1] = '\0'; /* remove trailing \n */
	VPRINTF( stderr, "token created: %s (%s)\n", date, zone );

	valid_for = hashcash_valid_for( token_time, validity_period, 
					now_time );
	switch( valid_for )
	{
	case HASHCASH_VALID_IN_FUTURE:
	    QPUTS( stderr, "rejected: not yet valid\n" );
	    goto leave;
	    break;
	case HASHCASH_EXPIRED:
	    QPUTS( stderr, "rejected: expired\n" );
	    goto leave;
	    break;
	case HASHCASH_VALID_FOREVER:
	default: /* days left */
	    if ( verbose_flag || left_flag )
	    {
		QPUTS( stderr, "valid for: " );
		if ( valid_for > 0 )
		{
		    QPRINTF( stderr, "%ld seconds remaining\n", 
			     (long)valid_for );
		    PPRINTF( stdout, "%ld\n", (long)valid_for ); 
		    fflush( stdout );
		} 
		else
		{
		    QPUTS( stderr, "forever\n" );
		    PPUTS( stdout, "-1\n" ); fflush( stdout );
		}
	    }
	    break;
	}

	collision_bits = hashcash_count( token_resource, token );

	if ( bits_flag && collision_bits < bits )
	{
	    VPRINTF( stderr, "required: %d bits\n", bits );
	    VPRINTF( stderr, "token value: %d bits\n", collision_bits );
	    QPUTS( stderr, "rejected: too few bits\n" );
	    valid_for = HASHCASH_INVALID;
	    goto leave;
	}

	/* we only consider the token checked if we had enough
	 * information that we will have fully verified that
	 * the token is valid
	 */

	if ( yes_flag || ( !name_flag && !width_flag && !left_flag &&
			   res_flag && bits_flag && database_flag ) )
	{
	    checked = 1;
	}

        /* if we're checking the database */
	/* don't add unchecked or invalid tokens to the database */
	if ( database_flag && checked && valid_for >= 0 )
	{
	    /* new file add empty purged time */
	    if ( !file_exists( db_filename, &err ) )
	    {
		if ( err ) { die( err ); }
		if ( !sdb_create( &db, db_filename, &err ) ) { die( err ); }
		if ( !sdb_add( &db, PURGED_KEY, "", &err ) ) { die( err ); }
	    }
	    else		/* existing file, open it */
	    {
		if ( !sdb_open( &db, db_filename, &err ) ) { die( err ); }
	    }
	    in_db = sdb_lookup( &db, token, token_utime, MAX_UTC, &err ); 
	    if ( err ) { die( err ); }
	    if ( in_db )	/* if it's in there, it's a failure */
	    {
		if ( !sdb_close( &db, &err ) ) { die( err ); }
		QPUTS( stderr, "rejected: double spent\n" ); 
		valid_for = HASHCASH_INVALID; 
		goto leave;
	    }
	    else
	    {
		VPUTS( stderr, "database: not double spent\n" );
		sprintf( period, "%ld", (long)validity_period );
		sdb_add( &db, token, period, &err );
	    }
	    if ( !sdb_close( &db, &err ) ) { die( err ); }
	}

	if ( res_flag ) 
	{
	    VPRINTF( stderr, "required resource name: %s\n", token_resource );
	}

	if ( name_flag || verbose_flag )
	{
	    QPRINTF( stderr, "token resource name: %s\n", token_resource );
	    PPRINTF( stdout, "%s\n", token_resource ); fflush( stdout );
	}

	VPRINTF( stderr, "required: %d bits\n", bits );

	if ( width_flag || verbose_flag ) 
	{
	    QPRINTF( stderr, "token value: %d bits\n", collision_bits );
	    PPRINTF( stdout, "%d\n", collision_bits ); fflush( stdout );
	}
	
    leave:
	if ( width_flag || left_flag || name_flag )
	{
	    exit( yes_flag ? EXIT_SUCCESS : EXIT_UNCHECKED );
	}
	QPRINTF( stderr, "token check: %s", 
		 ( valid_for >= 0 ) ? 
		 ( checked ? "ok" : "ok but not fully checked as" ) : 
		 "failed" );
	if ( valid_for >= 0 )
	{
	    if ( !checked )
	    {
		if ( !bits_flag )
		{
		    comma = 1; QPUTS( stderr, " bits" );
		}
		if ( !res_flag )
		{
		    if ( comma ) { QPUTS( stderr, "," ); } 
		    comma = 1; QPUTS( stderr, " resource" );
		}
		if ( !database_flag )
		{
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
    exit( EXIT_ERROR );		/* shouldn't get here */
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
    if ( ! isdigit( last_char ) && ! strchr( "YyMdhmsw", last_char ) )
    {
	return 0;
    }

    sstrncpy( period, aperiod, MAX_PERIOD );

    if ( ! isdigit( last_char ) ) { period[--period_len] = '\0'; }
    if ( period[0] == '+' || period[0] == '-' )
    {
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
    fputs( msg, stderr );
    fprintf( stderr, "mint:\t\thashcash [-m] [opts] -b bits resource\n" );
    fprintf( stderr, "measure speed:\thashcash -s [-b bits]\n" );
    fprintf( stderr, "check:\t\thashcash -c [opts] -d -b bits -r resource [-e period] [token]\n" );
    fprintf( stderr, "purge expired:\thashcash -p now [-k] [-j resource] [-t time] [-u]\n" );
    fprintf( stderr, "count bits:\thashcash -w [opts] [token]\n" );
    fprintf( stderr, "get resource:\thashcash -n [opts] [token]\n" );
    fprintf( stderr, "time remaining:\thashcash -l [opts] -e period [token]\n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "\t-b bits\t\tfind or check partial hash collision of length bits\n" );
    fprintf( stderr, "\t-d\t\tuse database (for double spending detection)\n" );
    fprintf( stderr, "\t-r resource\tresource name for minting or checking token\n" );
    fprintf( stderr, "\t-e period\ttime until token expires\n" );
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
    fprintf( stderr, "\t-i\t\twith -x and -c, check msg body as well\n" );
    fprintf( stderr, "\t-y\t\treturn success if token is valid but not fully checked\n" );
    fprintf( stderr, "examples:\n" );
    fprintf( stderr, "\thashcash -b20 foo                               # mint 20 bit collision\n" );
    fprintf( stderr, "\thashcash -cdb20 -r foo 020313:foo:65b411397aabeaec    # check collision\n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "see man page or http://www.cypherspace.org/hashcash/ for more details.\n" );
    exit( EXIT_ERROR );
}

typedef struct
{
    time_t expires_before;
    char now_utime[ MAX_UTC+1 ];
    int all;
    char* resource;
} db_arg;

/* compile time assert */

#if MAX_UTC > MAX_VAL
#error "MAX_UTC must be less than MAX_VAL"
#endif

static int sdb_cb_token_matcher( const char* key, char* val,
				 void* argp, int* err )
{
    db_arg* arg = (db_arg*)argp;
    char token_utime[ MAX_UTC+1 ];
    char resource[ MAX_RES+1 ];
    time_t expires;
    time_t expiry_period;
    time_t created;

    *err = 0;
    if ( strcmp( key, PURGED_KEY ) == 0 ) 
    {
	sstrncpy( val, arg->now_utime, MAX_UTC ); 
	return 1;		/* update purge time */
    } 

    if ( !hashcash_parse( key, token_utime, MAX_UTC, resource, MAX_RES ) )
    {
	*err = EINPUT;	/* corrupted token in DB */
	return 0;
    }
    if ( arg->resource != NULL ) /* if purging only for given resource */
    {
	if ( strncmp( resource, arg->resource, MAX_RES ) != 0 )
	{
	    return 1;		/* if it doesn't match keep it */
	}
    }
    if ( arg->all ) { return 0; } /* delete all regardless of expiry */
    if ( val[0] == '0' && val[1] == '\0' ) { return 1; } /* keep forever */
    if ( strlen( val ) != strspn( val, "0123456789" ) )
    {
	*err = EINPUT; return 0;
    }
    expiry_period = atoi( val );
    if ( expiry_period < 0 ) { *err = EINPUT; return 0; } /* corrupted */
    created = from_utctimestr( token_utime );
    if ( created < 0 ) { *err = EINPUT; return 0; } /* corrupted */
    expires = created + expiry_period;
    if ( expires <= arg->expires_before ) { return 0; }	/* delete if expired */
    return 1;			/* otherwise keep */
}

int do_purge( DB* db, time_t expires_before, time_t now, int all, 
	      char* purge_resource, int* err )
{
    int ret = 0;
    db_arg arg;

    arg.expires_before = expires_before;
    arg.resource = purge_resource;
    arg.all = all;
    if ( !to_utctimestr( arg.now_utime, MAX_UTC, now ) )
    {
	*err = EINPUT; return 0; 
    }

    VPRINTF( stderr, "purging database: ..." );
    ret = sdb_updateiterate( db, sdb_cb_token_matcher, (void*)&arg, err );
    VPRINTF( stderr, ret ? "done\n" : "failed\n" ); 
    return ret;
}

void die( int err )
{
    const char* str = "";

    switch ( err )
    {
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
    exit( EXIT_ERROR );
}

/* remove unix, DOS and MAC linefeeds from line ending */

void chomplf( char* token )
{
    int tok_len = strlen(token);
    if ( token[tok_len-1] == '\n' ) { token[--tok_len] = '\0'; }
    if ( token[tok_len-1] == '\r' ) { token[--tok_len] = '\0'; }
    if ( token[tok_len-1] == '\n' ) { token[--tok_len] = '\0'; }
}
