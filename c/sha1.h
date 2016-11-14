/* -*- Mode: C; c-file-style: "stroustrup" -*- */

#if !defined( _sha1_h )
#define _sha1_h

#if defined(WIN32)
#include <windows.h>
#endif

/* for size_t */
#include <string.h>
#include "types.h"

#if defined( __cplusplus )
extern "C" {
#endif

#define SHA1_INPUT_BYTES 64	/* 512 bits */
#define SHA1_INPUT_WORDS ( SHA1_INPUT_BYTES >> 2 )
#define SHA1_DIGEST_WORDS 5	/* 160 bits */
#define SHA1_DIGEST_BYTES ( SHA1_DIGEST_WORDS * 4 )

#if defined( OPENSSL )

#include <openssl/sha.h>
#define SHA1_ctx SHA_CTX
#define SHA1_Final( ctx, digest ) SHA1_Final( digest, ctx )
#undef SHA1_DIGEST_BYTES
#define SHA1_DIGEST_BYTES SHA_DIGEST_LENGTH

#define SHA1_Init_With_IV( ctx, iv )		\
    do {					\
        (ctx)->h0 = iv[0];			\
        (ctx)->h1 = iv[1];			\
        (ctx)->h2 = iv[2];			\
        (ctx)->h3 = iv[3];			\
        (ctx)->h4 = iv[4];			\
        (ctx)->Nh = 0;				\
        (ctx)->Nl = 0;				\
        (ctx)->num = 0l				\
    } while (0)

#define SHA1_Transform( iv, data ) SHA1_Xform( iv, data )

#else

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

#endif

void SHA1_Xform( word32[ SHA1_DIGEST_WORDS ], 
		 const byte[ SHA1_INPUT_BYTES ] );

#if defined( __cplusplus )
}
#endif

#endif
