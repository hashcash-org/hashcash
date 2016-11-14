#include <stdio.h>
#include "libfastmint.h"

int minter_ansi_compact_2_test(void)
{
	/* This minter runs on any hardware */
	return 1;
}

/* Define low-level primitives in terms of operations */
#define S(n,X) ( ( (X) << (n) ) | ( (X) >> ( 32 - (n) ) ) )
#define XOR(a,b) ( (a) ^ (b) )
#define AND(a,b) ( (a) & (b) )
#define ANDNOT(a,b) ( (a) & ~(b) )
#define OR(a,b) ( (a) | (b) )
#define ADD(a,b) ( (a) + (b) )

#define F1( B, C, D ) ( ( (B) & (C) ) | ( ~(B) & (D) ) )
/* #define F1( B, C, D ) ( (D) ^ ( (B) & ( (C) ^ (D) ) ) ) */
/* #define F1( B, C, D, F, G ) ( \
	F = AND(B,C), \
	G = ANDNOT(D,B), \
	OR(F,G) ) */
#define F2( B, C, D ) ( (B) ^ (C) ^ (D) )
/* #define F2( B, C, D, F, G ) ( \
	F = XOR(B,C), \
	XOR(F,D) ) */
/* #define F3( B, C, D ) ( (B) & (C) ) | ( (C) & (D) ) | ( (B) & (D) ) */
#define F3( B, C, D ) ( ( (B) & ( (C) | (D) )) | ( (C) & (D) ) )
/* #define F3( B, C, D, F, G ) ( \
	F = OR(C,D), \
	G = AND(C,D), \
	F = AND(B,F), \
	OR(F,G) ) */
#define F4( B, C, D ) ( (B) ^ (C) ^ (D) )
/* #define F4(B,C,D,F,G) F2(B,C,D,F,G) */

#define K1 0x5A827999  /* constant used for rounds 0..19 */
#define K2 0x6ED9EBA1  /* constant used for rounds 20..39 */
#define K3 0x8F1BBCDC  /* constant used for rounds 40..59 */
#define K4 0xCA62C1D6  /* constant used for rounds 60..79 */

#define Wf(W,t) ((W)[t] = S(1, (W)[t-16] ^ (W)[t-14] ^ (W)[t-8] ^ (W)[t-3]))
/* #define Wf(W,t,F,G) ( \
	F = XOR((W)[t-16], (W)[t-14]), \
	G = XOR((W)[t-8], (W)[t-3]), \
	F = XOR(F,G), \
	(W)[t] = S(1,F) ) */

#define Wfly(W,t) ( (t) < 16 ? (W)[t] : Wf(W,t) )

/* #define ROUND(t,A,B,C,D,E,F,G,Func,K,W) \
	E = ADD(E,K); \
	F = S(5,A); \
	E = ADD(F,E); \
	E = ADD((W)[t],E); \
	F = Func(B,C,D,F,G); \
	E = ADD(F,E); \
	B = S(30,B);
	*/

#define ROUND(u,t,A,B,C,D,E,Func,K,W) \
	E += S(5,A) + Func(B,C,D) + ((u) ? Wfly(W,t) : (W)[t]) + K; \
	B = S(30,B);

#define ROUNDn(t,A,B,C,D,E,Func,K,W) \
	ROUND(0,t,A##1,B##1,C##1,D##1,E##1,Func,K,W##1); \
	ROUND(0,t,A##2,B##2,C##2,D##2,E##2,Func,K,W##2);

#define ROUNDu(t,A,B,C,D,E,Func,K,W) \
	ROUND(1,t,A##1,B##1,C##1,D##1,E##1,Func,K,W##1); \
	ROUND(1,t,A##2,B##2,C##2,D##2,E##2,Func,K,W##2);

#define ROUND5n( t, Func, K ) \
    ROUNDn( t + 0, A, B, C, D, E, Func, K, W );\
    ROUNDn( t + 1, E, A, B, C, D, Func, K, W );\
    ROUNDn( t + 2, D, E, A, B, C, Func, K, W );\
    ROUNDn( t + 3, C, D, E, A, B, Func, K, W );\
    ROUNDn( t + 4, B, C, D, E, A, Func, K, W );

#define ROUND5u( t, Func, K ) \
    ROUNDu( t + 0, A, B, C, D, E, Func, K, W );\
    ROUNDu( t + 1, E, A, B, C, D, Func, K, W );\
    ROUNDu( t + 2, D, E, A, B, C, Func, K, W );\
    ROUNDu( t + 3, C, D, E, A, B, Func, K, W );\
    ROUNDu( t + 4, B, C, D, E, A, Func, K, W );

unsigned long minter_ansi_compact_2(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS)
{
	MINTER_CALLBACK_VARS;
	unsigned long iters = 0 ;
	int n = 0, t = 0, gotBits = 0, maxBits = (bits > 16) ? 16 : bits;
	uInt32 bitMask1Low = 0 , bitMask1High = 0 , s = 0 ;
	uInt32 A = 0 , B = 0 , *W = 0 ;
	/*register*/ uInt32 A1 = 0 , B1 = 0 , C1 = 0 , D1 = 0 , E1 = 0 ;
	/*register*/ uInt32 A2 = 0 , B2 = 0 , C2 = 0 , D2 = 0 , E2 = 0 ;
	uInt32 W1[80] = {0};
	uInt32 W2[80] = {0};
	uInt32 H[5] = {0}, pH[5] = {0};
	const char *p = encodeAlphabets[EncodeBase64];
	unsigned char *X1 = (unsigned char*) W1;
	unsigned char *X2 = (unsigned char*) W2;
	int addressMask = 0 ;
	static const int endTest = 3;
	unsigned char *output = (unsigned char*) block;
	const int W32[] = {21,23,24,26,27,29,30,31,0}, W52[] = {20,23,26,28,29,31,0};
	char wordUpdate[80] = {0};
	
	if ( *best > 0 ) { maxBits = *best+1; }

	/* Work out whether we need to swap bytes during encoding */
	addressMask = ( *(char*)&endTest );
	
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
	maxBits = 0;
	
	/* Copy block and IV to internal storage */
	for(t=0; t < 16; t++)
		W1[t] = W2[t] = GET_WORD(output + t*4);
	for(t=0; t < 5; t++)
		pH[t] = H[t] = IV[t];
	
	/* Precalculate which words need the W buffer updating */
	switch(tailIndex) {
		default:
			for(t=16; t < 32; t++)
				wordUpdate[t] = 1;
			break;
			
		case 32:
			for(t=0; W32[t]; t++)
				wordUpdate[W32[t]] = 1;
			break;
			
		case 52:
			for(t=0; W52[t]; t++)
				wordUpdate[W52[t]] = 1;
			break;
	}
	for(t=32; t < 80; t++)
		wordUpdate[t] = 1;
	
	/* The Tight Loop - everything in here should be extra efficient */
	for(iters=0; iters < maxIter-2; iters += 2) {

		/* Encode iteration count into tail */
		X1[(tailIndex - 1) ^ addressMask] = p[((iters+0)      ) & 0x3f];
		X2[(tailIndex - 1) ^ addressMask] = p[((iters+1)      ) & 0x3f];
		if(!(iters & 0x3f)) {
			X1[(tailIndex - 2) ^ addressMask] = X2[(tailIndex - 2) ^ addressMask] = p[((iters) >>  6) & 0x3f];
			X1[(tailIndex - 3) ^ addressMask] = X2[(tailIndex - 3) ^ addressMask] = p[((iters) >> 12) & 0x3f];
			X1[(tailIndex - 4) ^ addressMask] = X2[(tailIndex - 4) ^ addressMask] = p[((iters) >> 18) & 0x3f];
			X1[(tailIndex - 5) ^ addressMask] = X2[(tailIndex - 5) ^ addressMask] = p[((iters) >> 24) & 0x3f];
			X1[(tailIndex - 6) ^ addressMask] = X2[(tailIndex - 6) ^ addressMask] = p[((iters) >> 30) & 0x3f];
		}

		/* Bypass shortcuts below on certain iterations */
		if((!(iters & 0xffffff)) && (tailIndex == 52 || tailIndex == 32)) {
			A1 = H[0];
			B1 = H[1];
			C1 = H[2];
			D1 = H[3];
			E1 = H[4];
			
			for(t=16; t < 32; t++) {
				Wf(W1,t);
				Wf(W2,t);
			}
			
	    ROUND(0, 0, A1, B1, C1, D1, E1, F1, K1, W1 );
	    ROUND(0, 1, E1, A1, B1, C1, D1, F1, K1, W1 );
	    ROUND(0, 2, D1, E1, A1, B1, C1, F1, K1, W1 );
	    ROUND(0, 3, C1, D1, E1, A1, B1, F1, K1, W1 );
	    ROUND(0, 4, B1, C1, D1, E1, A1, F1, K1, W1 );
	    ROUND(0, 5, A1, B1, C1, D1, E1, F1, K1, W1 );
	    ROUND(0, 6, E1, A1, B1, C1, D1, F1, K1, W1 );
			
			if(tailIndex == 52) {
		    ROUND(0, 7, D1, E1, A1, B1, C1, F1, K1, W1 );
		    ROUND(0, 8, C1, D1, E1, A1, B1, F1, K1, W1 );
		    ROUND(0, 9, B1, C1, D1, E1, A1, F1, K1, W1 );
		    ROUND(0,10, A1, B1, C1, D1, E1, F1, K1, W1 );
		    ROUND(0,11, E1, A1, B1, C1, D1, F1, K1, W1 );
			}
			
			pH[0] = A1;
			pH[1] = B1;
			pH[2] = C1;
			pH[3] = D1;
			pH[4] = E1;
		}

		/* Fill W buffer */
		for(t=16; t < 80; t++) {
			if(wordUpdate[t]) {
				Wf(W1,t);
				Wf(W2,t);
			}
		}
		
		/* Set up working variables */
		A1 = A2 = pH[0];
		B1 = B2 = pH[1];
		C1 = C2 = pH[2];
		D1 = D2 = pH[3];
		E1 = E2 = pH[4];
		
		/* Do the rounds */
		switch(tailIndex) {
			default:
		    ROUNDn( 0, A, B, C, D, E, F1, K1, W );
		    ROUNDn( 1, E, A, B, C, D, F1, K1, W );
		    ROUNDn( 2, D, E, A, B, C, F1, K1, W );
		    ROUNDn( 3, C, D, E, A, B, F1, K1, W );
		    ROUNDn( 4, B, C, D, E, A, F1, K1, W );
		    ROUNDn( 5, A, B, C, D, E, F1, K1, W );
		    ROUNDn( 6, E, A, B, C, D, F1, K1, W );
			case 32:
		    ROUNDn( 7, D, E, A, B, C, F1, K1, W );
		    ROUNDn( 8, C, D, E, A, B, F1, K1, W );
		    ROUNDn( 9, B, C, D, E, A, F1, K1, W );
		    ROUNDn(10, A, B, C, D, E, F1, K1, W );
		    ROUNDn(11, E, A, B, C, D, F1, K1, W );
			case 52:
		    ROUNDn(12, D, E, A, B, C, F1, K1, W );
		    ROUNDn(13, C, D, E, A, B, F1, K1, W );
		    ROUNDn(14, B, C, D, E, A, F1, K1, W );
		}

		ROUND5n(15, F1, (K1) );

		ROUND5n(20, F2, (K2) );
		ROUND5n(25, F2, (K2) );
		ROUND5n(30, F2, (K2) );
		ROUND5n(35, F2, (K2) );

		ROUND5n(40, F3, (K3) );
		ROUND5n(45, F3, (K3) );
		ROUND5n(50, F3, (K3) );
		ROUND5n(55, F3, (K3) );
	
		ROUND5n(60, F4, (K4) );
		ROUND5n(65, F4, (K4) );
		ROUND5n(70, F4, (K4) );
		ROUND5n(75, F4, (K4) );
		
		/* Mix in the IV again */
		A1 += H[0];
		B1 += H[1];

		A2 += H[0];
		B2 += H[1];
		
		/* Debugging! */
		if(0 && iters==0) {
			for(t=0; t < 80; t++) {
				X1 = (unsigned char*) (W1+t);
				X2 = (unsigned char*) (W2+t);
				printf("%2X %2X %2X %2X | %2X %2X %2X %2X\n",
					X1 [ 0 ] , X1 [ 1 ] , X1 [ 2 ] , X1 [ 3 ] , 
					X2 [ 0 ] , X2 [ 1 ] , X2 [ 2 ] , X2 [ 3 ] ) ;
			}
			
			X1 = (unsigned char*) W1;
			X2 = (unsigned char*) W2;
		}
		
		/* Quickly check which pipe contains the best result */
		if(A1 < A2)
			n = 0;
		else if(A1 > A2)
			n = 1;
		else if(B1 < B2)
			n = 0;
		else
			n = 1;
		
		switch(n) {
			case 0:
				A = A1;
				B = B1;
				W = W1;
				break;
			
			case 1:
				A = A2;
				B = B2;
				W = W2;
				break;
		}
		
		/* Is this the best bit count so far? */
		if(!(A & bitMask1Low) && !(B & bitMask1High)) {
			/* Count bits */
			gotBits = 0;
			if(A) {
				s = A;
				while(!(s & 0x80000000)) {
					s <<= 1;
					gotBits++;
				}
			} else {
				gotBits = 32;
				if(B) {
					s = B;
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

			/* Copy this result back to the block buffer */
			for(t=0; t < 16; t++)
				PUT_WORD(output + t*4, W[t]);
			
			/* Is it good enough to bail out? */
			if(gotBits >= bits) {
				return iters+2;
			}
		}
		
		if(0) return 0;
		MINTER_CALLBACK();
	}
	
	return iters+2;
}
