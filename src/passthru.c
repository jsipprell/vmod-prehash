
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
#include "vbm.h"

#include "vdir.h"
#include "vcc_prehash_if.h"
#include "prehash.h"

struct vmod_prehash_passthru {
  unsigned magic;
#define VMOD_PREHASH_PASSTHRU_MAGIC 0x99aa682f
  const struct director *be;
};

VCL_BACKEND __match_proto__(td_prehash_passthru_backend)
vmod_passthru_backend(VRT_CTX, struct vmod_prehash_passthru *p)
{
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(p, VMOD_PREHASH_PASSTHRU_MAGIC);
  CHECK_OBJ_NOTNULL(p->be, DIRECTOR_MAGIC);
  return p->be;
}

VCL_VOID __match_proto__(td_prehash_passthru__init)
vmod_passthru__init(VRT_CTX, struct vmod_prehash_passthru **pp, const char *vcl_name, VCL_BACKEND be)
{
  struct vmod_prehash_passthru *p;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  (void)vcl_name;
  AN(pp);
  AZ(*pp);
  ALLOC_OBJ(p, VMOD_PREHASH_PASSTHRU_MAGIC);
  AN(p);
  *pp = p;

  CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
  p->be = be;
}

VCL_VOID __match_proto__(td_prehash_passthru__fini)
vmod_passthru__fini(struct vmod_prehash_passthru **pp)
{
  struct vmod_prehash_passthru *p;

  AN(pp);
  CHECK_OBJ_NOTNULL(*pp, VMOD_PREHASH_PASSTHRU_MAGIC);
  p = *pp;
  *pp = NULL;
  FREE_OBJ(p);
}

