#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#define DLIST_TYPES_INTERNAL
struct mpr_dlist_t;
typedef struct mpr_dlist_t *mpr_dlist;
#include <mapper/dlist.h>
#undef DLIST_TYPES_INTERNAL
#include "util/debug_macro.h"

#define DLIST_ELEMENTS \

/* a reference counted list cell */
struct mpr_dlist_t {
    int query;
    mpr_dlist prev; /* a pointer (not a ref, not counted) to a dlist rc */
    mpr_dlist next; /* a ref (counted) to a dlist rc */
    mpr_rc data;    /* the data of the cell */
};

typedef struct _mpr_dlist_query_t {
    mpr_dlist parent;
    mpr_dlist_filter_predicate *predicate;
    char * types;
    mpr_union *va;
} mpr_dlist_query_t, *mpr_dlist_query;

#undef DLIST_ELEMENTS

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
    mpr_rc_free(ll->data);
}

mpr_dlist mpr_dlist_new(mpr_rc data)
{
    mpr_dlist ldst = (mpr_dlist)mpr_rc_new(sizeof(struct mpr_dlist_t), &mpr_dlist_internal_destructor);
    TRACE_RETURN_UNLESS(ldst, 0, "Unable to alloc via mpr_rc_new in mpr_dlist_new.\n");

    ldst->prev = 0;
    ldst->next = 0;
    ldst->data = data;
    ldst->query = 0;
    return ldst;
}

void mpr_dlist_free(mpr_dlist ll)
{
    mpr_rc_free((mpr_rc)ll);
}

mpr_dlist mpr_dlist_copy(mpr_dlist src)
{
    if (src == 0) return 0;

    mpr_dlist cp = (mpr_dlist)mpr_rc_new(sizeof(struct mpr_dlist_t), &mpr_dlist_internal_destructor);
    TRACE_RETURN_UNLESS(cp, 0, "Unable to alloc via mpr_rc_new in mpr_dlist_copy.\n");

    cp->prev = src->prev;
    cp->next = mpr_dlist_make_ref(src->next);
    cp->data = mpr_rc_make_ref(src->data);

    return cp;
}

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
        ll->next = mpr_dlist_make_ref(next); 
    }
}

void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, mpr_rc rc)
{
    if (dst == 0 && (iter == 0 || iter->prev == 0)) return;
    mpr_dlist ldst = mpr_dlist_new(rc);
    if (0 == ldst) return;
    mpr_dlist prev = iter ? iter->prev : 0;
    _insert_cell(prev, ldst, iter);
    mpr_dlist_move(dst, &ldst);
}

void mpr_dlist_insert_after(mpr_dlist *dst, mpr_dlist iter, mpr_rc rc)
{
    if (dst == 0 && iter == 0) return;
    mpr_dlist ldst = mpr_dlist_new(rc);
    if (0 == ldst) return;
    mpr_dlist next = iter ? iter->next : 0;
    _insert_cell(iter, ldst, next);
    mpr_dlist_move(dst, &ldst);
}

void mpr_dlist_append(mpr_dlist *front, mpr_dlist *back, mpr_rc rc)
{
    if (front == 0 || back == 0 || rc == 0) return;
    if (*front == 0) {
        mpr_dlist ll = mpr_dlist_new(rc);
        if (0 == ll) return;
        *front = ll;
        /* we assume back must also be 0, since front points to a null list, so no need to free existing back */
        *back = mpr_dlist_make_ref(ll);
        return;
    }
    mpr_dlist lback = 0;
    if (*back && ((*back)->next != 0)) mpr_dlist_get_back(&lback, *back);
    else mpr_dlist_get_back(&lback, *front);
    mpr_dlist_insert_after(&lback, lback, rc);
    mpr_dlist_move(back, &lback);
}

void mpr_dlist_prepend(mpr_dlist *front, mpr_rc rc)
{
    if (front == 0) return;
    if (*front == 0) {
        *front = mpr_dlist_new(rc);
        return;
    }
    if ((*front)->prev != 0) mpr_dlist_get_front(front, *front);
    mpr_dlist_insert_before(front, *front, rc);
}

static inline void _remove_cell(mpr_dlist prev, mpr_dlist ll, mpr_dlist next)
{
    /* we assume ll is not a null list, but either or both of prev and next may be */
	if (prev) {
    	mpr_dlist_free(prev->next);
		prev->next = mpr_dlist_make_ref(next);
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

void _query_next(mpr_dlist *iter, mpr_dlist ql)
{
    mpr_dlist_query q = mpr_dlist_data_as(mpr_dlist_query, ql);
    /* scan q to the next matching list element 
     * or the end of the list if there are no matches */
    for ( ; q->parent; mpr_dlist_next(&q->parent)) {
        if (q->predicate(mpr_dlist_data(q->parent), q->types, q->va)) {
            break;
        }
    }
    mpr_dlist ret;
    mpr_dlist ll = *iter;
    if (0 == q->parent) {
        ret = 0;
        mpr_dlist_free(ql);
        mpr_dlist_free(*iter);
        *iter = 0;
    }
    else {
        ret = mpr_dlist_new(mpr_rc_make_ref(q->parent->data));
        mpr_dlist_next(&q->parent);
        if (0 == q->parent) {
            mpr_dlist_free(ql);
            ql = 0;
        }
        ret->prev = ll;
        ret->next = ql;
    }
    if (ll) ll->next = mpr_dlist_make_ref(ret);
    mpr_dlist_move(iter, &ret);
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
    if (next->query) return _query_next(iter, (*iter)->next);
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

void mpr_dlist_query_destructor(void *rc)
{
    mpr_dlist_query q = (mpr_dlist_query)rc;
    mpr_rc_free(q->parent);
    free(q->va);
    free(q->types);
}

mpr_dlist mpr_dlist_new_filter(mpr_dlist src, mpr_dlist_filter_predicate *cb, const char * types, ...)
{
    if (0 == src) return 0;

    /* parse args into a mpr_union array */
    size_t argc = 0;
    for (const char *argt = types; *argt != '\0'; ++argt) ++argc;
    mpr_union *argv = calloc(argc, sizeof(mpr_union));
    TRACE_RETURN_UNLESS(argv, 0, "unable to calloc in mpr_dlist_new_filter.\n");
    char *types_cpy = strdup(types);
    if (0 == types_cpy) {
        free(argv);
        trace("Unable to strdup in mpr_dlist_new_filter.\n");
        return 0;
    }
    {
        va_list ap;
        va_start(ap, types);
        for (size_t i = 0; types[i] != '\0'; ++i) switch (types[i]) {
            case MPR_BOOL:
                argv[i].b = va_arg(ap, int);
                break;
            case MPR_TYPE:
                argv[i].c = va_arg(ap, int);
                break;
            case MPR_DBL:
                argv[i].d = va_arg(ap, typeof(argv[i].d));
                break;
            case MPR_FLT:
                argv[i].f = va_arg(ap, double);
                break;
            case MPR_INT64:
                argv[i].h = va_arg(ap, typeof(argv[i].h));
                break;
            case MPR_INT32:
                argv[i].i = va_arg(ap, typeof(argv[i].i));
                break;
            case MPR_STR:
                argv[i].s = va_arg(ap, typeof(argv[i].s));
                break;
            case MPR_TIME:
                argv[i].t = va_arg(ap, typeof(argv[i].t));
                break;
            case MPR_DEV:
            case MPR_SIG_IN:
            case MPR_SIG_OUT:
            case MPR_SIG:
            case MPR_MAP_IN:
            case MPR_MAP_OUT:
            case MPR_MAP:
            case MPR_DATA_SIG:
            case MPR_DATA_MAP:
            case MPR_DATA_OBJ:
            case MPR_OBJ:
            case MPR_LIST:
            case MPR_GRAPH:
            case MPR_PTR:
                argv[i].v = va_arg(ap, typeof(argv[i].v));
                break;
            case MPR_NULL:
                trace("Ignoring MPR_NULL not allowed in mpr_dlist_new_filter.\n", types[i]);
                break;
            default:
                trace("Ingoring unrecognized type %c in mpr_dlist_new_filter.\n", types[i]);
                break;
        }
    }

    mpr_dlist_query q = (mpr_dlist_query)mpr_rc_new(sizeof(mpr_dlist_query_t), &mpr_dlist_query_destructor);
    if (0 == q) {
        free(argv);
        free(types_cpy);
        trace("unable to mpr_rc_new in mpr_dlist_new_filter.\n");
        return 0;
    }
    q->parent = mpr_dlist_make_ref(src);
    q->predicate = cb;
    q->types = types_cpy;
    q->va = argv;

    mpr_dlist ll = mpr_dlist_new((mpr_rc)q);
    if (0 == ll) {
        mpr_rc_free(q);
        trace("unable to mpr_dlist_new in mpr_dlist_new_filter.\n");
        return 0;
    }
    ll->query = 1;
    mpr_dlist ret = 0;
    _query_next(&ret, ll);

    return ret;
}

void mpr_dlist_evaluate_filter(mpr_dlist front)
{
    mpr_dlist iter = mpr_dlist_make_ref(front);
    while (iter) mpr_dlist_next(&iter); /* < causes the whole source list to be iterated */
    return;
}
