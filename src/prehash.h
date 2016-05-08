#ifndef VMOD_PREHASH_DIRECTOR_MAGIC
struct voverride {
  unsigned              magic;
#define VDIR_OVERRIDE_MAGIC        0xfeb09a2e
  pthread_rwlock_t      mtx;
  unsigned              n_backend;
  unsigned              l_backend;
  VCL_BACKEND           *backend;
  const char            **names;
  struct vmapping       **mapping;
  double                *hashvals;
  struct director       *dir;
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
  const char     *hdr;
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

void voverride_new(struct voverride **vop, struct ws *ws,
    const char *name, const char *vcl_name,
    vdi_healthy_f *healthy, vdi_resolve_f *resolve, void *priv);
void voverride_delete(struct voverride **vop);
void voverride_rdlock(struct voverride *vo);
void voverride_wrlock(struct voverride *vo);
void voverride_unlock(struct voverride *vo);
int voverride_add_backend(struct voverride *vo, VCL_BACKEND be,
                               double hv, const char *name);
VCL_BACKEND voverride_get_be(struct voverride *vo, double hv,
                             const struct busyobj*, struct vmapping **vmp, int *healthy);
void voverride_create_mappings(struct voverride *vo, struct vdir *vd);

const struct director * __match_proto__(vdi_resolve_f)
prehash_random_resolve(const struct director*, struct worker*, struct busyobj*);
const struct director * __match_proto__(vdi_resolve_f)
prehash_rr_resolve(const struct director*, struct worker*, struct busyobj*);

void sha256update(SHA256_CTX *ctx, const char *p, unsigned char digest[32]);
void sha256reset(SHA256_CTX *ctx, SHA256_CTX *base);
#endif /* VMOD_PREHASH_DIRECTOR_MAGIC */
