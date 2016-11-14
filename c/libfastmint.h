#if !defined( _libfastmint_h )
#define _libfastmint_h

#if defined(WIN32)
#include <windows.h>
#undef small
#else
#include <sys/time.h>
#endif
#include "hashcash.h"

#if defined(WIN32)
#define MILLISEC 1
#define TIMETYPE DWORD
#define TIME0 0
#define timer(x) (*(x)=GetTickCount())
#else
#define MILLISEC 1000
#define TIME0 {0,0}
#define TIMETYPE struct timeval
#define timer(x) gettimeofday(x,NULL)
#endif

#define MINTER_CALLBACK_ARGS hashcash_callback cb, void* user_args,	\
	double counter, double expected

#define MINTER_CALLBACK_VARS double percent;	\
	TIMETYPE prev=TIME0, curr;		\
        int lastBits = *best

/* in minter if have to do something for acces to floating point
 * operations override this */

/* eg mmx minter sets to this: __builtin_ia32_emms() */

#define MINTER_CALLBACK_CLEANUP_FP /**/


#if defined(WIN32)
#define small_time_diff_gt( c, p, intv ) \
	((c>p?c-p:(c+(((TIMETYPE)-1)-p)))>(intv))
#else
/* works for intervals < 1 second */
#define small_time_diff_gt( c, p, intv )				\
        (c.tv_sec-p.tv_sec>1 ||						\
	(c.tv_usec+((c.tv_sec - p.tv_sec)?1000000:0))-p.tv_usec>(intv))
#endif

#define MINTER_CALLBACK()						     \
	do {								     \
		if (cb != NULL && (iters & 0xFFFF) == 0) {		     \
			timer(&curr);                                        \
			if (gotBits > lastBits ||                            \
			    small_time_diff_gt(curr,prev,100*MILLISEC)) {    \
                                MINTER_CALLBACK_CLEANUP_FP;		     \
				percent = (int)(((counter+iters)/	     \
					expected*100)+0.5);		     \
				if (!cb(percent,*best,bits,                  \
					counter+iters,expected,user_args)) { \
					*best = -1;                          \
					return 0;			     \
				}					     \
				prev = curr;                                 \
				lastBits = gotBits;			     \
			}						     \
		}							     \
	} while (0)

typedef unsigned int uInt32;
typedef unsigned long (* HC_Mint_Routine)(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
typedef int (* HC_Mint_Capable_Routine)(void);

typedef enum {
	EncodeHexUpper,     /* 0123456789ABCDEF */
	EncodeHexLower,     /* 0123456789abcdef */
	EncodeAlpha16Upper, /* ABCDEFGHIJKLMNOP */
	EncodeAlpha16Lower, /* abcdefghijklmnop */
	EncodeBase64        /* 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/ */
} EncodeAlphabet;


typedef struct {
	const char *name;
	EncodeAlphabet encoding;
	HC_Mint_Routine func;
	HC_Mint_Capable_Routine test;
} HC_Minter;

#define HC_CPU_SUPPORTS_ALTIVEC 0x01
#define HC_CPU_SUPPORTS_MMX 0x02
extern int gProcessorSupportFlags;

extern const char *encodeAlphabets[];
extern const int EncodeBitRate[];

extern void hashcash_select_minter();

/* Portably write a word into a byte array */
#define PUT_WORD(_dst, _src) { \
		*((unsigned char*)(_dst)+0) = ((_src) >> 24) & 0xFF; \
		*((unsigned char*)(_dst)+1) = ((_src) >> 16) & 0xFF; \
		*((unsigned char*)(_dst)+2) = ((_src) >>  8) & 0xFF; \
		*((unsigned char*)(_dst)+3) = ((_src)      ) & 0xFF; \
	}

/* Portably load bytes into a word */
#define GET_WORD(_src) ( \
		(*((unsigned char*)(_src)+0) << 24) | \
		(*((unsigned char*)(_src)+1) << 16) | \
		(*((unsigned char*)(_src)+2) <<  8) | \
		(*((unsigned char*)(_src)+3)      ) )

/* Attempt to mint a hashcash token with a given bit-value.
 * Will append a random string to token that produces the required preimage,
 * then return a pointer to the resultant string in result.  Caller must free()
 * result buffer after use.
 * Returns the number of bits actually minted (may be more or less than requested).
 */
extern double hashcash_fastmint(const int bits, const char *token, int small, char **result, hashcash_callback cb, void* user_arg);

/* Perform a quick benchmark of the selected minting backend.  Returns speed. */
extern unsigned long hashcash_per_sec(void);

/* Test and benchmark available hashcash minting backends.  Returns the speed
 * of the fastest valid routine, and updates fastest_minter as appropriate.
 * Uses one or more known solutions to gauge both speed and accuracy.
 * Optionally displays status and benchmark results on console.
 */
extern unsigned long hashcash_benchtest(int verbose, int core);

/* Minting backend routines.
 * These are in two parts:
 * - HC_Mint_Routine is the actual backend.  Attempts to produce at least N-bit
 *     preimage by incrementing the tail section of the block.  Block need not
 *     be initial block, as custom IV is provided, but must be terminal block.
 *     Tail index points just after last character - ie. to beginning of SHA-1
 *     padding - do not overwrite padding.  At least 8 characters will be present.
 *     Will bail out after maxIter attempts, within some reasonable tolerance.
 * - HC_Mint_Capable_Routine is a trivial test to see if the current machine
 *     supports necessary hardware features for the backend.  Returns boolean.
 */

/* Standard ANSI-C 1-pipe "compact" implementation - for use on x86 and other
 * register-limited architectures.
 */
extern unsigned long minter_ansi_compact_1(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
extern int minter_ansi_compact_1_test(void);

/* Standard ANSI-C 1-pipe "Method B" implementation - for use on register-rich architectures.
 */
extern unsigned long minter_ansi_standard_1(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
extern int minter_ansi_standard_1_test(void);

/* Standard ANSI-C 1-pipe "ultra-compact" implementation - for use on architectures with
 * severely limited L1 caches (eg. M68K).
 */
extern unsigned long minter_ansi_ultracompact_1(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
extern int minter_ansi_ultracompact_1_test(void);

/* Standard ANSI-C 2-pipe "compact" implementation - for use on register-rich architectures.
 */
extern unsigned long minter_ansi_compact_2(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
extern int minter_ansi_compact_2_test(void);

/* Standard ANSI-C 2-pipe "Method B" implementation - for use on register-rich architectures.
 */
extern unsigned long minter_ansi_standard_2(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
extern int minter_ansi_standard_2_test(void);

/* PowerPC Altivec 1x4-pipe "Method B" implementation - for use on G4 and higher systems.
 */
extern unsigned long minter_altivec_standard_1(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
extern int minter_altivec_standard_1_test(void);

/* PowerPC Altivec 2x4-pipe "Method B" implementation - for use on G4 and higher systems.  Hand-optimised.
 */
extern unsigned long minter_altivec_standard_2(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
extern int minter_altivec_standard_2_test(void);

/* PowerPC Altivec 2x4-pipe "compact" implementation - for use on G4 and higher systems.  Hand-optimised.
 */
extern unsigned long minter_altivec_compact_2(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
extern int minter_altivec_compact_2_test(void);

/* AMD64/x86 MMX 1x2-pipe "Method B" implementation - for use on Pentium-MMX/2/3/4, K6-2/3,
 * Athlon, Athlon64, Opteron, and compatible systems.  Hand-optimised.
 */
extern unsigned long minter_mmx_standard_1(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);

extern int minter_mmx_standard_1_test(void);

/* AMD64/x86 MMX 1x2-pipe "compact" implementation - for use on Pentium-MMX/2/3/4, K6-2/3,
 * Athlon, Athlon64, Opteron, and compatible systems.  Hand-optimised.
 */
extern unsigned long minter_mmx_compact_1(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);
extern int minter_mmx_compact_1_test(void);

/* use SHA1 library (integrated or openSSL depending on how compiled) */

extern int minter_library_test(void);

extern unsigned long minter_library(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS);


#endif
