#include "libfastmint.h"

int minter_ansi_standard_1_test(void)
{
	/* This minter runs on any hardware */
	return 1;
}

#define F1( B, C, D ) ( ( (B) & (C) ) | ( ~(B) & (D) ) )
/* #define F1( B, C, D ) ( (D) ^ ( (B) & ( (C) ^ (D) ) ) ) */
#define F2( B, C, D ) ( (B) ^ (C) ^ (D) )
/* #define F3( B, C, D ) ( (B) & (C) ) | ( (C) & (D) ) | ( (B) & (D) ) */
#define F3( B, C, D ) ( ( (B) & ( (C) | (D) )) | ( (C) & (D) ) )
#define F4( B, C, D ) ( (B) ^ (C) ^ (D) )

#define K1 0x5A827999  /* constant used for rounds 0..19 */
#define K2 0x6ED9EBA1  /* constant used for rounds 20..39 */
#define K3 0x8F1BBCDC  /* constant used for rounds 40..59 */
#define K4 0xCA62C1D6  /* constant used for rounds 60..79 */

#define S(n, X) ( ( (X) << (n) ) | ( (X) >> ( 32 - (n) ) ) )
#define Wf(t) (W[t] = S(1, W[t-16] ^ W[t-14] ^ W[t-8] ^ W[t-3]))
#define Wfly(t) ( (t) < 16 ? W[t] : Wf( (t) ) )

#define ROUNDu(t,A,B,C,D,E,Func,K) \
	E += S(5,A) + Func(B,C,D) + Wfly(t) + K; \
	B = S(30,B);

#define ROUNDn(t,A,B,C,D,E,Func,K) \
	E += S(5,A) + Func(B,C,D) + W[t] + K; \
	B = S(30,B);
	
#define ROUND(t,A,B,C,D,E,Func,K) ROUNDu(t,A,B,C,D,E,Func,K)

#define ROUND5( t, Func, K ) \
    ROUND( t + 0, A, B, C, D, E, Func, K );\
    ROUND( t + 1, E, A, B, C, D, Func, K );\
    ROUND( t + 2, D, E, A, B, C, Func, K );\
    ROUND( t + 3, C, D, E, A, B, Func, K );\
    ROUND( t + 4, B, C, D, E, A, Func, K )

#define ROUND20( t, Func, K )\
    ROUND5( t +  0, Func, K );\
    ROUND5( t +  5, Func, K );\
    ROUND5( t + 10, Func, K );\
    ROUND5( t + 15, Func, K )

unsigned long minter_ansi_standard_1(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS)
{
        MINTER_CALLBACK_VARS;
	unsigned long iters = 0;
	int t = 0, gotBits = 0, maxBits = (bits > 16) ? 16 : bits ;
	uInt32 bitMask1Low = 0 , bitMask1High = 0 , s = 0 ;
	uInt32 A = 0 , B = 0 , C = 0 , D = 0 , E = 0 ;
	uInt32 W[80] = {0};
	uInt32 H[5] = {0}, pH[5] = {0};
	const char *p = encodeAlphabets[EncodeBase64];
	unsigned char *X = (unsigned char*) W;
	int addressMask = 0 ;
	static const int endTest = 3;
	unsigned char *output = (unsigned char*) block;
	
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
		W[t] = GET_WORD(output + t*4);
	for(t=0; t < 5; t++)
		pH[t] = H[t] = IV[t];
	
	/* The Tight Loop - everything in here should be extra efficient */
	for(iters=0; iters < maxIter; iters++) {
		/* Encode iteration count into tail */
		X[(tailIndex - 1) ^ addressMask] = p[(iters      ) & 0x3f];
    		if(!(iters & 0x3f)) {
		  	if ( iters >> 6 ) {
				X[(tailIndex - 2) ^ addressMask] = p[(iters >>  6) & 0x3f];
			}
			if ( iters >> 12 ) { 
				X[(tailIndex - 3) ^ addressMask] = p[(iters >> 12) & 0x3f];
			}
			if ( iters >> 18 ) {
				X[(tailIndex - 4) ^ addressMask] = p[(iters >> 18) & 0x3f];
			}
			if ( iters >> 24 ) { 
				X[(tailIndex - 5) ^ addressMask] = p[(iters >> 24) & 0x3f];
			}
			if ( iters >> 30 ) {
				X[(tailIndex - 6) ^ addressMask] = p[(iters >> 30) & 0x3f];
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
				Wf(t);
			
	    ROUND( 0, A, B, C, D, E, F1, K1 );
	    ROUND( 1, E, A, B, C, D, F1, K1 );
	    ROUND( 2, D, E, A, B, C, F1, K1 );
	    ROUND( 3, C, D, E, A, B, F1, K1 );
	    ROUND( 4, B, C, D, E, A, F1, K1 );
	    ROUND( 5, A, B, C, D, E, F1, K1 );
	    ROUND( 6, E, A, B, C, D, F1, K1 );
			
			if(tailIndex == 52) {
		    ROUND( 7, D, E, A, B, C, F1, K1 );
		    ROUND( 8, C, D, E, A, B, F1, K1 );
		    ROUND( 9, B, C, D, E, A, F1, K1 );
		    ROUND(10, A, B, C, D, E, F1, K1 );
		    ROUND(11, E, A, B, C, D, F1, K1 );
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
		    ROUND( 0, A, B, C, D, E, F1, K1 );
		    ROUND( 1, E, A, B, C, D, F1, K1 );
		    ROUND( 2, D, E, A, B, C, F1, K1 );
		    ROUND( 3, C, D, E, A, B, F1, K1 );
		    ROUND( 4, B, C, D, E, A, F1, K1 );
		    ROUND( 5, A, B, C, D, E, F1, K1 );
		    ROUND( 6, E, A, B, C, D, F1, K1 );
			case 32:
		    ROUND( 7, D, E, A, B, C, F1, K1 );
		    ROUND( 8, C, D, E, A, B, F1, K1 );
		    ROUND( 9, B, C, D, E, A, F1, K1 );
		    ROUND(10, A, B, C, D, E, F1, K1 );
		    ROUND(11, E, A, B, C, D, F1, K1 );
			case 52:
		    ROUND(12, D, E, A, B, C, F1, K1 );
		    ROUND(13, C, D, E, A, B, F1, K1 );
		    ROUND(14, B, C, D, E, A, F1, K1 );
		    ROUND(15, A, B, C, D, E, F1, K1 );
		}

		if(tailIndex == 52) {
	    ROUNDn(16, E, A, B, C, D, F1, K1 );
	    ROUNDn(17, D, E, A, B, C, F1, K1 );
	    ROUNDn(18, C, D, E, A, B, F1, K1 );
	    ROUNDn(19, B, C, D, E, A, F1, K1 );
	    ROUNDu(20, A, B, C, D, E, F2, K2 );
	    ROUNDn(21, E, A, B, C, D, F2, K2 );
	    ROUNDn(22, D, E, A, B, C, F2, K2 );
	    ROUNDu(23, C, D, E, A, B, F2, K2 );
	    ROUNDn(24, B, C, D, E, A, F2, K2 );
	    ROUNDn(25, A, B, C, D, E, F2, K2 );
	    ROUNDu(26, E, A, B, C, D, F2, K2 );
	    ROUNDn(27, D, E, A, B, C, F2, K2 );
	    ROUNDu(28, C, D, E, A, B, F2, K2 );
	    ROUNDu(29, B, C, D, E, A, F2, K2 );
	    ROUNDn(30, A, B, C, D, E, F2, K2 );
		} else if (tailIndex == 32) {
	    ROUNDn(16, E, A, B, C, D, F1, K1 );
	    ROUNDn(17, D, E, A, B, C, F1, K1 );
	    ROUNDn(18, C, D, E, A, B, F1, K1 );
	    ROUNDn(19, B, C, D, E, A, F1, K1 );
	    ROUNDn(20, A, B, C, D, E, F2, K2 );
	    ROUNDu(21, E, A, B, C, D, F2, K2 );
	    ROUNDn(22, D, E, A, B, C, F2, K2 );
	    ROUNDu(23, C, D, E, A, B, F2, K2 );
	    ROUNDu(24, B, C, D, E, A, F2, K2 );
	    ROUNDn(25, A, B, C, D, E, F2, K2 );
	    ROUNDu(26, E, A, B, C, D, F2, K2 );
	    ROUNDu(27, D, E, A, B, C, F2, K2 );
	    ROUNDn(28, C, D, E, A, B, F2, K2 );
	    ROUNDu(29, B, C, D, E, A, F2, K2 );
	    ROUNDu(30, A, B, C, D, E, F2, K2 );
		} else {
	    ROUNDu(16, E, A, B, C, D, F1, K1 );
	    ROUNDu(17, D, E, A, B, C, F1, K1 );
	    ROUNDu(18, C, D, E, A, B, F1, K1 );
	    ROUNDu(19, B, C, D, E, A, F1, K1 );
	    ROUNDu(20, A, B, C, D, E, F2, K2 );
	    ROUNDu(21, E, A, B, C, D, F2, K2 );
	    ROUNDu(22, D, E, A, B, C, F2, K2 );
	    ROUNDu(23, C, D, E, A, B, F2, K2 );
	    ROUNDu(24, B, C, D, E, A, F2, K2 );
	    ROUNDu(25, A, B, C, D, E, F2, K2 );
	    ROUNDu(26, E, A, B, C, D, F2, K2 );
	    ROUNDu(27, D, E, A, B, C, F2, K2 );
	    ROUNDu(28, C, D, E, A, B, F2, K2 );
	    ROUNDu(29, B, C, D, E, A, F2, K2 );
	    ROUNDu(30, A, B, C, D, E, F2, K2 );
	  }
	  
    ROUNDu(31, E, A, B, C, D, F2, K2 );
    ROUNDu(32, D, E, A, B, C, F2, K2 );
    ROUNDu(33, C, D, E, A, B, F2, K2 );
    ROUNDu(34, B, C, D, E, A, F2, K2 );
    ROUNDu(35, A, B, C, D, E, F2, K2 );
    ROUNDu(36, E, A, B, C, D, F2, K2 );
    ROUNDu(37, D, E, A, B, C, F2, K2 );
    ROUNDu(38, C, D, E, A, B, F2, K2 );
    ROUNDu(39, B, C, D, E, A, F2, K2 );
		
		ROUND20(40,F3,K3);
		ROUND20(60,F4,K4);
		
		/* Mix in the IV again */
		A += H[0];
		B += H[1];
		C += H[2];
		D += H[3];
		E += H[4];
		
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
				return iters+1;
			}
		}
		MINTER_CALLBACK();
	}
	
	return iters+1;
}
