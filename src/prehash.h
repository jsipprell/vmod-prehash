#ifndef VMOD_PREHASH_DIRECTOR_MAGIC
struct vmod_prehash_director {
  unsigned        magic;
#define VMOD_PREHASH_DIRECTOR_MAGIC 0x3fe1beef
  const char     *hdr;
  struct vdir    *prevd, *vd, *lrvd;

  struct         ws *ws;
  unsigned char __scratch[4096];
};

struct vmod_prehash_lastresort_director {
  unsigned       magic;
#define VMOD_PREHASH_LASTRESORT_MAGIC 0x9667b1af
  struct vdir    *vd;
  unsigned       nxt;
};

struct voverride {
  unsigned        magic;
#define VDIR_OVERRIDE_MAGIC        0xfeb09a2e
  pthread_rwlock_t      mtx;
  unsigned        n_backend;
  unsigned        l_backend;
  VCL_BACKEND       *backend;
  char              **names;
  double             *hashvals;
  struct director       *dir;
  struct ws             *ws;
};

void voverride_new(struct voverride **vop, struct ws *ws,
    const char *name, const char *vcl_name,
    vdi_healthy_f *healthy, vdi_resolve_f *resolve, void *priv);
void voverride_delete(struct voverride **vop);
void voverride_rdlock(struct voverride *vo);
void voverride_wrlock(struct voverride *vo);
void voverride_unlock(struct voverride *vo);
unsigned voverride_add_backend(struct voverride *vo, VCL_BACKEND be,
                               double hv, const char *name);
VCL_BACKEND voverride_get_be(struct voverride *vo, double hv,
                             const struct busyobj*, int *healthy);

const struct director * __match_proto__(vdi_resolve_f)
prehash_random_resolve(const struct director*, struct worker*, struct busyobj*);
const struct director * __match_proto__(vdi_resolve_f)
prehash_rr_resolve(const struct director*, struct worker*, struct busyobj*);

#endif /* VMOD_PREHASH_DIRECTOR_MAGIC */
