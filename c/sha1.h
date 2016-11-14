/* -*- Mode: C; c-file-style: "bsd" -*- */

#if !defined( _sha1_h )
#define _sha1_h

#include "types.h"

#if defined( __cplusplus )
extern "C" {
#endif

#define SHA1_INPUT_BYTES 64	/* 512 bits */
#define SHA1_INPUT_WORDS ( SHA1_INPUT_BYTES >> 2 )
#define SHA1_DIGEST_WORDS 5	/* 160 bits */
#define SHA1_DIGEST_BYTES ( SHA1_DIGEST_WORDS * 4 )

typedef struct {
    word32 H[ SHA1_DIGEST_WORDS ];
#if defined( word64 )
    word64 bits;		/* we want a 64 bit word */
#else
    word32 hbits, lbits;	/* if we don't have one we simulate it */
#endif
    byte M[ SHA1_INPUT_BYTES ];
} SHA1_ctx;

void SHA1_Init  ( SHA1_ctx* );
void SHA1_Update( SHA1_ctx*, const void*, size_t );
void SHA1_Final ( SHA1_ctx*, byte[ SHA1_DIGEST_BYTES ] );

/* these provide extra access to internals of SHA1 for MDC and MACs */

void SHA1_Init_With_IV( SHA1_ctx*, const byte[ SHA1_DIGEST_BYTES ] );
void SHA1_Transform( word32[ SHA1_DIGEST_WORDS ], 
		     const byte[ SHA1_INPUT_BYTES ] );

#if defined( __cplusplus )
}
#endif

#endif
