/*!
 * \defgroup dlist Data Lists
 * @{
 * The `mpr_dlist` API provides a generic list cell for making simple linked lists.
 * A reference counting mechanism ([mpr_rc](#mpr_rc.h)) is used to ensure that the lifetime of dlist
 * managed memory exceeds that of all references to it, including references made by other
 * dlist cells in a linked chain.
 * 
 * The counting mechanism tracks data shared between cells,
 * and forward links within lists of cells; backwards links are not counted so as to avoid
 * creating reference cycles. Because of this, if the user does not keep a reference to the
 * front of the list, e.g. while iterating over a list, then the list will be progressively
 * freed as the iterator advances through the list.
 * 
 * A null pointer is always considered a valid list, i.e. a null list.
 * Note however that many methods require a pointer to a list; this pointer should not be null,
 * although the dereferenced value may be.
 * 
 * A mpr_dlist is represented by a pointer to a mpr_rc, i.e. a `void**`. The user can treat this
 * as a pointer to a pointer to whatever, and dereference it to access the data in the list cell.
 * Example: To access a mpr_dataset held in a list of datasets, the user may write
 * `mpr_dataset data = *(mpr_dataset*)dlist;`
 */

#ifndef DLIST_H_INCLUDED
#define DLIST_H_INCLUDED

#include <stddef.h>
#include <mapper/rc.h>
#include <mapper/mapper_constants.h>

typedef mpr_rc *mpr_dlist;

/* Memory handling */

/*! Allocate a new mpr_dlist cell pointing to an rc.
 * Note that this does not make a new reference to rc; the caller must do so explicitly unless
 * they intend to pass responsibility for their reference to the dlist. The `data` rc is guaranteed
 * to be freed eventually by the dlist, so if the user wishes to ensure they can continue to use
 * their reference even after the dlist is garbage collected, they must explicitly make a new
 * reference before passing `data` into the dlist. This applies also to the insertion methods
 * below.
 * \param data  The rc cell that the list should refer to.
 * \return  A newly allocated list. */
mpr_dlist mpr_dlist_new(mpr_rc data);

/*! Free a list cell. This decrements the reference count of the data referred to by the list. */
void mpr_dlist_free(mpr_dlist dlist);

/*! Make a reference to the cell `src`.
 * This increments the reference count of the resources referred to or cared for by `src` and returns
 * an additional pointer to `src`. If you make a reference, remember you have to free it. */
mpr_dlist mpr_dlist_make_ref(mpr_dlist dlist);

/*! Move src into dst.
 * If `src == 0` this is a no-op.
 * If `*src` is a null list, this is equivalent to `mpr_dlist_free(dst)`, conceptually moving the
 * null list into `dst`. If `dst == 0` this is equivalent to `mpr_dlist_free(src)`, conceptually
 * discarding a reference to `src` by moving it into oblivion.
 * Otherwise, this is equivalent to `*dst = *src; *src = 0;`. If `*dst != 0`, `dst` is first freed.
 * The user should generally not need to call this; it is mainly exposed for testing. */
void mpr_dlist_move(mpr_dlist *dst, mpr_dlist *src);


/*! Make a weak copy of a cell `src`.
 * This takes a reference to the data resource of `src`; a true copy is not made. However,
 * the copied cell can be incorporated into a new list without modifying the original.
 * I.e. the list links are copied, but the data is referenced. Be careful not to create
 * cyclic structures, or memory may eventually be leaked. */
mpr_dlist  mpr_dlist_copy(mpr_dlist dlist);

/* Methods for editing structures made of `mpr_dlist` cells. */
/* Note that these methods edit the underlying data structure, and this will be reflected by
 * any references held by the user, but not by copies/queries
 * (which have their own independent structure.)
 * See also the note on `mpr_dlist_new` about the reference and lifetime semantics of passing
 * a mpr_rc into a dlist.
 */

/*! Allocates a new cell before/after the cell pointed to by `iter`.
 * If `dst != 0`, this allocates a new cell in `*dst` and inserts it into the list.
 * If `dst == 0`, a new list is still inserted into the list `iter`, but no reference is returned.
 * Consequently, if (`iter` is a null list OR `iter` is the front of a list in an `insert_before`
 * operation AND `dst == 0`), then this is a no-op. In these cases, the newly created cell has no
 * incoming references, so conceptually it is immediately freed. */
void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, mpr_rc rc);
void mpr_dlist_insert_after (mpr_dlist *dst, mpr_dlist iter, mpr_rc rc);

/*! Add a new cell at the back of a list.
 * The list will be iterated from `*back` if it is not null, or otherwise from `*front`, to find
 * the back of the list. A new cell will be inserted after the back, and if `back != 0`
 * a reference to the back will be returned.
 * `*front` is passed by reference so that if an empty list is passed in, a reference to the new
 * cell can be returned. If `front == 0` this is a no-op. `front` has to be passed here since it
 * is assumed the user must keep a reference to the front of the list (otherwise it might be
 * garbage collected by the reference counting mechanism). `back` is included so that the scan
 * to find the back of the list can be avoided. */
void mpr_dlist_append(mpr_dlist *front, mpr_dlist *back, mpr_rc rc);

/*! Add a new cell at the front of a list.
 * The list will be iterated over from `*front` to find the front of the list.
 * A new cell will be inserted before the front, and `*front` will be updated to point to it.
 * This is a no-op if `front == 0`. */
void mpr_dlist_prepend(mpr_dlist *front, mpr_rc rc);

/*! Remove the cell `iter`, putting it into `dst`,
 * and advancing/reversing `iter` to the next/previous cell.
 * This is a no-op if `iter == 0`.
 * If `*iter` is a null list, this is equivalent to `mpr_dlist_free(dst)`:
 * conceptually, the null list popped from the null list and moved into `dst`.
 * If `dst == 0`, then the popped list is simply freed.
 * It follows that if `dst == 0` and `iter` is a null list, this is a no-op. */
void mpr_dlist_pop (mpr_dlist *dst, mpr_dlist *iter);
void mpr_dlist_rpop(mpr_dlist *dst, mpr_dlist *iter);

/* Methods for traversing and inspecting `mpr_dlist` structures. */
/* Note that while these methods do not conceptually modify the list structure, cells may be
 * implicitly freed if their reference count drops to zero as a consequence of these methods.
 * Notably, `mpr_dlist_next` can cause the front of the list to be freed if a reference is
 * not retained to it. */

/*! Determine the length of the list `dlist`.
 * `dlist` may refer to any cell in the list; the size of the whole list will be returned. */
size_t mpr_dlist_get_length(mpr_dlist dlist);

/*! Iterate `dst` to the front/back of the list `iter`.
 * Returns the number of cells from `iter` to the front/back of the list inclusive of both,
 * i.e. `mpr_dlist_get_front(0, iter) + mpr_dlist_get_back(0, iter) - 1 == mpr_dlist_get_size(iter)`
 * If `dst` is a null pointer, this still iterates over the list to get its size. */
size_t mpr_dlist_get_front(mpr_dlist *dst, mpr_dlist iter);
size_t mpr_dlist_get_back (mpr_dlist *dst, mpr_dlist iter);

/*! Tranform `iter` to a ref to the next/previous cell it refers to.
 * `iter`s current references are decremented, and its neighbour's are incremented.
 * If `iter` is a null list this is a no-op: conceptually, null's neighbours are also null */
void mpr_dlist_next(mpr_dlist *iter);
void mpr_dlist_prev(mpr_dlist *iter);

/* Test helpers. These functions are mainly provided for testing purposes. */

size_t mpr_dlist_refcount(mpr_dlist dlist);

/*! Check if two list cells are equivalent, e.g. one is a reference to another. */
int mpr_dlist_equals(mpr_dlist, mpr_dlist);

/* Queries */

/*! Query callback function. Return non-zero to indicate a query match. 
 * \param datum  The data held by a dlist cell to evaluate for in-/ex-clusion by the filter.
 * \param types  The type format string describing the additional arguments supplied by the user.
 * \param va     Additional arguments supplied by the user when creating the filter.
 *               Technically the callback may mutate the contents of the variadic arguments,
 *               and subsequent calls to the callback will observe these modifications,
 *               but arguably this is an implementation detail and should not be relied on. */
typedef int mpr_dlist_filter_predicate(mpr_rc datum, const char * types, mpr_union *va);

/*! Lazily evaluate a new list by filtering an existing one.
 * When the query is constructed, `src` will be iterated over, passing its data to `cb` along with
 * variadic arguments described by an OSC-like typespec string consisting only of mpr_types, until
 * an initial match is located. Subsequent calls to `mpr_dlist_next` or `mpr_dlist_pop` passing the
 * result will repeat this process until the end of `src` has been reached. If a reference
 * to the front of the query list is maintained, the results of the query will be cached as a list
 * accessible from the front. As usual, if a reference to the front is not kept, then iterating
 * through the list will iteratively free the previous link.
 * The whole list is always filtered starting from the front, even if `src` is an iterator into
 * some other part of the list.
 * \param src    the list to filter
 * \param cb     the predicate by which to include or exclude elements of `src`
 * \param types  an OSC-like typespec string describing the callback's arguments
 * \param ...    additional arguments to the callback predicaate
 * \return       a new lazily-evaluated list containing only elements of `src` where the predicate
 *               returns a non-zero value. */
mpr_dlist mpr_dlist_new_filter(mpr_dlist src, mpr_dlist_filter_predicate *cb, const char * types, ...);

/* TODO: ? re-implement isect, diff, union, filter by prop as seen in the mpr_list API,
 * then replace the latter with dlist ? */
/* TODO: ? could it be useful to have lazy map and reduce methods as well ? */

/*! Non-lazily evaluate the whole filtered list.
 * This will iterate over a filter so that the whole list is evaluated and cached as a linked
 * list starting at `filter_front`. */
void mpr_dlist_evaluate_filter(mpr_dlist filter_front);

/* Helpful predicates */

/*! Compare two pointers. The source list is assumed to be a list of pointers to objects,
 * such as datasets or signals. The user should supply a mpr_op and a pointer to compare with.
 * For example, to find a specific pointer in a list of pointers:
 * ```
 * void * ptr = 100;
 * mpr_dlist found = mpr_dlist_new_filter(src,
 *                                        &mpr_dlist_ptr_compare, &mpr_dlist_ptr_compare_types,
 *                                        MPR_OP_EQ, ptr);
 * if (found) {
 *     void * the_ptr = *(void**)found;
 *     // do something
 * } else // do something else
 * ```
 */
int mpr_dlist_ptr_compare(mpr_rc datum, const char * types, mpr_union *va);
extern const char * mpr_dlist_ptr_compare_types;

/*! @}@} */ /* end dlist documentation group */
#endif // DLIST_H_INCLUDED
