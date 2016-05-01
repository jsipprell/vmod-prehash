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

extern const struct director * __match_proto__(vdi_resolve_f)
prehash_random_resolve(const struct director*, struct worker*, struct busyobj*);
extern const struct director * __match_proto__(vdi_resolve_f)
prehash_rr_resolve(const struct director*, struct worker*, struct busyobj*);

#endif /* VMOD_PREHASH_DIRECTOR_MAGIC */
