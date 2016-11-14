#include <setjmp.h>
#include "libfastmint.h"

#if (defined(__i386__) || defined(__AMD64__)) && defined(__GNUC__)
typedef int mmx_d_t __attribute__ ((mode(V2SI)));
typedef int mmx_q_t __attribute__ ((mode(DI)));
#endif

#if (defined(__i386__) || defined(__AMD64__)) && defined(__GNUC__)
#include <signal.h>

#if defined(_WIN32)
#define sigjmp_buf jmp_buf
#define siglongjmp(x,y) longjmp((x),(y))
#define sigsetjmp(x,y) setjmp(x)
#define sighandler_t __p_sig_fn_t
#endif

static volatile int gIsMMXPresent = -1;
static sigjmp_buf gEnv;

static void sig_ill_handler(int sig)
{
	gIsMMXPresent = 0;
	siglongjmp(gEnv,0);
}
#endif

int minter_mmx_compact_1_test(void)
{
  /* This minter runs only on x86 and AMD64 hardware supporting MMX - and will only compile on GCC */
#if (defined(__i386__) || defined(__AMD64__)) && defined(__GNUC__)
#if defined(_WIN32)
        sighandler_t oldhandler;
#else
	sigset_t signame;
	struct sigaction sa_new, sa_old;
#endif	
        int mmx_available;

	if(gIsMMXPresent == -1) {
		gIsMMXPresent = 1;
#if defined(_WIN32)
		oldhandler = signal(SIGILL,sig_ill_handler);
#else
		sigemptyset(&signame);
		sigaddset(&signame, SIGILL);
		
		/* Install illegal-instruction handler in case CPUID isn't supported */
		sa_new.sa_handler = sig_ill_handler;
		sa_new.sa_mask = signame;
		sa_new.sa_flags = 0;
		sigaction(SIGILL, &sa_new, &sa_old);
#endif
		
		if(!sigsetjmp(gEnv, 0)) {
		  asm volatile (
										"movl $1, %%eax\n\t"
										"cpuid\n\t"
										"andl $0x800000, %%edx\n\t"
										: "=d" (mmx_available)
										: /* no input */
										: "eax", "ebx", "ecx"
										);
		}
		
#if defined(_WIN32)
		signal(SIGILL, oldhandler);
#else
		sigaction(SIGILL, &sa_old, &sa_new);
#endif
	} else {
		mmx_available = gIsMMXPresent;
	}
	
  return !!mmx_available;
#endif
  
  /* Not an x86 or AMD64, or compiler doesn't support MMX */
  return 0;
}

/* Define low-level primitives in terms of operations */
/* #define S(n, X) ( ( (X) << (n) ) | ( (X) >> ( 32 - (n) ) ) ) */
#define XOR(a,b) ( (mmx_d_t) __builtin_ia32_pxor( (mmx_q_t) a, (mmx_q_t) b) )
#define AND(a,b) ( (mmx_d_t) __builtin_ia32_pand( (mmx_q_t) a, (mmx_q_t) b) )
#define ANDNOT(a,b) ( (mmx_d_t) __builtin_ia32_pandn( (mmx_q_t) b, (mmx_q_t) a) )
#define OR(a,b) ( (mmx_d_t) __builtin_ia32_por( (mmx_q_t) a, (mmx_q_t) b) )
#define ADD(a,b) ( __builtin_ia32_paddd(a,b) )

#if (defined(__i386__) || defined(__AMD64__)) && defined(__GNUC__)
static inline mmx_d_t S(int n, mmx_d_t X)
{
  mmx_d_t G;

  asm ("movq %[x],%[g]\n\t"
       "pslld %[sl],%[x]\n\t"
       "psrld %[sr],%[g]\n\t"
       "por %[g],%[x]"
       : [g] "=y" (G), [x] "=y" (X)
       : "[x]" (X), [sl] "g" (n), [sr] "g" (32-n)
       );

  return X;
}
#endif

/* #define F1( B, C, D ) ( ( (B) & (C) ) | ( ~(B) & (D) ) ) */
/* #define F1( B, C, D ) ( (D) ^ ( (B) & ( (C) ^ (D) ) ) ) */
#define F1( B, C, D ) ( \
	F = AND(B,C), \
	G = ANDNOT(D,B), \
	OR(F,G) )
/* #define F2( B, C, D ) ( (B) ^ (C) ^ (D) ) */
#define F2( B, C, D ) ( \
	F = XOR(B,C), \
	XOR(F,D) )
/* #define F3( B, C, D ) ( (B) & (C) ) | ( (C) & (D) ) | ( (B) & (D) ) */
/* #define F3( B, C, D ) ( ( (B) & ( (C) | (D) )) | ( (C) & (D) ) ) */
#define F3( B, C, D ) ( \
	F = OR(C,D), \
	G = AND(C,D), \
	F = AND(B,F), \
	OR(F,G) )
/* #define F4( B, C, D ) ( (B) ^ (C) ^ (D) ) */
#define F4(B,C,D) F2(B,C,D)

#define K1 0x5A827999  /* constant used for rounds 0..19 */
#define K2 0x6ED9EBA1  /* constant used for rounds 20..39 */
#define K3 0x8F1BBCDC  /* constant used for rounds 40..59 */
#define K4 0xCA62C1D6  /* constant used for rounds 60..79 */

/* #define Wf(t) (W[t] = S(1, W[t-16] ^ W[t-14] ^ W[t-8] ^ W[t-3])) */
#define Wf(W,t) ( \
	F = XOR((W)[t-16], (W)[t-14]), \
	G = XOR((W)[t-8], (W)[t-3]), \
	F = XOR(F,G), \
	(W)[t] = S(1,F) )

#define Wfly(W,t) ( (t) < 16 ? (W)[t] : Wf(W,t) )

/*
	#define ROUND(t,A,B,C,D,E,Func,K,W) \
	E = ADD(E,K); \
	F = S(5,A); \
	E = ADD(F,E); \
	F = Wfly(W,t); \
	E = ADD(F,E); \
	F = Func(B,C,D); \
	E = ADD(F,E); \
	B = S(30,B);
*/

#define ROUND_F1_n(t,A,B,C,D,E,K,W) \
	asm ( \
		"\n\t movq  %[d], %%mm5" /* begin F1(B,C,D) */ \
		"\n\t pxor  %[c], %%mm5" \
		"\n\t movq  %[a], %%mm7" /* begin S(5,A) */ \
		"\n\t pand  %[b], %%mm5" \
		"\n\t pslld $5,   %%mm7" \
		"\n\t pxor  %[d], %%mm5" \
		"\n\t movq  %[a], %%mm6" \
		"\n\t paddd %%mm5,%[e]"  /* sum F1(B,C,D) to E */ \
		"\n\t psrld $27,  %%mm6" \
		"\n\t paddd %[k], %[e]"  /* sum K to E */ \
		"\n\t por   %%mm6,%%mm7" \
		"\n\t movq  %[b], %%mm5" /* begin S(30,B) */ \
		"\n\t paddd %%mm7,%[e]"  /* sum S(5,A) to E */ \
		"\n\t pslld $30,  %[b]" \
		"\n\t psrld $2,   %%mm5" \
		"\n\t paddd %[Wt],%[e]"  /* sum W[t] to E */ \
		"\n\t por   %%mm5,%[b]"  /* complete S(30,B) */ \
		: [a] "+y" (A), [b] "+y" (B), [c] "+y" (C), [d] "+y" (D), [e] "+y" (E) \
		: [Wt] "m" ((W)[t]), [k] "m" (K) \
		: "mm5", "mm6", "mm7" );

#define ROUND_F2_n(t,A,B,C,D,E,K,W) \
	asm ( \
		"\n\t movq  %[b], %%mm5" /* begin F2(B,C,D) */ \
		"\n\t movq  %[a], %%mm7" /* begin S(5,A) */ \
		"\n\t pxor  %[c], %%mm5" \
		"\n\t pslld $5,   %%mm7" \
		"\n\t pxor  %[d], %%mm5" \
		"\n\t movq  %[a], %%mm6" \
		"\n\t paddd %%mm5,%[e]"  /* sum F2(B,C,D) to E */ \
		"\n\t psrld $27,  %%mm6" \
		"\n\t paddd %[k], %[e]"  /* sum K to E */ \
		"\n\t por   %%mm6,%%mm7" \
		"\n\t movq  %[b], %%mm5" /* begin S(30,B) */ \
		"\n\t paddd %%mm7,%[e]"  /* sum S(5,A) to E */ \
		"\n\t pslld $30,  %[b]" \
		"\n\t psrld $2,   %%mm5" \
		"\n\t paddd %[Wt],%[e]"  /* sum W[t] to E */ \
		"\n\t por   %%mm5,%[b]"  /* complete S(30,B) */ \
		: [a] "+y" (A), [b] "+y" (B), [c] "+y" (C), [d] "+y" (D), [e] "+y" (E) \
		: [Wt] "m" ((W)[t]), [k] "m" (K) \
		: "mm5", "mm6", "mm7" );

#define ROUND_F3_n(t,A,B,C,D,E,K,W) \
	asm ( \
		"\n\t movq  %[d], %%mm5" /* begin F3(B,C,D) */ \
		"\n\t movq  %[d], %%mm6" \
		"\n\t por   %[c], %%mm5" \
		"\n\t pand  %[c], %%mm6" \
		"\n\t movq  %[a], %%mm7" /* begin S(5,A) */ \
		"\n\t pand  %[b], %%mm5" \
		"\n\t pslld $5,   %%mm7" \
		"\n\t por   %%mm6,%%mm5" \
		"\n\t movq  %[a], %%mm6" \
		"\n\t paddd %%mm5,%[e]"  /* sum F3(B,C,D) to E */ \
		"\n\t psrld $27,  %%mm6" \
		"\n\t paddd %[k], %[e]"  /* sum K to E */ \
		"\n\t por   %%mm6,%%mm7" \
		"\n\t movq  %[b], %%mm5" /* begin S(30,B) */ \
		"\n\t paddd %%mm7,%[e]"  /* sum S(5,A) to E */ \
		"\n\t pslld $30,  %[b]" \
		"\n\t psrld $2,   %%mm5" \
		"\n\t paddd %[Wt],%[e]"  /* sum W[t] to E */ \
		"\n\t por   %%mm5,%[b]"  /* complete S(30,B) */ \
		: [a] "+y" (A), [b] "+y" (B), [c] "+y" (C), [d] "+y" (D), [e] "+y" (E) \
		: [Wt] "m" ((W)[t]), [k] "m" (K) \
		: "mm5", "mm6", "mm7" );

#define ROUND_F4_n ROUND_F2_n

#define ROUND(t,A,B,C,D,E,Func,K) \
		ROUND_##Func##_n(t,A,B,C,D,E,K,W); \

#define ROUND5( t, Func, K ) \
    ROUND( t + 0, A, B, C, D, E, Func, K );\
    ROUND( t + 1, E, A, B, C, D, Func, K );\
    ROUND( t + 2, D, E, A, B, C, Func, K );\
    ROUND( t + 3, C, D, E, A, B, Func, K );\
    ROUND( t + 4, B, C, D, E, A, Func, K )

int minter_mmx_compact_1(int bits, char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter)
{
#if (defined(__i386__) || defined(__AMD64__)) && defined(__GNUC__)
  unsigned long iters;
  int n, t, gotBits, maxBits = (bits > 16) ? 16 : bits;
  uInt32 bitMask1Low, bitMask1High, s;
  mmx_d_t vBitMaskHigh, vBitMaskLow;
  register mmx_d_t A,B,C,D,E;
  mmx_d_t MA,MB;
  mmx_d_t W[80];
  mmx_d_t H[5], pH[5];
  mmx_d_t K[4];
  uInt32 *Hw = (uInt32*) H;
  uInt32 *pHw = (uInt32*) pH;
  uInt32 IA, IB;
  const char *p = encodeAlphabets[EncodeBase64];
  unsigned char *X = (unsigned char*) W;
  unsigned char *output = (unsigned char*) block;
	
  /* Splat Kn constants into MMX-style array */
  ((uInt32*)K)[0] = ((uInt32*)K)[1] = K1;
  ((uInt32*)K)[2] = ((uInt32*)K)[3] = K2;
  ((uInt32*)K)[4] = ((uInt32*)K)[5] = K3;
  ((uInt32*)K)[6] = ((uInt32*)K)[7] = K4;
	
  /* Work out which bits to mask out for test */
  if(maxBits < 32) {
    bitMask1Low = ~((((uInt32) 1) << (32 - maxBits)) - 1);
    bitMask1High = 0;
  } else {
    bitMask1Low = ~0;
    bitMask1High = ~((((uInt32) 1) << (64 - maxBits)) - 1);
  }
  ((uInt32*) &vBitMaskLow )[0] = bitMask1Low ;
  ((uInt32*) &vBitMaskLow )[1] = bitMask1Low ;
  ((uInt32*) &vBitMaskHigh)[0] = bitMask1High;
  ((uInt32*) &vBitMaskHigh)[1] = bitMask1High;
  maxBits = 0;
	
  /* Copy block and IV to vectorised internal storage */
  /* Assume little-endian order, as we're on x86 or AMD64 */
  for(t=0; t < 16; t++) {
    X[t*8+ 0] = X[t*8+ 4] = output[t*4+3];
    X[t*8+ 1] = X[t*8+ 5] = output[t*4+2];
    X[t*8+ 2] = X[t*8+ 6] = output[t*4+1];
    X[t*8+ 3] = X[t*8+ 7] = output[t*4+0];
  }
  for(t=0; t < 5; t++) {
    Hw[t*2+0] = Hw[t*2+1] =
    pHw[t*2+0] = pHw[t*2+1] = IV[t];
	}
	
  /* The Tight Loop - everything in here should be extra efficient */
  for(iters=0; iters < maxIter; iters += 2) {
    /* Encode iteration count into tail */
    /* Iteration count is always 2-aligned, so only least-significant character needs multiple lookup */
    /* Further, we assume we're always little-endian */
    X[(((tailIndex - 1) & ~3) << 1) + (((tailIndex - 1) & 3) ^ 3) +  0] = p[(iters & 0x3e) + 0];
    X[(((tailIndex - 1) & ~3) << 1) + (((tailIndex - 1) & 3) ^ 3) +  4] = p[(iters & 0x3e) + 1];
    X[(((tailIndex - 2) & ~3) << 1) + (((tailIndex - 2) & 3) ^ 3) +  0] =
		X[(((tailIndex - 2) & ~3) << 1) + (((tailIndex - 2) & 3) ^ 3) +  4] = p[(iters >>  6) & 0x3f];
    X[(((tailIndex - 3) & ~3) << 1) + (((tailIndex - 3) & 3) ^ 3) +  0] =
		X[(((tailIndex - 3) & ~3) << 1) + (((tailIndex - 3) & 3) ^ 3) +  4] = p[(iters >> 12) & 0x3f];
    X[(((tailIndex - 4) & ~3) << 1) + (((tailIndex - 4) & 3) ^ 3) +  0] =
		X[(((tailIndex - 4) & ~3) << 1) + (((tailIndex - 4) & 3) ^ 3) +  4] = p[(iters >> 18) & 0x3f];
    X[(((tailIndex - 5) & ~3) << 1) + (((tailIndex - 5) & 3) ^ 3) +  0] =
		X[(((tailIndex - 5) & ~3) << 1) + (((tailIndex - 5) & 3) ^ 3) +  4] = p[(iters >> 24) & 0x3f];
    X[(((tailIndex - 6) & ~3) << 1) + (((tailIndex - 6) & 3) ^ 3) +  0] =
		X[(((tailIndex - 6) & ~3) << 1) + (((tailIndex - 6) & 3) ^ 3) +  4] = p[(iters >> 30) & 0x3f];

    /* Bypass shortcuts below on certain iterations */
    if((!(iters & 0xffffff)) && (tailIndex == 52 || tailIndex == 32)) {
      A = H[0];
      B = H[1];
      C = H[2];
      D = H[3];
      E = H[4];
			
      ROUND( 0, A, B, C, D, E, F1, K[0] );
      ROUND( 1, E, A, B, C, D, F1, K[0] );
      ROUND( 2, D, E, A, B, C, F1, K[0] );
      ROUND( 3, C, D, E, A, B, F1, K[0] );
      ROUND( 4, B, C, D, E, A, F1, K[0] );
      ROUND( 5, A, B, C, D, E, F1, K[0] );
      ROUND( 6, E, A, B, C, D, F1, K[0] );
			
      if(tailIndex == 52) {
				ROUND( 7, D, E, A, B, C, F1, K[0] );
				ROUND( 8, C, D, E, A, B, F1, K[0] );
				ROUND( 9, B, C, D, E, A, F1, K[0] );
				ROUND(10, A, B, C, D, E, F1, K[0] );
				ROUND(11, E, A, B, C, D, F1, K[0] );
      }
			
      pH[0] = A;
      pH[1] = B;
      pH[2] = C;
      pH[3] = D;
      pH[4] = E;
    }
		
    /* Populate W buffer */
    for(t=16; t < 80; t += 4) {
      asm volatile (
										/* Use pairs of adjacent MMX registers to build four nearly-independent chains */
										"\n\t movq -128(%[w]),%%mm0"
										"\n\t movq -120(%[w]),%%mm2"
										"\n\t movq -112(%[w]),%%mm4"
										"\n\t movq -104(%[w]),%%mm6"
      	
										"\n\t pxor  %%mm4,    %%mm0"
										"\n\t pxor  %%mm6,    %%mm2"
										"\n\t pxor  -96(%[w]),%%mm4"
										"\n\t pxor  -88(%[w]),%%mm6"
      	
										"\n\t pxor  -64(%[w]),%%mm0"
										"\n\t pxor  -56(%[w]),%%mm2"
										"\n\t pxor  -48(%[w]),%%mm4"
										"\n\t pxor  -40(%[w]),%%mm6"
      	
										"\n\t pxor  -24(%[w]),%%mm0"
										"\n\t pxor  -16(%[w]),%%mm2"
										/* 0(%[w]) is not yet valid! */
      	
										"\n\t movq  %%mm0, %%mm1"
										"\n\t movq  %%mm2, %%mm3"
										"\n\t pslld $1,    %%mm0"
										"\n\t psrld $31,   %%mm1"
										"\n\t pslld $1,    %%mm2"
										"\n\t psrld $31,   %%mm3"
										"\n\t por   %%mm1, %%mm0"
										"\n\t por   %%mm3, %%mm2"
      	
										/* ...now it is */
										"\n\t pxor   -8(%[w]),%%mm4"
										"\n\t pxor  %%mm0, %%mm6"
      	
										"\n\t movq  %%mm4, %%mm5"
										"\n\t movq  %%mm6, %%mm7"
										"\n\t pslld $1,    %%mm4"
										"\n\t psrld $31,   %%mm5"
										"\n\t pslld $1,    %%mm6"
										"\n\t psrld $31,   %%mm7"
										"\n\t por   %%mm5, %%mm4"
										"\n\t por   %%mm7, %%mm6"
      	
										"\n\t movq  %%mm0, 0(%[w])"
										"\n\t movq  %%mm2, 8(%[w])"
										"\n\t movq  %%mm4,16(%[w])"
										"\n\t movq  %%mm6,24(%[w])"
      	
										: /* no outputs */
										: [w] "r" (W+t)
										: "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7", "memory"
										);
    }

    /* Set up working variables */
    A = pH[0];
    B = pH[1];
    C = pH[2];
    D = pH[3];
    E = pH[4];
		
    /* Do the rounds */
    switch(tailIndex) {
    default:
      ROUND( 0, A, B, C, D, E, F1, K[0] );
      ROUND( 1, E, A, B, C, D, F1, K[0] );
      ROUND( 2, D, E, A, B, C, F1, K[0] );
      ROUND( 3, C, D, E, A, B, F1, K[0] );
      ROUND( 4, B, C, D, E, A, F1, K[0] );
      ROUND( 5, A, B, C, D, E, F1, K[0] );
      ROUND( 6, E, A, B, C, D, F1, K[0] );
    case 32:
      ROUND( 7, D, E, A, B, C, F1, K[0] );
      ROUND( 8, C, D, E, A, B, F1, K[0] );
      ROUND( 9, B, C, D, E, A, F1, K[0] );
      ROUND(10, A, B, C, D, E, F1, K[0] );
      ROUND(11, E, A, B, C, D, F1, K[0] );
    case 52:
      ROUND(12, D, E, A, B, C, F1, K[0] );
      ROUND(13, C, D, E, A, B, F1, K[0] );
      ROUND(14, B, C, D, E, A, F1, K[0] );
      ROUND(15, A, B, C, D, E, F1, K[0] );
      ROUND(16, E, A, B, C, D, F1, K[0] );
      ROUND(17, D, E, A, B, C, F1, K[0] );
      ROUND(18, C, D, E, A, B, F1, K[0] );
      ROUND(19, B, C, D, E, A, F1, K[0] );
    }
		
    ROUND5(20, F2, K[1] );
    ROUND5(25, F2, K[1] );
    ROUND5(30, F2, K[1] );
    ROUND5(35, F2, K[1] );
		
    ROUND5(40, F3, K[2] );
    ROUND5(45, F3, K[2] );
    ROUND5(50, F3, K[2] );
    ROUND5(55, F3, K[2] );
		
    ROUND5(60, F4, K[3] );
    ROUND5(65, F4, K[3] );
    ROUND5(70, F4, K[3] );
    ROUND5(75, F4, K[3] );
		
    /* Mix in the IV again */
    MA = ADD(A, H[0]);
    MB = ADD(B, H[1]);
					
    /* Go over each vector element in turn */
    for(n=0; n < 2; n++) {
      /* Extract A and B components */
      IA = ((uInt32*) &MA)[n];
      IB = ((uInt32*) &MB)[n];
				
      /* Is this the best bit count so far? */
      if(!(IA & bitMask1Low) && !(IB & bitMask1High)) {
				/* Count bits */
				gotBits = 0;
				if(IA) {
					s = IA;
					while(!(s & 0x80000000)) {
						s <<= 1;
						gotBits++;
					}
				} else {
					gotBits = 32;
					if(IB) {
						s = IB;
						while(!(s & 0x80000000)) {
							s <<= 1;
							gotBits++;
						}
					} else {
						gotBits = 64;
					}
				}
				
				/* Regenerate the bit mask */
				maxBits = gotBits+1;
				if(maxBits < 32) {
					bitMask1Low = ~((((uInt32) 1) << (32 - maxBits)) - 1);
					bitMask1High = 0;
				} else {
					bitMask1Low = ~0;
					bitMask1High = ~((((uInt32) 1) << (64 - maxBits)) - 1);
				}
				((uInt32*) &vBitMaskLow )[0] = bitMask1Low ;
				((uInt32*) &vBitMaskLow )[1] = bitMask1Low ;
				((uInt32*) &vBitMaskHigh)[0] = bitMask1High;
				((uInt32*) &vBitMaskHigh)[1] = bitMask1High;
				
				/* Copy this result back to the block buffer, little-endian */
				for(t=0; t < 16; t++) {
					output[t*4+0] = X[t*8+3+n*4];
					output[t*4+1] = X[t*8+2+n*4];
					output[t*4+2] = X[t*8+1+n*4];
					output[t*4+3] = X[t*8+0+n*4];
				}
				
				/* Is it good enough to bail out? */
				if(gotBits >= bits) {
					/* Shut down use of MMX */
					__builtin_ia32_emms();
				  
					return gotBits;
				}
      }
    }
  }
	
  /* Shut down use of MMX */
  __builtin_ia32_emms();

  /* Return the best result we found so far */
	if(maxBits)
		return maxBits-1;
	else
		return 0;

  /* For other platforms */
#else
  return 0;
#endif
}
