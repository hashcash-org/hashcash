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
#else
    #include <unistd.h>
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
#define MAX_RES 256
#define MAX_HDR 256
#define MAX_CTR 64
#define MAX_TOK ((MAX_RES)+1+(MAX_UTCTIME)+1+(MAX_CTR))
#define MAX_LINE 1024

#define TIME_MINUTE 60
#define TIME_HOUR (TIME_MINUTE*60)
#define TIME_DAY (TIME_HOUR*24)
#define TIME_YEAR (TIME_DAY*365)
#define TIME_MONTH (TIME_YEAR/12)
#define TIME_AEON (TIME_YEAR*1000000000.0)
#define TIME_MILLI_SECOND (1.0/1000)
#define TIME_MICRO_SECOND (TIME_MILLI_SECOND/1000)
#define TIME_NANO_SECOND (TIME_MICRO_SECOND/1000)

#define VALID_FOREVER ((time_t)0)
#define VALID_IN_FUTURE ((time_t)-1)
#define VALID_EXPIRED ((time_t)-2)
#define VALID_NOT ((time_t)-3)

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

#define sstrncpy(d,s,l) (d[l]='\0',strncpy(d,s,l))

void die( int err );
void die_msg( const char* );
void usage( const char* );
word32 find_collision( char utct[MAX_UTCTIME+1], char*, int, char*, 
		       word32, char* );
int count_collision_bits( char*, char* );
double estimate_time( int );
double expected_tries( int );
time_t still_valid_for( time_t, time_t, time_t );
int parse_token( const char*, char*, char* );
int parse_period( const char* aperiod, time_t* resp );
word32 hashes_per_sec( void );

#define GROUP_SIZE 0xFFFFFFFFU
#define GROUP_DIGITS 8
#define GFORMAT "%08x"

#if 0
/* smaller base for debugging */
#define GROUP_SIZE 255
#define GROUP_DIGITS 2
#define GFORMAT "%02x"
#endif

int quiet_flag;
int verbose_flag;
int in_is_tty;

time_t round_off( time_t now_time, int digits );
int do_purge( DB* db, time_t expires_before, time_t now, int all, 
	      char* purge_resource, int* err );

#define PURGED_KEY "last_purged"

int main( int argc, char* argv[] )
{
    word32 i0;
    word32 ran0, ran1;
    const char* db_filename = "hashcash.db";
    int db_exists = 0;
    int found;
    int err;
    int non_opt_argc = 0;
    int anon_flag = 0;
    time_t anon_period;
    long anon_random;
    int bits = 0;
    int collision_bits;
    int check_flag = 0;
    int hdr_flag = 0;
    int left_flag = 0;
    int time_width = 10;
    int z_flag = 0;		/* default no trailing Z on UTCTIME */
    int speed_flag = 0;
    int utc_flag = 0;
    int bits_flag = 0;
    int validity_flag = 0;
    int database_flag = 0;
    int yes_flag = 0;
    int width_flag = 0;
    int purge_flag = 0;
    int mint_flag = 0;
    int name_flag = 0;
    int res_flag = 0;
    int checked = 0;
    int comma = 0;
    int in_db;
    char header[ MAX_HDR+1 ];
    int header_len;
    char token[ MAX_TOK+1 ];
    int tok_len;
    char line[ MAX_LINE+1 ];
    char token_resource[ MAX_RES+1 ];
    char resource[ MAX_RES+1 ];
    char* zone = "UTC";
    char date[ MAX_DATE+1 ];

    time_t purge_period = 0;
    char purge_utime[ MAX_UTCTIME+1 ];	/* time token created */
    int purge_all = 0;
    int just_flag = 0;
    char purge_resource[ MAX_RES+1 ];
    time_t last_time;
    time_t valid_for = 0;

    time_t validity_period = 0;	/* default validity period: forever */
    char period[ MAX_PERIOD+1 ];

    char token_utime[ MAX_UTCTIME+1 ];	/* time token created */
    time_t token_time;
    time_t local_time;
    char now_utime[ MAX_UTCTIME+1 ]; /* current time */
    time_t real_time = utctime(0); /* now, in UTCTIME */
    time_t now_time = real_time;
    time_t time_period;

    char counter[ MAX_CTR+1 ];
    char* counter_ptr;
    char iformat[ 10 ];		/* holds "%100s" where MAX_RES = 256 */
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
			    "a:b:cde:f:hj:klmnp:qr:st:uvwx:yz" ) ) > 0 )
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
	case 'z': z_flag = 1; break;
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

    if ( now_time < 0 )
    {
	die_msg( "error: outside unix time Epoch\n" );
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
			    MAX_UTCTIME, &err ) ) )
	{
	    goto dbleave;	/* should always be there */
	}

	last_time = from_utctimestr( purge_utime );
	if ( last_time < 0 ) /* not first time, but corrupted */
	{ 
	    purge_period = 0; /* purge now */
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
		    sprintf( iformat, "%%%ds", MAX_TOK );
		    scanf( iformat, token ); token[MAX_TOK] = '\0';
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

	    if ( parse_token( token, token_utime, token_resource ) ) 
	    {
		usage( "looks like token, use -c option to check tokens\n\n" );
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
	    QPRINTF( stderr, "speed: %u collision tests per second\n",
		     hashes_per_sec() );
	    if ( speed_flag && !bits_flag )
	    {
		PPRINTF( stdout, "%u\n", hashes_per_sec() ); fflush( stdout );
	    }
	}

	if ( bits_flag || verbose_flag || speed_flag )
	{
	    time_estimated = estimate_time( bits );
	    tries_expected = expected_tries( bits );
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

	if ( !random_getbytes( &ran0, sizeof( word32 ) ) ||
	     !random_getbytes( &ran1, sizeof( word32 ) ) )
	{
	    die_msg( "error: random number generator failed\n" );
	}

	timer_start();
   
	counter_ptr = counter;
	found = 0;

	if ( anon_flag )
	{
	    if ( validity_flag && anon_period > validity_period )
	    {
		die_msg( "error: -a token may expire on creation\n" );
	    }
	    if ( !random_rectangular( (long)anon_period, &anon_random ) )
	    {
		die_msg( "error: random number generator failed\n\n" );
	    }
	    VPRINTF( stderr, "anon period: %ld seconds\n", (long)anon_period );
	    VPRINTF( stderr, "adding: %ld seconds\n", anon_random );
	    now_time += anon_random;
	}

	if ( validity_flag )
	{  /* YYMMDDhhmmss or YYMMDDhhmm or YYMMDDhh or YYMMDD or YYMM or YY */
	    if ( validity_period < 2*TIME_MINUTE ) { time_width = 12; } 
	    else if ( validity_period < 2*TIME_HOUR ) {	time_width = 10; }
	    else if ( validity_period < 2*TIME_DAY ) { time_width = 8; }
	    else if ( validity_period < 2*TIME_MONTH ) { time_width = 6; }
	    else if ( validity_period < 2*TIME_YEAR ) {	time_width = 4; }
	    else { time_width = 2; }
	}
	else
	{
	    time_width = 6;	/* default to YYMMDD */
	}

	now_time = round_off( now_time, 12-time_width );
	if ( z_flag )		/* generate standard UTC  */
	{ 
	    if ( time_width < 10 ) { time_width = 10; }
	    time_width++; 
	}

	to_utctimestr( now_utime, time_width, now_time );

	for ( i0 = 0; !found; )
	{
	    sprintf( counter_ptr, GFORMAT GFORMAT, ( i0 + ran0 ) & GROUP_SIZE,
		     ran1 & GROUP_SIZE );
	    found = find_collision( now_utime, resource, bits, token,
				    GROUP_SIZE, counter_ptr );
	    if ( !found )
	    {
		i0 = ( i0 + 1 ) & GROUP_SIZE;
		if ( i0 == 0 )	/* ie if wrapped */
		{ 
		    QPRINTF( stderr, 
			     "error: failed to find collision in 2^64 tries!\n" );
		    exit( EXIT_ERROR );
		    /* shouldn't get here without trying  */
		    /* for a very long time */
		}
	    }
	}

	counter_ptr = counter;
   
	tries_taken = ( (double) i0 ) * ULONG_MAX + ( (double) found );

	VPRINTF( stderr, "tries: %.0f", tries_taken );
	VPRINTF( stderr, " %.2f x expected\n", tries_taken / tries_expected );

	timer_stop();
	taken = timer_time() / 1000000.0;
	VPRINTF( stderr, "time: %.0f seconds\n", taken );
	if ( hdr_flag ) { QPRINTF( stderr, "%s%s\n", header, token ); }
	else { QPRINTF( stderr, "hashcash token: %s\n", token ); }
	if ( hdr_flag ) { PPUTS( stdout, header ); } 
	PPRINTF( stdout, "%s\n", token ); fflush( stdout );
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
		if ( in_is_tty ) 
		{ 
		    fprintf( stderr, "input text contained header %s: ", 
			     header );
		}
		for ( found = 0, header_len = strlen( header );
		      !found && !feof( stdin ) && 
			  fgets( line, MAX_LINE, stdin ); )
		{
		    found = (strncmp( line, header, header_len ) == 0);
		}
		if ( !found )
		{
		    QPRINTF( stderr, 
			     "error: no line matching %s found in input\n",
			     header );
		    exit( EXIT_ERROR );
		}
		sstrncpy( token, line+header_len, MAX_TOK );
	    }
	    else 
	    {
		if ( in_is_tty )
		{ 
		    fputs ( "input token to check: ", stderr ); 
		}
		fgets( token, MAX_TOK, stdin );
	    }
	    tok_len = strlen(token);
	    if ( token[tok_len-1] == '\n' ) { token[tok_len-1] = '\0'; }
	    if ( hdr_flag ) { VPRINTF( stderr, "token: %s\n", token ); }
	}

	if ( !parse_token( token, token_utime, token_resource ) ) 
	{ 
	    QPUTS( stderr, "rejected: malformed token\n" );
	    valid_for = VALID_NOT;
	    goto leave;
	}

	if ( res_flag ) 
	{
	    if ( strncmp( resource, token_resource, MAX_RES ) != 0 ) 
	    {
		VPRINTF( stderr, "required resource name: %s\n", 
			 token_resource );
		QPUTS( stderr, "rejected: wrong resource name\n" );
		valid_for = VALID_NOT;
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
	    valid_for = VALID_NOT;
	    goto leave;
	}
	if ( utc_flag ) { local_time = token_time; zone = "UTC"; }
	else { local_time = utctime_to_local( token_time ); zone = tzname[0]; }
	sstrncpy( date, ctime( &local_time ), MAX_DATE );
	date[strlen(date)-1] = '\0'; /* remove trailing \n */
	VPRINTF( stderr, "token created: %s (%s)\n", date, zone );

	valid_for = still_valid_for( token_time, validity_period, now_time );
	switch( valid_for )
	{
	case VALID_IN_FUTURE:
	    QPUTS( stderr, "rejected: not yet valid\n" );
	    goto leave;
	    break;
	case VALID_EXPIRED:
	    QPUTS( stderr, "rejected: expired\n" );
	    goto leave;
	    break;
	case VALID_FOREVER:
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

	collision_bits = count_collision_bits( token_resource, token );

	if ( bits_flag && collision_bits < bits )
	{
	    VPRINTF( stderr, "required: %d bits\n", bits );
	    VPRINTF( stderr, "token value: %d bits\n", collision_bits );
	    QPUTS( stderr, "rejected: too few bits\n" );
	    valid_for = VALID_NOT;
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
	if ( database_flag && checked &&
	     ( valid_for == VALID_FOREVER || valid_for > 0 ) )
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
	    in_db = sdb_lookup( &db, token, token_utime, MAX_UTCTIME, &err ); 
	    if ( err ) { die( err ); }
	    if ( in_db )	/* if it's in there, it's a failure */
	    {
		if ( !sdb_close( &db, &err ) ) { die( err ); }
		QPUTS( stderr, "rejected: double spent\n" ); 
		valid_for = VALID_NOT; 
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
		 ( valid_for > 0 || valid_for == VALID_FOREVER ) ? 
		 ( checked ? "ok" : "ok but not fully checked as" ) : 
		 "failed" );
	if ( valid_for > 0 || valid_for == VALID_FOREVER )
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
	if ( valid_for == VALID_IN_FUTURE ||
	     valid_for == VALID_EXPIRED ||
	     valid_for == VALID_NOT )
	{
	    exit( EXIT_FAILURE ); 
	}
	exit( checked ? EXIT_SUCCESS : 
	      ( yes_flag ? EXIT_SUCCESS : EXIT_UNCHECKED ) );
    }
    exit( EXIT_ERROR );		/* shouldn't get here */
    return 0;
}

int parse_token( const char* token, char* utct, char* token_resource ) 
{
    char* first_colon;
    char* second_colon;
    int utct_len;
    int res_len;

    first_colon = strchr( token, ':' );

    if ( first_colon == 0 ) { return 0; }
    utct_len = (int)(first_colon - token);
    /* parse out the resource name component */
    /* format:   utctime[Z]:resource:counter  */
    second_colon = strchr( first_colon+1, ':' );
    res_len = (int)(second_colon-1 - first_colon);
    if ( second_colon == 0 || utct_len > MAX_UTCTIME || res_len > MAX_RES ) 
    {
	return 0;
    }
    memcpy( utct, token, utct_len ); utct[utct_len] = '\0';
    sstrncpy( token_resource, first_colon+1, res_len );

    return 1;
}

int parse_period( const char* aperiod, time_t* resp )
{
    int period_len;
    char last_char;
    time_t res = 1;
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

double expected_tries( int b )
{
    double expected_tests = 1;
    #define CHUNK ( sizeof( unsigned long ) - 1 )
    for ( ; b > CHUNK; b -= CHUNK )
    {
	expected_tests *= ((unsigned long)1) << CHUNK;
    }
    expected_tests *= ((unsigned long)1) << b;
    return expected_tests;
}

double estimate_time( int b )
{
    return expected_tries( b ) / (double)hashes_per_sec();
}

word32 hashes_per_sec( void )
{
    timer t1, t2;
    double elapsed;
    unsigned long n_collisions = 0;
    char token[ MAX_TOK+1 ];
    char counter[ MAX_CTR+1 ];
    word32 step = 100;

    /* wait for start of tick */

    timer_read( &t2 );
    do 
    {
	timer_read( &t1 );
    }
    while ( timer_usecs( &t1 ) == timer_usecs( &t2 ) &&
	    timer_secs( &t1 ) == timer_secs( &t2 ) );

    /* do computations for next tick */

    do
    {
	n_collisions += step;
	find_collision( "000101", "flame", 25, token, step, counter );
	timer_read( &t2 );
    }
    while ( timer_usecs( &t1 ) == timer_usecs( &t2 ) &&
	    timer_secs( &t1 ) == timer_secs( &t2 ) );

/* see how many us the tick took */
    elapsed = timer_interval( &t1, &t2 );
    return (word32) ( 1000000.0 / elapsed * (double)n_collisions
		      + 0.499999999 );
}

time_t still_valid_for( time_t token_time, 
			time_t validity_period, time_t now_time )
{
    int expiry_time;

    /* for ever -- return infinity */
    if ( validity_period == 0 )	{ return VALID_FOREVER; }

    /* future date in token */
    if ( token_time > now_time ) { return VALID_IN_FUTURE; }; 

    expiry_time = token_time + validity_period;
    if ( expiry_time > now_time )
    {				/* valid return seconds left */
	return expiry_time - now_time;
    }
    return VALID_EXPIRED;	/* otherwise expired */
}

int count_collision_bits( char* resource, char* token )
{
    char target[ MAX_TOK+1 ];	/* bigger than it needs to be */
    SHA1_ctx ctx;
    byte target_digest[ 20 ];
    byte token_digest[ 20 ];
    char* first_colon;
    int utct_len;
    int i;
    int last;
    int collision_bits;

    first_colon = strchr( token, ':' );
    if ( first_colon == NULL ) { return 0; } /* should really fail */
    utct_len = first_colon - token;
    memcpy( target, token, utct_len+1 ); target[utct_len+1] = '\0';
    strncat( target, resource, MAX_RES );

    SHA1_Init( &ctx );
    SHA1_Update( &ctx, target, strlen( target ) );
    SHA1_Final( &ctx, target_digest );
      
    SHA1_Init( &ctx );
    SHA1_Update( &ctx, token, strlen( token ) );
    SHA1_Final( &ctx, token_digest );
   
    for ( i = 0; i < 20 && token_digest[ i ] == target_digest[ i ]; i++ )
    {
    }

    last = i;
    collision_bits = 8 * i;

#define bit( n, c ) (((c) >> (7 - (n))) & 1)

    for ( i = 0; i < 8; i++ )
    {
	if ( bit( i, token_digest[ last ] ) == 
	     bit( i, target_digest[ last ] ) )
	{
	    collision_bits++;
	}
	else
	{
	    break;
	}
    }
    return collision_bits;
}

word32 find_collision( char utct[ MAX_UTCTIME+1 ], char* resource, int bits, 
		       char* token, word32 tries, char* counter )
{
    char* hex = "0123456789abcdef";
    char target[ MAX_TOK+1 ];
    char ctry[ MAX_TOK+1 ];
    char* changing_part_of_try;
    SHA1_ctx ctx;
    SHA1_ctx precomputed_ctx;
    word32 i;
    int j;
    word32 trial;
    word32 tries2;
    int counter_len;
    int try_len;
    int try_strlen;
    byte target_digest[ 20 ];
    byte try_digest[ 20 ];
    int partial_byte = bits & 7;
    int check_bytes;
    int partial_byte_index = 0;	/* suppress dumb warning */
    int partial_byte_mask = 0xFF; /* suppress dumb warning */
    char last_char;
   
    sstrncpy( target, utct, MAX_TOK );
    strcat( target, ":" );
    strncat( target, resource, MAX_RES );

    counter_len = (int)(strlen( counter ) - GROUP_DIGITS);
    sscanf( counter + counter_len, "%08x", &trial );
    trial -= trial % 16;		/* lop off last digit */

    SHA1_Init( &ctx );
    SHA1_Update( &ctx, target, strlen( target ) );
    SHA1_Final( &ctx, target_digest );

    if ( partial_byte )
    {
	partial_byte_index = bits / 8;
	partial_byte_mask = ~ (( 1 << (8 - (bits & 7))) -1 );
	check_bytes = partial_byte_index + 1;
	target_digest[ partial_byte_index ] &= partial_byte_mask;
    }
    else
    {
	check_bytes = bits / 8;
    }

    sstrncpy( ctry, target, MAX_TOK ); 
    strcat( ctry, ":" );
    strncat( ctry, counter, counter_len );
    try_len = (int)strlen( ctry );

/* length of try is fixed, GFORMAT is %08x, so move strlen outside loop */

    changing_part_of_try = ctry + try_len;
    sprintf( changing_part_of_try, GFORMAT, 0 );
    try_strlen = (int)strlen( ctry );

/* part of the ctx context can be precomputed as not all of the
   message is changing
*/

    tries2 = (int) ( (double) tries / 16.0 + 0.5 );
    for ( i = 0; i < tries2; i++, trial = (trial + 16) & GROUP_SIZE )
    {
/* move precompute closer to the inner loop to precompute more */

	SHA1_Init( &precomputed_ctx );

	sprintf( changing_part_of_try, GFORMAT, trial );

	SHA1_Update( &precomputed_ctx, ctry, try_strlen - 1 );

	for ( j = 0; j < 16; j++ )
	{
#if defined( DEBUG )
	    fprintf( stderr, "try: %s\n", ctry );
#endif
	    memcpy( &ctx, &precomputed_ctx, sizeof( SHA1_ctx ) );
	    last_char = hex[ j ];
	    SHA1_Update( &ctx, &last_char, 1 );
	    SHA1_Final( &ctx, try_digest );

	    if ( bits > 7 )
	    {
		if ( try_digest[ 0 ] != target_digest[ 0 ] )
		{
		    continue;
		}
	    }
	    if ( partial_byte )
	    {
		try_digest[ partial_byte_index ] &= partial_byte_mask;
	    }
	    if ( memcmp( target_digest, try_digest, check_bytes ) == 0 )
	    {
		changing_part_of_try[ GROUP_DIGITS - 1 ] = hex[ j ];
		sstrncpy( token, ctry, MAX_TOK );
		return i * 16 + j + 1;
	    }
	}
    }
    return 0;
}

time_t round_off( time_t now_time, int digits )
{
    struct tm* now;

    if ( digits != 2 && digits != 4 && digits != 6 && 
	 digits != 8 && digits != 10 )
    {
	return now_time;
    }
    now_time = utctime_to_local( now_time ); /* to counter act gmtime */
    now = gmtime( &now_time );	/* converts local -> utct as side effect */

    switch ( digits )
    {
    case 10: now->tm_mon = 0;
    case 8: now->tm_mday = 1;
    case 6: now->tm_hour = 0;
    case 4: now->tm_min = 0;
    case 2: now->tm_sec = 0;
    }
    return mktime( now );
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
    fprintf( stderr, "\t-z\t\tproduce standard UTCTIME format\n" );
    fprintf( stderr, "\t-q\t\tquiet -- suppress all informational output\n" );
    fprintf( stderr, "\t-v\t\tprint verbose informational output\n" );
    fprintf( stderr, "\t-h\t\tprint this usage info\n" );
    fprintf( stderr, "\t-f dbfile\tuse filename dbfile for database\n" );
    fprintf( stderr, "\t-j resource\twith -p delete just tokens matching the given resource\n" );
    fprintf( stderr, "\t-k\t\twith -p delete all not just expired\n" );
    fprintf( stderr, "\t-x string\tprepend token output to string (or input match string)\n" );
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
    char now_utime[ MAX_UTCTIME+1 ];
    int all;
    char* resource;
} db_arg;

/* compile time assert */

#if MAX_UTCTIME > MAX_VAL
#error "MAX_UTCTIME must be less than MAX_VAL"
#endif

static int sdb_cb_token_matcher( const char* key, char* val,
				 void* argp, int* err )
{
    db_arg* arg = (db_arg*)argp;
    char token_utime[ MAX_UTCTIME+1 ];
    char resource[ MAX_RES+1 ];
    time_t expires;
    time_t expiry_period;
    time_t created;

    *err = 0;
    if ( strcmp( key, PURGED_KEY ) == 0 ) 
    {
	sstrncpy( val, arg->now_utime, MAX_UTCTIME ); 
	return 1;		/* update purge time */
    } 

    if ( !parse_token( key, token_utime, resource ) )
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
    if ( !to_utctimestr( arg.now_utime, MAX_UTCTIME, now ) )
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
