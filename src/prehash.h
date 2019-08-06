#ifndef VMOD_PREHASH_DIRECTOR_MAGIC

#include "atomic.h"
struct voverride {
  unsigned              magic;
#define VDIR_OVERRIDE_MAGIC        0xfeb09a2e
  pthread_rwlock_t      mtx;
  unsigned              n_backend;
  unsigned              l_backend;
  VCL_BACKEND           *backend;
  const char            **names;
  struct vmapping       **mapping;
  struct vdi_methods    *methods;
  double                *hashvals;
  VCL_BACKEND           dir;
  struct ws             *ws;
  unsigned char         *scratch;
};

struct vmapping {
  unsigned          magic;
#define VDIR_MAPPING_MAGIC          0xff09462a
  unsigned          n_alias;
  unsigned          l_alias;
  unsigned char     *alias;
};

struct vmod_prehash_director {
  unsigned        magic;
#define VMOD_PREHASH_DIRECTOR_MAGIC 0x3fe1beef
  atomic_ptr_t    hdr;
  struct vdir    *vd, *lrvd;
  struct voverride *vo;
  struct          ws *ws;
  unsigned char __scratch[4096];
};

struct vmod_prehash_lastresort_director {
  unsigned       magic;
#define VMOD_PREHASH_LASTRESORT_MAGIC 0x9667b1af
  struct vdir    *vd;
  unsigned       nxt;
};

void vmapping_new(struct vmapping **vmp, unsigned size);
void vmapping_delete(struct vmapping **vmp);
void vmapping_create_aliases(struct vmapping *vm, struct vdir *vd, struct SHA256Context *ctx);

#define VMAPALIAS(vm,i) (((vm) != NULL && (i) < (vm)->n_alias) ? ((vm)->alias[(i)]) : (i))

void voverride_new(VRT_CTX, struct voverride **vop, struct ws *ws,
    const char *fmt, const char *vcl_name, vdi_healthy_f *healthy, vdi_resolve_f *resolve, vdi_list_f *list, void *priv);
void voverride_delete(struct voverride **vop);
void voverride_rdlock(struct voverride *vo);
void voverride_wrlock(struct voverride *vo);
void voverride_unlock(struct voverride *vo);
int voverride_add_backend(struct voverride *vo, VCL_BACKEND be,
                               double hv, const char *name);
VCL_BACKEND voverride_get_be(VRT_CTX, struct voverride *vo, double hv,
                             struct vmapping **vmp, int *healthy);
void voverride_create_mappings(struct voverride *vo, struct vdir *vd);
void voverride_list(VRT_CTX, struct voverride *vo, struct vsb *vsb, int pflag, int jflag);

VCL_BACKEND v_matchproto_(vdi_resolve_f) prehash_random_resolve(VRT_CTX, VCL_BACKEND);
VCL_BACKEND v_matchproto_(vdi_resolve_f) prehash_rr_resolve(VRT_CTX, VCL_BACKEND);

void sha256update(SHA256_CTX *ctx, const char *p, unsigned char digest[32]);
void sha256reset(SHA256_CTX *ctx, SHA256_CTX *base);
#endif /* VMOD_PREHASH_DIRECTOR_MAGIC */
