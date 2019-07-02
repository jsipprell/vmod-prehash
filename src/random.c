#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>

#include "cache/cache.h"

#include "vcl.h"
#include "vend.h"
#include "vsha256.h"

#include "vdir.h"
#include "vcc_prehash_if.h"
#include "prehash.h"

VCL_BACKEND v_matchproto_(vdi_resolve_f)
prehash_random_resolve(VRT_CTX, VCL_BACKEND dir)
{
  struct vmod_prehash_director *rr;
  VCL_BACKEND be;
  double r = 0;

  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);

  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);

  if (!vdir_any_healthy(ctx, rr->vd, NULL)) {
    be = rr->lrvd->dir;
  } else {
    for (r = scalbn(random(), -31); r == 0; r = scalbn(random(), -31)) ;
    assert(r > 0 && r < 1.0);
    be = vdir_pick_be(ctx, rr->vd, r);
  }
  return (be);
}

