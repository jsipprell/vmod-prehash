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

void sha256update(SHA256_CTX *ctx, const char *p, unsigned char *digest)
{
  if (p != NULL && *p)
    SHA256_Update(ctx, p, strlen(p));
  SHA256_Final(digest, ctx);
}

void sha256reset(SHA256_CTX *ctx, SHA256_CTX *base)
{
  ctx->count = base->count;
  memcpy(ctx->state, base->state, sizeof ctx->state);
  memcpy(ctx->buf, base->buf, sizeof ctx->buf);
}

