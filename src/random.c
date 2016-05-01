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

const struct director * __match_proto__(vdi_resolve_f)
prehash_random_resolve(const struct director *dir,
                       struct worker *wrk,
                       struct busyobj *bo)
{
  struct vmod_prehash_director *rr;
  VCL_BACKEND be;
  double r = 0;

  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CHECK_OBJ_NOTNULL(wrk, WORKER_MAGIC);
  CHECK_OBJ_NOTNULL(bo, BUSYOBJ_MAGIC);

  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);

  if (!vdir_any_healthy(rr->vd, bo, NULL)) {
    be = rr->lrvd->dir;
  } else {
    for (r = scalbn(random(), -31); r == 0; r = scalbn(random(), -31)) ;
    assert(r > 0 && r < 1.0);
    be = vdir_pick_be(rr->vd, r, bo);
  }
  return (be);
}

