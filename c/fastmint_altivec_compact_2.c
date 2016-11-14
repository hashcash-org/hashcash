#include <stdio.h>
#include "libfastmint.h"

int minter_altivec_compact_2_test(void)
{
	/* This minter runs only on PowerPC G4 and higher hardware */
#if defined(__POWERPC__) && defined(__ALTIVEC__) && defined(__GNUC__)
	return (gProcessorSupportFlags & HC_CPU_SUPPORTS_ALTIVEC) != 0;
#endif
	
	/* Not a PowerPC, or compiler doesn't support Altivec or GNU assembly */
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
#define F1( B, C, D, F, G ) ( \
	F = AND(B,C), \
	G = ANDNOT(D,B), \
	OR(F,G) )
/* #define F2( B, C, D ) ( (B) ^ (C) ^ (D) ) */
#define F2( B, C, D, F, G ) ( \
	F = XOR(B,C), \
	XOR(F,D) )
/* #define F3( B, C, D ) ( (B) & (C) ) | ( (C) & (D) ) | ( (B) & (D) ) */
/* #define F3( B, C, D ) ( ( (B) & ( (C) | (D) )) | ( (C) & (D) ) ) */
#define F3( B, C, D, F, G ) ( \
	F = OR(C,D), \
	G = AND(C,D), \
	F = AND(B,F), \
	OR(F,G) )
/* #define F4( B, C, D ) ( (B) ^ (C) ^ (D) ) */
#define F4(B,C,D,F,G) F2(B,C,D,F,G)

#define K1 0x5A827999  /* constant used for rounds 0..19 */
#define K2 0x6ED9EBA1  /* constant used for rounds 20..39 */
#define K3 0x8F1BBCDC  /* constant used for rounds 40..59 */
#define K4 0xCA62C1D6  /* constant used for rounds 60..79 */

/* #define Wf(t) (W[t] = S(1, W[t-16] ^ W[t-14] ^ W[t-8] ^ W[t-3])) */
#define Wf(W,t,F,G) ( \
	F = XOR((W)[t-16], (W)[t-14]), \
	G = XOR((W)[t-8], (W)[t-3]), \
	F = XOR(F,G), \
	(W)[t] = S(1,F) )

#define Wfly(W,t,F,G) ( (t) < 16 ? (W)[t] : Wf(W,t,F,G) )

#define ROUND_F1_n(t,A,B,C,D,E,K,W) \
	asm ( \
		"\n\t vand      v5,  %[b1], %[c1]" /* begin F1(B1,C1,D1) */ \
		"\n\t lvx      v16,  %[w1], %[T]"  /* load W1[t] in advance */ \
		"\n\t vand     v13,  %[b2], %[c2]" /* begin F1(B2,C2,D2) */ \
		"\n\t lvx      v17,  %[w2], %[T]"  /* load W2[t] in advance */ \
		"\n\t vandc     v6,  %[d1], %[b1]" \
		"\n\t vandc    v14,  %[d2], %[b2]" \
		"\n\t vrlw      v7,  %[a1], %[s5]" /* S(5,A1) */ \
		"\n\t vrlw     v15,  %[a2], %[s5]" /* S(5,A2) */ \
		"\n\t vor       v5,    v5,    v6"  /* finish F1(B1,C1,D1) */ \
		"\n\t vor      v13,   v13,   v14"  /* finish F1(B2,C2,D2) */ \
		"\n\t vadduwm   v7,    v7,  %[k]"  /* sum K, S(5,A1) */ \
		"\n\t vadduwm  v15,   v15,  %[k]"  /* sum K, S(5,A2) */ \
		"\n\t vadduwm %[e1], %[e1],  v16"  /* sum W1[t] to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v17"  /* sum W2[t] to E1 */ \
		"\n\t vadduwm   v7,    v7,    v5"  /* sum K, S(5,A1), F1(B1,C1,D1) */ \
		"\n\t vadduwm  v15,   v15,   v13"  /* sum K, S(5,A2), F1(B2,C2,D2) */ \
		"\n\t vrlw    %[b1], %[b1],%[s30]" /* S(30,B1) */ \
		"\n\t vrlw    %[b2], %[b2],%[s30]" /* S(30,B2) */ \
		"\n\t vadduwm %[e1], %[e1],   v7"  /* sum everything to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v15"  /* sum everything to E2 */ \
		: [a1] "+v" (A##1), [b1] "+v" (B##1), [c1] "+v" (C##1), [d1] "+v" (D##1), [e1] "+v" (E##1), \
		  [a2] "+v" (A##2), [b2] "+v" (B##2), [c2] "+v" (C##2), [d2] "+v" (D##2), [e2] "+v" (E##2) \
		: [w1] "r" (W##1), [w2] "r" (W##2), [k] "v" (K), [T] "r" (t), \
		  [s30] "v" ((vector unsigned int) (30)), [s5] "v" ((vector unsigned int) (5)) \
		: "v5", "v6", "v7", "v13", "v14", "v15", "v16", "v17", "r0" \
		);

#define ROUND_F1_u(t,A,B,C,D,E,K,W) \
	asm volatile ( \
		"\n\t vand      v5,  %[b1], %[c1]" /* begin F1(B1,C1,D1) */ \
		"\n\t lvx      v15,  %[w1], %[T]"  /* load W1[t-16] */ \
		"\n\t addi     r20,  %[T],    32"  \
		"\n\t vand     v13,  %[b2], %[c2]" /* begin F1(B2,C2,D2) */ \
		"\n\t lvx      v16,  %[w2], %[T]"  /* load W2[t-16] */ \
		"\n\t addi    %[T],  %[T],   128"  \
		"\n\t lvx       v6,  %[w1],  r20"  /* load W1[t-14] */ \
		"\n\t lvx      v14,  %[w2],  r20"  /* load W2[t-14] */ \
		"\n\t addi     r20,   r20,   176"  \
		"\n\t lvx       v7,  %[w1], %[T]"  /* load W1[t-8] */ \
		"\n\t vxor      v6,    v6,   v15"  \
		"\n\t lvx      v15,  %[w2], %[T]"  /* load W2[t-8] */ \
		"\n\t vxor     v14,   v14,   v16"  \
		"\n\t lvx      v16,  %[w1],  r20"  /* load W1[t-3] */ \
		"\n\t vxor      v7,    v7,    v6"  \
		"\n\t lvx      v17,  %[w2],  r20"  /* load W2[t-3] */ \
		"\n\t vxor     v15,   v15,   v14"  \
		"\n\t vxor     v16,   v16,    v7"  \
		"\n\t vxor     v17,   v17,   v15"  \
		"\n\t vandc     v6,  %[d1], %[b1]" \
		"\n\t vandc    v14,  %[d2], %[b2]" \
		"\n\t addi    %[T],   r20,    48"  \
		"\n\t vrlw     v16,   v16,  %[s1]" /* complete W1[t] */ \
		"\n\t vrlw     v17,   v17,  %[s1]" /* complete W2[t] */ \
		"\n\t vrlw      v7,  %[a1], %[s5]" /* S(5,A1) */ \
		"\n\t vrlw     v15,  %[a2], %[s5]" /* S(5,A2) */ \
		"\n\t stvx     v16,  %[w1], %[T]"  /* store W1[t] */ \
		"\n\t vor       v5,    v5,    v6"  /* finish F1(B1,C1,D1) */ \
		"\n\t stvx     v17,  %[w2], %[T]"  /* store W2[t] */ \
		"\n\t vor      v13,   v13,   v14"  /* finish F1(B2,C2,D2) */ \
		"\n\t vadduwm   v7,    v7,  %[k]"  /* sum K, S(5,A1) */ \
		"\n\t vadduwm  v15,   v15,  %[k]"  /* sum K, S(5,A2) */ \
		"\n\t vadduwm %[e1], %[e1],  v16"  /* sum W1[t] to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v17"  /* sum W2[t] to E1 */ \
		"\n\t vadduwm   v7,    v7,    v5"  /* sum K, S(5,A1), F1(B1,C1,D1) */ \
		"\n\t vadduwm  v15,   v15,   v13"  /* sum K, S(5,A2), F1(B2,C2,D2) */ \
		"\n\t vrlw    %[b1], %[b1],%[s30]" /* S(30,B1) */ \
		"\n\t vrlw    %[b2], %[b2],%[s30]" /* S(30,B2) */ \
		"\n\t vadduwm %[e1], %[e1],   v7"  /* sum everything to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v15"  /* sum everything to E2 */ \
		: [a1] "+v" (A##1), [b1] "+v" (B##1), [c1] "+v" (C##1), [d1] "+v" (D##1), [e1] "+v" (E##1), \
		  [a2] "+v" (A##2), [b2] "+v" (B##2), [c2] "+v" (C##2), [d2] "+v" (D##2), [e2] "+v" (E##2) \
		: [w1] "r" (W##1), [w2] "r" (W##2), [k] "v" (K), [T] "r" (t), \
		  [s30] "v" ((vector unsigned int) (30)), [s5] "v" ((vector unsigned int) (5)), [s1] "v" ((vector unsigned int) (1)) \
		: "v5", "v6", "v7", "v13", "v14", "v15", "v16", "v17", "r20", "r0", "memory" \
		);

#define ROUND_F2_n(t,A,B,C,D,E,K,W) \
	asm ( \
		"\n\t vxor      v5,  %[b1], %[c1]" /* begin F2(B1,C1,D1) */ \
		"\n\t lvx      v16,  %[w1], %[T]"  /* load W1[t] in advance */ \
		"\n\t vxor     v13,  %[b2], %[c2]" /* begin F2(B2,C2,D2) */ \
		"\n\t lvx      v17,  %[w2], %[T]"  /* load W2[t] in advance */ \
		"\n\t vrlw      v7,  %[a1], %[s5]" /* S(5,A1) */ \
		"\n\t vrlw     v15,  %[a2], %[s5]" /* S(5,A2) */ \
		"\n\t vxor      v5,    v5,  %[d1]" /* finish F2(B1,C1,D1) */ \
		"\n\t vxor     v13,   v13,  %[d2]" /* finish F2(B2,C2,D2) */ \
		"\n\t vadduwm   v7,    v7,  %[k]"  /* sum K, S(5,A1) */ \
		"\n\t vadduwm  v15,   v15,  %[k]"  /* sum K, S(5,A2) */ \
		"\n\t vadduwm %[e1], %[e1],  v16"  /* sum W1[t] to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v17"  /* sum W2[t] to E1 */ \
		"\n\t vadduwm   v7,    v7,    v5"  /* sum K, S(5,A1), F1(B1,C1,D1) */ \
		"\n\t vadduwm  v15,   v15,   v13"  /* sum K, S(5,A2), F1(B2,C2,D2) */ \
		"\n\t vrlw    %[b1], %[b1],%[s30]" /* S(30,B1) */ \
		"\n\t vrlw    %[b2], %[b2],%[s30]" /* S(30,B2) */ \
		"\n\t vadduwm %[e1], %[e1],   v7"  /* sum everything to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v15"  /* sum everything to E2 */ \
		: [a1] "+v" (A##1), [b1] "+v" (B##1), [c1] "+v" (C##1), [d1] "+v" (D##1), [e1] "+v" (E##1), \
		  [a2] "+v" (A##2), [b2] "+v" (B##2), [c2] "+v" (C##2), [d2] "+v" (D##2), [e2] "+v" (E##2) \
		: [w1] "r" (W##1), [w2] "r" (W##2), [k] "v" (K), [T] "r" (t), \
		  [s30] "v" ((vector unsigned int) (30)), [s5] "v" ((vector unsigned int) (5)) \
		: "v5", "v6", "v7", "v13", "v14", "v15", "v16", "v17", "r0" \
		);

#define ROUND_F2_u(t,A,B,C,D,E,K,W) \
	asm volatile ( \
		"\n\t vxor      v5,  %[b1], %[c1]" /* begin F2(B1,C1,D1) */ \
		"\n\t lvx      v15,  %[w1], %[T]"  /* load W1[t-16] */ \
		"\n\t addi     r20,  %[T],    32"  \
		"\n\t vxor     v13,  %[b2], %[c2]" /* begin F2(B2,C2,D2) */ \
		"\n\t lvx      v16,  %[w2], %[T]"  /* load W2[t-16] */ \
		"\n\t addi    %[T],  %[T],   128"  \
		"\n\t lvx       v6,  %[w1],  r20"  /* load W1[t-14] */ \
		"\n\t lvx      v14,  %[w2],  r20"  /* load W2[t-14] */ \
		"\n\t addi     r20,   r20,   176"  \
		"\n\t lvx       v7,  %[w1], %[T]"  /* load W1[t-8] */ \
		"\n\t vxor      v6,    v6,   v15"  \
		"\n\t lvx      v15,  %[w2], %[T]"  /* load W2[t-8] */ \
		"\n\t vxor     v14,   v14,   v16"  \
		"\n\t lvx      v16,  %[w1],  r20"  /* load W1[t-3] */ \
		"\n\t vxor      v7,    v7,    v6"  \
		"\n\t lvx      v17,  %[w2],  r20"  /* load W2[t-3] */ \
		"\n\t vxor     v15,   v15,   v14"  \
		"\n\t vxor     v16,   v16,    v7"  \
		"\n\t vxor     v17,   v17,   v15"  \
		"\n\t addi    %[T],   r20,    48"  \
		"\n\t vrlw     v16,   v16,  %[s1]" /* complete W1[t] */ \
		"\n\t vrlw     v17,   v17,  %[s1]" /* complete W2[t] */ \
		"\n\t vrlw      v7,  %[a1], %[s5]" /* S(5,A1) */ \
		"\n\t vrlw     v15,  %[a2], %[s5]" /* S(5,A2) */ \
		"\n\t stvx     v16,  %[w1], %[T]"  /* store W1[t] */ \
		"\n\t vxor      v5,    v5,  %[d1]" /* finish F2(B1,C1,D1) */ \
		"\n\t stvx     v17,  %[w2], %[T]"  /* store W2[t] */ \
		"\n\t vxor     v13,   v13,  %[d2]" /* finish F2(B2,C2,D2) */ \
		"\n\t vadduwm   v7,    v7,  %[k]"  /* sum K, S(5,A1) */ \
		"\n\t vadduwm  v15,   v15,  %[k]"  /* sum K, S(5,A2) */ \
		"\n\t vadduwm %[e1], %[e1],  v16"  /* sum W1[t] to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v17"  /* sum W2[t] to E1 */ \
		"\n\t vadduwm   v7,    v7,    v5"  /* sum K, S(5,A1), F1(B1,C1,D1) */ \
		"\n\t vadduwm  v15,   v15,   v13"  /* sum K, S(5,A2), F1(B2,C2,D2) */ \
		"\n\t vrlw    %[b1], %[b1],%[s30]" /* S(30,B1) */ \
		"\n\t vrlw    %[b2], %[b2],%[s30]" /* S(30,B2) */ \
		"\n\t vadduwm %[e1], %[e1],   v7"  /* sum everything to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v15"  /* sum everything to E2 */ \
		: [a1] "+v" (A##1), [b1] "+v" (B##1), [c1] "+v" (C##1), [d1] "+v" (D##1), [e1] "+v" (E##1), \
		  [a2] "+v" (A##2), [b2] "+v" (B##2), [c2] "+v" (C##2), [d2] "+v" (D##2), [e2] "+v" (E##2) \
		: [w1] "r" (W##1), [w2] "r" (W##2), [k] "v" (K), [T] "r" (t), \
		  [s30] "v" ((vector unsigned int) (30)), [s5] "v" ((vector unsigned int) (5)), [s1] "v" ((vector unsigned int) (1)) \
		: "v5", "v6", "v7", "v13", "v14", "v15", "v16", "v17", "r20", "r0", "memory" \
		);

#define ROUND_F3_n(t,A,B,C,D,E,K,W) \
	asm ( \
		"\n\t vor       v5,  %[d1], %[c1]" /* begin F3(B1,C1,D1) */ \
		"\n\t lvx      v16,  %[w1], %[T]"  /* load W1[t] in advance */ \
		"\n\t vor      v13,  %[d2], %[c2]" /* begin F3(B2,C2,D2) */ \
		"\n\t vand      v6,  %[d1], %[c1]" \
		"\n\t vand     v14,  %[d2], %[c2]" \
		"\n\t lvx      v17,  %[w2], %[T]"  /* load W2[t] in advance */ \
		"\n\t vand      v5,    v5,  %[b1]" \
		"\n\t vand     v13,   v13,  %[b2]" \
		"\n\t vrlw      v7,  %[a1], %[s5]" /* S(5,A1) */ \
		"\n\t vrlw     v15,  %[a2], %[s5]" /* S(5,A2) */ \
		"\n\t vor       v5,    v5,    v6"  /* finish F3(B1,C1,D1) */ \
		"\n\t vor      v13,   v13,   v14"  /* finish F3(B2,C2,D2) */ \
		"\n\t vadduwm   v7,    v7,  %[k]"  /* sum K, S(5,A1) */ \
		"\n\t vadduwm  v15,   v15,  %[k]"  /* sum K, S(5,A2) */ \
		"\n\t vadduwm %[e1], %[e1],  v16"  /* sum W1[t] to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v17"  /* sum W2[t] to E1 */ \
		"\n\t vadduwm   v7,    v7,    v5"  /* sum K, S(5,A1), F1(B1,C1,D1) */ \
		"\n\t vadduwm  v15,   v15,   v13"  /* sum K, S(5,A2), F1(B2,C2,D2) */ \
		"\n\t vrlw    %[b1], %[b1],%[s30]" /* S(30,B1) */ \
		"\n\t vrlw    %[b2], %[b2],%[s30]" /* S(30,B2) */ \
		"\n\t vadduwm %[e1], %[e1],   v7"  /* sum everything to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v15"  /* sum everything to E2 */ \
		: [a1] "+v" (A##1), [b1] "+v" (B##1), [c1] "+v" (C##1), [d1] "+v" (D##1), [e1] "+v" (E##1), \
		  [a2] "+v" (A##2), [b2] "+v" (B##2), [c2] "+v" (C##2), [d2] "+v" (D##2), [e2] "+v" (E##2) \
		: [w1] "r" (W##1), [w2] "r" (W##2), [k] "v" (K), [T] "r" (t), \
		  [s30] "v" ((vector unsigned int) (30)), [s5] "v" ((vector unsigned int) (5)) \
		: "v5", "v6", "v7", "v13", "v14", "v15", "v16", "v17", "r0" \
		);

#define ROUND_F3_u(t,A,B,C,D,E,K,W) \
	asm volatile ( \
		"\n\t vor       v5,  %[d1], %[c1]" /* begin F3(B1,C1,D1) */ \
		"\n\t lvx      v15,  %[w1], %[T]"  /* load W1[t-16] */ \
		"\n\t addi     r20,  %[T],    32"  \
		"\n\t vor      v13,  %[d2], %[c2]" /* begin F3(B2,C2,D2) */ \
		"\n\t lvx      v16,  %[w2], %[T]"  /* load W2[t-16] */ \
		"\n\t addi    %[T],  %[T],   128"  \
		"\n\t lvx       v6,  %[w1],  r20"  /* load W1[t-14] */ \
		"\n\t lvx      v14,  %[w2],  r20"  /* load W2[t-14] */ \
		"\n\t addi     r20,   r20,   176"  \
		"\n\t lvx       v7,  %[w1], %[T]"  /* load W1[t-8] */ \
		"\n\t vxor      v6,    v6,   v15"  \
		"\n\t lvx      v15,  %[w2], %[T]"  /* load W2[t-8] */ \
		"\n\t vxor     v14,   v14,   v16"  \
		"\n\t lvx      v16,  %[w1],  r20"  /* load W1[t-3] */ \
		"\n\t vxor      v7,    v7,    v6"  \
		"\n\t lvx      v17,  %[w2],  r20"  /* load W2[t-3] */ \
		"\n\t vxor     v15,   v15,   v14"  \
		"\n\t vand      v6,  %[d1], %[c1]" \
		"\n\t vand     v14,  %[d2], %[c2]" \
		"\n\t vxor     v16,   v16,    v7"  \
		"\n\t vxor     v17,   v17,   v15"  \
		"\n\t vand      v5,    v5,  %[b1]" \
		"\n\t vand     v13,   v13,  %[b2]" \
		"\n\t addi    %[T],   r20,    48"  \
		"\n\t vrlw     v16,   v16,  %[s1]" /* complete W1[t] */ \
		"\n\t vrlw     v17,   v17,  %[s1]" /* complete W2[t] */ \
		"\n\t vrlw      v7,  %[a1], %[s5]" /* S(5,A1) */ \
		"\n\t vrlw     v15,  %[a2], %[s5]" /* S(5,A2) */ \
		"\n\t stvx     v16,  %[w1], %[T]"  /* store W1[t] */ \
		"\n\t vor       v5,    v5,    v6"  /* finish F3(B1,C1,D1) */ \
		"\n\t stvx     v17,  %[w2], %[T]"  /* store W2[t] */ \
		"\n\t vor      v13,   v13,   v14"  /* finish F3(B2,C2,D2) */ \
		"\n\t vadduwm   v7,    v7,  %[k]"  /* sum K, S(5,A1) */ \
		"\n\t vadduwm  v15,   v15,  %[k]"  /* sum K, S(5,A2) */ \
		"\n\t vadduwm %[e1], %[e1],  v16"  /* sum W1[t] to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v17"  /* sum W2[t] to E1 */ \
		"\n\t vadduwm   v7,    v7,    v5"  /* sum K, S(5,A1), F1(B1,C1,D1) */ \
		"\n\t vadduwm  v15,   v15,   v13"  /* sum K, S(5,A2), F1(B2,C2,D2) */ \
		"\n\t vrlw    %[b1], %[b1],%[s30]" /* S(30,B1) */ \
		"\n\t vrlw    %[b2], %[b2],%[s30]" /* S(30,B2) */ \
		"\n\t vadduwm %[e1], %[e1],   v7"  /* sum everything to E1 */ \
		"\n\t vadduwm %[e2], %[e2],  v15"  /* sum everything to E2 */ \
		: [a1] "+v" (A##1), [b1] "+v" (B##1), [c1] "+v" (C##1), [d1] "+v" (D##1), [e1] "+v" (E##1), \
		  [a2] "+v" (A##2), [b2] "+v" (B##2), [c2] "+v" (C##2), [d2] "+v" (D##2), [e2] "+v" (E##2) \
		: [w1] "r" (W##1), [w2] "r" (W##2), [k] "v" (K), [T] "r" (t), \
		  [s30] "v" ((vector unsigned int) (30)), [s5] "v" ((vector unsigned int) (5)), [s1] "v" ((vector unsigned int) (1)) \
		: "v5", "v6", "v7", "v13", "v14", "v15", "v16", "v17", "r20", "r0", "memory" \
		);

#define ROUND_F4_n ROUND_F2_n
#define ROUND_F4_u ROUND_F2_u

#define ROUNDn(t,A,B,C,D,E,Func,K,W) \
		o = (t) * sizeof(vector unsigned int); \
		ROUND_##Func##_n(o,A,B,C,D,E,K,W);

#define ROUND5( t, Func, K ) \
    ROUNDn( t + 0, A, B, C, D, E, Func, K, W );\
    ROUNDn( t + 1, E, A, B, C, D, Func, K, W );\
    ROUNDn( t + 2, D, E, A, B, C, Func, K, W );\
    ROUNDn( t + 3, C, D, E, A, B, Func, K, W );\
    ROUNDn( t + 4, B, C, D, E, A, Func, K, W );

#if defined(__POWERPC__) && defined(__ALTIVEC__) && defined(__GNUC__)
static void WF(vector unsigned int *W, int tailIndex)
{
	asm volatile (
		"\n\t mfctr r14"
		"\n\t li r10,4"
		"\n\t li r11,0"
		"\n\t li r12,16"
		"\n\t mtctr r10"
		
		/* load entire circular W buffer into registers */
		"\n\t lvx  v0,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx  v1,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t lvx  v2,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx  v3,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t lvx  v4,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx  v5,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t lvx  v6,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx  v7,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t lvx  v8,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx  v9,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t lvx v10,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx v11,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t lvx v12,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx v13,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t lvx v14,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t cmpwi cr0,%[tail],52"
		"\n\t lvx v15,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t cmpwi cr1,%[tail],32"
		"\n\t addi r13,r11,32"
		
		/* special-case for the fast paths */
		"\n\t beq+  cr0,myword12"
		"\n\t bne-  cr1,myloop"
		
		/* for counting in word 7 */
		"\n\t li r10,3"
		"\n\t mtctr r10"
		
		"\n\t lvx  v0,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx  v1,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t lvx  v2,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx  v3,%[w],r12"
		/* "\n\t lvx  v5,%[w],r12"  -- word 21 */
		"\n\t vxor  v5, v5, v7"
		"\n\t addi r12,r12,32"
		"\n\t vxor  v5, v5,v13"
		"\n\t lvx  v4,%[w],r11"
		"\n\t vxor  v5, v5, v2"
		"\n\t addi r11,r11,32"
		"\n\t vrlw  v5, v5, %[s]"
		"\n\t stvx  v5, %[w], r12"
		"\n\t addi r12,r12,32"

		"\n\t lvx  v6,%[w],r11"
		"\n\t addi r11,r11,32"
		/* "\n\t lvx  v7,%[w],r12"  -- word 23 */
		/* "\n\t lvx  v8,%[w],r11"  -- word 24 */
		"\n\t vxor  v7, v7, v9"
		"\n\t vxor  v8, v8,v10"
		"\n\t vxor  v7, v7,v15"
		"\n\t vxor  v8, v8, v0"
		"\n\t vxor  v7, v7, v4"
		"\n\t vxor  v8, v8, v5"
		"\n\t vrlw  v7, v7, %[s]"
		"\n\t vrlw  v8, v8, %[s]"
		"\n\t stvx  v7, %[w], r12"
		"\n\t addi r12,r12,32"
		"\n\t stvx  v8, %[w], r11"
		"\n\t addi r11,r11,32"
		
		"\n\t lvx  v9,%[w],r12"
		"\n\t addi r12,r12,32"
		/* "\n\t lvx v10,%[w],r11"  -- word 26 */
		/* "\n\t lvx v11,%[w],r12"  -- word 27 */
		"\n\t vxor v10,v10,v12"
		"\n\t vxor v11,v11,v13"
		"\n\t vxor v10,v10, v2"
		"\n\t vxor v11,v11, v3"
		"\n\t vxor v10,v10, v7"
		"\n\t vxor v11,v11, v8"
		"\n\t vrlw v10,v10, %[s]"
		"\n\t vrlw v11,v11, %[s]"
		"\n\t stvx v10, %[w], r11"
		"\n\t addi r11,r11,32"
		"\n\t stvx v11, %[w], r12"
		"\n\t addi r12,r12,32"

		"\n\t lvx v12,%[w],r11"
		"\n\t addi r11,r11,32"
		/* "\n\t lvx v13,%[w],r12"  -- word 29 */
		/* "\n\t lvx v14,%[w],r11"  -- word 30 */
		/* "\n\t lvx v15,%[w],r12"  -- word 31 */
		"\n\t vxor v13,v13,v15"
		"\n\t vxor v14,v14, v0"
		"\n\t vxor v15,v15, v1"
		"\n\t vxor v13,v13, v5"
		"\n\t vxor v14,v14, v6"
		"\n\t vxor v15,v15, v7"
		"\n\t vxor v13,v13,v10"
		"\n\t vxor v14,v14,v11"
		"\n\t vxor v15,v15,v12"
		"\n\t vrlw v13,v13, %[s]"
		"\n\t vrlw v14,v14, %[s]"
		"\n\t vrlw v15,v15, %[s]"
		"\n\t stvx v13, %[w], r12"
		"\n\t addi r12,r12,32"
		"\n\t stvx v14, %[w], r11"
		"\n\t addi r11,r11,32"
		"\n\t stvx v15, %[w], r12"
		"\n\t addi r12,r12,32"

		"\n\t addi r13,r11,32"
		"\n\t b myloop"
		
		/* for counting in word 12 */
		"\nmyword12:"
		"\n\t li r10,3"
		"\n\t mtctr r10"
		
		"\n\t lvx  v0,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx  v1,%[w],r12"
		"\n\t addi r12,r12,32"
		"\n\t lvx  v2,%[w],r11"
		"\n\t addi r11,r11,32"
		"\n\t lvx  v3,%[w],r12"
		"\n\t addi r12,r12,32"
		/* "\n\t lvx  v4,%[w],r11" -- word 20 */
		"\n\t vxor  v4, v4, v6"
		"\n\t addi r13,r11,32"
		"\n\t lvx  v5,%[w],r12"
		"\n\t vxor  v4, v4,v12"
		"\n\t lvx  v6,%[w],r13"
		"\n\t addi r12,r12,32"
		"\n\t vxor  v4, v4, v1"
		"\n\t vrlw  v4, v4, %[s]"
		"\n\t stvx  v4, %[w], r11"
		"\n\t addi r11,r13,32"
		/* "\n\t lvx  v7,%[w],r12" -- word 23 */
		"\n\t vxor  v7, v7, v9"
		"\n\t addi r13,r12,32"
		"\n\t lvx  v8,%[w],r11"
		"\n\t vxor  v7, v7,v15"
		"\n\t lvx  v9,%[w],r13"
		"\n\t addi r11,r11,32"
		"\n\t vxor  v7, v7, v4"
		"\n\t vrlw  v7, v7, %[s]"
		"\n\t stvx  v7, %[w], r12"
		"\n\t addi r12,r13,32"
		/* "\n\t lvx v10,%[w],r11" -- word 26 */
		"\n\t vxor v10,v10,v12"
		"\n\t vxor v12,v12,v14"
		"\n\t lvx v11,%[w],r12"
		"\n\t vxor v10,v10, v2"
		"\n\t vxor v12,v12, v4"
		"\n\t addi r12,r12,32"
		"\n\t vxor v10,v10, v7"
		"\n\t vxor v12,v12, v9"
		/* "\n\t lvx v12,%[w],r11" -- word 28 */
		"\n\t addi r13,r11,32"
		"\n\t vrlw v10,v10, %[s]"
		"\n\t vrlw v12,v12, %[s]"
		"\n\t stvx v10, %[w], r11"
		"\n\t addi r11,r13,32"
		"\n\t stvx v12, %[w], r13"
		/* "\n\t lvx v13,%[w],r12" -- word 29 */
		"\n\t vxor v13,v13,v15"
		"\n\t vxor v15,v15, v1"
		"\n\t lvx v14,%[w],r11"
		"\n\t vxor v13,v13, v5"
		"\n\t vxor v15,v15, v7"
		"\n\t addi r11,r11,32"
		"\n\t vxor v13,v13,v10"
		"\n\t vxor v15,v15,v12"
		/* "\n\t lvx v15,%[w],r12" -- word 31 */
		"\n\t addi r13,r12,32"
		"\n\t vrlw v13,v13, %[s]"
		"\n\t vrlw v15,v15, %[s]"
		"\n\t stvx v13, %[w], r12"
		"\n\t addi r12,r13,32"
		"\n\t stvx v15, %[w], r13"

		"\n\t addi r13,r11,32"
		
		/* spew out the rest of the W buffer, in three or four passes */
		"\nmyloop:"
		
		"\n\t vxor  v0, v0, v2"
		"\n\t vxor  v1, v1, v3"
		"\n\t vxor  v2, v2, v4"
		"\n\t vxor  v0, v0, v8"
		"\n\t vxor  v1, v1, v9"
		"\n\t vxor  v2, v2,v10"
		"\n\t vxor  v0, v0,v13"
		"\n\t vxor  v1, v1,v14"
		"\n\t vxor  v2, v2,v15"
		"\n\t vrlw  v0, v0, %[s]"
		"\n\t vrlw  v1, v1, %[s]"
		"\n\t vrlw  v2, v2, %[s]"
		"\n\t stvx  v0, %[w], r11"
		"\n\t addi  r11, r11, 48"
		"\n\t stvx  v1, %[w], r12"
		"\n\t addi  r12, r12, 48"
		"\n\t stvx  v2, %[w], r13"
		"\n\t addi  r13, r13, 48"
		
		"\n\t vxor  v3, v3, v5"
		"\n\t vxor  v4, v4, v6"
		"\n\t vxor  v5, v5, v7"
		"\n\t vxor  v3, v3,v11"
		"\n\t vxor  v4, v4,v12"
		"\n\t vxor  v5, v5,v13"
		"\n\t vxor  v3, v3, v0"
		"\n\t vxor  v4, v4, v1"
		"\n\t vxor  v5, v5, v2"
		"\n\t vrlw  v3, v3, %[s]"
		"\n\t vrlw  v4, v4, %[s]"
		"\n\t vrlw  v5, v5, %[s]"
		"\n\t stvx  v3, %[w], r11"
		"\n\t addi  r11, r11, 48"
		"\n\t stvx  v4, %[w], r12"
		"\n\t addi  r12, r12, 48"
		"\n\t stvx  v5, %[w], r13"
		"\n\t addi  r13, r13, 48"
		
		"\n\t vxor  v6, v6, v8"
		"\n\t vxor  v7, v7, v9"
		"\n\t vxor  v8, v8,v10"
		"\n\t vxor  v6, v6,v14"
		"\n\t vxor  v7, v7,v15"
		"\n\t vxor  v8, v8, v0"
		"\n\t vxor  v6, v6, v3"
		"\n\t vxor  v7, v7, v4"
		"\n\t vxor  v8, v8, v5"
		"\n\t vrlw  v6, v6, %[s]"
		"\n\t vrlw  v7, v7, %[s]"
		"\n\t vrlw  v8, v8, %[s]"
		"\n\t stvx  v6, %[w], r11"
		"\n\t addi  r11, r11, 48"
		"\n\t stvx  v7, %[w], r12"
		"\n\t addi  r12, r12, 48"
		"\n\t stvx  v8, %[w], r13"
		"\n\t addi  r13, r13, 48"
		
		"\n\t vxor  v9, v9,v11"
		"\n\t vxor v10,v10,v12"
		"\n\t vxor v11,v11,v13"
		"\n\t vxor  v9, v9, v1"
		"\n\t vxor v10,v10, v2"
		"\n\t vxor v11,v11, v3"
		"\n\t vxor  v9, v9, v6"
		"\n\t vxor v10,v10, v7"
		"\n\t vxor v11,v11, v8"
		"\n\t vrlw  v9, v9, %[s]"
		"\n\t vrlw v10,v10, %[s]"
		"\n\t vrlw v11,v11, %[s]"
		"\n\t stvx  v9, %[w], r11"
		"\n\t addi  r11, r11, 48"
		"\n\t stvx v10, %[w], r12"
		"\n\t addi  r12, r12, 48"
		"\n\t stvx v11, %[w], r13"
		"\n\t addi  r13, r13, 48"
		
		"\n\t vxor v12,v12,v14"
		"\n\t vxor v13,v13,v15"
		"\n\t vxor v12,v12, v4"
		"\n\t vxor v13,v13, v5"
		"\n\t vxor v12,v12, v9"
		"\n\t vxor v13,v13,v10"
		"\n\t vrlw v12,v12, %[s]"
		"\n\t vrlw v13,v13, %[s]"
		"\n\t stvx v12, %[w], r11"
		"\n\t addi  r11, r11, 32"
		"\n\t stvx v13, %[w], r12"
		"\n\t addi  r12, r12, 32"
		
		"\n\t vxor v14,v14, v0"
		"\n\t vxor v15,v15, v1"
		"\n\t vxor v14,v14, v6"
		"\n\t vxor v15,v15, v7"
		"\n\t vxor v14,v14,v11"
		"\n\t vxor v15,v15,v12"
		"\n\t vrlw v14,v14, %[s]"
		"\n\t vrlw v15,v15, %[s]"
		"\n\t stvx v14, %[w], r11"
		"\n\t addi  r11, r11, 32"
		"\n\t stvx v15, %[w], r12"
		"\n\t addi  r12, r12, 32"
		"\n\t addi  r13, r11, 32"
		
		"\n\t bdnz myloop"
		"\n\t mtctr r14"
	: /* no outputs */
	: [w] "r" (W), [s] "v" ((vector unsigned int)(1)), [tail] "r" (tailIndex)
	: "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
	  "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
	  "r10", "r11", "r12", "r13", "r14", "r0", "memory"
	);
}
#endif

unsigned long minter_altivec_compact_2(int bits, int* best, unsigned char *block, const uInt32 IV[5], int tailIndex, unsigned long maxIter, MINTER_CALLBACK_ARGS)
{
	#if defined(__POWERPC__) && defined(__ALTIVEC__) && defined(__GNUC__)
	MINTER_CALLBACK_VARS;
	unsigned long iters;
	unsigned int m, n, o, t, gotBits = 0, maxBits = (bits > 16) ? 16 : bits;
	uInt32 bitMask1Low, bitMask1High, s;
	vector unsigned int vBitMaskHigh, vBitMaskLow;
	register vector unsigned int A1,B1,C1,D1,E1,Fa,Ga;
	register vector unsigned int A2,B2,C2,D2,E2,Fb,Gb;
	vector unsigned int A,B,N;
	vector unsigned int W1[80], W2[80], *W;
	vector unsigned int H[5], pH[5], K[4] = {
		(vector unsigned int) (K1),
		(vector unsigned int) (K2),
		(vector unsigned int) (K3),
		(vector unsigned int) (K4) };
	vector bool int M;
	uInt32 *Hw = (uInt32*) H;
	const char *p = encodeAlphabets[EncodeBase64];
	unsigned char *X1 = (unsigned char*) W1;
	unsigned char *X2 = (unsigned char*) W2;
	unsigned char *output = (unsigned char*) block, *X;
	
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
		X1[t*16+ 0] = X1[t*16+ 4] = X1[t*16+ 8] = X1[t*16+12] = output[t*4+0];
		X1[t*16+ 1] = X1[t*16+ 5] = X1[t*16+ 9] = X1[t*16+13] = output[t*4+1];
		X1[t*16+ 2] = X1[t*16+ 6] = X1[t*16+10] = X1[t*16+14] = output[t*4+2];
		X1[t*16+ 3] = X1[t*16+ 7] = X1[t*16+11] = X1[t*16+15] = output[t*4+3];

		X2[t*16+ 0] = X2[t*16+ 4] = X2[t*16+ 8] = X2[t*16+12] = output[t*4+0];
		X2[t*16+ 1] = X2[t*16+ 5] = X2[t*16+ 9] = X2[t*16+13] = output[t*4+1];
		X2[t*16+ 2] = X2[t*16+ 6] = X2[t*16+10] = X2[t*16+14] = output[t*4+2];
		X2[t*16+ 3] = X2[t*16+ 7] = X2[t*16+11] = X2[t*16+15] = output[t*4+3];
	}
	for(t=0; t < 5; t++) {
		Hw[t*4+0] = Hw[t*4+1] = Hw[t*4+2] = Hw[t*4+3] = IV[t];
		pH[t] = H[t];
	}
	
	/* The Tight Loop - everything in here should be extra efficient */
	for(iters=0; iters < maxIter-8; iters += 8) {
		/* Encode iteration count into tail */
		/* Iteration count is always 8-aligned, so only least-significant character needs multiple lookup */
		/* Further, we assume we're always big-endian */
		X1[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) +  0] = p[(iters & 0x38) + 0];
		X2[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) +  0] = p[(iters & 0x38) + 1];
		X1[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) +  4] = p[(iters & 0x38) + 2];
		X2[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) +  4] = p[(iters & 0x38) + 3];
		X1[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) +  8] = p[(iters & 0x38) + 4];
		X2[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) +  8] = p[(iters & 0x38) + 5];
		X1[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) + 12] = p[(iters & 0x38) + 6];
		X2[(((tailIndex - 1) & ~3) << 2) + ((tailIndex - 1) & 3) + 12] = p[(iters & 0x38) + 7];
		
		if(!(iters & 0x3f)) {
			if ( iters >> 6 ) {
				X1[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) +  0] =
				X1[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) +  4] =
				X1[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) +  8] =
				X1[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) + 12] =
				X2[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) +  0] =
				X2[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) +  4] =
				X2[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) +  8] =
				X2[(((tailIndex - 2) & ~3) << 2) + ((tailIndex - 2) & 3) + 12] = p[(iters >>  6) & 0x3f];
			}
			if ( iters >> 12 ) { 
				X1[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) +  0] =
				X1[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) +  4] =
				X1[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) +  8] =
				X1[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) + 12] =
				X2[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) +  0] =
				X2[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) +  4] =
				X2[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) +  8] =
				X2[(((tailIndex - 3) & ~3) << 2) + ((tailIndex - 3) & 3) + 12] = p[(iters >> 12) & 0x3f];
			}
			if ( iters >> 18 ) {
				X1[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) +  0] =
				X1[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) +  4] =
				X1[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) +  8] =
				X1[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) + 12] =
				X2[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) +  0] =
				X2[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) +  4] =
				X2[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) +  8] =
				X2[(((tailIndex - 4) & ~3) << 2) + ((tailIndex - 4) & 3) + 12] = p[(iters >> 18) & 0x3f];
			}
			if ( iters >> 24 ) {
				X1[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) +  0] =
				X1[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) +  4] =
				X1[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) +  8] =
				X1[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) + 12] =
				X2[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) +  0] =
				X2[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) +  4] =
				X2[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) +  8] =
				X2[(((tailIndex - 5) & ~3) << 2) + ((tailIndex - 5) & 3) + 12] = p[(iters >> 24) & 0x3f];
			}
			if ( iters >> 30 ) {
				X1[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) +  0] =
				X1[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) +  4] =
				X1[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) +  8] =
				X1[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) + 12] =
				X2[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) +  0] =
				X2[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) +  4] =
				X2[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) +  8] =
				X2[(((tailIndex - 6) & ~3) << 2) + ((tailIndex - 6) & 3) + 12] = p[(iters >> 30) & 0x3f];
			}
		}
		
		/* Bypass shortcuts below on certain iterations */
		if((!(iters & 0xffffff)) && (tailIndex == 52 || tailIndex == 32)) {
			WF(W1, 0);
			WF(W2, 0);

			A1 = A2 = H[0];
			B1 = B2 = H[1];
			C1 = C2 = H[2];
			D1 = D2 = H[3];
			E1 = E2 = H[4];
			
	    ROUNDn( 0, A, B, C, D, E, F1, K[0], W );
	    ROUNDn( 1, E, A, B, C, D, F1, K[0], W );
	    ROUNDn( 2, D, E, A, B, C, F1, K[0], W );
	    ROUNDn( 3, C, D, E, A, B, F1, K[0], W );
	    ROUNDn( 4, B, C, D, E, A, F1, K[0], W );
	    ROUNDn( 5, A, B, C, D, E, F1, K[0], W );
	    ROUNDn( 6, E, A, B, C, D, F1, K[0], W );
			
			if(tailIndex == 52) {
		    ROUNDn( 7, D, E, A, B, C, F1, K[0], W );
		    ROUNDn( 8, C, D, E, A, B, F1, K[0], W );
		    ROUNDn( 9, B, C, D, E, A, F1, K[0], W );
		    ROUNDn(10, A, B, C, D, E, F1, K[0], W );
		    ROUNDn(11, E, A, B, C, D, F1, K[0], W );
			}
			
			pH[0] = A1;
			pH[1] = B1;
			pH[2] = C1;
			pH[3] = D1;
			pH[4] = E1;
		}

		/* Populate W buffer */
		WF(W1, tailIndex);
		WF(W2, tailIndex);
		
		/*
		for(t=16; t < 80; t++) {
			if(vec_any_ne(W1[t], W2[t])) {
				printf("\nW buffers differ at round %d!\n", t);
				return 0;
			}
		}
		*/

		/* Set up working variables */
		A1 = A2 = pH[0];
		B1 = B2 = pH[1];
		C1 = C2 = pH[2];
		D1 = D2 = pH[3];
		E1 = E2 = pH[4];
		
		/* Do the rounds */
		switch(tailIndex) {
			default:
		    ROUNDn( 0, A, B, C, D, E, F1, K[0], W );
		    ROUNDn( 1, E, A, B, C, D, F1, K[0], W );
		    ROUNDn( 2, D, E, A, B, C, F1, K[0], W );
		    ROUNDn( 3, C, D, E, A, B, F1, K[0], W );
		    ROUNDn( 4, B, C, D, E, A, F1, K[0], W );
		    ROUNDn( 5, A, B, C, D, E, F1, K[0], W );
		    ROUNDn( 6, E, A, B, C, D, F1, K[0], W );
			case 32:
		    ROUNDn( 7, D, E, A, B, C, F1, K[0], W );
		    ROUNDn( 8, C, D, E, A, B, F1, K[0], W );
		    ROUNDn( 9, B, C, D, E, A, F1, K[0], W );
		    ROUNDn(10, A, B, C, D, E, F1, K[0], W );
		    ROUNDn(11, E, A, B, C, D, F1, K[0], W );
			case 52:
		    ROUNDn(12, D, E, A, B, C, F1, K[0], W );
		    ROUNDn(13, C, D, E, A, B, F1, K[0], W );
		    ROUNDn(14, B, C, D, E, A, F1, K[0], W );
		}
    ROUND5(15, F1, K[0] );
		
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
		
		/*
		for(n=0; n < 4; n++) {
	    ROUND5( 0 + n*5, F1, K[0] );
		}
		
		for(n=0; n < 4; n++) {
	    ROUND5( 0 + n*5, F2, K[1] );
		}
		
		for(n=0; n < 4; n++) {
	    ROUND5( 0 + n*5, F3, K[2] );
		}
		
		for(n=0; n < 4; n++) {
	    ROUND5( 0 + n*5, F4, K[3] );
		}
		*/
		
		/* Mix in the IV again */
		A1 = vec_add(A1, H[0]);
		B1 = vec_add(B1, H[1]);
		A2 = vec_add(A2, H[0]);
		B2 = vec_add(B2, H[1]);
		
		/* Debugging! */
		if(0 && iters==0) {
			for(t=0; t < 80; t++) {
				X1 = (unsigned char*) (W1+t);
				printf("%2X %2X %2X %2X | %2X %2X %2X %2X\n",
					*(X1++), *(X1++), *(X1++), *(X1++),
					*(X1++), *(X1++), *(X1++), *(X1++) );
			}
			
			X1 = (unsigned char*) W1;
		}
		
		/* Quickly extract the best results from each pipe into a single vector set */
		M = vec_sel(vec_cmpgt(A1,A2), vec_cmpgt(B1,B2), vec_cmpeq(A1,A2));
		N = vec_sel((vector unsigned int) (0), (vector unsigned int) (1), M);
		A = vec_sel(A1, A2, M);
		B = vec_sel(B1, B2, M);
		
		/* Is this the best bit count so far? */
		if(vec_any_ne( vec_and( vec_cmpeq(vec_and(A, vBitMaskLow), (vector unsigned int) (0)), vec_cmpeq(vec_and(A, vBitMaskHigh), (vector unsigned int) (0)) ), (vector unsigned int) (0))) {
			uInt32 IA, IB;
			
			/* Go over each vector element in turn */
			for(n=0; n < 4; n++) {
				/* Extract A and B components */
				IA = ((uInt32*) &A)[n];
				IB = ((uInt32*) &B)[n];
				m  = ((uInt32*) &N)[n];
				
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
				if(m)
					X = X2;
				else
					X = X1;
				
				for(t=0; t < 16; t++) {
					output[t*4+0] = X[t*16+0+n*4];
					output[t*4+1] = X[t*16+1+n*4];
					output[t*4+2] = X[t*16+2+n*4];
					output[t*4+3] = X[t*16+3+n*4];
				}
				
				/* Is it good enough to bail out? */
				if(gotBits >= bits) {
					return iters+8;
				}
			}
		}
		if(0) return 0;
		MINTER_CALLBACK();
	}
	
	return iters+8;

	/* For other platforms */
	#else
	return 0;
	#endif
}
