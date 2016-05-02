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

static
void vmapping_expand(struct vmapping *vm, unsigned sz)
{
  CHECK_OBJ_NOTNULL(vm, VDIR_MAPPING_MAGIC);

  vm->alias = realloc(vm->alias, sz * sizeof *vm->alias);
  vm->l_alias = sz;
}

static
unsigned push_alias(struct vmapping *vm, unsigned a)
{
  unsigned u;
  CHECK_OBJ_NOTNULL(vm, VDIR_MAPPING_MAGIC);

  u = vm->n_alias;
  if (u < vm->l_alias + 16) {
    vmapping_expand(vm, vm->l_alias+16);
  }
  vm->alias[u++] = a;
  vm->n_alias = u;
  return u;
}

void vmapping_new(struct vmapping **vmp, unsigned size)
{
  struct vmapping *vm;

  ALLOC_OBJ(vm, VDIR_MAPPING_MAGIC);
  AN(vm);
  *vmp = vm;

  if (size > 0)
    vmapping_expand(vm, size);
}

void vmapping_delete(struct vmapping **vmp)
{
  struct vmapping *vm;
  AN(vmp);
  vm = *vmp;
  *vmp = NULL;

  CHECK_OBJ_NOTNULL(vm, VDIR_MAPPING_MAGIC);

  free(vm->alias);
  FREE_OBJ(vm);
}

static int pick_be(struct vdir *vd, double hv, struct vbitmap *vbm)
{
  unsigned u;
  double ttw = 0;
  double tw = 0;

  for(u = 0; u < vd->n_backend; u++) {
    ttw += vd->weight[u];
    if (!vbit_test(vbm, u)) {
      tw += vd->weight[u];
    }
  }

  if (tw > 0.0) {
    return vdir_pick_by_weight(vd, hv * tw, vbm);
  }
  return -1;
}

void vmapping_create_aliases(struct vmapping *vm, struct vdir *vd, struct SHA256Context *basectx)
{
  struct vbitmap *map;
  struct SHA256Context ctx;
  unsigned u,d;
  int dest;
  double hv;
  unsigned char sha256[SHA256_LEN];
  VCL_BACKEND be;

  CHECK_OBJ_NOTNULL(vm, VDIR_MAPPING_MAGIC);
  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);

  map = vbit_init(8);
  AN(map);
  vdir_rdlock(vd);
  for (u = 0; u < vd->n_backend; u++) {
    be = vd->backend[u];
    CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
    sha256reset(&ctx, basectx);
    sha256update(&ctx, be->vcl_name, sha256);
    hv = scalbn(vbe32dec(&sha256[0]), -32);
    assert(hv >= 0 && hv <= 1.0);
    dest = pick_be(vd, hv, map);
    assert(dest > -1 && dest < vd->n_backend);
    vbit_set(map, dest);
    d = push_alias(vm, dest);
    assert(d == u+1);
  }
  vdir_unlock(vd);
  vbit_destroy(map);
}
