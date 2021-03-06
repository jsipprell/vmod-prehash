#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>
#include <stdlib.h>

#include "cache/cache.h"
#include "cache/cache_director.h"

#include "vend.h"
#include "vsha256.h"
#include "vbm.h"
#include "vcl.h"
#include "vsb.h"

#include "vdir.h"
#include "vcc_prehash_if.h"
#include "prehash.h"

static unsigned v_matchproto_(vdi_healthy_f)
prehash_healthy(VRT_CTX, const struct director *dir, double *changed)
{
  struct vmod_prehash_director *rr;
  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);
  return (vdir_any_healthy(ctx, rr->vd, changed));
}

static void v_matchproto_(vdi_list_f)
prehash_list(VRT_CTX, VCL_BACKEND dir, struct vsb *vsb, int pflag, int jflag)
{
  struct vmod_prehash_director *rr;
  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);
  AN(vsb);


  if (pflag) {
    if (jflag) {
      VSB_cat(vsb, "{\n");
      VSB_indent(vsb, 2);
      VSB_cat(vsb, "\"backends\": {\n");
      VSB_indent(vsb, 2);
    } else VSB_cat(vsb, "\n\n\tBackend\tRange\tHealth\n");
  }

  vdir_list(ctx, rr->vd, vsb, pflag, jflag);
}

static void v_matchproto_(vdi_list_f)
prehash_list_excl(VRT_CTX, VCL_BACKEND dir, struct vsb *vsb, int pflag, int jflag)
{
  struct vmod_prehash_director *rr;
  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);
  AN(vsb);

  if (pflag) {
    if (jflag) {
      VSB_cat(vsb, "{\n");
      VSB_indent(vsb, 2);
      VSB_cat(vsb, "\"backends\": {\n");
      VSB_indent(vsb, 2);
    } else VSB_cat(vsb, "\n\n\tBackend\tKey\tHealth\n");
  }

  voverride_list(ctx, rr->vo, vsb, pflag, jflag);
}

/*
static unsigned v_matchproto_(vdi_healthy)
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
*/

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
pick_be(VRT_CTX, struct vdir *vd, double w, const struct vmapping *m)
{
  unsigned u;
  double tw = 0.0;
  VCL_BACKEND be = NULL;

  vdir_rdlock(vd);
  for (u = 0; u < vd->n_backend; u++) {
    if (VRT_Healthy(ctx, vd->backend[u], NULL)) {
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

static unsigned v_matchproto_(vdi_healthy)
lrvd_healthy(VRT_CTX, const struct director *dir, double *changed)
{
  struct vmod_prehash_lastresort_director *rr;
  CHECK_OBJ_NOTNULL(dir, DIRECTOR_MAGIC);
  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_LASTRESORT_MAGIC);
  return (vdir_any_healthy(ctx, rr->vd, changed));
}

static double vcalchash(const char **dp, unsigned maxlen, const char *arg, va_list ap)
{
  struct SHA256Context sha_ctx;
  unsigned char sha256[SHA256_LEN];
  char *d;
  double r;
  unsigned l;

  d = dp ? (char*)*dp : NULL;

  SHA256_Init(&sha_ctx);
  for (; arg != vrt_magic_string_end; arg = va_arg(ap, const char*)) {
    if (arg != NULL && *arg != '\0') {
      l = strlen(arg);
      if (d) {
        if (maxlen < l+1)
          d = NULL;
        else {
          d = strcpy(d, arg) + l;
          maxlen -= l;
        }
      }
      SHA256_Update(&sha_ctx, arg, l);
    }
  }
  SHA256_Final(sha256, &sha_ctx);

  r = scalbn(vbe32dec(&sha256[0]), -32);
  assert(r >= 0 && r <= 1.0);
  if (d) {
    *d = '\0';
    *dp = d+1;
  } else if (dp)
    *dp = NULL;
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

#ifdef VMOD_PREHASH_DEBUG
  VSL(SLT_Debug, 0, "vbe32dec(sha256) -> 0x%04x / scalbn(-32) -> %f", rv, r);
#endif
  return r;
}

static VCL_BACKEND resolve(VRT_CTX, const struct vmod_prehash_director *rr,
                                      const char *value)
{
  struct vmapping *m = NULL;
  int healthy = 0;
  double r;
  VCL_BACKEND be = NULL;

  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  if (!vdir_any_healthy(ctx, rr->vd, NULL))
    return (rr->lrvd->dir);

  if (value != NULL && *value != '\0') {
    r = calchash(value, NULL);
    be = voverride_get_be(ctx, rr->vo, r, &m, &healthy);
    if (!be || !healthy) {
      if (!m)
        be = vdir_pick_be(ctx, rr->vd, r);
      else
        be = pick_be(ctx, rr->vd, r, m);
    }
  } else
    be = rr->vd->dir;

  return (be ? be : rr->lrvd->dir);
}

static VCL_BACKEND v_matchproto_(vdi_resolve_f)
vmod_director_resolve(VRT_CTX, VCL_BACKEND dir)
{
  struct vmod_prehash_director *rr;
  struct gethdr_s gethdr;
  const char *value = NULL;

  CAST_OBJ_NOTNULL(rr, dir->priv, VMOD_PREHASH_DIRECTOR_MAGIC);

  if ((gethdr.what = ATOMIC_GET(rr->vd, rr->hdr)) == NULL)
    gethdr.what = H_Host;

  if(ctx->bo == NULL) {
    WRONG("resolve called with no bo");
  }

  if(ctx->bo->bereq == NULL) {
    WRONG("resolve called with no bereq");
  }

  CHECK_OBJ(ctx->bo, BUSYOBJ_MAGIC);
#ifdef VMOD_PREHASH_DEBUG
  if(http_GetHdr(ctx->bo->bereq, gethdr.what, &value) && value != NULL)
    VSL(SLT_Debug, 0, "prehash hash header is '%s %s'", gethdr.what+1, value);
#else
  (void)http_GetHdr(ctx->bo->bereq, gethdr.what, &value);
#endif
  return resolve(ctx, rr, value);
}

VCL_BACKEND v_matchproto_(td_prehash_director_self)
vmod_director_self(VRT_CTX, struct vmod_prehash_director *rr)
{
  struct gethdr_s gethdr;
  const char *value;
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  if ((gethdr.what = ATOMIC_GET(rr->vd, rr->hdr)) == NULL)
    gethdr.what = H_Host;

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

  return resolve(ctx, rr, value);
}


VCL_BACKEND v_matchproto_(td_prehash_director_backend)
vmod_director_backend(VRT_CTX, struct vmod_prehash_director *rr)
{ 
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);
  CHECK_OBJ_NOTNULL(rr->vo, VDIR_OVERRIDE_MAGIC);
  CHECK_OBJ_NOTNULL(rr->vo->dir, DIRECTOR_MAGIC);
  return rr->vo->dir;
}

VCL_BACKEND v_matchproto_(td_prehash_director_hash)
vmod_director_hash(VRT_CTX, struct vmod_prehash_director *rr, const char *args, ...)
{
  va_list ap;
  double r;
  int healthy = 0;
  VCL_BACKEND be = NULL;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  va_start(ap, args);
  r = vcalchash(NULL, 0, args, ap);
  va_end(ap);

  be = voverride_get_be(ctx, rr->vo, r, NULL, &healthy);
  if (!healthy) {
    be = vdir_pick_be(ctx, rr->vd, r);
  }

  return be;
}

VCL_VOID v_matchproto_(td_prehash_director_finalize)
vmod_director_finalize(VRT_CTX, struct vmod_prehash_director *rr)
{
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  voverride_create_mappings(rr->vo, rr->vd);
}

VCL_VOID v_matchproto_(td_prehash_director_set_hash_header)
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
    ATOMIC_SET(rr->vd, rr->hdr, t.b);
  } else {
    WS_Release(rr->ws, 0);
  }
  voverride_unlock(rr->vo);
}

VCL_VOID v_matchproto_(td_prehash_director__init)
vmod_director__init(VRT_CTX, struct vmod_prehash_director **rrp,
                    const char *vcl_name)
{
  struct vmod_prehash_director *rr;
  struct vmod_prehash_lastresort_director *lrr;
  unsigned char *s;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  AN(rrp);
  AZ(*rrp);
  ALLOC_OBJ(rr, VMOD_PREHASH_DIRECTOR_MAGIC);
  AN(rr);
  *rrp = rr;
  rr->ws = (struct ws*)PRNDUP(&rr->__scratch[0]);
  s = (unsigned char*)PRNDUP((char*)rr->ws + sizeof(struct ws));
  WS_Init(rr->ws, "mii", s, sizeof(rr->__scratch) - (s - &rr->__scratch[0]));

  vdir_new(ctx, &rr->vd, "%s_random", vcl_name, prehash_healthy, prehash_random_resolve, prehash_list, rr);
  voverride_new(ctx, &rr->vo, rr->ws, "%s_excl", vcl_name, prehash_healthy, vmod_director_resolve, prehash_list_excl, rr);

  lrr = (struct vmod_prehash_lastresort_director*)WS_Alloc(rr->ws, sizeof *lrr);
  INIT_OBJ(lrr, VMOD_PREHASH_LASTRESORT_MAGIC);

  vdir_new(ctx, &rr->lrvd, "%s_lastresort", vcl_name, lrvd_healthy, prehash_rr_resolve, NULL, lrr);
  lrr->vd = rr->lrvd;
}


VCL_VOID v_matchproto_(td_prehash_director__fini)
vmod_director__fini(struct vmod_prehash_director **rrp) {
  struct vmod_prehash_director *rr;

  rr = *rrp;
  *rrp = NULL;
  vdir_delete(&rr->vd);
  voverride_delete(&rr->vo);
  vdir_delete(&rr->lrvd);
  FREE_OBJ(rr);
}

VCL_VOID v_matchproto_(td_prehash_director_add_backend)
vmod_director_add_backend(VRT_CTX, struct vmod_prehash_director *rr,
                                          VCL_BACKEND be, double w)
{
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);

  if (w == 0) {
    vmod_director_add_lastresort_backend(ctx, rr, be, 1.0);
  } else {
    (void)vdir_add_backend(ctx, rr->vd, be, w);
    VSL(SLT_Debug, 0, "prehash backend '%s' registered with weight %f", be->vcl_name, w);
  }
}

VCL_VOID v_matchproto_(td_prehash_director_add_exclusive_backend)
vmod_director_add_exclusive_backend(VRT_CTX, struct vmod_prehash_director *rr,
                                          VCL_BACKEND be, const char *arg, ...)
{
  va_list ap;
  unsigned u;
  double w;
  txt t;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);
  CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);

  voverride_wrlock(rr->vo);
  u = WS_Reserve(rr->vo->ws, 0);
  t.e = t.b = rr->vo->ws->f;
  va_start(ap, arg);
  w = vcalchash(&t.e, u-1, arg, ap);
  va_end(ap);

  if (t.e) {
    assert(t.e > t.b);
    WS_Release(rr->vo->ws, t.e - t.b);
    AZ(*(t.e-1));
    if (voverride_add_backend(rr->vo, be, w, t.b) >= 0)
      VSL(SLT_Debug, 0, "prehash backend override '%s' registered with hash value %f (%s)", be->vcl_name, w, t.b);
  } else {
    WS_Release(rr->vo->ws, 0);
    if (ctx->msg)
      VSB_printf(ctx->msg, "prehash: insufficent ws allocation available for new exclusive backend (req=%u bytes)\n", u-1);
    else
      VSLb(ctx->vsl, SLT_VCL_Error, "prehash: insufficient ws allocation available for new exclusive backend (req=%u bytes)", u-1);
    VRT_handling(ctx, VCL_RET_FAIL);
  }
  voverride_unlock(rr->vo);
}

VCL_VOID v_matchproto_(td_prehash_director_add_lastresort_backend)
vmod_director_add_lastresort_backend(VRT_CTX, struct vmod_prehash_director *rr,
                                              VCL_BACKEND be, double w)
{
  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
  CHECK_OBJ_NOTNULL(rr, VMOD_PREHASH_DIRECTOR_MAGIC);
  if (vdir_add_backend(ctx, rr->lrvd, be, w) > -1)
    VSL(SLT_Debug, 0, "prehash lastresort backend '%s' registered with weight %f", be->vcl_name, w);
}

