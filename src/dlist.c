#include <stdlib.h>
#include <stdio.h>
#define DLIST_TYPES_INTERNAL
struct mpr_dlist_t;
typedef struct mpr_dlist_t *mpr_dlist;
#include <mapper/dlist.h>
#undef DLIST_TYPES_INTERNAL
#include <mapper/rc.h>

/* a reference counted list cell */
struct mpr_dlist_t {
    mpr_dlist prev; /* a pointer (not a ref, not counted) to a dlist rc */
    mpr_dlist next; /* a ref (counted) to a dlist rc */
    void * data;    /* the data of the cell */
    mpr_dlist_destructor *destructor;
};

void mpr_dlist_no_destructor(void* ll)
{
    return;
}

void mpr_dlist_internal_destructor(mpr_rc rc)
{
    mpr_dlist ll = (mpr_dlist)rc;
    if (ll->next) {
        ll->next->prev = 0;
        mpr_rc_free(ll->next);
    }
    ll->destructor(ll->data);
}

mpr_dlist mpr_dlist_new(void * data, mpr_dlist_destructor *destructor)
{
    if (0 == data) return 0;
    mpr_dlist ldst = (mpr_dlist)mpr_rc_new(sizeof(struct mpr_dlist_t), &mpr_dlist_internal_destructor);

    ldst->prev = 0;
    ldst->next = 0;
    ldst->data = data;
    ldst->destructor = destructor;
    return ldst;
}

void mpr_dlist_free(mpr_dlist ll)
{
    mpr_rc_free((mpr_rc)ll);
}

//mpr_dlist mpr_dlist_copy(mpr_dlist src)
//{
//    if (src == 0) return; /* conceptually, freeing dst is the same as making it a ref to a null list */
//
//    mpr_dlist cp = (mpr_dlist)mpr_rc_new(sizeof(struct mpr_dlist_t), &mpr_dlist_internal_destructor);
//    if (cp == 0) return;
//
//    cp->prev = src->prev;
//    cp->next = (mpr_dlist)mpr_rc_make_ref((mpr_rc)src->next);
//    cp->data = mpr_rc_make_ref(src->data);
//
//    return cp;
//}

mpr_dlist mpr_dlist_make_ref(mpr_dlist dlist)
{
    return (mpr_dlist)mpr_rc_make_ref((mpr_rc)dlist);
}

void mpr_dlist_move(mpr_dlist *dst, mpr_dlist *src)
{
    if (src == 0) return;
    if (dst == 0) {
        mpr_dlist_free(*src);
        *src = 0;
        return;
    }
    if (*dst != 0) mpr_dlist_free(*dst);
    *dst = *src;
    *src = 0;
}

mpr_rc mpr_dlist_data(mpr_dlist dlist)
{
    return dlist->data;
}

static inline void _insert_cell(mpr_dlist prev, mpr_dlist ll, mpr_dlist next)
{
    /* we assume ll is not a null list, but either or both of prev and next may be null lists */
    /* link backward */
    if (next) next->prev = ll;
    ll->prev = prev;
    /* link forward */
    if (prev) {
        /* we assume prev->next == next */
        prev->next = mpr_dlist_make_ref(ll); /* ll takes the ref already in prev->next, next's refcount is unchanged */
        ll->next = next;
    }
    else { /* no prev, i.e. next currently has no precedent */
        /* next is now referred to by ll where before it had no incoming ref */
        ll->next = mpr_rc_make_ref((mpr_rc)next); 
    }
}

void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, void * data, mpr_dlist_destructor *destructor)
{
    if (dst == 0 && (iter == 0 || iter->prev == 0)) return;
    mpr_dlist ldst = mpr_dlist_new(data, destructor);
    if (0 == ldst) return;
    mpr_dlist prev = iter ? iter->prev : 0;
    _insert_cell(prev, ldst, iter);
    mpr_dlist_move(dst, &ldst);
}

void mpr_dlist_insert_after(mpr_dlist *dst, mpr_dlist iter, void * data, mpr_dlist_destructor *destructor)
{
    if (dst == 0 && iter == 0) return;
    mpr_dlist ldst = mpr_dlist_new(data, destructor);
    if (0 == ldst) return;
    mpr_dlist next = iter ? iter->next : 0;
    _insert_cell(iter, ldst, next);
    mpr_dlist_move(dst, &ldst);
}

void mpr_dlist_append(mpr_dlist *front, mpr_dlist *back, void * data, mpr_dlist_destructor *destructor)
{
    if (front == 0 || back == 0 || data == 0) return;
    if (*front == 0) {
        mpr_dlist ll = mpr_dlist_new(data, destructor);
        if (0 == ll) return;
        *front = ll;
        /* we assume back must also be 0, since front points to a null list, so no need to free existing back */
        *back = mpr_dlist_make_ref(ll);
        return;
    }
    mpr_dlist lback = 0;
    if (*back) mpr_dlist_get_back(&lback, *back);
    else mpr_dlist_get_back(&lback, *front);
    mpr_dlist_insert_after(&lback, lback, data, destructor);
    mpr_dlist_move(back, &lback);
}

void mpr_dlist_prepend(mpr_dlist *front, void * data, mpr_dlist_destructor *destructor)
{
    if (front == 0) return;
    if (*front == 0) {
        *front = mpr_dlist_new(data, destructor);
        return;
    }
    mpr_dlist_get_front(front, *front);
    mpr_dlist_insert_before(front, *front, data, destructor);
}

static inline void _remove_cell(mpr_dlist prev, mpr_dlist ll, mpr_dlist next)
{
    /* we assume ll is not a null list, but either or both of prev and next may be */
	if (prev) {
    	mpr_dlist_free(prev->next);
		prev->next = mpr_rc_make_ref((mpr_rc)next);
	}
	if (next) next->prev = prev;
	if (ll) {
		ll->prev = 0;
		mpr_dlist_free(ll->next);
		ll->next = 0;
	}
}

void mpr_dlist_pop(mpr_dlist *dst, mpr_dlist *iter)
{
	if (iter == 0) return;
	if (dst) {
    	mpr_dlist_free(*dst);
    	*dst = 0;
	}
	if (*iter == 0) return; /* freeing dst is equivalent to popping a null list into it */
	mpr_dlist prev, ll, next;
	prev = ll = next = 0;
	ll = mpr_dlist_make_ref(*iter);
	mpr_dlist_next(iter);
	prev = ll->prev;
	next = *iter;
	_remove_cell(prev, ll, next);
	mpr_dlist_move(dst, &ll);
}

void mpr_dlist_rpop(mpr_dlist *dst, mpr_dlist *iter)
{
	if (iter == 0) return;
	if (dst) {
    	mpr_dlist_free(*dst);
    	*dst = 0;
	}
	if (*iter == 0) return; /* freeing dst is equivalent to popping a null list into it */
	mpr_dlist prev, ll, next;
	prev = ll = next = 0;
	ll = mpr_dlist_make_ref(*iter);
	mpr_dlist_prev(iter);
	next = ll->next;
	prev = *iter;
	_remove_cell(prev, ll, next);
	mpr_dlist_move(dst, &ll);
}

void mpr_dlist_next(mpr_dlist *iter)
{
    if (iter == 0 || *iter == 0) return;
    mpr_dlist next = (*iter)->next;
    if (next == 0) {
        mpr_dlist_free(*iter);
        *iter = 0;
        return;
    }
    mpr_dlist ldst = mpr_dlist_make_ref(next);
    mpr_dlist_move(iter, &ldst);
}

void mpr_dlist_prev(mpr_dlist *iter)
{
    if (iter == 0 || *iter == 0) return;
    mpr_dlist prev = (*iter)->prev;
    if (prev == 0) {
        mpr_dlist_free(*iter);
        *iter = 0;
        return;
    }
    mpr_dlist ldst = mpr_dlist_make_ref(prev);
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
        if (dst) {
            mpr_dlist front = mpr_dlist_make_ref(iter);
            mpr_dlist_move(dst, &front);
        }
        return 1;
    }
    size_t count = 1;
    while (iter->prev) {
        ++count;
        iter = iter->prev;
    }
    if (dst) {
        mpr_dlist front = mpr_dlist_make_ref(iter);
        mpr_dlist_move(dst, &front);
    }
    return count;
}

size_t mpr_dlist_get_back(mpr_dlist *dst, mpr_dlist iter)
{
    if (iter == 0) return 0;
    if (iter->next == 0) {
        if (dst) {
            mpr_dlist back = mpr_dlist_make_ref(iter);
            mpr_dlist_move(dst, &back);
        }
        return 1;
    }
    size_t count = 1;
    while (iter->next) {
        ++count;
        iter = iter->next;
    }
    if (dst) {
        mpr_dlist back = mpr_dlist_make_ref(iter);
        mpr_dlist_move(dst, &back);
    }
    return count;
}

size_t mpr_dlist_refcount(mpr_dlist dlist)
{
    return mpr_rc_refcount((mpr_rc)dlist);
}

int mpr_dlist_equals(mpr_dlist a, mpr_dlist b)
{
    return a == b   
           ||(  ( a->next == b->next )
             && ( a->prev == b->prev )
             && ( a->data == b->data )
             );
}

mpr_dlist mpr_dlist_new_filter(mpr_dlist src, mpr_dlist_filter_predicate *cb, const char * types, ...)
{
    return 0;
}

void mpr_dlist_evaluate_filter(mpr_dlist filter_front)
{
    return;
}
