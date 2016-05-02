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



void voverride_new(struct voverride **vop, struct ws *ws,
    const char *name, const char *vcl_name,
    vdi_healthy_f *healthy, vdi_resolve_f *resolve, void *priv)
{
  struct voverride *vo;

  CHECK_OBJ_NOTNULL(ws, WS_MAGIC);

  AN(name);
  AN(vcl_name);
  AN(vop);
  AZ(*vop);
  ALLOC_OBJ(vo, VDIR_OVERRIDE_MAGIC);
  AN(vo);
  *vop = vo;
  AZ(pthread_rwlock_init(&vo->mtx, NULL));

  ALLOC_OBJ(vo->dir, DIRECTOR_MAGIC);
  AN(vo->dir);
  vo->dir->name = name;
  REPLACE(vo->dir->vcl_name, vcl_name);
  vo->dir->priv = priv;
  vo->dir->healthy = healthy;
  vo->dir->resolve = resolve;
  vo->ws = ws;
}

void
voverride_delete(struct voverride **vop)
{
  struct voverride *vo;

  AN(vop);
  vo = *vop;
  *vop = NULL;

  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);

  free(vo->backend);
  free((void*)vo->names);
  free(vo->hashvals);
  AZ(pthread_rwlock_destroy(&vo->mtx));
  free(vo->dir->vcl_name);
  FREE_OBJ(vo->dir);
  FREE_OBJ(vo);
}

void voverride_rdlock(struct voverride *vo)
{
  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);
  AZ(pthread_rwlock_rdlock(&vo->mtx));
}

void voverride_wrlock(struct voverride *vo)
{
  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);
  AZ(pthread_rwlock_wrlock(&vo->mtx));
}

void voverride_unlock(struct voverride *vo)
{
  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);
  AZ(pthread_rwlock_unlock(&vo->mtx));
}

static
void voverride_expand(struct voverride *vo, unsigned sz)
{
  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);

  vo->backend = realloc(vo->backend, sz * sizeof *vo->backend);
  AN(vo->backend);
  vo->names = realloc(vo->names, sz * sizeof *vo->names);
  AN(vo->names);
  vo->hashvals = realloc(vo->hashvals, sz * sizeof *vo->hashvals);
  AN(vo->hashvals);

  vo->l_backend = sz;
}

unsigned voverride_add_backend(struct voverride *vo, VCL_BACKEND be,
                               double hv, const char *name)
{
  unsigned u;
  char *n;

  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);
  voverride_wrlock(vo);
  u = strlen(name);
  n = WS_Alloc(vo->ws, u+1);
  AN(n);
  strcpy(n, name);
  name = n;

  if (vo->n_backend >= vo->l_backend)
    voverride_expand(vo, vo->l_backend + 16);
  assert(vo->n_backend < vo->l_backend);
  u = vo->n_backend++;
  vo->backend[u] = be;
  vo->names[u] = n;
  vo->hashvals[u] = hv;
  voverride_unlock(vo);

  return u;
}

VCL_BACKEND
voverride_get_be(struct voverride *vo, double hv, const struct busyobj *bo, int *healthy)
{
  unsigned u;
  VCL_BACKEND be = NULL;

  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);
  CHECK_OBJ_ORNULL(bo, BUSYOBJ_MAGIC);

  voverride_rdlock(vo);
  for (u = 0; u < vo->n_backend; u++) {
    if (vo->hashvals[u] == hv) {
      be = vo->backend[u];
      if (healthy) {
        *healthy = be->healthy(be, bo, NULL);
      }
      break;
    }
  }
  voverride_unlock(vo);
  return (be);
}

