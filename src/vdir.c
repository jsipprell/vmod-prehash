/*-
 * Copyright (c) 2013-2015 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include <stdlib.h>
#include "cache/cache.h"
#include "vbm.h"
#include "vsb.h"
#include "vdir.h"

static void
vdir_expand(struct vdir *vd, unsigned n)
{
  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);

  vd->backend = realloc(vd->backend, n * sizeof *vd->backend);
  AN(vd->backend);
  vd->weight = realloc(vd->weight, n * sizeof *vd->weight);
  AN(vd->weight);
  vd->l_backend = n;
}

void
vdir_new(VRT_CTX, struct vdir **vdp, const char *fmt, const char *vcl_name,
    vdi_healthy_f *healthy, vdi_resolve_f *resolve, vdi_list_f *list, void *priv)
{
  struct vdir *vd;

  AN(vcl_name);
  AN(vdp);
  AZ(*vdp);
  ALLOC_OBJ(vd, VDIR_MAGIC);
  AN(vd);
  *vdp = vd;
  AZ(pthread_rwlock_init(&vd->mtx, NULL));

  ALLOC_OBJ(vd->methods, VDI_METHODS_MAGIC);
  vd->methods->type = "prehash";
  vd->methods->healthy = healthy;
  vd->methods->resolve = resolve;
  vd->methods->list = list;
  vd->dir = VRT_AddDirector(ctx, vd->methods, priv, fmt ? fmt : "%s", vcl_name);
  vd->vbm = vbit_new(SLT__MAX);
  AN(vd->vbm);
}

void
vdir_delete(struct vdir **vdp)
{
  struct vdir *vd;

  AN(vdp);
  vd = *vdp;
  *vdp = NULL;

  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);

  VRT_DelDirector(&vd->dir);
  free(vd->backend);
  free(vd->weight);
  AZ(pthread_rwlock_destroy(&vd->mtx));
  FREE_OBJ(vd->methods);
  vbit_destroy(vd->vbm);
  FREE_OBJ(vd);
}

void
vdir_rdlock(struct vdir *vd)
{
  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);
  AZ(pthread_rwlock_rdlock(&vd->mtx));
}

void
vdir_wrlock(struct vdir *vd)
{
  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);
  AZ(pthread_rwlock_wrlock(&vd->mtx));
}

void
vdir_unlock(struct vdir *vd)
{
  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);
  AZ(pthread_rwlock_unlock(&vd->mtx));
}


int
vdir_add_backend(VRT_CTX, struct vdir *vd, VCL_BACKEND be, double weight)
{
  unsigned u;

  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);
  AN(be);
  vdir_wrlock(vd);
  if (vd->n_backend >= SLT__MAX) {
    VRT_fail(ctx, "prehash directors cannot have more than %d backends", SLT__MAX);
    vdir_unlock(vd);
    return (-1);
  }
  if (vd->n_backend >= vd->l_backend)
    vdir_expand(vd, vd->l_backend + 16);
  assert(vd->n_backend < vd->l_backend);
  u = vd->n_backend++;
  vd->backend[u] = be;
  vd->weight[u] = weight;
  vd->total_weight += weight;
  vdir_unlock(vd);
  return (u);
}

unsigned
vdir_remove_backend(struct vdir *vd, VCL_BACKEND be)
{
  unsigned u, n;

  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);
  if (be == NULL)
    return (vd->n_backend);
  CHECK_OBJ(be, DIRECTOR_MAGIC);
  vdir_wrlock(vd);
  for (u = 0; u < vd->n_backend; u++)
    if (vd->backend[u] == be)
      break;
  if (u == vd->n_backend) {
    vdir_unlock(vd);
    return (vd->n_backend);
  }
  vd->total_weight -= vd->weight[u];
  n = (vd->n_backend - u) - 1;
  memmove(&vd->backend[u], &vd->backend[u+1], n * sizeof(vd->backend[0]));
  memmove(&vd->weight[u], &vd->weight[u+1], n * sizeof(vd->weight[0]));
  vd->n_backend--;
  vdir_unlock(vd);
  return (vd->n_backend);
}

unsigned
vdir_any_healthy(VRT_CTX, struct vdir *vd, double *changed)
{
  unsigned retval = 0;
  VCL_BACKEND be;
  unsigned u;
  double c = 0;

  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);
  vdir_rdlock(vd);
  if (changed)
    *changed = 0;
  for (u = 0; u < vd->n_backend; u++) {
    VCL_BOOL h;
    be = vd->backend[u];
    CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);

    h = VRT_Healthy(ctx, be, &c);
    if (changed != NULL && c > *changed)
      *changed = c;
    if (h) {
      retval++;
      break;
    }
  }
  vdir_unlock(vd);
  return (retval);
}

unsigned
vdir_pick_by_weight(const struct vdir *vd, double w,
    const struct vbitmap *blacklist)
{
  double a = 0.0;
  VCL_BACKEND be = NULL;
  unsigned u;

  for (u = 0; u < vd->n_backend; u++) {
    be = vd->backend[u];
    CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
    if (blacklist != NULL && vbit_test(blacklist, u))
      continue;
    a += vd->weight[u];
    if (w < a)
      return (u);
  }
  WRONG("");
}

static unsigned
vdir_update_health(VRT_CTX, struct vdir *vd, double *total_weight)
{
  VCL_BOOL h;
  VCL_TIME c, changed = 0;
  unsigned u, count = 0;
  double tw = 0.0;

  for (u = 0; u < vd->n_backend; u++) {
    CHECK_OBJ_NOTNULL(vd->backend[u], DIRECTOR_MAGIC);
    c = 0;
    h = VRT_Healthy(ctx, vd->backend[u], &c);
    if (c > changed)
      changed = c;
    if (h) {
      tw += vd->weight[u];
      count++;
      if (vbit_test(vd->vbm, u))
        vbit_clr(vd->vbm, u);
    } else if (!vbit_test(vd->vbm, u))
      vbit_set(vd->vbm, u);
  }

  VRT_SetChanged(vd->dir, changed);
  if (total_weight)
    *total_weight = tw;
  return (count);
}

void
vdir_list(VRT_CTX, struct vdir *vd, struct vsb *vsb, int pflag, int jflag)
{
  VCL_BACKEND be;
  unsigned u, nh = 0;
  VCL_BOOL unhealthy;
  uint32_t range, last_range = 0;
  double tw = 0.0;

  CHECK_OBJ_NOTNULL(vd, VDIR_MAGIC);
  vdir_rdlock(vd);
  vdir_update_health(ctx, vd, &tw);
  
  for (u = 0; u < vd->n_backend; u++) {
    be = vd->backend[u];
    CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
    if (!(unhealthy = vbit_test(vd->vbm, u)))
      nh++;

    if (pflag) {
      range = u+1 >= vd->n_backend ? 0xffff : last_range + (vd->weight[u] / tw) * 0xffff;
      if (jflag) {
        if (u)
          VSB_cat(vsb, ",\n");
        VSB_printf(vsb, "\"%s\": {\n", be->vcl_name);
        VSB_indent(vsb, 2);
        if (unhealthy)
          VSB_cat(vsb, "\"health\": \"sick\",\n");
        else
          VSB_cat(vsb, "\"health\": \"healthy\",\n");
        VSB_printf(vsb, "\"range\": \"%04x-%04x\"\n", last_range, range);
        VSB_indent(vsb, -2);
        VSB_cat(vsb, "}");
      } else {
        VSB_cat(vsb, "\t");
        VSB_cat(vsb, be->vcl_name);
        VSB_printf(vsb, "\t%04x-%04x\t", last_range, range);
        VSB_cat(vsb, unhealthy ? "sick" : "healthy");
        VSB_cat(vsb, "\n");
      }
      last_range = range + 1;
    }
  }
  u = vd->n_backend;
  vdir_unlock(vd);

  if (jflag && pflag) {
    VSB_cat(vsb, "\n");
    VSB_indent(vsb, -2);
    VSB_cat(vsb, "}\n");
    VSB_indent(vsb, -2);
    VSB_cat(vsb, "},\n");
  }

  if (pflag)
    return;

  if (jflag)
    VSB_printf(vsb, "[%u, %u, \"%s\"]", nh, u, nh ? "healthy" : "sick");
  else
    VSB_printf(vsb, "%u/%u\t%s", nh, u, nh ? "healthy" : "sick");
}

VCL_BACKEND
vdir_pick_be(VRT_CTX, struct vdir *vd, double w)
{
  unsigned u;
  double tw = 0.0;
  VCL_BACKEND be = NULL;

  vdir_rdlock(vd);
  vdir_update_health(ctx, vd, &tw);
  if (tw > 0.0) {
    u = vdir_pick_by_weight(vd, w * tw, vd->vbm);
    assert(u < vd->n_backend);
    be = vd->backend[u];
    CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
  }
  vdir_unlock(vd);
  return (be);
}

VCL_BACKEND
vdir_exact_be(VRT_CTX, struct vdir *vd, double w, int *healthy)
{
  unsigned u;
  VCL_BACKEND be = NULL;


  vdir_rdlock(vd);
  for (u = 0; u < vd->n_backend; u++) {
    if (vd->weight[u] == w) {
      be = vd->backend[u];
      if (healthy) {
        *healthy = VRT_Healthy(ctx, be, NULL);
      }
      break;
    }
  }
  vdir_unlock(vd);
  return (be);
}
