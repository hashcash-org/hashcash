/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#if !defined( _hashcash_h )
#define _hashcash_h

#if defined( __cplusplus )
extern "C" {
#endif

#include <time.h>

#if defined(WIN32) && !defined(MONOLITHIC)
    #include <windows.h>
#endif

#if !defined(HCEXPORT)
    #if !defined(WIN32) || defined(MONOLITHIC)
        #define HCEXPORT
    #elif defined(BUILD_DLL)
        #define HCEXPORT __declspec(dllexport)
    #else /* USE_DLL */
        #define HCEXPORT extern __declspec(dllimport)
    #endif
#endif

extern int verbose_flag;
extern int no_purge_flag;

#define HASHCASH_VERSION 1.22
#define HASHCASH_FORMAT_VERSION 1
#define stringify( x ) stringify2( x )
#define stringify2( x ) #x
#define HASHCASH_VERSION_STRING stringify(HASHCASH_VERSION)

#define TYPE_STR 0
#define TYPE_WILD 1
#define TYPE_REGEXP 2

#define MAX_UTC 13
#define MAX_CTR 64
#define MAX_RES 256
#define MAX_STR 256
#define MAX_VER 2
#define MAX_EXT 10240
#define MAX_TOK ((MAX_VER)+1+(MAX_RES)+1+(MAX_EXT)+1+(MAX_UTC)+1+(MAX_STR))

#define TIME_MINUTE 60
#define TIME_HOUR (TIME_MINUTE*60)
#define TIME_DAY (TIME_HOUR*24)
#define TIME_YEAR (TIME_DAY*365)
#define TIME_MONTH (TIME_YEAR/12)
#define TIME_AEON (TIME_YEAR*1000000000.0)
#define TIME_MILLI_SECOND (1.0/1000)
#define TIME_MICRO_SECOND (TIME_MILLI_SECOND/1000)
#define TIME_NANO_SECOND (TIME_MICRO_SECOND/1000)

#define HASHCASH_OK 1
#define HASHCASH_FAIL 0
#define HASHCASH_VALID_FOREVER 0
#define HASHCASH_INVALID_TOK_LEN -1
#define HASHCASH_RNG_FAILED -2
#define HASHCASH_INVALID_TIME -3
#define HASHCASH_TOO_MANY_TRIES -4
#define HASHCASH_EXPIRED_ON_CREATION -5
#define HASHCASH_INVALID_VALIDITY_PERIOD -6
#define HASHCASH_INTERNAL_ERROR -7
#define HASHCASH_INVALID_TIME_WIDTH -8
#define HASHCASH_VALID_IN_FUTURE -9
#define HASHCASH_EXPIRED -10
#define HASHCASH_INVALID -11
#define HASHCASH_WRONG_RESOURCE -12
#define HASHCASH_INSUFFICIENT_BITS -13
#define HASHCASH_UNSUPPORTED_VERSION -14
#define HASHCASH_SPENT -15
#define HASHCASH_NO_TOKEN -16
#define HASHCASH_REGEXP_ERROR -17
#define HASHCASH_OUT_OF_MEMORY -18
#define HASHCASH_USER_ABORT -19

/* return hashcash version number major.minor eg "1.13" */

HCEXPORT
const char* hashcash_version( void );

/* function hashcash_mint calling info:
 *
 * returns HASHCASH_OK on success or one of the error codes above on
 * failure (values < 0)
 * 
 * arguments are:
 *
 * now_time        -- should be time in UTC, use time(2) 
 *
 * time_width      -- how many chars to abbreviate the time to 
 *                    default 0 is 6 chars YYMMDD
 *
 * resource        -- resource name, unique descriptor for resource you're
 *                    trying to protect
 *
 * bits            -- bits of partial preimage the resource demands
 * 
 * anon_period     -- add (or subtract if negative) a random period
 *                    in seconds between this value and 0
 *                    to get same as mixmaster does set this to -3days
 *                    (default 0)
 * 
 * stamp           -- the stamp output
 *
 * anon_random     -- default NULL, if set to pointer to long
 *                    returns actual random offset added to time
 *
 * tries_taken     -- default NULL, if set to pointer to double
 *                    returns number of stamps tested before finding
 *                    returned stamp.
 *
 * ext             -- extension field
 *
 * compress        -- compress the stamp 0 = fast (no compression), 1 = medium
 *                    (some compression), 2 = very compressed (but slow)
 *
 * small           -- optimize for stamp size (true) or stamp generation speed
 *                    (false)
 *
 * cb              -- user defined callback function which is called every 
 *		      1/10th second; if it returns false hashcash_mint
 *                    will return HASHCASH_USER_ABORT; if you do not need
 *		      the callback function, pass NULL
 *
 * user_arg        -- user argument passed to the callback function, if you
 * 		      do not need the callback function or if you do not need
 *		      the arguement pass NULL.
 *
 */

/* note: it is the callers responsibility to call hashcash_free on
 * stamp when finished */

typedef int (*hashcash_callback)(int percent, int largest, int target, 
				 double count, double expected,
				 void* user);

HCEXPORT
int hashcash_mint( time_t now_time, int time_width, const char* resource, 
		   unsigned bits, long anon_period, char** stamp, 
		   long* anon_random, double* tries_taken, char* ext,
		   int compress, hashcash_callback cb, void* user_arg );

/* simpler API for minting  */

HCEXPORT
char* hashcash_simple_mint( const char* resource, unsigned bits, 
			    long anon_period, char* ext, int compress );

/* convert a stamp into a X-Hashcash: header wrapping lines at line_len
 * chars.  If line_len = 0, does not wrap, just prepends header.
 * 
 * arguments are:
 *
 * stamp     -- the stamp to put in the header
 * line_len  -- the number of chars you want to wrap after (or no wrapping
 *              if 0 is given)
 * lf        -- what kind of line feed you want "\r\n" (DOS), "\n" (UNIX), 
 *              "\r" (MAC).  Actually I think RFC822 requires DOS lf.  If
 *              you pass NULL it uses the standard RFC822 lf ("\r\n").
 * 
 */

HCEXPORT
char* hashcash_make_header( const char* stamp, int line_len, 
			    const char* header, char cont, 
			    const char* lf  );

/* return time field width necessary to express time in sufficient
 * resolution to avoid premature expiry of stamp minted
 * for a resource which advertises the given validity period
 *
 */

HCEXPORT
int hashcash_validity_to_width( long validity_period );

/* count bits of preimage in stamp */

HCEXPORT
unsigned hashcash_count( const char* stamp );

/* parse stamp into time field and resouce name 
 * max lengths exclude space for trailing \0 
 *
 * arguments are:
 *
 * stamp          -- stamp to parse
 * vers           -- stamp version (currently 0 or 1)
 * bits           -- (used in vers 1 only; with ver 0 returns -1)
 * utct           -- utc time field
 * utct_max       -- max width of utct field (definition is 13 max)
 * stamp_resource -- resource name from stamp
 * res_max        -- max width of resource name
 * ext            -- extension field
 * ext_max        -- max width of extension field
 */

/* note: if don't care about ext field pass NULL argument */

/* note: if you pass non-NULL in ext, but *ext = NULL, hashcash_parse
 * will allocate the space, in this case it is the callers
 * responsibility to call hashcash_free on ext when finished */

HCEXPORT
int hashcash_parse( const char* stamp, int* vers, int* bits, char* utct, 
		    int utct_max, char* stamp_resource, int res_max, 
		    char** ext, int ext_max );

/* return how many seconds the stamp remains valid for
 * return value HASHCASH_VALID_FOREVER (value 0) means forever
 * return value < 0 is error code
 * codes are: HASHCASH_VALID_IN_FUTURE
 *            HASHCASH_EXPIRED
 *
 * arguments are:
 *
 * stamp_time      -- time field from stamp
 * validity_period -- how long stamps are valid for
 * grace_period    -- how much clock error to tolerate
 * now_time        -- current time (UTC)
 */

HCEXPORT
long hashcash_valid_for( time_t stamp_time, long validity_period, 
			 long grace_period, time_t now_time );

/* simple function calling hashcash_parse, hashcash_count for convenience 
 *
 * on error returns -ve values:
 * 
 * HASHCASH_INVALID
 * HASHCASH_UNSUPPORTED_VERSION,
 * HASHCASH_WRONG_RESOURCE
 * HASHCASH_REGEXP_ERROR
 * HASHCASH_INSUFFICIENT_BITS
 * 
 * on success returns time: left
 *
 * HASHCASH_FOREVER (actually 0)
 * +ve number being remaining validity time in seconds
 *
 * arguments are:
 *
 * stamp           -- stamp to check
 * case_flag       -- if set true case-sensitive (false case insensitive)
 * resource        -- resource expecting (maybe wildcard, regexp...)
 * compile         -- cache for compiled regexp (caller free afterwards)
 * re_err          -- if get HASHCASH_REGEXP_ERROR this contains error 
 * type            -- type of string (TYPE_STR, TYPE_WILD, TYPE_REGEXP)
 * now_time        -- current time (UTC)
 * validity_period -- how long stamps are valid for
 * grace_period    -- how much clock error to tolerate
 * required_bits   -- how many bits the stamp must have
 * stamp_time      -- returns: parsed and decoded time field from stamp
 */

/* note: compile argument can be NULL in which case regexps
 * compilations are not cached (and compile arg does not have to be
 * hashcash_free'd) 
 */

/* note: it is the callers responsibility to call hashcash_free on
 * compile when finished if regexps are used */

HCEXPORT
int hashcash_check( const char* stamp, int case_flag, const char* resource, 
		    void **compile, char** re_err, int type, time_t now_time, 
		    long validity_period, long grace_period, 
		    int required_bits, time_t* stamp_time );

/* return how many tries per second the machine can do */

HCEXPORT
unsigned long hashcash_per_sec( void );

/* estimate how many seconds it would take to mint a stamp of given size */

HCEXPORT
double hashcash_estimate_time( int b );

/* expected number of tries to mint stamp of size b */
/* note: equivalent to pow( 2, b ) but without using libm */

HCEXPORT
double hashcash_expected_tries( int b );

/* hashcash_resource_match
 *
 * For comparing received stamps to addresses the user is willing to
 * receive email as.
 *
 * returns 0 and *err == NULL if no match
 * returns 0 and *err != NULL if regexp error, regexp explanatory error msg
 *                            held in *err
 * returns 1 on match
 *
 * arguments are:
 *
 * type            -- type of comparison: TYPE_STR (simple string), 
 *                    TYPE_WILD (wildcard '*'), TYPE_REGEXP
 *                    (full regular expression)
 *
 * stamp_res       -- resource from the stamp being compared
 *
 * res             -- resource we're comparing against
 *
 * compile         -- cache for regexp compilation result
 *                    (hashcash_free it when you've finished)
 * 
 * err             -- NULL on success, regexp error string if regexp fails
 */

HCEXPORT
int hashcash_resource_match( int type, const char* stamp_res, const char* res, 
			     void** compile, char** err );

/* free memory which may beallocated by:
 *
 *                hashcash_check
 *                hashcash_mint
 *                hashcash_parse
 *                hashcash_simple_mint
 *                hashcash_resource_match
 */

HCEXPORT
void hashcash_free( void* ptr);


/* bit of redundancy to avoid needing to distribute multiple header
 * files with the DLL (this is an excerpt from utct.h)
 */

#define MAX_UTC 13

/* convert utct string into a time_t
 *
 * returns (time_t)-1 on error
 * 
 * arguments are:
 *
 * utct        -- utct string to convert
 *
 * utc         -- if true use UTC, else express in local time zone
 */

HCEXPORT
time_t hashcash_from_utctimestr( const char utct[MAX_UTC+1], int utc );

/* convert time_t into utct string (note utct is always in utc time)
 *
 * returns 1 on success, 0 on failure
 *
 * arguments are:
 *
 * utct        -- return utct converted string
 *
 * len         -- how many digits of utc to use; valid utc widths are:
 *                6,10,12 (though the code can generate 2,4,6,8,10,12
 */

HCEXPORT
int hashcash_to_utctimestr( char utct[MAX_UTC+1], int len, time_t t );

/* returns max rate of hashcash / sec using a slower but more accurate
 * benchmarking method
 *
 * arguments are:
 *
 * verbose     -- verbosity level 0 = no output, 1 = some, 2 = more, 3 = most
 * core        -- core number CORE_ALL (= -1) = all cores, or specific
 */

/* returns speed of hashcash */

HCEXPORT
unsigned long hashcash_benchtest( int verbose, int core );

/* returns current core */

HCEXPORT
int hashcash_core(void);

/* use specified core */

HCEXPORT
int hashcash_use_core(int);

/* give name of specified core */

HCEXPORT
const char* hashcash_core_name(int);


#if defined( __cplusplus )
}
#endif

#endif
