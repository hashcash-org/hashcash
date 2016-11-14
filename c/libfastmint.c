#if defined(__POWERPC__) && defined(__ALTIVEC__) && (!defined(__GNUC__) || !defined(__MACH__))
#include <altivec.h>
#endif

#ifdef __CARBON__
#include <CoreServices/CoreServices.h>
#endif

#if defined(__POWERPC__) && !defined(__MACH__) && !defined(__UNIX__) && !defined(__CARBON__)
#include <Gestalt.h>
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

#include "libfastmint.h"
#include "random.h"
#include "sha1.h"

#if defined( WIN32 )
#include "mydll.h"
#else
#define EXPORT
#endif

/* Index into array of available minters */
static int fastest_minter = -1;
static unsigned int num_minters = 0;
static HC_Minter *minters = NULL;

const char *encodeAlphabets[] = {
	"0123456789ABCDEF",
	"0123456789abcdef",
	"ABCDEFGHIJKLMNOP",
	"abcdefghijklmnop",
	"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/"
};

/* Keep track of what the CPU supports */
static volatile int gIllegalInstructionTrapped = 0;
static jmp_buf gEnv;
int gProcessorSupportFlags = 0;

/* SHA-1 magic gunge */
#define H0 0x67452301
#define H1 0xEFCDAB89
#define H2 0x98BADCFE
#define H3 0x10325476
#define H4 0xC3D2E1F0
static const uInt32 SHA1_IV[ 5 ] = { H0, H1, H2, H3, H4 };

/* SIGILL handler */
static void sig_ill_handler(int sig)
{
	sig = sig;
	gIllegalInstructionTrapped = 1;
	longjmp(gEnv,1);
}

#if defined(__ALTIVEC__) && !defined(__GNUC__)
#pragma dont_inline on
/* Dummy Altivec operation for testing */
static void dummy_altivec_fn(void)
{
	volatile vector unsigned int v;
	v = vec_or(v,v);
}
#pragma dont_inline reset
#endif

/* Detect whether extended CPU features like Altivec, MMX, etc are supported */
static void hashcash_detect_features(void)
{
#if defined(__POWERPC__) && defined(__ALTIVEC__)
	int hasAltivec = 0;
	
	#if defined(__UNIX__) || defined(__MACH__)
			/* Use generic SIGILL trap handler */
			void *oldhandler;
			
			gIllegalInstructionTrapped = 0;
			oldhandler = signal(SIGILL, sig_ill_handler);
			
			if(!setjmp(gEnv)) {
				#ifdef __GNUC__
				asm volatile ( "vor v0,v0,v0" );
				#else
				dummy_altivec_fn();
				#endif
			}
			
			signal(SIGILL, oldhandler);
			
			hasAltivec = !gIllegalInstructionTrapped;
	#else
			/* Carbon and MacOS Classic */
			long cpuAttributes;
			OSErr err = Gestalt(gestaltPowerPCProcessorFeatures, &cpuAttributes);
			if(err == 0)
				hasAltivec = ((1 << gestaltPowerPCHasVectorInstructions) & cpuAttributes) != 0;
	#endif
	
	if(hasAltivec)
		gProcessorSupportFlags |= HC_CPU_SUPPORTS_ALTIVEC;
	else
		gProcessorSupportFlags &= ~(HC_CPU_SUPPORTS_ALTIVEC);
#elif defined(__i386__) && defined(__GNUC__)
	void *oldhandler;
	int hasMMX = 0;
	
	gIllegalInstructionTrapped = 0;
	oldhandler = signal(SIGILL, sig_ill_handler);
	
	if(!setjmp(gEnv)) {
		asm volatile (
									"movl $1, %%eax\n\t"
									"push %%ebx\n\t"
									"cpuid\n\t"
									"andl $0x800000, %%edx\n\t"
									"pop %%ebx\n\t"
									: "=d" (hasMMX)
									: /* no input */
									: "eax", "ecx"
									);
	}
	
	signal(SIGILL, oldhandler);
	
	if(hasMMX && !gIllegalInstructionTrapped)
		gProcessorSupportFlags |= HC_CPU_SUPPORTS_MMX;
	else
		gProcessorSupportFlags &= ~(HC_CPU_SUPPORTS_MMX);
#elif defined(__AMD64__)
	gProcessorSupportFlags = HC_CPU_SUPPORTS_MMX;
#else
	gProcessorSupportFlags = 0;
#endif
}

/* Statically guesstimate the fastest hashcash minting routine.  Takes
 * into account only the gross hardware architecture and features
 * available.  Updates fastest_minter.  Also initialises and populates
 * the available minter array if necessary.
 */

void hashcash_select_minter()
{
	static const HC_Mint_Routine funcs[] = {
		minter_ansi_compact_1,
		minter_ansi_standard_1,
		minter_ansi_ultracompact_1,
		minter_ansi_compact_2,
		minter_ansi_standard_2,
		minter_altivec_standard_1,
		minter_altivec_compact_2,
		minter_altivec_standard_2,
		minter_mmx_compact_1,
		minter_mmx_standard_1,
		NULL };
	static const HC_Mint_Capable_Routine tests[] = {
		minter_ansi_compact_1_test,
		minter_ansi_standard_1_test,
		minter_ansi_ultracompact_1_test,
		minter_ansi_compact_2_test,
		minter_ansi_standard_2_test,
		minter_altivec_standard_1_test,
		minter_altivec_compact_2_test,
		minter_altivec_standard_2_test,
		minter_mmx_compact_1_test,
		minter_mmx_standard_1_test,
		NULL };
	static const char *names[] = {
		"ANSI Compact 1-pipe",
		"ANSI Standard 1-pipe",
		"ANSI Ultra-Compact 1-pipe",
		"ANSI Compact 2-pipe",
		"ANSI Standard 2-pipe",
		"PowerPC Altivec Standard 1x4-pipe",
		"PowerPC Altivec Compact 2x4-pipe",
		"PowerPC Altivec Standard 2x4-pipe",
		"AMD64/x86 MMX Compact 1x2-pipe",
		"AMD64/x86 MMX Standard 1x2-pipe",
		NULL };
	static const EncodeAlphabet encodings[] = {
		EncodeBase64,
		EncodeBase64,
		EncodeBase64,
		EncodeBase64,
		EncodeBase64,
		EncodeBase64,
		EncodeBase64,
		EncodeBase64,
		EncodeBase64,
		EncodeBase64 };
	int i = 0 ;
	
	/* Populate array */
	if(!num_minters || !minters) {
		if(minters)
			free(minters);
		
		num_minters = (sizeof(funcs) / sizeof(*funcs)) - 1;
		minters = malloc(sizeof(HC_Minter) * num_minters);
		for(i=0; i < num_minters; i++) {
			minters[i].name = names[i];
			minters[i].func = funcs[i];
			minters[i].test = tests[i];
			minters[i].encoding = encodings[i];
		}
	}
	
	/* If nothing else works, just use the compact_1 minter on x86
	   and standard_1 elsewhere */

	#ifdef __i386__
	fastest_minter = 0;
	#elif defined(__M68000__)
	fastest_minter = 2;
	#else
	fastest_minter = 1;
	#endif
	
	/* Detect actual CPU capabilities */
	hashcash_detect_features();
	
	/* See if any of the vectorised minters work, choose the
	   highest-numbered one that does */

	for(i=5; i < num_minters; i++) {
		if(minters[i].test()) {	fastest_minter = i; }
	}
}

/* Do a quick, silent benchmark of the selected backend.  Assumes it
 * works. */
unsigned long hashcash_per_sec_calc(void)
{
	static const unsigned int test_bits = 64;
	static const char *test_string = 
		"1:32:040404:foo@fnord.gov::0123456789abcdef:00000000";
	static const int test_tail = 52;
	unsigned long rate = 0, iter_count = 256;
	volatile clock_t begin = 0 , end = 0 , tmp = 0 , res = 0 , taken = 0 ;
	double elapsed = 0 , multiple = 0 ;
	char block[SHA1_INPUT_BYTES] = {0};
	
	/* Ensure a valid minter backend is selected */
	if(!num_minters || !minters)
		hashcash_select_minter();
	
	/* Determine clock resolution */
	end = clock();
	while((begin = clock()) == end)
		;
	while((end = clock()) == begin)
		;
	if ( end < begin ) { tmp = begin; begin = end; end = tmp; }
	res = end - begin;
	
        /* where there is poor resolution use this */
	/* less accurate but faster -- otherwise takes 0.5secs */

	if ( res > 1000 ) {
		/* Run minter, with clock running */
		//		end = clock();
	        //		while((begin = clock()) == end) {}
		begin = end;
		do {
			/* set up SHA-1 block */
			strncpy(block, test_string, SHA1_INPUT_BYTES);
			block[test_tail] = 0x80;
			memset(block+test_tail+1, 0, 59-test_tail);
			PUT_WORD(block+60, test_tail << 3);

			if(minters[fastest_minter].func(test_bits, block, 
							SHA1_IV, test_tail, 
							iter_count) 
			   >= test_bits) {
				/* The benchmark will be inaccurate 
				   if we actually find a collision! */
				fprintf(stderr, 
"Error in hashcash_quickbench(): found collision while trying to benchmark!\n");
				return 1;
			}
			rate += iter_count;
			end = clock();
			if ( end < begin ) { taken = begin-end; }
			else { taken = end-begin; }
		} while ( taken < 8*res );
		multiple = CLOCKS_PER_SEC / (double)(8*res);
		rate *= multiple;
		
		return rate;
	}

	/* Run increasing lengths of minting until we have sufficient
	 * elapsed time for precision */
	while(iter_count) {
		/* set up SHA-1 block */
		strncpy(block, test_string, SHA1_INPUT_BYTES);
		block[test_tail] = 0x80;
		memset(block+test_tail+1, 0, 59-test_tail);
		PUT_WORD(block+60, test_tail << 3);
		
		/* Run minter, with clock running */
		end = clock();
		while((begin = clock()) == end) {}
		if(minters[fastest_minter].func(test_bits, block, SHA1_IV, 
						test_tail, iter_count) >= 
		   test_bits) {
			/* The benchmark will be inaccurate if we
			 * actually find a collision! */
			fprintf(stderr, "Error in hashcash_quickbench(): found collision while trying to benchmark!\n");
			return 1;
		}
		end = clock();
		if ( end < begin ) { tmp = begin; begin = end; end = tmp; }
		elapsed = (end-begin) / (double) CLOCKS_PER_SEC;
		
		if(end-begin > (res * 16))
			break;
		
		iter_count <<= 1;
	}
	
	rate = iter_count / elapsed;
	return rate;
}

EXPORT
unsigned long hashcash_per_sec(void)
{
        static int calculated = 0;
        static unsigned long per_sec = 0;
        if ( !calculated ) {
                per_sec = hashcash_per_sec_calc();
                calculated = 1;
        }
        return per_sec;
}

/* Test and benchmark available hashcash minting backends.  Returns
 * the speed of the fastest valid routine, and updates fastest_minter
 * as appropriate.
 */
EXPORT
unsigned long hashcash_benchtest(int verbose)
{
	unsigned long i, a, b, got_bits;
	int best_minter = -1;
	static const unsigned int test_bits = 22;
	static const char *test_string = 
		"1:22:040404:foo@bar.net::0123456789abcdef:0000000000";
	static const int test_tail = 52;  /* must be less than 56 */
	static const int bit_stats[] = { 8, 10, 16, 20, 22, 
					 24, 26, 28, 30, 0 };
	char block[SHA1_INPUT_BYTES] = {0};
	volatile clock_t begin = 0 , end = 0 , tmp = 0 ;
	double elapsed = 0 , rate = 0 , peak_rate = 0;
	SHA1_ctx crypter;
	unsigned char hash[SHA1_DIGEST_BYTES] = {0};
	const char *p = NULL , *q = NULL ;
	
	/* If minter list isn't valid, make it so */
	hashcash_select_minter();
	
	/* print header */
	if(verbose > 0 ) {
	        printf("    Rate  Name (* machine default)\n");
	}
	if(verbose >= 3) { printf("\n"); }
	
	for(i=0; i < num_minters; i++) {
		/* If the minter can't run... */
		if(!minters[i].test()) {
			if(verbose >= 2) {
				printf("   ---    %s  (Not available on this machine)\n", minters[i].name);
			}
			continue;
		}
		
		if(verbose) {
			printf("          %s\r", minters[i].name);
			fflush(stdout);
		}
		
		/* set up SHA-1 block */
		strncpy(block, test_string, SHA1_INPUT_BYTES);
		block[test_tail] = 0x80;
		memset(block+test_tail+1, 0, 59-test_tail);
		PUT_WORD(block+60, test_tail << 3);
		
		/* Run minter, with clock running */
		end = clock();
		while((begin = clock()) == end) {}
		got_bits = minters[i].func(test_bits, block, SHA1_IV, 
					   test_tail, 1 << 30);
		end = clock();
		if ( end < begin ) { tmp = begin; begin = end; end = tmp; }
		elapsed = (end-begin) / (double) CLOCKS_PER_SEC;
		
		/* Different minter iteration patterns will find
		 * different solutions */
		/* Verify solution correctness first, using reference
		 * SHA-1 library */

		SHA1_Init(&crypter);
		SHA1_Update(&crypter, block, test_tail);
		SHA1_Final(&crypter, hash);
		for(a=0; a < SHA1_DIGEST_BYTES-1 && hash[a] == 0; a++) {}
		for(b=0; b < 8 && (hash[a] & 0x80) == 0; b++) {
			hash[a] <<= 1;
		}
		if(got_bits != (a*8)+b || got_bits < test_bits || 
		   block[test_tail] != (char) 0x80) {
			if(verbose) {
				printf("ERROR!\n");
				printf("    Wanted %d bits, reported %lu bits, got %lu bits.\n", test_bits, got_bits, (a*8)+b);
				if(block[test_tail] == (char) 0x80) {
					printf("    End-of-block marker remains intact.\n");
				} else {
					printf("    End-of-block marker damaged!\n");
				}
				block[test_tail] = 0;
				printf("    \"%s\"\n", block);
				printf("    Time taken: %.3f\n\n", elapsed);
			}
			continue;
		}
		
		/* Use knowledge of encoding alphabet to calculate
		   iterations taken */
		a = test_tail-8;
		b = 0;
		p = encodeAlphabets[minters[i].encoding];
		while(a < test_tail && block[a] == '0') {
			a++;
		}
		for( ; a < test_tail; a++) {
			q = strchr(p, block[a]);
			if(!q)
				break;
			b = (b * strlen(p)) + (q - p);
		}
		if(a != test_tail) {
			if(verbose) {
				printf("ERROR!\n");
				printf("    Unable to parse iteration count.\n");
				printf("    \"%s\"\n", block);
				printf("    \"%s\"\n", p);
			}
			continue;
		}
		
		/* We know the elapsed time and the iteration count,
		   so calculate the rate */
		rate = b / elapsed;
		if(verbose) {
			printf("%9lu %s %c\n", (unsigned long) rate, 
			       minters[i].name, 
			       (i == fastest_minter) ? '*' : ' ');
		}

		if(rate > peak_rate) {
			peak_rate = rate;
			best_minter = i;
		}
		
		/* Optionally print out the stats */
		if(verbose >= 3) {
			block[test_tail] = 0;
			printf("    Solution:   %s\n", block);
			printf("    Iterations: %lu\n", b);
			printf("    Time taken: %.3f\n\n", elapsed);
		}
	}
	
	fastest_minter = best_minter;
	
	if(verbose && best_minter >= 0) {
		printf("Best minter: %s (%lu hashes/sec)\n", 
		       minters[best_minter].name, (unsigned long) peak_rate);
	}
	if(verbose >= 2 && best_minter >= 0) {
		printf("Projected average times to mint:\n");
		
		for(i = 0; bit_stats[i]; i++) {
			elapsed = (1 << bit_stats[i]) / peak_rate;
			printf("%3d bits: %9.3f seconds", bit_stats[i], 
			       elapsed);
			if(elapsed > 200000) {
				printf(" (%.1f days)", elapsed/(3600*24));
			} else if(elapsed > 5000) {
				printf(" (%.1f hours)", elapsed/3600);
			} else if(elapsed > 100) {
				printf(" (%.1f minutes)", elapsed/60);
			} else if(elapsed < 0.005) {
				printf(" (%.1f microseconds)", elapsed * 
				       1000000);
			}
			printf("\n");
		}
	}
	
	return (unsigned long) peak_rate;
}

/* Attempt to mint a hashcash token with a given bit-value.
 * Will append a random string to token that produces the required
 * collision, then return a pointer to the resultant string in result.
 * Caller must free() result buffer after use.
 * Returns the number of bits actually minted (may be more or less
 * than requested).
 */

unsigned int hashcash_fastmint(const int bits, const char *token, 
			       char **result)
{
	SHA1_ctx crypter;
	unsigned char hash[SHA1_DIGEST_BYTES] = {0};
	unsigned int IV[SHA1_DIGEST_WORDS] = {0};
	char *buffer = NULL, *block = NULL, c = 0;
	unsigned int buflen = 0, tail = 0, gotbits = 0, a = 0, b = 0;
	unsigned long t = 0;
	
	/* Make sure list of minters is valid */
	if(!minters || fastest_minter < 0)
		hashcash_select_minter();
	
 again:

	/* Set up string for hashing */
	tail = strlen(token);
	buflen = (tail - (tail % SHA1_INPUT_BYTES)) + 2*SHA1_INPUT_BYTES;
	buffer = malloc(buflen);
	memset(buffer, 0, buflen);
	strncpy(buffer, token, buflen);
	
	/* Add 96 bits of random data */
	t = tail + 16;
	for( ; tail < t; tail++) {
		random_getbytes(&c, 1);
		buffer[tail] = encodeAlphabets[EncodeBase64][c & 0x3f];
	}
	
	/* Add separator and zeroed count field (for v1 hashcash format) */
	buffer[tail++] = ':';
	t = tail + 8;
	for( ; tail < t; tail++)
		buffer[tail] = '0';
	for( ; (tail % SHA1_INPUT_BYTES) != 32 && (tail % SHA1_INPUT_BYTES) != 52; tail++)
		buffer[tail] = '0';
	
	/* Hash all but the final block, due to invariance */
	t = tail - (tail % SHA1_INPUT_BYTES);
	SHA1_Init(&crypter);
	SHA1_Update(&crypter, buffer, t);
	for(a=0; a < 5; a++)
		IV[a] = crypter.H[a];
	block = buffer + t;
	
	/* Fill in the padding and trailer */
	buffer[tail] = 0x80;
	PUT_WORD(block+60, tail << 3);
	tail -= t;
	
	/* Run the minter over the last block */
	if(bits) {
		gotbits = minters[fastest_minter].func(bits, block, IV, tail, 0xFFFFFFF0U);
	}
	block[tail] = 0;
	
	/* Verify solution using reference library */
	SHA1_Update(&crypter, block, tail);
	SHA1_Final(&crypter, hash);
	for(a=0; a < SHA1_DIGEST_BYTES-1 && hash[a] == 0; a++)
		;
	for(b=0; b < 8 && (hash[a] & 0x80) == 0; b++)
		hash[a] <<= 1;
	b += a*8;
	
	/* The minter appears to be broken! */
	if(b < gotbits) {
		fprintf(stderr, "ERROR: requested %d bits, reported %d bits, got %d bits using %s minter: \"%s\"\n",
			bits, gotbits, b, minters[fastest_minter].name, 
			buffer);
		exit(3);
	}
	
	/* The minter might not be able to detect unusually large
	 * (32+) bit counts, so we're allowed to give it another try.
	 */
	if(b < bits) {
		fprintf( stderr, "buffer = %s\n", buffer );
		fprintf( stderr, "wrapped\n" );
		free(buffer);
		goto again;
	}
	
	*result = buffer;
	return b;
}

