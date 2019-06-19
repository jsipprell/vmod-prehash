#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "cache/cache.h"

#include "vend.h"
#include "vcl.h"
#include "vsha256.h"

#include "vdir.h"
#include "vcc_prehash_if.h"
#include "prehash.h"



void voverride_new(VRT_CTX, struct voverride **vop, struct ws *ws,
    const char *fmt, const char *vcl_name,
    vdi_healthy_f *healthy, vdi_resolve_f *resolve, void *priv)
{
  struct voverride *vo;

  CHECK_OBJ_ORNULL(ws, WS_MAGIC);

  AN(vcl_name);
  AN(vop);
  AZ(*vop);
  ALLOC_OBJ(vo, VDIR_OVERRIDE_MAGIC);
  AN(vo);
  *vop = vo;
  AZ(pthread_rwlock_init(&vo->mtx, NULL));

  ALLOC_OBJ(vo->methods, VDI_METHODS_MAGIC);
  vo->methods->type = "prehash";
  vo->methods->healthy = healthy;
  vo->methods->resolve = resolve;
  if (!ws) {
    unsigned char *s;

    vo->scratch = malloc(4096);
    AN(vo->scratch);
    s = (unsigned char*)PRNDUP(vo->scratch);
    ws = (struct ws*)s;
    s += PRNDUP(sizeof *ws);
    WS_Init(ws, "mii", s, 4096 - (s - vo->scratch));
  }
  vo->ws = ws;
  CHECK_OBJ_NOTNULL(vo->ws, WS_MAGIC);
  vo->dir = VRT_AddDirector(ctx, vo->methods, priv, fmt ? fmt :"%s_override", vcl_name);
}

void
voverride_delete(struct voverride **vop)
{
  unsigned u;
  struct voverride *vo;
  struct vmapping *vm;

  AN(vop);
  vo = *vop;
  *vop = NULL;

  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);

  VRT_DelDirector(&vo->dir);
  if (vo->mapping != NULL) {
    for (u = 0; u < vo->n_backend; u++) {
      vm = vo->mapping[u];
      CHECK_OBJ_ORNULL(vm, VDIR_MAPPING_MAGIC);
      if (vm != NULL) {
        vmapping_delete(&vo->mapping[u]);
      }
    }
  }
  free(vo->mapping);
  free(vo->backend);
  free((void*)vo->names);
  free(vo->hashvals);
  AZ(pthread_rwlock_destroy(&vo->mtx));
  free(vo->methods);
  if (vo->scratch)
    free(vo->scratch);
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
  vo->mapping = realloc(vo->mapping, sz * sizeof *vo->mapping);
  AN(vo->mapping);


  for (; vo->l_backend < sz; vo->l_backend++)
    vo->mapping[vo->l_backend] = NULL;

  vo->l_backend = sz;
}

int voverride_add_backend(struct voverride *vo, VCL_BACKEND be,
                               double hv, const char *name)
{
  unsigned u;

  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);

  if (vo->n_backend >= vo->l_backend)
    voverride_expand(vo, vo->l_backend + 16);
  assert(vo->n_backend < vo->l_backend);
  u = vo->n_backend++;
  vo->backend[u] = be;
  vo->names[u] = name;
  vo->hashvals[u] = hv;
  return u;
}

void voverride_create_mappings(struct voverride *vo, struct vdir *vd)
{
  struct SHA256Context sha_ctx;
  struct vmapping *vm;
  unsigned u;

  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);
  voverride_wrlock(vo);

  for (u = 0; u < vo->n_backend; u++) {
    SHA256_Init(&sha_ctx);
    SHA256_Update(&sha_ctx, vo->names[u], strlen(vo->names[u]));
    vm = vo->mapping[u];
    if (vm != NULL) {
      CHECK_OBJ_NOTNULL(vm, VDIR_MAPPING_MAGIC);
      vmapping_delete(&vo->mapping[u]);
    }
    vmapping_new(&vm, 0);
    vo->mapping[u] = vm;
    vmapping_create_aliases(vm, vd, &sha_ctx);
  }
  voverride_unlock(vo);
}

VCL_BACKEND
voverride_get_be(VRT_CTX, struct voverride *vo, double hv, struct vmapping **vmp, int *healthy)
{
  unsigned u;
  VCL_BACKEND be = NULL;

  CHECK_OBJ_NOTNULL(vo, VDIR_OVERRIDE_MAGIC);

  if (vmp != NULL)
    *vmp = NULL;

  voverride_rdlock(vo);
  for (u = 0; u < vo->n_backend; u++) {
    if (vo->hashvals[u] == hv) {
      be = vo->backend[u];
      if (healthy) {
        *healthy = VRT_Healthy(ctx, be, NULL);
      }
      if (vmp != NULL)
        *vmp = vo->mapping[u];
      break;
    }
  }
  voverride_unlock(vo);
  return (be);
}

