#include "libfastmint.h"
#if defined(__POWERPC__) && defined(__ALTIVEC__)
	#if !defined(__GNUC__) || !defined(__MACH__)
		#include <altivec.h>
	#endif
#endif

int minter_altivec_standard_1_test(void)
{
	/* This minter runs only on PowerPC G4 and higher hardware */
#if defined(__POWERPC__) && defined(__ALTIVEC__)
	return (gProcessorSupportFlags & HC_CPU_SUPPORTS_ALTIVEC) != 0;
#endif
	
	/* Not a PowerPC, or compiler doesn't support Altivec */
	return 0;
}

/* Define low-level primitives in terms of operations */
/* #define S(n, X) ( ( (X) << (n) ) | ( (X) >> ( 32 - (n) ) ) ) */
#define S(n,X) ( vec_rl( X, (vector unsigned int) (n) ) )
#define XOR(a,b) ( vec_xor(a,b) )
#define AND(a,b) ( vec_and(a,b) )
#define ANDNOT(a,b) ( vec_andc(a,b) )
#define OR(a,b) ( vec_or(a,b) )
#define ADD(a,b) ( vec_add(a,b) )

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
#define Wfly(W,t,u) ( (!(u) || (t) < 16) ? (W)[t] : Wf(W,t) )

#define ROUND(u,t,A,B,C,D,E,Func,K) \
	E = ADD(E,K); \
	F = S(5,A); \
	E = ADD(F,E); \
	F = Wfly(W,t,u); \
	E = ADD(F,E); \
	F = Func(B,C,D); \
	E = ADD(F,E); \
	B = S(30,B);

#define ROUNDu(t,A,B,C,D,E,Func,K) ROUND(1,t,A,B,C,D,E,Func,K)
#define ROUNDn(t,A,B,C,D,E,Func,K) ROUND(0,t,A,B,C,D,E,Func,K)

#define ROUND5( t, Func, K ) \
    ROUNDu( t + 0, A, B, C, D, E, Func, K );\
    ROUNDu( t + 1, E, A, B, C, D, Func, K );\
    ROUNDu( t + 2, D, E, A, B, C, Func, K );\
    ROUNDu( t + 3, C, D, E, A, B, Func, K );\
    ROUNDu( t + 4, B, C, D, E, A, Func, K )

#define ROUND20( t, Func, Kn )\
		K = vec_splat(Ka, Kn-1);\
    ROUND5( t +  0, Func, K );\
    ROUND5( t +  5, Func, K );\
    ROUND5( t + 10, Func, K );\
    ROUND5( t + 15, Func, K )

unsigned long minter_altivec_standard_1(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS)
{
	#if defined(__POWERPC__) && defined(__ALTIVEC__)
	MINTER_CALLBACK_VARS;
	unsigned long iters;
	int n, t, gotBits = 0, maxBits = (bits > 16) ? 16 : bits;
	uInt32 bitMask1Low, bitMask1High, s;
	vector unsigned int vBitMaskHigh, vBitMaskLow;
	vector unsigned int A,B,C,D,E, F,G;
	vector unsigned int W[80];
	vector unsigned int H[5], pH[5];
	vector unsigned int Ka = (vector unsigned int) (K1, K2, K3, K4);
	uInt32 *Hw = (uInt32*) H;
	const char *p = encodeAlphabets[EncodeBase64];
	unsigned char *X = (unsigned char*) W;
	unsigned char *output = (unsigned char*) block;
	
	if ( *best > 0 ) { maxBits = *best+1; }

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
	*((uInt32*) &vBitMaskLow ) = bitMask1Low ;
	vBitMaskLow  = vec_splat(vBitMaskLow , 0);
	*((uInt32*) &vBitMaskHigh) = bitMask1High;
	vBitMaskHigh = vec_splat(vBitMaskHigh, 0);
	maxBits = 0;
	
	/* Copy block and IV to vectorised internal storage */
	for(t=0; t < 16; t++) {
		X[t*16+ 0] = X[t*16+ 4] = X[t*16+ 8] = X[t*16+12] = output[t*4+0];
		X[t*16+ 1] = X[t*16+ 5] = X[t*16+ 9] = X[t*16+13] = output[t*4+1];
		X[t*16+ 2] = X[t*16+ 6] = X[t*16+10] = X[t*16+14] = output[t*4+2];
		X[t*16+ 3] = X[t*16+ 7] = X[t*16+11] = X[t*16+15] = output[t*4+3];
	}
	for(t=0; t < 5; t++) {
		Hw[t*4+0] = Hw[t*4+1] = Hw[t*4+2] = Hw[t*4+3] = IV[t];
		pH[t] = H[t];
	}
	
	/* The Tight Loop - everything in here should be extra efficient */
	for(iters=0; iters < maxIter-4; iters += 4) {
		/* Encode iteration count into tail */
		/* Iteration count is always 4-aligned, so only least-significant character needs multiple lookup */
		/* Further, we assume we're always big-endian */
		X[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) +  0] = p[(iters & 0x3c) + 0];
		X[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) +  4] = p[(iters & 0x3c) + 1];
		X[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) +  8] = p[(iters & 0x3c) + 2];
		X[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) + 12] = p[(iters & 0x3c) + 3];
		if(!(iters & 0x3f)) {
			if ( iters >> 6 ) {
				X[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) +  0] =
				X[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) +  4] =
				X[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) +  8] =
				X[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) + 12] = p[(iters >>  6) & 0x3f];
			}
			if ( iters >> 12 ) {
				X[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) +  0] =
				X[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) +  4] =
				X[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) +  8] =
				X[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) + 12] = p[(iters >> 12) & 0x3f];
			}
			if ( iters >> 18 ) {
				X[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) +  0] =
				X[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) +  4] =
				X[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) +  8] =
				X[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) + 12] = p[(iters >> 18) & 0x3f];
			}
			if ( iters >> 24 ) {
				X[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) +  0] =
				X[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) +  4] =
				X[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) +  8] =
				X[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) + 12] = p[(iters >> 24) & 0x3f];
			}
			if ( iters >> 30 ) {
				X[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) +  0] =
				X[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) +  4] =
				X[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) +  8] =
				X[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) + 12] = p[(iters >> 30) & 0x3f];
			}
		}

		/* Bypass shortcuts below on certain iterations */
		if((!(iters & 0xffffff)) && (tailIndex == 52 || tailIndex == 32)) {
			A = H[0];
			B = H[1];
			C = H[2];
			D = H[3];
			E = H[4];
			
			for(t=16; t < 32; t++)
				Wf(W,t);
			
	    ROUNDn( 0, A, B, C, D, E, F1, (vector unsigned int) (K1) );
	    ROUNDn( 1, E, A, B, C, D, F1, (vector unsigned int) (K1) );
	    ROUNDn( 2, D, E, A, B, C, F1, (vector unsigned int) (K1) );
	    ROUNDn( 3, C, D, E, A, B, F1, (vector unsigned int) (K1) );
	    ROUNDn( 4, B, C, D, E, A, F1, (vector unsigned int) (K1) );
	    ROUNDn( 5, A, B, C, D, E, F1, (vector unsigned int) (K1) );
	    ROUNDn( 6, E, A, B, C, D, F1, (vector unsigned int) (K1) );
			
			if(tailIndex == 52) {
		    ROUNDn( 7, D, E, A, B, C, F1, (vector unsigned int) (K1) );
		    ROUNDn( 8, C, D, E, A, B, F1, (vector unsigned int) (K1) );
		    ROUNDn( 9, B, C, D, E, A, F1, (vector unsigned int) (K1) );
		    ROUNDn(10, A, B, C, D, E, F1, (vector unsigned int) (K1) );
		    ROUNDn(11, E, A, B, C, D, F1, (vector unsigned int) (K1) );
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
		    ROUNDn( 0, A, B, C, D, E, F1, (vector unsigned int) (K1) );
		    ROUNDn( 1, E, A, B, C, D, F1, (vector unsigned int) (K1) );
		    ROUNDn( 2, D, E, A, B, C, F1, (vector unsigned int) (K1) );
		    ROUNDn( 3, C, D, E, A, B, F1, (vector unsigned int) (K1) );
		    ROUNDn( 4, B, C, D, E, A, F1, (vector unsigned int) (K1) );
		    ROUNDn( 5, A, B, C, D, E, F1, (vector unsigned int) (K1) );
		    ROUNDn( 6, E, A, B, C, D, F1, (vector unsigned int) (K1) );
			case 32:
		    ROUNDn( 7, D, E, A, B, C, F1, (vector unsigned int) (K1) );
		    ROUNDn( 8, C, D, E, A, B, F1, (vector unsigned int) (K1) );
		    ROUNDn( 9, B, C, D, E, A, F1, (vector unsigned int) (K1) );
		    ROUNDn(10, A, B, C, D, E, F1, (vector unsigned int) (K1) );
		    ROUNDn(11, E, A, B, C, D, F1, (vector unsigned int) (K1) );
			case 52:
		    ROUNDn(12, D, E, A, B, C, F1, (vector unsigned int) (K1) );
		    ROUNDn(13, C, D, E, A, B, F1, (vector unsigned int) (K1) );
		    ROUNDn(14, B, C, D, E, A, F1, (vector unsigned int) (K1) );
		    ROUNDn(15, A, B, C, D, E, F1, (vector unsigned int) (K1) );
		}

		if(tailIndex == 52) {
	    ROUNDn(16, E, A, B, C, D, F1, (vector unsigned int) (K1) );
	    ROUNDn(17, D, E, A, B, C, F1, (vector unsigned int) (K1) );
	    ROUNDn(18, C, D, E, A, B, F1, (vector unsigned int) (K1) );
	    ROUNDn(19, B, C, D, E, A, F1, (vector unsigned int) (K1) );
	    ROUNDu(20, A, B, C, D, E, F2, (vector unsigned int) (K2) );
	    ROUNDn(21, E, A, B, C, D, F2, (vector unsigned int) (K2) );
	    ROUNDn(22, D, E, A, B, C, F2, (vector unsigned int) (K2) );
	    ROUNDu(23, C, D, E, A, B, F2, (vector unsigned int) (K2) );
	    ROUNDn(24, B, C, D, E, A, F2, (vector unsigned int) (K2) );
	    ROUNDn(25, A, B, C, D, E, F2, (vector unsigned int) (K2) );
	    ROUNDu(26, E, A, B, C, D, F2, (vector unsigned int) (K2) );
	    ROUNDn(27, D, E, A, B, C, F2, (vector unsigned int) (K2) );
	    ROUNDu(28, C, D, E, A, B, F2, (vector unsigned int) (K2) );
	    ROUNDu(29, B, C, D, E, A, F2, (vector unsigned int) (K2) );
	    ROUNDn(30, A, B, C, D, E, F2, (vector unsigned int) (K2) );
		} else if (tailIndex == 32) {
	    ROUNDn(16, E, A, B, C, D, F1, (vector unsigned int) (K1) );
	    ROUNDn(17, D, E, A, B, C, F1, (vector unsigned int) (K1) );
	    ROUNDn(18, C, D, E, A, B, F1, (vector unsigned int) (K1) );
	    ROUNDn(19, B, C, D, E, A, F1, (vector unsigned int) (K1) );
	    ROUNDn(20, A, B, C, D, E, F2, (vector unsigned int) (K2) );
	    ROUNDu(21, E, A, B, C, D, F2, (vector unsigned int) (K2) );
	    ROUNDn(22, D, E, A, B, C, F2, (vector unsigned int) (K2) );
	    ROUNDu(23, C, D, E, A, B, F2, (vector unsigned int) (K2) );
	    ROUNDu(24, B, C, D, E, A, F2, (vector unsigned int) (K2) );
	    ROUNDn(25, A, B, C, D, E, F2, (vector unsigned int) (K2) );
	    ROUNDu(26, E, A, B, C, D, F2, (vector unsigned int) (K2) );
	    ROUNDu(27, D, E, A, B, C, F2, (vector unsigned int) (K2) );
	    ROUNDn(28, C, D, E, A, B, F2, (vector unsigned int) (K2) );
	    ROUNDu(29, B, C, D, E, A, F2, (vector unsigned int) (K2) );
	    ROUNDu(30, A, B, C, D, E, F2, (vector unsigned int) (K2) );
		} else {
	    ROUNDu(16, E, A, B, C, D, F1, (vector unsigned int) (K1) );
	    ROUNDu(17, D, E, A, B, C, F1, (vector unsigned int) (K1) );
	    ROUNDu(18, C, D, E, A, B, F1, (vector unsigned int) (K1) );
	    ROUNDu(19, B, C, D, E, A, F1, (vector unsigned int) (K1) );
	    ROUNDu(20, A, B, C, D, E, F2, (vector unsigned int) (K2) );
	    ROUNDu(21, E, A, B, C, D, F2, (vector unsigned int) (K2) );
	    ROUNDu(22, D, E, A, B, C, F2, (vector unsigned int) (K2) );
	    ROUNDu(23, C, D, E, A, B, F2, (vector unsigned int) (K2) );
	    ROUNDu(24, B, C, D, E, A, F2, (vector unsigned int) (K2) );
	    ROUNDu(25, A, B, C, D, E, F2, (vector unsigned int) (K2) );
	    ROUNDu(26, E, A, B, C, D, F2, (vector unsigned int) (K2) );
	    ROUNDu(27, D, E, A, B, C, F2, (vector unsigned int) (K2) );
	    ROUNDu(28, C, D, E, A, B, F2, (vector unsigned int) (K2) );
	    ROUNDu(29, B, C, D, E, A, F2, (vector unsigned int) (K2) );
	    ROUNDu(30, A, B, C, D, E, F2, (vector unsigned int) (K2) );
	  }
	  
    ROUNDu(31, E, A, B, C, D, F2, (vector unsigned int) (K2) );
    ROUNDu(32, D, E, A, B, C, F2, (vector unsigned int) (K2) );
    ROUNDu(33, C, D, E, A, B, F2, (vector unsigned int) (K2) );
    ROUNDu(34, B, C, D, E, A, F2, (vector unsigned int) (K2) );
    ROUNDu(35, A, B, C, D, E, F2, (vector unsigned int) (K2) );
    ROUNDu(36, E, A, B, C, D, F2, (vector unsigned int) (K2) );
    ROUNDu(37, D, E, A, B, C, F2, (vector unsigned int) (K2) );
    ROUNDu(38, C, D, E, A, B, F2, (vector unsigned int) (K2) );
    ROUNDu(39, B, C, D, E, A, F2, (vector unsigned int) (K2) );
		
		ROUND5(40, F3, (vector unsigned int) (K3) );
		ROUND5(45, F3, (vector unsigned int) (K3) );
		ROUND5(50, F3, (vector unsigned int) (K3) );
		ROUND5(55, F3, (vector unsigned int) (K3) );
		
		ROUND5(60, F4, (vector unsigned int) (K4) );
		ROUND5(65, F4, (vector unsigned int) (K4) );
		ROUND5(70, F4, (vector unsigned int) (K4) );
		ROUND5(75, F4, (vector unsigned int) (K4) );
		
		/* Mix in the IV again */
		A = vec_add(A, H[0]);
		B = vec_add(B, H[1]);
		C = vec_add(C, H[2]);
		D = vec_add(D, H[3]);
		E = vec_add(E, H[4]);
		
		/* Is this the best bit count so far? */
		if(vec_any_ne( vec_and( vec_cmpeq(vec_and(A, vBitMaskLow), (vector unsigned int) (0)), vec_cmpeq(vec_and(A, vBitMaskHigh), (vector unsigned int) (0)) ), (vector unsigned int) (0))) {
			uInt32 IA, IB;
			
			/* Go over each vector element in turn */
			for(n=0; n < 4; n++) {
				/* Extract A and B components */
				IA = ((uInt32*) &A)[n];
				IB = ((uInt32*) &B)[n];
				
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
				*((uInt32*) &vBitMaskLow ) = bitMask1Low ;
				vBitMaskLow  = vec_splat(vBitMaskLow , 0);
				*((uInt32*) &vBitMaskHigh) = bitMask1High;
				vBitMaskHigh = vec_splat(vBitMaskHigh, 0);
				
				/* Copy this result back to the block buffer */
				for(t=0; t < 16; t++) {
					output[t*4+0] = X[t*16+0+n*4];
					output[t*4+1] = X[t*16+1+n*4];
					output[t*4+2] = X[t*16+2+n*4];
					output[t*4+3] = X[t*16+3+n*4];
				}
				
				/* Is it good enough to bail out? */
				if(gotBits >= bits) {
					return iters+4;
				}
			}
		}
		MINTER_CALLBACK();
	}
	
	return iters+4;

	/* For other platforms */
	#else
	return 0;
	#endif
}
