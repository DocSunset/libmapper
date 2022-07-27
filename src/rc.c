#include <mapper/rc.h>
#include <stdlib.h>

/* Local definition of the rc struct type. (lrc is for 'local reference counter') */
typedef struct _mpr_lrc_t {
    /* reference count. The cell will be actually freed by calling the destructor when this drops to 0 */
    size_t refcount;
    mpr_rc_data_destructor *destructor;
    /* stub for a memory resource.
     * A pointer to `data` is the outward-facing interface to the rc API; the destructor and ref count
     * are hidden in front of the data so that the rc can be treated as being a pointer to its data. */
    void *data;
} mpr_lrc_t, *mpr_lrc;

/* hopefully the compiler is smart enough to make this a compile-time constant
 * and never actually dereference the pointer... */
static inline size_t _lrc_header_size()
{
    mpr_lrc lrc;
    return (char*)&lrc->data - (char*)lrc;
}

/* Get the lrc header from an rc, which is a pointer to the lrc's data member */
static inline mpr_lrc _lrc_from_rc(mpr_rc rc)
{
    return (mpr_lrc)( (char*)rc - _lrc_header_size() );
}

/* Get the rc (pointer to data) from an lrc (pointer to header) */
static inline mpr_rc _rc_from_lrc(mpr_lrc lrc)
{
    return (mpr_rc)&lrc->data;
}

mpr_rc mpr_rc_new(size_t size, mpr_rc_data_destructor *destructor)
{
    mpr_lrc lrc = calloc(1, size + _lrc_header_size());
    if (0 == lrc) return 0;
    lrc->refcount = 1;
    lrc->destructor = destructor;
    return _rc_from_lrc(lrc);
}

mpr_rc mpr_rc_make_ref(mpr_rc rc)
{
    if (0 == rc) return 0;
    mpr_lrc lrc = _lrc_from_rc(rc);
    ++lrc->refcount;
    return rc;
}

void mpr_rc_free(mpr_rc rc)
{
    if (0 == rc) return;
    mpr_lrc lrc = _lrc_from_rc(rc);
    if (--lrc->refcount == 0) {
        lrc->destructor(rc);
        free(lrc);
    }
}

size_t mpr_rc_refcount(mpr_rc rc)
{
    if (0 == rc) return 0;
    return _lrc_from_rc(rc)->refcount;
}
