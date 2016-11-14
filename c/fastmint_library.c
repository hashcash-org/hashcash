#include "sha1.h"
#include "libfastmint.h"

int minter_library_test(void)
{
	/* This minter runs on any hardware */
	return 1;
}

unsigned long minter_library(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS)
{
        MINTER_CALLBACK_VARS;
	unsigned long iters = 0;
	int t = 0, gotBits = 0, maxBits = (bits > 16) ? 16 : bits ;
	uInt32 bitMask1Low = 0 , bitMask1High = 0 , s = 0 ;
	const char *p = encodeAlphabets[EncodeBase64];
	uInt32 A = 0, B = 0;
	int addressMask = 0 ;
	static const int endTest = 3;
	unsigned char *output = (unsigned char*) block;
	unsigned char Xa[ SHA1_INPUT_BYTES*2 ]; /* worst case */
	unsigned char* X = Xa;
	uInt32 *W = (uInt32*)Xa;
	uInt32 H[ SHA1_DIGEST_WORDS ];
	int blocks = (tailIndex+1+8) > SHA1_INPUT_BYTES ? 2 : 1;

	*best = 0;

	/* Work out whether we need to swap bytes during encoding */
	addressMask = ( *(char*)&endTest );

	for(t=0; t < 16*blocks; t++) { W[t] = GET_WORD(output + t*4); }

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

		memcpy( H, IV, SHA1_DIGEST_BYTES );
		SHA1_Transform( H, X );
		if ( blocks == 2 ) {
		  SHA1_Transform( H, X+SHA1_INPUT_BYTES );
		}

		A = H[0];
		B = H[1];

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
			
			*best=gotBits;
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
			for(t=0; t < 16*blocks; t++) { 
				PUT_WORD(output + t*4, W[t]); 
			}

			/* Is it good enough to bail out? */
			if(gotBits >= bits) {
				return iters+1;
			}
		}
		MINTER_CALLBACK();
	}
	
	return iters+1;
}
