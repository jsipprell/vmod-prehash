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
prehash_rr_resolve(VRT_CTX, VCL_BACKEND dir)
{
  struct vmod_prehash_lastresort_director *rr;
  VCL_BACKEND be = NULL;
  unsigned u;

  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);

  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_LASTRESORT_MAGIC);
  vdir_rdlock(rr->vd);
  for (u = 0; u < rr->vd->n_backend; u++)  {
    be = rr->vd->backend[rr->nxt % rr->vd->n_backend];
    rr->nxt++;
    CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
    if (VRT_Healthy(ctx, be, NULL))
      break;
  }
  if (u == rr->vd->n_backend)
    be = NULL;
  vdir_unlock(rr->vd);
  return (be);
}

