typedef unsigned int uInt32;
typedef int (* HC_Mint_Routine)(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
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
 * Will append a random string to token that produces the required collision,
 * then return a pointer to the resultant string in result.  Caller must free()
 * result buffer after use.
 * Returns the number of bits actually minted (may be more or less than requested).
 */
extern unsigned int hashcash_fastmint(const int bits, const char *token, char **result);

/* Perform a quick benchmark of the selected minting backend.  Returns speed. */
extern unsigned long hashcash_per_sec(void);

/* Test and benchmark available hashcash minting backends.  Returns the speed
 * of the fastest valid routine, and updates fastest_minter as appropriate.
 * Uses one or more known solutions to gauge both speed and accuracy.
 * Optionally displays status and benchmark results on console.
 */
extern unsigned long hashcash_benchtest(int verbose);

/* Minting backend routines.
 * These are in two parts:
 * - HC_Mint_Routine is the actual backend.  Attempts to produce at least N-bit
 *     collision by incrementing the tail section of the block.  Block need not
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
extern int minter_ansi_compact_1(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_ansi_compact_1_test(void);

/* Standard ANSI-C 1-pipe "Method B" implementation - for use on register-rich architectures.
 */
extern int minter_ansi_standard_1(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_ansi_standard_1_test(void);

/* Standard ANSI-C 1-pipe "ultra-compact" implementation - for use on architectures with
 * severely limited L1 caches (eg. M68K).
 */
extern int minter_ansi_ultracompact_1(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_ansi_ultracompact_1_test(void);

/* Standard ANSI-C 2-pipe "compact" implementation - for use on register-rich architectures.
 */
extern int minter_ansi_compact_2(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_ansi_compact_2_test(void);

/* Standard ANSI-C 2-pipe "Method B" implementation - for use on register-rich architectures.
 */
extern int minter_ansi_standard_2(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_ansi_standard_2_test(void);

/* PowerPC Altivec 1x4-pipe "Method B" implementation - for use on G4 and higher systems.
 */
extern int minter_altivec_standard_1(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_altivec_standard_1_test(void);

/* PowerPC Altivec 2x4-pipe "Method B" implementation - for use on G4 and higher systems.  Hand-optimised.
 */
extern int minter_altivec_standard_2(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_altivec_standard_2_test(void);

/* PowerPC Altivec 2x4-pipe "compact" implementation - for use on G4 and higher systems.  Hand-optimised.
 */
extern int minter_altivec_compact_2(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_altivec_compact_2_test(void);

/* AMD64/x86 MMX 1x2-pipe "Method B" implementation - for use on Pentium-MMX/2/3/4, K6-2/3,
 * Athlon, Athlon64, Opteron, and compatible systems.  Hand-optimised.
 */
extern int minter_mmx_standard_1(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_mmx_standard_1_test(void);

/* AMD64/x86 MMX 1x2-pipe "compact" implementation - for use on Pentium-MMX/2/3/4, K6-2/3,
 * Athlon, Athlon64, Opteron, and compatible systems.  Hand-optimised.
 */
extern int minter_mmx_compact_1(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter);
extern int minter_mmx_compact_1_test(void);

