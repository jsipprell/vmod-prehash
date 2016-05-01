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
  struct vmod_prehash_director *rr;
  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);
  return (vdir_any_healthy(rr->prevd, bo, changed));
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
  uint32_t rv;
  double r;

  memset(&sha_ctx, 0, sizeof(sha_ctx));
  memset(sha256, 0, sizeof(sha256));
  SHA256_Init(&sha_ctx);
  for (p = arg; p != vrt_magic_string_end; p = va_arg(ap, const char*)) {
    if (p != NULL && *p != '\0')
      SHA256_Update(&sha_ctx, p, strlen(p));
  }
  SHA256_Final(sha256, &sha_ctx);

  rv = vbe32dec(&sha256[0]);
  r = scalbn(rv, -32);
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
  VSL(SLT_Debug, 0, "vbe32dec(sha256) -> 0x%04x", rv);
  r = scalbn(rv, -32);
  assert(r >= 0 && r <= 1.0);
  return r;
}

VCL_BACKEND __match_proto__()
vmod_director_backend(VRT_CTX, struct vmod_prehash_director *rr)
{
  struct gethdr_s gethdr;
  const char *value;
  VCL_BACKEND be = NULL;
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  if (!vdir_any_healthy(rr->vd, NULL, NULL))
    return (rr->lrvd->dir);

  vdir_rdlock(rr->prevd);
  gethdr.what = rr->hdr ? rr->hdr : H_Host;

  VSL(SLT_Debug, 0, "prehash hash header is '%s'", gethdr.what+1);
  if ((ctx->method & (VCL_MET_BACKEND_FETCH|VCL_MET_BACKEND_RESPONSE|VCL_MET_BACKEND_ERROR))  != 0) {
    CHECK_OBJ_NOTNULL(ctx->bo, BUSYOBJ_MAGIC);
    gethdr.where = HDR_BEREQ;
  } else if ((ctx->method & (VCL_MET_INIT|VCL_MET_FINI|VCL_MET_DELIVER|VCL_MET_PIPE)) == 0) {
    CHECK_OBJ_ORNULL(ctx->bo, BUSYOBJ_MAGIC);
    gethdr.where = HDR_REQ;
  } else {
    VSL(SLT_Error, 0, "prehash: current varnish context method unknown, defaulting to random backend");
    vdir_unlock(rr->prevd);
    return rr->vd->dir;
  }

  value = VRT_GetHdr(ctx, &gethdr);
  vdir_unlock(rr->prevd);

  if (value != NULL) {
    int healthy = 0;
    double r;
    r = calchash(value, NULL);
    be  = vdir_exact_be(rr->prevd, r, ctx->bo, &healthy);
    if (be == NULL || !healthy) {
      be = vdir_pick_be(rr->vd, r, ctx->bo);
    }
  } else {
    be = rr->vd->dir;
  }

  if (be == NULL)
    be = rr->lrvd->dir;

  return be;
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

  be = vdir_exact_be(rr->prevd, r, ctx->bo, &healthy);
  if (!healthy) {
    be = vdir_pick_be(rr->vd, r, ctx->bo);
  }

  return be;
}

VCL_VOID __match_proto__()
vmod_director_set_hash_header(VRT_CTX, struct vmod_prehash_director *rr, const char *arg, ...)
{
  va_list ap;
  unsigned u;
  struct ws *ws;
  txt t;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  ws = (ctx->ws != NULL) ? ctx->ws : rr->ws;
  CHECK_OBJ_NOTNULL(ws, WS_MAGIC);

  u = WS_Reserve(ws, 0);
  t.b = ws->f;
  va_start(ap, arg);
  t.e = VRT_StringList(ws->f+1, u-2, arg, ap);
  va_end(ap);
  if (t.e != NULL) {  
    assert(t.e > t.b);
    *((char*)t.e-1) = ':'; t.e++;
    *((char*)t.e-1) = 0; t.e++;
    *ws->f = (char)strlen(ws->f+1);
    WS_Release(ws, t.e - t.b);
    vdir_wrlock(rr->prevd);
    vdir_wrlock(rr->vd);
    rr->hdr = (const char*)t.b;
    vdir_unlock(rr->vd);
    vdir_unlock(rr->prevd);
  } else {
    WS_Release(ws, 0);
  }
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

  vdir_new(&rr->vd, "prehash", vcl_name, prehash_healthy, prehash_random_resolve, rr);
  vdir_new(&rr->prevd, "prehash_override", vcl_name, prehash_override_healthy, NULL, rr);


  lrr = (struct vmod_prehash_lastresort_director*)WS_Alloc(rr->ws, sizeof *lrr);
  INIT_OBJ(lrr, VMOD_PREHASH_LASTRESORT_MAGIC);

  vdir_new(&rr->lrvd, "prehash_lastresort", vcl_name, lrvd_healthy, prehash_rr_resolve, lrr);
  lrr->vd = rr->lrvd;
}


VCL_VOID __match_proto__()
vmod_director__fini(struct vmod_prehash_director **rrp) {
  struct vmod_prehash_director *rr;

  rr = *rrp;
  *rrp = NULL;
  vdir_delete(&rr->vd);
  vdir_delete(&rr->prevd);
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

  (void)vdir_add_backend(rr->prevd, be, w);
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

