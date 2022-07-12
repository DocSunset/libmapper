#define DLIST_TYPES_INTERNAL
#include <stddef.h>
struct _mpr_dlist_t;

typedef union {
    void *mem;
    struct _mpr_dlist_t * ref;
} _data;

/* Handler function type for freeing memory cared for by a `mpr_dlist`.
 * This is called automatically if the reference count in a `mpr_dlist` cell drops to zero. */
typedef void mpr_dlist_data_destructor(void * data);

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

typedef mpr_dlist_t* mpr_dlist;

#include <mapper/dlist.h>
#undef DLIST_TYPES_INTERNAL

#include <stdlib.h>
#include <stdio.h>

/* Manual reference counting is tricky. To try to keep things obvious, any time a pointer is assigned
 * that points to a list, there should be a refcount inc/decrement on the same line. Use _incref
 * and _decref throughout. */

/* Note that only `next` and `data.ref` references are counted; otherwise every pair of cells would
 * form a reference cycle and you could never free a list. */

static inline void _incref(mpr_dlist ll)
{
    if (ll) ++ll->refcount;
}

static int _maybe_really_free(mpr_dlist);

static inline void _decref(mpr_dlist *ll)
{
    if (ll && *ll) --(*ll)->refcount;
    if (ll && _maybe_really_free(*ll)) *ll = 0;
}

void mpr_dlist_new(mpr_dlist *dst, void ** data, size_t size, mpr_dlist_data_destructor *destructor)
{
    if (dst == 0 || destructor == 0) return;
    void * _data;
    if (data == 0 || *data == 0) {
        if (size == 0) return;
        _data = calloc(1, size);
        if (_data == 0) return;
        if (data != 0) *data = _data;
    } else _data = *data;

    /* could we optimize this by reusing *dst's memory instead of freeing and newly callocing */
    if (*dst != 0) mpr_dlist_free(dst);
    mpr_dlist ldst = calloc(1, sizeof(mpr_dlist_t));
    if (ldst == 0) {
        if (size != 0) free(data);
        return;
    }

    ldst->prev = 0;
    ldst->next = 0;
    ldst->destructor = destructor;
    ldst->data.mem = _data;

    *dst = ldst; _incref(ldst);
}

void mpr_dlist_no_destructor(void* data) {return;};

static int _maybe_really_free(mpr_dlist ll)
{
	if (ll == 0) return 0;
    mpr_dlist next = ll->next;
    if (0 == ll->refcount) {

        /* yep, really free it. */
        if (ll->destructor) ll->destructor(ll->data.mem);
        else {
	        mpr_dlist ref = ll->data.ref;
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
    _decref(dlist);
    *dlist = 0;
}

void mpr_dlist_copy(mpr_dlist *dst, mpr_dlist src)
{
    /* could we optimize by reusing memory in case dst is freed? */
    if (dst == 0) return;
    if (*dst != 0) mpr_dlist_free(dst);
    if (src == 0) return; /* conceptually, freeing dst is the same as making it a ref to a null list */

    mpr_dlist ldst = calloc(1, sizeof(mpr_dlist_t));
    if (ldst == 0) return;

    ldst->prev = src->prev;
    ldst->next = src->next; _incref(src->next);
    ldst->destructor = 0; /* copies refer to their parent's data and don't have a destructor */
    ldst->data.ref = src; _incref(src);

    *dst = ldst; _incref(ldst);
}

void mpr_dlist_make_ref(mpr_dlist *dst, mpr_dlist src)
{
    if (dst == 0) return;
    if (*dst != 0) {
        if (*dst == src) return;
        else mpr_dlist_free(dst);
    }
    if (src == 0) return;
    *dst = src; _incref(src);
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
    if (dlist->destructor) return dlist->data.mem;
    else return dlist->data.ref->data.mem;
}

static inline void _insert_cell(mpr_dlist prev, mpr_dlist ll, mpr_dlist next)
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

void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, void **data, size_t size
                            , mpr_dlist_data_destructor *destructor)
{
    if (dst == 0 && (iter == 0 || iter->prev == 0)) return;
    mpr_dlist ldst = 0; mpr_dlist_new(&ldst, data, size, destructor);
    mpr_dlist prev = iter ? iter->prev : 0;
    _insert_cell(prev, ldst, iter);
    mpr_dlist_move(dst, &ldst);
}

void mpr_dlist_insert_after(mpr_dlist *dst, mpr_dlist iter, void **data, size_t size
                           , mpr_dlist_data_destructor *destructor)
{
    if (dst == 0 && iter == 0) return;
    mpr_dlist ldst = 0; mpr_dlist_new((mpr_dlist*)&ldst, data, size, destructor);
    mpr_dlist next = iter ? iter->next : 0;
    _insert_cell(iter, ldst, next);
    mpr_dlist_move(dst, &ldst);
}

void mpr_dlist_append(mpr_dlist *front, mpr_dlist *back, void **data, size_t size
                     , mpr_dlist_data_destructor *destructor)
{
    if (front == 0) return;
    if (*front == 0) {
        mpr_dlist_new(front, data, size, destructor);
        mpr_dlist_make_ref(back, *front);
        return;
    }
    mpr_dlist lback = 0;
    if (*back) mpr_dlist_get_back(&lback, *back);
    else mpr_dlist_get_back(&lback, *front);
    mpr_dlist_insert_after(&lback, lback, data, size, destructor);
    mpr_dlist_move(back, &lback);
}

void mpr_dlist_prepend(mpr_dlist *front, void **data, size_t size
                     , mpr_dlist_data_destructor *destructor)
{
    if (front == 0) return;
    if (*front == 0) return mpr_dlist_new(front, data, size, destructor);
    mpr_dlist_get_front(front, *front);
    mpr_dlist_insert_before(front, *front, data, size, destructor);
}

static inline void _remove_cell(mpr_dlist prev, mpr_dlist ll, mpr_dlist next)
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
	mpr_dlist prev, ll, next;
	prev = ll = next = 0;
	mpr_dlist_make_ref(&ll, *iter);
	mpr_dlist_next(iter);
	prev = ll->prev;
	next = *iter;
	_remove_cell(prev, ll, next);
	mpr_dlist_move(dst, &ll);
}

void mpr_dlist_rpop(mpr_dlist *dst, mpr_dlist *iter)
{
	if (iter == 0) return;
	mpr_dlist_free(dst);
	if (*iter == 0) return; /* freeing dst is equivalent to popping a null list into it */
	mpr_dlist prev, ll, next;
	prev = ll = next = 0;
	mpr_dlist_make_ref((mpr_dlist*)&ll, *iter);
	mpr_dlist_prev(iter);
	next = ll->next;
	prev = *iter;
	_remove_cell(prev, ll, next);
	mpr_dlist_move(dst, (mpr_dlist*)&ll);
}

void mpr_dlist_next(mpr_dlist *iter)
{
    if (iter == 0 || *iter == 0) return;
    mpr_dlist next = (*iter)->next;
    if (next == 0) return mpr_dlist_free(iter);
    mpr_dlist ldst = 0; mpr_dlist_make_ref(&ldst, next);
    mpr_dlist_move(iter, &ldst);
}

void mpr_dlist_prev(mpr_dlist *iter)
{
    if (iter == 0 || *iter == 0) return;
    mpr_dlist prev = (*iter)->prev;
    if (prev == 0) return mpr_dlist_free(iter);
    mpr_dlist ldst = 0; mpr_dlist_make_ref(&ldst, prev);
    mpr_dlist_move(iter, &ldst);
}

size_t mpr_dlist_get_length(mpr_dlist dlist)
{
    return mpr_dlist_get_front(0, dlist) + mpr_dlist_get_back(0, dlist) - 1;
}

size_t mpr_dlist_get_front(mpr_dlist *dst, mpr_dlist iter)
{
    if (iter == 0) return 0;
    if (iter->prev == 0) {
        mpr_dlist_make_ref(dst, iter);
        return 1;
    }
    size_t count = 1;
    while (iter->prev) {
        ++count;
        iter = iter->prev;
    }
    mpr_dlist_make_ref(dst, iter);
    return count;
}

size_t mpr_dlist_get_back(mpr_dlist *dst, mpr_dlist iter)
{
    if (iter == 0) return 0;
    if (iter->next == 0) {
        mpr_dlist_make_ref(dst, iter);
        return 1;
    }
    size_t count = 1;
    while (iter->next) {
        ++count;
        iter = iter->next;
    }
    mpr_dlist_make_ref(dst, iter);
    return count;
}

size_t mpr_dlist_get_refcount(mpr_dlist dlist)
{
    return dlist->refcount;
}

int mpr_dlist_equals(mpr_dlist a, mpr_dlist b)
{
    return  a == b   
          || ( ( a->next == b->next )
            && ( a->prev == b->prev )
            && ( a->destructor ? a->data.mem == b->data.ref->data.mem
                                : a->data.ref->data.mem == b->data.mem )
             );
}
