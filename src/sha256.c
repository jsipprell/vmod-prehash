#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>

#include "vcl.h"
#include "cache/cache.h"
#include "cache/cache_director.h"

#include "vrt.h"
#include "vend.h"
#include "vsha256.h"

#include "vdir.h"
#include "vcc_prehash_if.h"
#include "prehash.h"

static const unsigned char PAD[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

extern void sha256update(struct SHA256Context *ctx, unsigned char digest[32]);

void sha256update(struct SHA256Context *ctx, unsigned char digest[32])
{
    unsigned char len[8];
    uint32_t r, plen;

    vbe64enc(len, ctx->count << 3);
    r = ctx->count & 0x3f;
    plen = (r < 64) ? (64 - r) : (128 - r);
    if (plen > 0)
      SHA256_Update(ctx, PAD, (size_t)plen);
    memcpy(digest, ctx->state, sizeof digest);
}


