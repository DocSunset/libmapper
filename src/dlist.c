#include "dlist.h"
#include <stdlib.h>
#include <stdio.h>

struct _mpr_dlist_t;

typedef union {
    void *mem;
    struct _mpr_dlist_t * ref;
} _data;

/* a reference counted list cell */
typedef struct _mpr_dlist_t {
    /* list links */
    struct _mpr_dlist_t *prev;
    struct _mpr_dlist_t *next;
    /* data destructor, or 0 if this is a cell that refers to another cell */
    mpr_dlist_data_destructor *destructor;
    /* reference count. The cell should be automatically freed when this drops to 0 */
    size_t refcount;
    /* pointer to a memory resource or another list cell */
    _data data;
} mpr_dlist_t;

typedef mpr_dlist_t* _dlist;

/* Throughout here, `mpr_dlist` variables are usually named `dst`, `src`, `iter`, or `dlist`.
 * When converted to `_dlist`, the variable is named `ldst`, `lsrc`, `lit`, or `ll`,
 * where the `l` prefix is thought to be for "local", as in, this variable is represented with a type
 * that is only known locally in this compilation unit. */

/* Manual reference counting is tricky. To try to keep things obvious, any time a pointer is assigned
 * that points to a list, there should be a refcount inc/decrement on the same line. Use _incref
 * and _decref throughout. */

/* Note that only `next` and `data.ref` references are counted; otherwise every pair of cells would
 * form a reference cycle and you could never free a list. */

static inline void _incref(_dlist ll)
{
    if (ll) ++ll->refcount;
}

static int _maybe_really_free(_dlist);

static inline void _decref(_dlist *ll)
{
    if (ll && *ll) --(*ll)->refcount;
    if (ll && _maybe_really_free(*ll)) *ll = 0;
}

void mpr_dlist_new(mpr_dlist *dst, void * data, size_t size, mpr_dlist_data_destructor *destructor)
{
    if (dst == 0 || destructor == 0) return;
    if (data == 0) {
        if (size == 0) return;
        data = calloc(1, size);
        if (data == 0) return;
    }
    /* we could optimize this by reusing *dst's memory instead of freeing and newly callocing */
    if (*dst != 0) mpr_dlist_free(dst);
    _dlist ldst = calloc(1, sizeof(mpr_dlist_t));
    if (ldst == 0) {
        if (size != 0) free(data);
        return;
    }

    ldst->prev = 0;
    ldst->next = 0;
    ldst->destructor = destructor;
    ldst->data.mem = data;

    *dst = (mpr_dlist*)ldst; _incref(ldst);
}

static int _maybe_really_free(_dlist ll)
{
	if (ll == 0) return 0;
    _dlist next = ll->next;
    if (0 == ll->refcount) {

        /* yep, really free it. */
        if (ll->destructor) ll->destructor(ll->data.mem);
        else {
	        _dlist ref = ll->data.ref;
	        _decref(&ref);
        }
        free(ll);

        /* recursively free neighbours */
        if (next) {
            _decref(&next);
            if (next) next->prev = 0;
        }

        /* no need to update ll->prev, since if it existed we wouldn't free ll */

        return 1;
    }
    /* else since there are still references to ll, don't actually free it. */
    return 0;
}

void mpr_dlist_free(mpr_dlist *dlist)
{
    if (dlist == 0 || *dlist == 0) return;
    _dlist ll = *((_dlist *)dlist);
    *dlist = 0; _decref(&ll);
}

void mpr_dlist_copy(mpr_dlist *dst, mpr_dlist src)
{
    /* could we optimize by reusing memory in case dst is freed? */
    if (dst == 0) return;
    if (*dst != 0) mpr_dlist_free(dst);
    if (src == 0) return; /* conceptually, freeing dst is the same as making it a ref to a null list */

    _dlist lsrc = (_dlist)src;
    _dlist ldst = calloc(1, sizeof(mpr_dlist_t));
    if (ldst == 0) return;

    ldst->prev = lsrc->prev;
    ldst->next = lsrc->next; _incref(lsrc->next);
    ldst->destructor = 0; /* copies refer to their parent's data and don't have a destructor */
    ldst->data.ref = lsrc; _incref(lsrc);

    *dst = (mpr_dlist*)ldst; _incref(ldst);
}

void mpr_dlist_make_ref(mpr_dlist *dst, mpr_dlist src)
{
    if (dst == 0) return;
    if (*dst != 0) mpr_dlist_free(dst);
    if (src == 0) return;
    *dst = src; _incref((_dlist)src);
}

void mpr_dlist_move(mpr_dlist *dst, mpr_dlist *src)
{
    if (src == 0) return;
    if (dst == 0) return mpr_dlist_free(src);
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

static inline void _insert_cell(_dlist prev, _dlist ll, _dlist next)
{
    /* link backward */
    if (next) next->prev = ll;
    if (ll) ll->prev = prev;
    /* link forward */
    if (prev) {
        prev->next = ll; _incref(ll); /* next's refcount is unchanged */
    }
    else {
        if (ll) _incref(next); /* next used to have no left neighbour, and now is referred to by ll */
    }
    if (ll) ll->next = next; 
}

void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, void * data, size_t size
                            , mpr_dlist_data_destructor *destructor)
{
    _dlist lit = (_dlist)iter;
    if (dst == 0 && (lit == 0 || lit->prev == 0)) return;
    _dlist ldst = 0; mpr_dlist_new((mpr_dlist*)&ldst, data, size, destructor);
    _dlist prev = lit ? lit->prev : 0;
    _insert_cell(prev, ldst, lit);
    mpr_dlist_move(dst, (mpr_dlist*)&ldst);
}

void mpr_dlist_insert_after(mpr_dlist *dst, mpr_dlist iter, void * data, size_t size
                           , mpr_dlist_data_destructor *destructor)
{
    if (dst == 0 && iter == 0) return;
    _dlist lit = (_dlist)iter;
    _dlist ldst = 0; mpr_dlist_new((mpr_dlist*)&ldst, data, size, destructor);
    _dlist next = lit ? lit->next : 0;
    _insert_cell(lit, ldst, next);
    mpr_dlist_move(dst, (mpr_dlist*)&ldst);
}

static inline void _remove_cell(_dlist prev, _dlist ll, _dlist next)
{
	if (prev) {
		prev->next = next; _decref(&ll); _incref(next);
	}
	if (next) next->prev = prev;
	if (ll) {
		ll->prev = 0;
		ll->next = 0; _decref(&next);
	}
}

void mpr_dlist_pop(mpr_dlist *dst, mpr_dlist *iter)
{
	if (iter == 0) return;
	mpr_dlist_free(dst);
	if (*iter == 0) return; /* freeing dst is equivalent to popping a null list into it */
	_dlist prev, ll, next;
	prev = ll = next = 0;
	mpr_dlist_make_ref((mpr_dlist*)&ll, *iter);
	mpr_dlist_next(iter);
	prev = ll->prev;
	next = (_dlist)*iter;
	_remove_cell(prev, ll, next);
	mpr_dlist_move(dst, (mpr_dlist*)&ll);
}

void mpr_dlist_rpop(mpr_dlist *dst, mpr_dlist *iter)
{
	if (iter == 0) return;
	mpr_dlist_free(dst);
	if (*iter == 0) return; /* freeing dst is equivalent to popping a null list into it */
	_dlist prev, ll, next;
	prev = ll = next = 0;
	mpr_dlist_make_ref((mpr_dlist*)&ll, *iter);
	mpr_dlist_prev(iter);
	next = ll->next;
	prev = (_dlist)*iter;
	_remove_cell(prev, ll, next);
	mpr_dlist_move(dst, (mpr_dlist*)&ll);
}

void mpr_dlist_next(mpr_dlist *iter)
{
    if (iter == 0 || *iter == 0) return;
    _dlist lit = *(_dlist*)iter;
    if (lit->next == 0) return mpr_dlist_free(iter);
    _dlist ldst = 0; mpr_dlist_make_ref((mpr_dlist*)&ldst, (mpr_dlist)lit->next);
    mpr_dlist_move(iter, (mpr_dlist*)&ldst);
}

void mpr_dlist_prev(mpr_dlist *iter)
{
    if (iter == 0 || *iter == 0) return;
    _dlist lit = *(_dlist*)iter;
    if (lit->prev == 0) return mpr_dlist_free(iter);
    _dlist ldst = 0; mpr_dlist_make_ref((mpr_dlist*)&ldst, (mpr_dlist)lit->prev);
    mpr_dlist_move(iter, (mpr_dlist*)&ldst);
}

size_t mpr_dlist_get_length(mpr_dlist dlist)
{
    return mpr_dlist_get_front(0, dlist) + mpr_dlist_get_back(0, dlist) - 1;
}

size_t mpr_dlist_get_front(mpr_dlist *dst, mpr_dlist iter)
{
    _dlist lit = (_dlist)iter;
    if (lit == 0) return 0;
    if (lit->prev == 0) {
        mpr_dlist_make_ref(dst, iter);
        return 1;
    }
    size_t count = 1;
    while (lit->prev) {
        ++count;
        lit = lit->prev;
    }
    mpr_dlist_make_ref(dst, (mpr_dlist*)&lit);
    return count;
}

size_t mpr_dlist_get_back(mpr_dlist *dst, mpr_dlist iter)
{
    _dlist lit = (_dlist)iter;
    if (lit == 0) return 0;
    if (lit->next == 0) {
        mpr_dlist_make_ref(dst, iter);
        return 1;
    }
    size_t count = 1;
    while (lit->next) {
        ++count;
        lit = lit->next;
    }
    mpr_dlist_make_ref(dst, (mpr_dlist*)&lit);
    return count;
}

size_t mpr_dlist_get_refcount(mpr_dlist dlist)
{
    return ((_dlist)dlist)->refcount;
}

int mpr_dlist_equals(mpr_dlist ap, mpr_dlist bp)
{
    _dlist la = (_dlist)ap;
    _dlist lb = (_dlist)bp;
    return  la == lb   
          || ( ( la->next == lb->next )
            && ( la->prev == lb->prev )
            && ( la->destructor ? la->data.mem == lb->data.ref->data.mem
                                : la->data.ref->data.mem == lb->data.mem )
             );
}
