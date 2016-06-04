#ifdef HAVE_ATOMICPTR

typedef struct { const void* volatile _atomic; } __attribute__((aligned(SIZEOF_VOIDP))) atomic_ptr_t;

#define ATOMIC_SET(vd,ap,v) ((ap)._atomic = (v))
#define ATOMIC_GET(vd,ap) ((ap)._atomic)

#else /* !HAVE_ATOMICPTR */

typedef const void* atomic_ptr_t;

#define ATOMIC_SET(vd,ap,v) do { vdir_wrlock((vd)); (ap) = (v); vdir_unlock((vd)); } while(0)
#define ATOMIC_GET atomic_get
static inline const void* atomic_get(struct vdir *vd, atomic_ptr_t ap) {
  const void *v;

  vdir_rdlock(vd);
  v = ap;
  vdir_unlock(vd);
  return v;
}

#endif
