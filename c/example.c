/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sstring.h"
#include "sdb.h"
#include "hashcash.h"
#include "getopt.h"

/* simplified hashcash.c example showing how to use the library
 * functions.  This code can only handle one operation per invocation
 */

void usage( const char* msg );
void die( int err );
void die_msg( const char* str );
int parse_period( const char* aperiod, long* resp );
int progress_callback(int percent, int largest, int target, 
		      double counter, double expected, void* user);

#define EOK 0			/* no error */
#define EINPUT -1		/* error invalid input */

#define MAX_PERIOD 11		/* max time_t = 10 plus 's' */
#define MAX_HDR 256
#define EXIT_ERROR 3

int main( int argc, char* argv[] ) {
    int err = 0;
    int opt, bits=20, str_type = TYPE_WILD, ignore_boundary_flag;
    int case_flag = 0, check_flag = 0, db_flag = 0, core, res = HASHCASH_FAIL;
    int mint_flag = 0, purge_flag = 0, core_flag = 0;
    int time_width = 6, compress = 0, checked = 0;
    long anon_random = 0, anon_period = 0, purge_period = 0, time_period = 0;
    time_t real_time = time(0); /* now, in UTCTIME */
    time_t now_time = real_time, token_time;
    int speed_flag = 0, utc_flag = 0, version_flag = 0, hdr_flag = 0;
    char* header = NULL;
    char* db_filename = "hashcash.sdb";
    char* purge_resource = NULL;
    char* resource = NULL;
    char* ext = NULL;
    int purge_all = 0;
    long validity_period = 28*TIME_DAY; /* default validity period: 28 days */
    long purge_validity_period = 28*TIME_DAY; /* same default */
    long grace_period = 2*TIME_DAY; /* default grace period: 2 days */
    long valid_for = HASHCASH_INVALID;
    char *new_token = NULL ;
    char token[MAX_TOK+1], token_utime[MAX_UTC+1], period[MAX_PERIOD+1];
    double tries_taken = 0;
    void** compile = NULL;
    char* re_err = NULL;
    DB db;
    hashcash_callback callback = NULL;

    while ( (opt=getopt( argc, argv, 
	 "-a:b:cde:f:g:hij:klmnop:qr:sSt:uvwx:yz:CEMO:PSVXZ:")) >0 ) {
	switch( opt ) {
	case 'a': 
	    if ( !parse_period( optarg, &anon_period ) ) {
		usage( "error: -a invalid period arg" );
	    }
	    break;
	case 'b': bits = atoi( optarg+1 );
	    if ( bits < 0 ) { usage( "error: -b invalid bits arg" ); }
	    break;
	case 'C': case_flag = 1; break;
	case 'c': check_flag = 1; break;
	case 'd': db_flag = 1; break;
	case 'e': 
	    if ( !parse_period( optarg, &validity_period ) ||
		 validity_period < 0 ) {
		usage( "error: -e invalid validity period" ); 
	    }
	    break;
	case 'E': str_type = TYPE_REGEXP; break;
	case 'f': db_filename = strdup( optarg ); break;
	case 'g': 
	    if ( optarg[0] == '-' ) {
		usage( "error: -g -ve grace period not valid" );
	    }
	    if ( !parse_period( optarg, &grace_period ) ) {
		usage( "error: -g invalid grace period format" );
	    }
	    break;
	case 'h': usage( "" ); break;
	case 'i': ignore_boundary_flag = 1; break;
	case 'j': purge_resource = optarg; break;
	case 'k': purge_all = 1; break;
	case 'm': mint_flag = 1; break;
	case 'M': str_type = TYPE_WILD; break;
	case 'O': core_flag = 1; 
	    core = atoi( optarg ); 
	    res = hashcash_use_core( core );
	    if ( res < 1 ) {
		usage( res == -1 ? "error: -O no such core\n" : 
		       "error: -O core does not work on this platform" );
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
	case 'P': callback = progress_callback; break;
	case 1:
	case 'r': resource = optarg; break;
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
        case 'V': version_flag = 1; break;
	case 'x': ext = strdup( optarg ); break;
	case 'X':
	    hdr_flag = 1;
	    sstrncpy( header, "X-Hashcash: ", MAX_HDR );
	    break;
	case 'z': 
	    time_width = atoi( optarg );
	    if ( time_width != 6 && time_width != 10 && time_width != 12 ) {
	        usage( "error: -z invalid time width: must be 6, 10 or 12" );
	    }
	    break;
	case 'Z': compress = atoi( optarg ); break;
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

    if ( version_flag ) {
	fprintf( stdout, "%s\n", HASHCASH_VERSION_STRING );
	exit( EXIT_FAILURE );
    }

    if ( speed_flag ) {
	if ( core_flag ) { 
	    hashcash_benchtest( 3, core );
	} else {
	    hashcash_benchtest( 3, -1 );
	}
	exit( EXIT_FAILURE ); 
    }

    if ( mint_flag ) {
	err = hashcash_mint( now_time, time_width, resource, bits, 
			     anon_period, &new_token, &anon_random, 
			     &tries_taken, ext, compress, 
			     callback, NULL );
    } else if ( purge_flag ) {
	if ( !hashcash_db_open( &db, db_filename, &err ) ) {
	    die(err);
	}
	if ( !hashcash_db_purge( &db, purge_resource, str_type, case_flag,
				 validity_period, grace_period, purge_all,
				 purge_period, now_time, &err ) ) {
	    die(err);
	}
	if ( !hashcash_db_close( &db, &err ) ) {
	    die(err);
	}
    } else if ( check_flag ) {
	valid_for = hashcash_check( token, case_flag, resource, compile, 
				    &re_err, str_type, now_time, 
				    validity_period, grace_period, bits, 
				    &token_time );
	if ( valid_for < 0 ) {
	    switch ( valid_for ) {
	    case HASHCASH_INSUFFICIENT_BITS:
		fprintf( stderr, "no match: token has insufficient bits\n" );
		break;
	    case HASHCASH_VALID_IN_FUTURE:
		fprintf( stderr, "no match: valid in future\n" );
		break;
	    case HASHCASH_EXPIRED:
		fprintf( stderr, "no match: token expired\n" );
		break;
	    case HASHCASH_WRONG_RESOURCE:
		fprintf( stderr, "no match: wrong resource\n" );
		break;
	    case HASHCASH_REGEXP_ERROR:
		fprintf( stderr, "regexp error: " );
		die_msg( re_err );
		break;
		
	    default:
		die_msg( "internal error" );
		break;
	    }
	    exit( EXIT_FAILURE );
	}
	checked = 1;
    } 

    if ( db_flag && check_flag && checked ) {
	if ( !hashcash_db_open( &db, db_filename, &err ) ) {
	    die(err);
	}
	if ( hashcash_db_in( &db, token, token_utime, &err ) ) {
	    if ( err ) { die(err); }
	    fprintf( stderr, "stamp: double spent\n" );
	}
	sprintf( period, "%ld", (long)validity_period );
	if ( !hashcash_db_add( &db, token, period, &err ) ) {
	    die(err);
	}
	if ( !hashcash_db_close( &db, &err ) ) {
	    die(err);
	}
    }

    exit( EXIT_SUCCESS );
}

int progress_callback(int percent, int largest, int target, 
		      double counter, double expected, void* user)
{
    static int previous_percent = -1;
    static int previous_largest = -1;
    static double previous_counter = -1;

    if ( previous_counter != counter || 
	 previous_percent != percent || previous_largest != largest ) {
	fprintf( stderr, "percent: %.0lf/%.0lf = %d%% [%d/%d bits]\r", 
		 counter, expected, percent, largest, target );
	previous_percent = percent;
	previous_largest = largest;
	previous_counter = counter;
    }
    return 1;
}

void usage( const char* msg )
{
    if ( msg ) { fputs( msg, stderr ); fputs( "\n\n", stderr ); }
    fprintf( stderr, "mint:\t\thashcash [-m] [opts] resource\n" );
    fprintf( stderr, "measure speed:\thashcash -s [-b bits]\n" );
    fprintf( stderr, "check:\t\thashcash -c [opts] -d -r resource [-e period] [token]\n" );
    fprintf( stderr, "purge expired:\thashcash -p now [-k] [-j resource] [-t time] [-u]\n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "\t-b bits\t\tfind or check partial preimage of length bits\n" );
    fprintf( stderr, "\t-d\t\tuse database (for double spending detection)\n" );
    fprintf( stderr, "\t-r resource\tresource name for minting or checking token\n" );
    fprintf( stderr, "\t-e period\ttime until token expires\n" );
    fprintf( stderr, "\t-g\t\tgrace period for clock skew\n" );
    fprintf( stderr, "\t-t time\t\tmodify current time token created at\n" );
    fprintf( stderr, "\t-a time\t\tmodify time by random amount in range given\n" );
    fprintf( stderr, "\t-u\t\tgive time in UTC instead of local time\n" );
    fprintf( stderr, "\t-h\t\tprint this usage info\n" );
    fprintf( stderr, "\t-f dbfile\tuse filename dbfile for database\n" );
    fprintf( stderr, "\t-j resource\twith -p delete just tokens matching the given resource\n" );
    fprintf( stderr, "\t-k\t\twith -p delete all not just expired\n" );
    fprintf( stderr, "\t-x ext\t\tput in extension field\n" );
    fprintf( stderr, "\t-X\t\toutput with header format 'X-Hashcash: '\n" );
    fprintf( stderr, "\t-i\t\twith -X and -c, check msg body as well\n" );
    fprintf( stderr, "\t-z width\twidth of time field 6,10 or 12 chars (default 6)\n" );
    fprintf( stderr, "\t-C\t\tmatch resources as case sensitive (default insensitive)\n" );
    fprintf( stderr, "\t-S\t\tmatch following resources as text strings\n" );
    fprintf( stderr, "\t-W\t\tmatch following resources with wildcards (default)\n" );
    fprintf( stderr, "\t-E\t\tmatch following resources as regular expression\n" );
    fprintf( stderr, "\t-P\t\tshow progress while searching\n");
    fprintf( stderr, "\t-O core\t\tuse specified minting core\n");
    fprintf( stderr, "examples:\n" );
    fprintf( stderr, "\thashcash -mb20 foo                               # mint 20 bit preimage\n" );
    fprintf( stderr, "\thashcash -cdb20 -r foo 1:20:040806:foo::831d0c6f22eb81ff:15eae4 # check preimage\n" );
    fprintf( stderr, "\n" );
    fprintf( stderr, "see hashcash (1) man page or http://www.hashcash.org/ for more details.\n" );
    exit( EXIT_ERROR );
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
    fprintf( stderr, "error: %s\n", str );
    exit( EXIT_ERROR );
}

void die_msg( const char* str ) 
{
    fprintf( stderr, str );
    fprintf( stderr, "\n" );
    exit( EXIT_ERROR );
}
