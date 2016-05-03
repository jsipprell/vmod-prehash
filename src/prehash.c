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

static unsigned __match_proto__(vdi_healthy)
prehash_healthy(const struct director *dir, const struct busyobj *bo, double *changed)
{
  struct vmod_prehash_director *rr;
  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);
  return (vdir_any_healthy(rr->vd, bo, changed));
}

static unsigned __match_proto__(vdi_healthy)
prehash_override_healthy(const struct director *dir, const struct busyobj *bo, double *changed)
{
  unsigned u;
  VCL_BACKEND be;

  struct vmod_prehash_director *rr;
  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);

  voverride_rdlock(rr->vo);
  for (u = 0; u < rr->vo->n_backend; u++) {
    be = rr->vo->backend[u];
    if (be != NULL && be->healthy(be, bo, changed)) {
      break;
    }
  }
  if (u == rr->vo->n_backend)
    u = 0;
  voverride_unlock(rr->vo);
  return u;
}

static unsigned
pick_by_weight(const struct vdir *vd, double w,
    const struct vbitmap *blacklist, const struct vmapping *vm)
{
  double a = 0.0;
  VCL_BACKEND be = NULL;
  unsigned u, mu;

  for (u = 0; u < vd->n_backend; u++) {
    mu = VMAPALIAS(vm,u);
    be = vd->backend[mu];
    CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
    if (blacklist != NULL && vbit_test(blacklist, mu))
      continue;
    a += vd->weight[mu];
    if (w < a)
      return (mu);
  }
  WRONG("");
}

static VCL_BACKEND
pick_be(struct vdir *vd, double w, const struct busyobj *bo, const struct vmapping *m)
{
  unsigned u;
  double tw = 0.0;
  VCL_BACKEND be = NULL;

  CHECK_OBJ_ORNULL(bo, BUSYOBJ_MAGIC);
  vdir_rdlock(vd);
  for (u = 0; u < vd->n_backend; u++) {
    if (vd->backend[u]->healthy(vd->backend[u], bo, NULL)) {
      vbit_clr(vd->vbm, u);
      tw += vd->weight[u];
    } else
      vbit_set(vd->vbm, u);
  }
  if (tw > 0.0) {
    u = pick_by_weight(vd, w * tw, vd->vbm, m);
    assert(u < vd->n_backend);
    be = vd->backend[u];
    CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
  }
  vdir_unlock(vd);
  return (be);
}


static unsigned __match_proto__(vdi_healthy)
lrvd_healthy(const struct director *dir, const struct busyobj *bo, double *changed)
{
  struct vmod_prehash_lastresort_director *rr;
  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_LASTRESORT_MAGIC);
  return (vdir_any_healthy(rr->vd, bo, changed));
}

static double vcalchash(const char *arg, va_list ap)
{
  struct SHA256Context sha_ctx;
  const char *p;
  unsigned char sha256[SHA256_LEN];
  double r;

  memset(&sha_ctx, 0, sizeof(sha_ctx));
  memset(sha256, 0, sizeof(sha256));
  SHA256_Init(&sha_ctx);
  for (p = arg; p != vrt_magic_string_end; p = va_arg(ap, const char*)) {
    if (p != NULL && *p != '\0')
      SHA256_Update(&sha_ctx, p, strlen(p));
  }
  SHA256_Final(sha256, &sha_ctx);

  r = scalbn(vbe32dec(&sha256[0]), -32);
  assert(r >= 0 && r <= 1.0);
  return r;
}

static double calchash(const char *data, ...)
{
  va_list ap;
  struct SHA256Context sha_ctx;
  unsigned char sha256[SHA256_LEN];
  uint32_t rv;
  double r;

  SHA256_Init(&sha_ctx);
  va_start(ap, data);
  for (; data != NULL; data = va_arg(ap, const char*)) {
    if (*data != '\0')
      SHA256_Update(&sha_ctx, data, strlen(data));
  }
  va_end(ap);
  SHA256_Final(sha256, &sha_ctx);

  rv = vbe32dec(&sha256[0]);
  r = scalbn(rv, -32);
  assert(r >= 0 && r <= 1.0);
  VSL(SLT_Debug, 0, "vbe32dec(sha256) -> 0x%04x / scalbn(-32) -> %f", rv, r);
  return r;
}

static const struct director *resolve(const struct vmod_prehash_director *rr,
                                      const char *value,
                                      struct busyobj *bo)
{
  struct vmapping *m = NULL;
  int healthy = 0;
  double r;
  VCL_BACKEND be = NULL;

  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);
  CHECK_OBJ_ORNULL(bo, BUSYOBJ_MAGIC);

  if (!vdir_any_healthy(rr->vd, NULL, NULL))
    return (rr->lrvd->dir);

  if (value != NULL && *value != '\0') {
    r = calchash(value, NULL);
    be = voverride_get_be(rr->vo, r, bo, &m, &healthy);
    if (!be || !healthy) {
      if (!m)
        be = vdir_pick_be(rr->vd, r, bo);
      else
        be = pick_be(rr->vd, r, bo, m);
    }
  } else
    be = rr->vd->dir;

  return (be ? be : rr->lrvd->dir);
}

static const struct director * __match_proto__(vdi_resolve_f)
vmod_director_resolve(const struct director *dir,
                       struct worker *wrk,
                       struct busyobj *bo)
{
  struct vmod_prehash_director *rr;
  struct gethdr_s gethdr;
  const char *value = NULL;

  (void)wrk;
  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);
  CHECK_OBJ_NOTNULL(bo, BUSYOBJ_MAGIC);

  voverride_rdlock(rr->vo);
  gethdr.what = rr->hdr ? rr->hdr : H_Host;
  voverride_unlock(rr->vo);

  if(http_GetHdr(bo->bereq, gethdr.what, &value) && value != NULL)
    VSL(SLT_Debug, 0, "prehash hash header is '%s %s'", gethdr.what+1, value);

  return resolve(rr, value, bo);
}

VCL_BACKEND __match_proto__()
vmod_director_self(VRT_CTX, struct vmod_prehash_director *rr)
{
  struct gethdr_s gethdr;
  const char *value;
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  voverride_rdlock(rr->vo);
  gethdr.what = rr->hdr ? rr->hdr : H_Host;
  voverride_unlock(rr->vo);

  if ((ctx->method & (VCL_MET_BACKEND_FETCH|VCL_MET_BACKEND_RESPONSE|VCL_MET_BACKEND_ERROR))  != 0) {
    CHECK_OBJ_NOTNULL(ctx->bo, BUSYOBJ_MAGIC);
    gethdr.where = HDR_BEREQ;
  } else if ((ctx->method & (VCL_MET_INIT|VCL_MET_FINI|VCL_MET_DELIVER|VCL_MET_PIPE)) == 0) {
    CHECK_OBJ_ORNULL(ctx->bo, BUSYOBJ_MAGIC);
    gethdr.where = HDR_REQ;
  } else {
    VSL(SLT_Error, 0, "prehash: current varnish context method unknown, defaulting to random backend");
    return rr->vd->dir;
  }

  value = VRT_GetHdr(ctx, &gethdr);

  if (value != NULL) {
    VSL(SLT_Debug, 0, "prehash hash header is '%s %s'", gethdr.what+1, value);
  }

  return resolve(rr, value, ctx->bo);
}


VCL_BACKEND __match_proto__()
vmod_director_backend(VRT_CTX, struct vmod_prehash_director *rr)
{ 
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);
  CHECK_OBJ_NOTNULL(rr->vo, VDIR_OVERRIDE_MAGIC);
  CHECK_OBJ_NOTNULL(rr->vo->dir, DIRECTOR_MAGIC);
  return rr->vo->dir;
}

VCL_BACKEND __match_proto__()
vmod_director_hash(VRT_CTX, struct vmod_prehash_director *rr, const char *args, ...)
{
  va_list ap;
  double r;
  int healthy = 0;
  VCL_BACKEND be = NULL;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  va_start(ap, args);
  r = vcalchash(args,ap);
  va_end(ap);

  be = voverride_get_be(rr->vo, r, ctx->bo, NULL, &healthy);
  if (!healthy) {
    be = vdir_pick_be(rr->vd, r, ctx->bo);
  }

  return be;
}

VCL_VOID __match_proto__()
vmod_director_finalize(VRT_CTX, struct vmod_prehash_director *rr)
{
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  voverride_create_mappings(rr->vo, rr->vd);
}

VCL_VOID __match_proto__()
vmod_director_set_hash_header(VRT_CTX, struct vmod_prehash_director *rr, const char *arg, ...)
{
  va_list ap;
  unsigned u;
  txt t;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  CHECK_OBJ_NOTNULL(rr->ws, WS_MAGIC);

  voverride_wrlock(rr->vo);
  u = WS_Reserve(rr->ws, 0);
  t.b = rr->ws->f;
  va_start(ap, arg);
  t.e = VRT_StringList(rr->ws->f+1, u-2, arg, ap);
  va_end(ap);
  if (t.e != NULL) {  
    assert(t.e > t.b);
    *((char*)t.e-1) = ':'; t.e++;
    *((char*)t.e-1) = 0; t.e++;
    *rr->ws->f = (char)strlen(rr->ws->f+1);
    WS_Release(rr->ws, t.e - t.b);
    vdir_wrlock(rr->vd);
    rr->hdr = (const char*)t.b;
    vdir_unlock(rr->vd);
  } else {
    WS_Release(rr->ws, 0);
  }
  voverride_unlock(rr->vo);
}

VCL_VOID __match_proto__()
vmod_director__init(VRT_CTX, struct vmod_prehash_director **rrp,
                    const char *vcl_name)
{
  struct vmod_prehash_director *rr;
  struct vmod_prehash_lastresort_director *lrr;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  AN(rrp);
  AZ(*rrp);
  ALLOC_OBJ(rr, VMOD_PREHASH_DIRECTOR_MAGIC);
  AN(rr);
  *rrp = rr;
  rr->ws = (struct ws*)&(rr->__scratch[0]);
  WS_Init(rr->ws, "mii", &(rr->__scratch[0]) + sizeof(struct ws),
                       sizeof(rr->__scratch) - sizeof(struct ws));

  
  vdir_new(&rr->vd, "prehash", WS_Printf(rr->ws,"%s_prehash", vcl_name), prehash_healthy, prehash_random_resolve, rr);
  voverride_new(&rr->vo, rr->ws, "prehash_override", WS_Printf(rr->ws,"%s_override", vcl_name), prehash_override_healthy, vmod_director_resolve, rr);


  lrr = (struct vmod_prehash_lastresort_director*)WS_Alloc(rr->ws, sizeof *lrr);
  INIT_OBJ(lrr, VMOD_PREHASH_LASTRESORT_MAGIC);

  vdir_new(&rr->lrvd, "prehash_lastresort", WS_Printf(rr->ws,"%s_lastresort", vcl_name), lrvd_healthy, prehash_rr_resolve, lrr);
  lrr->vd = rr->lrvd;
}


VCL_VOID __match_proto__()
vmod_director__fini(struct vmod_prehash_director **rrp) {
  struct vmod_prehash_director *rr;

  rr = *rrp;
  *rrp = NULL;
  vdir_delete(&rr->vd);
  voverride_delete(&rr->vo);
  vdir_delete(&rr->lrvd);
  FREE_OBJ(rr);
}

VCL_VOID __match_proto__() vmod_director_add_backend(VRT_CTX,
  struct vmod_prehash_director *rr, VCL_BACKEND be, double w)
{
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  if (w == 0) {
    vmod_director_add_lastresort_backend(ctx, rr, be, 1.0);
  } else {
    (void)vdir_add_backend(rr->vd, be, w);
    VSL(SLT_Debug, 0, "prehash backend '%s' registered with weight %f", be->vcl_name, w);
  }
}

VCL_VOID __match_proto__() vmod_director_add_hashed_backend(VRT_CTX,
  struct vmod_prehash_director *rr, VCL_BACKEND be, const char *arg, ...)
{
  va_list ap;
  double w;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);
  va_start(ap, arg);
  w = vcalchash(arg, ap);
  va_end(ap);

  (void)voverride_add_backend(rr->vo, be, w, arg);
  VSL(SLT_Debug, 0, "prehash backend override '%s' registered with hash value %f", be->vcl_name, w);
}

VCL_VOID __match_proto__() vmod_director_add_lastresort_backend(VRT_CTX,
  struct vmod_prehash_director *rr, VCL_BACKEND be, double w)
{
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);
  (void)vdir_add_backend(rr->lrvd, be, w);
  VSL(SLT_Debug, 0, "prehash lastresort backend '%s' registered with weight %f", be->vcl_name, w);
}

