#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <mapper/dlist.h>
#include "util/debug_macro.h"

/* note: all mpr_lls are wrapped in mpr_rc cells. */

/* local definition of a dlist structure (ll is for 'local list`) */
typedef struct _mpr_ll_t {
    int query;          /* flag indicating whether this list cell's data is a mpr_dlist_query */
    struct _mpr_ll_t *prev; /* a pointer (not a ref, not counted)mpr_dlist_header to a dlist rc */
    struct _mpr_ll_t *next; /* a reference (counted)mpr_dlist_header to a dlist rc */
    mpr_rc data;        /* the data of the cell */
} mpr_ll_t, *mpr_ll;

typedef struct _mpr_dlist_query_t {
    mpr_dlist parent;
    mpr_dlist_filter_predicate *predicate;
    char * types;
    mpr_union *va;
} mpr_dlist_query_t, *mpr_dlist_query;

/* hopefully the compiler is smart enough to make this a compile-time constant
 * and never actually dereference the pointer... */
static inline size_t _ll_header_size()
{
    mpr_ll lrc;
    return (char*)&lrc->data - (char*)lrc;
}

/* Get the dlist header from a dlist, which is a pointer to the list header's data member */
static inline mpr_ll _ll_from_dlist(mpr_dlist dlist)
{
    if (0 == dlist) return 0;
    return (mpr_ll)( (char*)dlist - _ll_header_size() );
}

/* Get the dlist (pointer to data) from a dlist header (pointer to header) */
static inline mpr_dlist _dlist_from_ll(mpr_ll ll)
{
    if (0 == ll) return 0;
    return &ll->data;
}

void mpr_ll_internal_destructor(mpr_rc rc)
{
    mpr_ll ll = (mpr_ll)rc;
    if (ll->next) {
        ll->next->prev = 0;
        mpr_rc_free(ll->next);
    }
    mpr_rc_free(ll->data);
}

mpr_dlist mpr_dlist_new(mpr_rc data)
{
    mpr_ll ll = (mpr_ll)mpr_rc_new(sizeof(mpr_ll_t), &mpr_ll_internal_destructor);
    if(0 == ll) {
        trace("Unable to alloc via mpr_rc_new in mpr_dlist_new.\n");
        mpr_rc_free(data);
        return 0;
    }

    ll->prev = 0;
    ll->next = 0;
    ll->data = data;
    ll->query = 0;
    return _dlist_from_ll(ll);
}

void mpr_dlist_free(mpr_dlist dlist)
{
    mpr_rc_free((mpr_rc)_ll_from_dlist(dlist));
}

mpr_dlist mpr_dlist_copy(mpr_dlist src)
{
    if (src == 0) return 0;

    mpr_ll ll = _ll_from_dlist(src);
    mpr_ll cp = (mpr_ll)mpr_rc_new(sizeof(mpr_ll_t), &mpr_ll_internal_destructor);
    TRACE_RETURN_UNLESS(cp, 0, "Unable to alloc via mpr_rc_new in mpr_dlist_copy.\n");

    cp->prev = ll->prev;
    cp->next = mpr_rc_make_ref((mpr_rc)ll->next);
    cp->data = mpr_rc_make_ref(ll->data);

    return _dlist_from_ll(cp);
}

mpr_dlist mpr_dlist_make_ref(mpr_dlist dlist)
{
    return _dlist_from_ll(mpr_rc_make_ref((mpr_rc)_ll_from_dlist(dlist)));
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

static inline void _insert_cell(mpr_ll prev, mpr_ll ll, mpr_ll next)
{
    /* we assume ll is not a null list, but either or both of prev and next may be null lists */
    /* link backward */
    if (next) next->prev = ll;
    ll->prev = prev;
    /* link forward */
    if (prev) {
        /* we assume prev->next == next */
        prev->next = mpr_rc_make_ref(ll); /* ll takes the ref already in prev->next, next's refcount is unchanged */
        ll->next = next;
    }
    else { /* no prev, i.e. next currently has no precedent */
        /* next is now referred to by ll where before it had no incoming ref */
        ll->next = mpr_rc_make_ref(next); 
    }
}

void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, mpr_rc rc)
{
    mpr_ll li = _ll_from_dlist(iter);
    if (dst == 0 && (li == 0 || li->prev == 0)) return mpr_rc_free(rc);
    mpr_dlist ldst = mpr_dlist_new(rc);
    if (0 == ldst) return;
    mpr_ll prev = li ? li->prev : 0;
    _insert_cell(prev, _ll_from_dlist(ldst), li);
    mpr_dlist_move(dst, &ldst);
}

void mpr_dlist_insert_after(mpr_dlist *dst, mpr_dlist iter, mpr_rc rc)
{
    if (dst == 0 && iter == 0) return mpr_rc_free(rc);
    mpr_dlist ldst = mpr_dlist_new(rc);
    if (0 == ldst) return;
    mpr_ll next = iter ? _ll_from_dlist(iter)->next : 0;
    _insert_cell(_ll_from_dlist(iter), _ll_from_dlist(ldst), next);
    mpr_dlist_move(dst, &ldst);
}

void mpr_dlist_append(mpr_dlist *front, mpr_dlist *back, mpr_rc rc)
{
    if (front == 0 || rc == 0) return mpr_rc_free(rc);
    if (*front == 0) {
        mpr_dlist dl = mpr_dlist_new(rc);
        if (0 == dl) return;
        *front = dl;
        /* we assume back must also be 0, since front points to a null list, so no need to free existing back */
        if (back) *back = mpr_dlist_make_ref(dl);
        return;
    }
    mpr_dlist lback = 0;
    if (back && *back && (_ll_from_dlist(*back)->next != 0)) mpr_dlist_get_back(&lback, *back);
    else mpr_dlist_get_back(&lback, *front);
    mpr_dlist_insert_after(&lback, lback, rc);
    mpr_dlist_move(back, &lback);
}

void mpr_dlist_prepend(mpr_dlist *front, mpr_rc rc)
{
    if (front == 0) return mpr_rc_free(rc);
    if (*front == 0) {
        *front = mpr_dlist_new(rc);
        return;
    }
    if (_ll_from_dlist(*front)->prev != 0) mpr_dlist_get_front(front, *front);
    mpr_dlist_insert_before(front, *front, rc);
}

static inline void _remove_cell(mpr_ll prev, mpr_ll ll, mpr_ll next)
{
    /* we assume ll is not a null list, but either or both of prev and next may be */
	if (prev) {
    	mpr_rc_free((mpr_rc)prev->next);
		prev->next = mpr_rc_make_ref((mpr_rc)next);
	}
	if (next) next->prev = prev;
	if (ll) {
		ll->prev = 0;
		mpr_rc_free(ll->next);
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
	mpr_ll prev, ll, next;
	prev = ll = next = 0;
	ll = _ll_from_dlist(mpr_dlist_make_ref(*iter));
	mpr_dlist_next(iter);
	prev = ll->prev;
	next = _ll_from_dlist(*iter);
	_remove_cell(prev, ll, next);
	mpr_dlist dlist = _dlist_from_ll(ll);
	mpr_dlist_move(dst, &dlist);
}

void mpr_dlist_rpop(mpr_dlist *dst, mpr_dlist *iter)
{
	if (iter == 0) return;
	if (dst) {
    	mpr_dlist_free(*dst);
    	*dst = 0;
	}
	if (*iter == 0) return; /* freeing dst is equivalent to popping a null list into it */
	mpr_ll prev, ll, next;
	prev = ll = next = 0;
	ll = _ll_from_dlist(mpr_dlist_make_ref(*iter));
	mpr_dlist_prev(iter);
	next = ll->next;
	prev = _ll_from_dlist(*iter);
	_remove_cell(prev, ll, next);
	mpr_dlist dlist = _dlist_from_ll(ll);
	mpr_dlist_move(dst, &dlist);
}

void _query_next(mpr_dlist *iter, mpr_ll ql)
{
    mpr_dlist_query q = (mpr_dlist_query)ql->data;
    /* scan q to the next matching list element 
     * or the end of the list if there are no matches */
    for ( ; q->parent; mpr_dlist_next(&q->parent)) {
        if (q->predicate(*q->parent, q->types, q->va)) {
            break;
        }
    }
    mpr_ll ret;
    mpr_ll ll = _ll_from_dlist(*iter);
    if (0 == q->parent) {
        ret = 0;
        mpr_rc_free(ql);
        mpr_rc_free(ll);
        *iter = 0;
    }
    else {
        ret = _ll_from_dlist(mpr_dlist_new(mpr_rc_make_ref(*q->parent)));
        mpr_dlist_next(&q->parent);
        if (0 == q->parent) {
            mpr_rc_free(ql);
            ql = 0;
        }
        ret->prev = ll;
        ret->next = ql;
    }
    if (ll) ll->next = mpr_rc_make_ref(ret);
    mpr_dlist dlist = _dlist_from_ll(ret);
    mpr_dlist_move(iter, &dlist);
}

void mpr_dlist_next(mpr_dlist *iter)
{
    if (iter == 0 || *iter == 0) return;
    mpr_ll next = _ll_from_dlist(*iter)->next;
    if (next == 0) {
        mpr_dlist_free(*iter);
        *iter = 0;
        return;
    }
    if (next->query) return _query_next(iter, _ll_from_dlist(*iter)->next);
    mpr_dlist ldst = mpr_dlist_make_ref(_dlist_from_ll(next));
    mpr_dlist_move(iter, &ldst);
}

void mpr_dlist_prev(mpr_dlist *iter)
{
    if (iter == 0 || *iter == 0) return;
    mpr_ll prev = _ll_from_dlist(*iter)->prev;
    if (prev == 0) {
        mpr_dlist_free(*iter);
        *iter = 0;
        return;
    }
    mpr_dlist ldst = mpr_dlist_make_ref(_dlist_from_ll(prev));
    mpr_dlist_move(iter, &ldst);
}

size_t mpr_dlist_get_length(mpr_dlist dlist)
{
    return mpr_dlist_get_front(0, dlist) + mpr_dlist_get_back(0, dlist) - 1;
}

size_t mpr_dlist_get_front(mpr_dlist *dst, mpr_dlist iter)
{
    if (iter == 0) return 0;
    mpr_ll li = _ll_from_dlist(iter);
    size_t count = 1;
    while (li->prev) {
        ++count;
        li = li->prev;
    }
    if (dst) {
        mpr_dlist front = mpr_dlist_make_ref(_dlist_from_ll(li));
        mpr_dlist_move(dst, &front);
    }
    return count;
}

size_t mpr_dlist_get_back(mpr_dlist *dst, mpr_dlist iter)
{
    if (iter == 0) return 0;
    size_t count = 1;
    mpr_dlist back = mpr_dlist_make_ref(iter);
    while (_ll_from_dlist(back)->next) {
        ++count;
        mpr_dlist_next(&back); /* must use next here in case iter is a query */
    }
    if (dst) {
        mpr_dlist_move(dst, &back);
    } else mpr_dlist_free(back);
    return count;
}

size_t mpr_dlist_refcount(mpr_dlist dlist)
{
    return mpr_rc_refcount((mpr_rc)_ll_from_dlist(dlist));
}

int mpr_dlist_equals(mpr_dlist a, mpr_dlist b)
{
    mpr_ll la, lb;
    la = _ll_from_dlist(a); lb = _ll_from_dlist(b);
    return la == lb   
           ||(  ( la->next == lb->next )
             && ( la->prev == lb->prev )
             && ( la->data == lb->data )
             );
}

void mpr_dlist_query_destructor(void *rc)
{
    mpr_dlist_query q = (mpr_dlist_query)rc;
    mpr_dlist_free(q->parent);
    free(q->va);
    free(q->types);
}

mpr_dlist mpr_dlist_new_filter(mpr_dlist src, mpr_dlist_filter_predicate *cb, const char * types, ...)
{
    if (0 == src) return 0;
    mpr_dlist_get_front(&src, src);

    /* parse args into a mpr_union array */
    mpr_union *argv;
    {
        size_t argc = 0;
        for (const char *argt = types; *argt != '\0'; ++argt) ++argc;
        argv = calloc(argc, sizeof(mpr_union));
        TRACE_RETURN_UNLESS(argv, 0, "unable to calloc in mpr_dlist_new_filter.\n");
    }
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
        va_end(ap);
    }

    mpr_dlist_query q = (mpr_dlist_query)mpr_rc_new(sizeof(mpr_dlist_query_t), &mpr_dlist_query_destructor);
    if (0 == q) {
        free(argv);
        free(types_cpy);
        trace("unable to alloc via mpr_rc_new in mpr_dlist_new_filter.\n");
        return 0;
    }
    q->parent = mpr_dlist_make_ref(src);
    q->predicate = cb;
    q->types = types_cpy;
    q->va = argv;

    mpr_dlist dlist = mpr_dlist_new((mpr_rc)q);
    if (0 == dlist) {
        trace("unable to alloc via mpr_dlist_new in mpr_dlist_new_filter.\n");
        return 0;
    }
    _ll_from_dlist(dlist)->query = 1;
    mpr_dlist ret = 0;
    _query_next(&ret, _ll_from_dlist(dlist));

    return ret;
}

void mpr_dlist_evaluate_filter(mpr_dlist front)
{
    mpr_dlist iter = mpr_dlist_make_ref(front);
    while (iter) mpr_dlist_next(&iter); /* < causes the whole source list to be iterated */
    return;
}

int mpr_dlist_ptr_compare(mpr_rc datum, const char * types, mpr_union *va)
{
    mpr_op op = va[0].i;
    void * ptr = va[1].v;
    switch (op) {
        case MPR_OP_EQ:
            return datum == ptr;
        case MPR_OP_GT:
            return datum > ptr;
        case MPR_OP_GTE:
            return datum >= ptr;
        case MPR_OP_LT:
            return datum < ptr;
        case MPR_OP_LTE:
            return datum <= ptr;
        case MPR_OP_NEQ:
            return datum != ptr;
        case MPR_OP_UNDEFINED:
        case MPR_OP_EX:
        case MPR_OP_NEX:
        case MPR_OP_ALL:
        case MPR_OP_ANY:
        case MPR_OP_NONE:
        default:
            trace("mpr_op %d not meaningful in mpr_dlist_ptr_compare.\n", op);
            return 0;
    }
}

const char * mpr_dlist_ptr_compare_types = "iv";
