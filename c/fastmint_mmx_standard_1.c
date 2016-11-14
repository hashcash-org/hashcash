#include <stdio.h>
#include <setjmp.h>
#include "libfastmint.h"

#if (defined(__i386__) || defined(__AMD64__)) && defined(__GNUC__) && defined(__MMX__)
typedef int mmx_d_t __attribute__ ((vector_size (8)));
typedef int mmx_q_t __attribute__ ((vector_size (8)));
#endif

int minter_mmx_standard_1_test(void)
{
  /* This minter runs only on x86 and AMD64 hardware supporting MMX - and will only compile on GCC */
#if (defined(__i386__) || defined(__AMD64__)) && defined(__GNUC__) && defined(__MMX__)
	return (gProcessorSupportFlags & HC_CPU_SUPPORTS_MMX) != 0;
#endif
  
  /* Not an x86 or AMD64, or compiler doesn't support MMX or GNU assembly */
  return 0;
}

/* Define low-level primitives in terms of operations */
/* #define S(n, X) ( ( (X) << (n) ) | ( (X) >> ( 32 - (n) ) ) ) */
#define XOR(a,b) ( (mmx_d_t) __builtin_ia32_pxor( (mmx_q_t) a, (mmx_q_t) b) )
#define AND(a,b) ( (mmx_d_t) __builtin_ia32_pand( (mmx_q_t) a, (mmx_q_t) b) )
#define ANDNOT(a,b) ( (mmx_d_t) __builtin_ia32_pandn( (mmx_q_t) b, (mmx_q_t) a) )
#define OR(a,b) ( (mmx_d_t) __builtin_ia32_por( (mmx_q_t) a, (mmx_q_t) b) )
#define ADD(a,b) ( __builtin_ia32_paddd(a,b) )

#if (defined(__i386__) || defined(__AMD64__)) && defined(__GNUC__) && defined(__MMX__)
static inline mmx_d_t S(int n, mmx_d_t X)
{
  mmx_d_t G = {} ;

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

#define ROUND_F1_u(t,A,B,C,D,E,K,W) \
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
		"\n\t movq  %[Wt_3],%%mm7" /* begin Wf(t) */ \
		"\n\t movq  %[Wt_8],%%mm6" \
		"\n\t pxor  %[Wt_14],%%mm7" \
		"\n\t pxor  %[Wt_16],%%mm6" \
		"\n\t pslld $30,  %[b]" \
		"\n\t pxor  %%mm6,%%mm7" \
		"\n\t movq  %%mm7,%%mm6" \
		"\n\t pslld $1,   %%mm7" \
		"\n\t psrld $31,  %%mm6" \
		"\n\t psrld $2,   %%mm5" \
		"\n\t por   %%mm6,%%mm7" \
		"\n\t por   %%mm5,%[b]"  /* complete S(30,B) */ \
		"\n\t paddd %%mm7,%[e]"  /* sum Wf(t) to E */ \
		"\n\t movq  %%mm7,%[Wt]" /* write back Wf(t) to W[t] */ \
		: [a] "+y" (A), [b] "+y" (B), [c] "+y" (C), [d] "+y" (D), [e] "+y" (E), [Wt] "=m" ((W)[t]) \
		: [Wt_3] "m" ((W)[t-3]), [Wt_14] "m" ((W)[t-14]), [Wt_8] "m" ((W)[t-8]), [Wt_16] "m" ((W)[t-16]), [k] "m" (K) \
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

#define ROUND_F2_u(t,A,B,C,D,E,K,W) \
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
		"\n\t movq  %[Wt_3],%%mm7" /* begin Wf(t) */ \
		"\n\t movq  %[Wt_8],%%mm6" \
		"\n\t pxor  %[Wt_14],%%mm7" \
		"\n\t pxor  %[Wt_16],%%mm6" \
		"\n\t pslld $30,  %[b]" \
		"\n\t pxor  %%mm6,%%mm7" \
		"\n\t movq  %%mm7,%%mm6" \
		"\n\t pslld $1,   %%mm7" \
		"\n\t psrld $31,  %%mm6" \
		"\n\t psrld $2,   %%mm5" \
		"\n\t por   %%mm6,%%mm7" \
		"\n\t por   %%mm5,%[b]"  /* complete S(30,B) */ \
		"\n\t paddd %%mm7,%[e]"  /* sum Wf(t) to E */ \
		"\n\t movq  %%mm7,%[Wt]" /* write back Wf(t) to W[t] */ \
		: [a] "+y" (A), [b] "+y" (B), [c] "+y" (C), [d] "+y" (D), [e] "+y" (E), [Wt] "=m" ((W)[t]) \
		: [Wt_3] "m" ((W)[t-3]), [Wt_14] "m" ((W)[t-14]), [Wt_8] "m" ((W)[t-8]), [Wt_16] "m" ((W)[t-16]), [k] "m" (K) \
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

#define ROUND_F3_u(t,A,B,C,D,E,K,W) \
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
		"\n\t movq  %[Wt_3],%%mm7" /* begin Wf(t) */ \
		"\n\t movq  %[Wt_8],%%mm6" \
		"\n\t pxor  %[Wt_14],%%mm7" \
		"\n\t pxor  %[Wt_16],%%mm6" \
		"\n\t pslld $30,  %[b]" \
		"\n\t pxor  %%mm6,%%mm7" \
		"\n\t movq  %%mm7,%%mm6" \
		"\n\t pslld $1,   %%mm7" \
		"\n\t psrld $31,  %%mm6" \
		"\n\t psrld $2,   %%mm5" \
		"\n\t por   %%mm6,%%mm7" \
		"\n\t por   %%mm5,%[b]"  /* complete S(30,B) */ \
		"\n\t paddd %%mm7,%[e]"  /* sum Wf(t) to E */ \
		"\n\t movq  %%mm7,%[Wt]" /* write back Wf(t) to W[t] */ \
		: [a] "+y" (A), [b] "+y" (B), [c] "+y" (C), [d] "+y" (D), [e] "+y" (E), [Wt] "=m" ((W)[t]) \
		: [Wt_3] "m" ((W)[t-3]), [Wt_14] "m" ((W)[t-14]), [Wt_8] "m" ((W)[t-8]), [Wt_16] "m" ((W)[t-16]), [k] "m" (K) \
		: "mm5", "mm6", "mm7" );

#define ROUND_F4_u ROUND_F2_u
#define ROUND_F4_n ROUND_F2_n

#define ROUNDu(t,A,B,C,D,E,Func,K) \
		if((t) < 16) { \
			ROUND_##Func##_n(t,A,B,C,D,E,K,W); \
		} else { \
			ROUND_##Func##_u(t,A,B,C,D,E,K,W); \
		}

#define ROUNDn(t,A,B,C,D,E,Func,K) \
		ROUND_##Func##_n(t,A,B,C,D,E,K,W); \

#define ROUND5( t, Func, K ) \
    ROUNDu( t + 0, A, B, C, D, E, Func, K );\
    ROUNDu( t + 1, E, A, B, C, D, Func, K );\
    ROUNDu( t + 2, D, E, A, B, C, Func, K );\
    ROUNDu( t + 3, C, D, E, A, B, Func, K );\
    ROUNDu( t + 4, B, C, D, E, A, Func, K );

#if defined(MINTER_CALLBACK_CLEANUP_FP)
#undef MINTER_CALLBACK_CLEANUP_FP
#endif
#define MINTER_CALLBACK_CLEANUP_FP __builtin_ia32_emms()

unsigned long minter_mmx_standard_1(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS)
{
#if (defined(__i386__) || defined(__AMD64__)) && defined(__GNUC__) && defined(__MMX__)
  MINTER_CALLBACK_VARS;
  unsigned long iters = 0 ;
  int n = 0, t = 0, gotBits = 0, maxBits = (bits > 16) ? 16 : bits;
  uInt32 bitMask1Low = 0 , bitMask1High = 0 , s = 0 ;
  mmx_d_t vBitMaskHigh = {} , vBitMaskLow = {} ;
  register mmx_d_t A = {} , B = {} , C = {} , D = {} , E = {} ;
  mmx_d_t MA = {} , MB = {} ;
  mmx_d_t W[80] = {} ;
  mmx_d_t H[5] = {} , pH[5] = {} ;
  mmx_d_t K[4] = {} ;
  uInt32 *Hw = (uInt32*) H;
  uInt32 *pHw = (uInt32*) pH;
  uInt32 IA = 0 , IB = 0 ;
  const char *p = encodeAlphabets[EncodeBase64];
  unsigned char *X = (unsigned char*) W;
  unsigned char *output = (unsigned char*) block;
	
  if ( *best > 0 ) { maxBits = *best+1; }

  /* Splat Kn constants into MMX-style array */
  ((uInt32*)K)[0] = ((uInt32*)K)[1] = K1;
  ((uInt32*)K)[2] = ((uInt32*)K)[3] = K2;
  ((uInt32*)K)[4] = ((uInt32*)K)[5] = K3;
  ((uInt32*)K)[6] = ((uInt32*)K)[7] = K4;
	
  /* Work out which bits to mask out for test */
  if(maxBits < 32) {
    if ( bits == 0 ) { bitMask1Low = 0; } else {
      bitMask1Low = ~((((uInt32) 1) << (32 - maxBits)) - 1);
    }
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
  for(iters=0; iters < maxIter-2; iters += 2) {

    /* Encode iteration count into tail */
    /* Iteration count is always 2-aligned, so only least-significant character needs multiple lookup */
    /* Further, we assume we're always little-endian */
    X[(((tailIndex - 1) & ~3) << 1) + (((tailIndex - 1) & 3) ^ 3) +  0] = p[(iters & 0x3e) + 0];
    X[(((tailIndex - 1) & ~3) << 1) + (((tailIndex - 1) & 3) ^ 3) +  4] = p[(iters & 0x3e) + 1];
    if(!(iters & 0x3f)) {
      if ( iters >> 6 ) {
	X[(((tailIndex - 2) & ~3) << 1) + (((tailIndex - 2) & 3) ^ 3) +  0] =
	X[(((tailIndex - 2) & ~3) << 1) + (((tailIndex - 2) & 3) ^ 3) +  4] = p[(iters >>  6) & 0x3f];
      }
      if ( iters >> 12 ) { 
	X[(((tailIndex - 3) & ~3) << 1) + (((tailIndex - 3) & 3) ^ 3) +  0] =
	X[(((tailIndex - 3) & ~3) << 1) + (((tailIndex - 3) & 3) ^ 3) +  4] = p[(iters >> 12) & 0x3f];
      }
      if ( iters >> 18 ) { 
	X[(((tailIndex - 4) & ~3) << 1) + (((tailIndex - 4) & 3) ^ 3) +  0] =
	X[(((tailIndex - 4) & ~3) << 1) + (((tailIndex - 4) & 3) ^ 3) +  4] = p[(iters >> 18) & 0x3f];
      }
      if ( iters >> 24 ) {
	X[(((tailIndex - 5) & ~3) << 1) + (((tailIndex - 5) & 3) ^ 3) +  0] =
	X[(((tailIndex - 5) & ~3) << 1) + (((tailIndex - 5) & 3) ^ 3) +  4] = p[(iters >> 24) & 0x3f];
      }
      if ( iters >> 30 ) { 
	X[(((tailIndex - 6) & ~3) << 1) + (((tailIndex - 6) & 3) ^ 3) +  0] =
	X[(((tailIndex - 6) & ~3) << 1) + (((tailIndex - 6) & 3) ^ 3) +  4] = p[(iters >> 30) & 0x3f];
      }
    }
		
    /* Force compiler to flush and reload MMX registers */
    asm volatile ( "nop" : : : "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7", "memory" );

    /* Bypass shortcuts below on certain iterations */
    if((!(iters & 0xffffff)) && (tailIndex == 52 || tailIndex == 32)) {
			/* Populate W buffer */
			for(t=16; t < 32; t += 4) {
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

      A = H[0];
      B = H[1];
      C = H[2];
      D = H[3];
      E = H[4];
			
      ROUNDn( 0, A, B, C, D, E, F1, K[0] );
      ROUNDn( 1, E, A, B, C, D, F1, K[0] );
      ROUNDn( 2, D, E, A, B, C, F1, K[0] );
      ROUNDn( 3, C, D, E, A, B, F1, K[0] );
      ROUNDn( 4, B, C, D, E, A, F1, K[0] );
      ROUNDn( 5, A, B, C, D, E, F1, K[0] );
      ROUNDn( 6, E, A, B, C, D, F1, K[0] );
			
      if(tailIndex == 52) {
				ROUNDn( 7, D, E, A, B, C, F1, K[0] );
				ROUNDn( 8, C, D, E, A, B, F1, K[0] );
				ROUNDn( 9, B, C, D, E, A, F1, K[0] );
				ROUNDn(10, A, B, C, D, E, F1, K[0] );
				ROUNDn(11, E, A, B, C, D, F1, K[0] );
      }
			
      pH[0] = A;
      pH[1] = B;
      pH[2] = C;
      pH[3] = D;
      pH[4] = E;
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
      ROUNDn( 0, A, B, C, D, E, F1, K[0] );
      ROUNDn( 1, E, A, B, C, D, F1, K[0] );
      ROUNDn( 2, D, E, A, B, C, F1, K[0] );
      ROUNDn( 3, C, D, E, A, B, F1, K[0] );
      ROUNDn( 4, B, C, D, E, A, F1, K[0] );
      ROUNDn( 5, A, B, C, D, E, F1, K[0] );
      ROUNDn( 6, E, A, B, C, D, F1, K[0] );
    case 32:
      ROUNDn( 7, D, E, A, B, C, F1, K[0] );
      ROUNDn( 8, C, D, E, A, B, F1, K[0] );
      ROUNDn( 9, B, C, D, E, A, F1, K[0] );
      ROUNDn(10, A, B, C, D, E, F1, K[0] );
      ROUNDn(11, E, A, B, C, D, F1, K[0] );
    case 52:
      ROUNDn(12, D, E, A, B, C, F1, K[0] );
      ROUNDn(13, C, D, E, A, B, F1, K[0] );
      ROUNDn(14, B, C, D, E, A, F1, K[0] );
      ROUNDn(15, A, B, C, D, E, F1, K[0] );
    }

    if(tailIndex == 52) {
      ROUNDn(16, E, A, B, C, D, F1, K[0] );
      ROUNDn(17, D, E, A, B, C, F1, K[0] );
      ROUNDn(18, C, D, E, A, B, F1, K[0] );
      ROUNDn(19, B, C, D, E, A, F1, K[0] );
      ROUNDu(20, A, B, C, D, E, F2, K[1] );
      ROUNDn(21, E, A, B, C, D, F2, K[1] );
      ROUNDn(22, D, E, A, B, C, F2, K[1] );
      ROUNDu(23, C, D, E, A, B, F2, K[1] );
      ROUNDn(24, B, C, D, E, A, F2, K[1] );
      ROUNDn(25, A, B, C, D, E, F2, K[1] );
      ROUNDu(26, E, A, B, C, D, F2, K[1] );
      ROUNDn(27, D, E, A, B, C, F2, K[1] );
      ROUNDu(28, C, D, E, A, B, F2, K[1] );
      ROUNDu(29, B, C, D, E, A, F2, K[1] );
      ROUNDn(30, A, B, C, D, E, F2, K[1] );
    } else if (tailIndex == 32) {
      ROUNDn(16, E, A, B, C, D, F1, K[0] );
      ROUNDn(17, D, E, A, B, C, F1, K[0] );
      ROUNDn(18, C, D, E, A, B, F1, K[0] );
      ROUNDn(19, B, C, D, E, A, F1, K[0] );
      ROUNDn(20, A, B, C, D, E, F2, K[1] );
      ROUNDu(21, E, A, B, C, D, F2, K[1] );
      ROUNDn(22, D, E, A, B, C, F2, K[1] );
      ROUNDu(23, C, D, E, A, B, F2, K[1] );
      ROUNDu(24, B, C, D, E, A, F2, K[1] );
      ROUNDn(25, A, B, C, D, E, F2, K[1] );
      ROUNDu(26, E, A, B, C, D, F2, K[1] );
      ROUNDu(27, D, E, A, B, C, F2, K[1] );
      ROUNDn(28, C, D, E, A, B, F2, K[1] );
      ROUNDu(29, B, C, D, E, A, F2, K[1] );
      ROUNDu(30, A, B, C, D, E, F2, K[1] );
    } else {
      ROUNDu(16, E, A, B, C, D, F1, K[0] );
      ROUNDu(17, D, E, A, B, C, F1, K[0] );
      ROUNDu(18, C, D, E, A, B, F1, K[0] );
      ROUNDu(19, B, C, D, E, A, F1, K[0] );
      ROUNDu(20, A, B, C, D, E, F2, K[1] );
      ROUNDu(21, E, A, B, C, D, F2, K[1] );
      ROUNDu(22, D, E, A, B, C, F2, K[1] );
      ROUNDu(23, C, D, E, A, B, F2, K[1] );
      ROUNDu(24, B, C, D, E, A, F2, K[1] );
      ROUNDu(25, A, B, C, D, E, F2, K[1] );
      ROUNDu(26, E, A, B, C, D, F2, K[1] );
      ROUNDu(27, D, E, A, B, C, F2, K[1] );
      ROUNDu(28, C, D, E, A, B, F2, K[1] );
      ROUNDu(29, B, C, D, E, A, F2, K[1] );
      ROUNDu(30, A, B, C, D, E, F2, K[1] );
    }
	  
    ROUNDu(31, E, A, B, C, D, F2, K[1] );
    ROUNDu(32, D, E, A, B, C, F2, K[1] );
    ROUNDu(33, C, D, E, A, B, F2, K[1] );
    ROUNDu(34, B, C, D, E, A, F2, K[1] );
    ROUNDu(35, A, B, C, D, E, F2, K[1] );
    ROUNDu(36, E, A, B, C, D, F2, K[1] );
    ROUNDu(37, D, E, A, B, C, F2, K[1] );
    ROUNDu(38, C, D, E, A, B, F2, K[1] );
    ROUNDu(39, B, C, D, E, A, F2, K[1] );
		
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
				if ( gotBits > *best ) { *best = gotBits; }
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
				  
					return iters+2;
				}
      }
    }
    MINTER_CALLBACK();
  }
	
  /* Shut down use of MMX */
  __builtin_ia32_emms();

  return iters+2;

  /* For other platforms */
#else
  return 0;
#endif
}
