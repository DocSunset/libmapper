#include "dlist.h"
#include <stdlib.h>
#include <stdio.h>

struct _mpr_dlist_t;

typedef union {
    void *mem;
    struct _mpr_dlist_t * ref;
} _data;

typedef struct _mpr_dlist_t {
    struct _mpr_dlist_t *prev;
    struct _mpr_dlist_t *next;
    mpr_dlist_data_destructor *destructor;
    size_t refcount;
    _data data;
} mpr_dlist_t;

typedef mpr_dlist_t* _dlist;

/* Throughout here, `mpr_dlist` variables are usually named `dst`, `src`, `iter`, or `dlist`.
 * When converted to `_dlist`, the variable is named `ldst`, `lsrc`, `lit`, or `ll`,
 * where the `l` prefix is thought to be for "local", as in, this variable is represented with a type
 * that is only known locally in this compilation unit. */

/* Manual reference counting is tricky. To try to keep things obvious, any time a pointer is assigned
 * that points to a list, there should be a refcount inc/decrement on the same line. Use ++ and --
 * throughout. */

void mpr_dlist_new(mpr_dlist *dst, void * data, mpr_dlist_data_destructor *destructor)
{
    if (dst == 0 || data == 0 || destructor == 0) return;
    if (*dst != 0) mpr_dlist_free(dst);
    _dlist ldst = calloc(1, sizeof(mpr_dlist_t));
    if (ldst == 0) return;

    ldst->prev = 0;
    ldst->next = 0;
    ldst->destructor = destructor;
    ldst->data.mem = data;

    *dst = (void*)ldst; ++ldst->refcount;
}

void mpr_dlist_free(mpr_dlist *dlist)
{
    if (dlist == 0) return;
    _dlist ll = *((_dlist *)dlist);
    if (ll->prev) --ll->prev->refcount;
    if (ll->next) --ll->next->refcount;
    /* todo: recursively free neighbours */
    *dlist = 0; --ll->refcount;
    if (0 == ll->refcount) {
        if (ll->destructor) ll->destructor(ll->data.mem);
        else --ll->data.ref->refcount;
        free(ll);
    }
    /* if there are still references to ll, don't actually free it. */
}

void mpr_dlist_copy(mpr_dlist *dst, mpr_dlist src)
{
    if (dst == 0) return;
    if (*dst != 0) mpr_dlist_free(dst);
    if (src == 0) return; /* conceptually, freeing dst is the same as making it a copy of a null list */

    _dlist lsrc = (_dlist)src;
    _dlist ldst = calloc(1, sizeof(mpr_dlist_t));
    if (ldst == 0) return;

    ldst->prev = lsrc->prev; if (ldst->prev) ++ldst->prev->refcount;
    ldst->next = lsrc->next; if (ldst->next) ++ldst->next->refcount;
    ldst->destructor = 0; /* copies refer to their parent's data and don't have a destructor */
    ldst->data.ref = lsrc; ++lsrc->refcount;

    *dst = (void*)ldst; ++ldst->refcount; 
}

void mpr_dlist_move(mpr_dlist *dst, mpr_dlist *src)
{
    if (dst == 0 || src == 0) return;
    if (*dst != 0) mpr_dlist_free(dst);
    *dst = *src;
    *src = 0;
}

const void * mpr_dlist_data(mpr_dlist dlist)
{
    _dlist ll = (_dlist)dlist;
    if (ll->destructor) return ll->data.mem;
    else return ((_dlist)ll->data.ref)->data.mem;
}

size_t mpr_dlist_get_refcount(mpr_dlist dlist)
{
    return ((_dlist)dlist)->refcount;
}

int mpr_dlist_equals(mpr_dlist ap, mpr_dlist bp)
{
    _dlist la = (_dlist)ap;
    _dlist lb = (_dlist)bp;
    return     ( la->next == lb->next )
            && ( la->prev == lb->prev )
            && ( la->destructor ? la->data.mem == lb->data.ref->data.mem
                                : la->data.ref->data.mem == lb->data.mem );
}
