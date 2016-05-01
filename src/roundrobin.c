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
prehash_rr_resolve(const struct director *dir,
                       struct worker *wrk,
                       struct busyobj *bo)
{
  struct vmod_prehash_lastresort_director *rr;
  VCL_BACKEND be = NULL;
  unsigned u;

  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CHECK_OBJ_NOTNULL(wrk, WORKER_MAGIC);
  CHECK_OBJ_NOTNULL(bo, BUSYOBJ_MAGIC);

  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_LASTRESORT_MAGIC);
  vdir_wrlock(rr->vd);
  for (u = 0; u < rr->vd->n_backend; u++)  {
    rr->nxt %= rr->vd->n_backend;
    be = rr->vd->backend[rr->nxt];
    rr->nxt++;
    CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
    if (be->healthy(be, bo, NULL))
      break;
  }
  vdir_unlock(rr->vd);
  if (u == rr->vd->n_backend)
    be = NULL;
  return (be);
}

