#ifndef DLIST_H_INCLUDED
#define DLIST_H_INCLUDED

#include <stddef.h>

#ifndef DLIST_TYPES_INTERNAL
typedef void * mpr_dlist;
#endif

/* Handler callback for freeing data managed by a dlist. This will be called when a dlist's refcount
 * drops to zero, and should free the data resource previously allocated and passed to the dlist. */
/* TODO: if all libmapper data were managed by mpr_rc, we wouldn't need this and could just store
 * an rc inside the list. */
typedef void mpr_dlist_destructor(void * data);

void mpr_dlist_no_destructor(void * data);

/* The `mpr_dquery` API provides a lazily evaluated query API over `mpr_dlist`s. */

/* Query callback function. Return non-zero to indicate a query match. */
typedef int mpr_dquery_callback(size_t num_items, void **items);

/* Lazily evaluate a new list by filtering an existing one.
 * When the query is constructed, `src` will be iterated over, passing its data to `cb`, until an
 * initial match is located. Subsequent calls to `mpr_dlist_next` or `mpr_dlist_pop` passing `query`
 * will repeat * this process until the end of `src` has been reached. If a copy or reference to the
 * front of the query list is maintained, the results of the query will be cached as a list
 * accessible from the front. */
void mpr_dlist_filter(mpr_dlist *query, mpr_dlist src, mpr_dquery_callback *cb);

/* The `mpr_dlist` API provides a generic list cell */

/* A reference counting mechanism is used to ensure that the lifetime of dlist managed memory
 * exceeds that of all references to it. 
 * 
 * The counting mechanism tracks cells that directly refer to other cells,
 * and forward links within lists of cells; backwards links are not counted so as to avoid
 * creating reference cycles. Because of this, if the user does not keep a reference to the
 * front of the list, e.g. while iterating over a list, then the list will be progressively
 * freed as the iterator advances through the list. */

/* The user should only assign 0 to `mpr_dlist`, as other assignments would circumvent the
 * reference counting mechanism. All functions that would require the user to receive a
 * value from the API should use a return variable in the form of a pointer to a `mpr_dlist`,
 * usually called `dst`. Defined behavior should be given for all values of `dst`
 * including a null pointer, a pointer to a null pointer, or a pointer to an existing list.
 * Obviously, undefined behavior will result if a pointer to something else is given. */

/* A null pointer is always considered a valid list, i.e. a null list.
 * Note however that many methods require a pointer to a list; this pointer should not be null,
 * although the dereferenced value may be. */

/* Memory handling */

/* Allocate a new mpr_dlist cell pointing to rc.
 * Note that this does not make a new reference to rc; the caller must do so explicitly unless
 * they intend to pass responsibility for their reference to the dlist. */
mpr_dlist mpr_dlist_new(void * data, mpr_dlist_destructor *destructor);

/* Free a list cell. This decrements the reference count of the data referred to by the list. */
void mpr_dlist_free(mpr_dlist dlist);

/* Make a reference to the cell `src`.
 * This increments the reference count of the resources referred to or cared for by `src` and returns
 * an additional pointer to `src`. If you make a reference, remember you have to free it. */
mpr_dlist mpr_dlist_make_ref(mpr_dlist dlist);

/* Move src into dst.
 * If `src == 0` this is a no-op.
 * If `*src` is a null list, this is equivalent to `mpr_dlist_free(dst)`, conceptually moving the
 * null list into `dst`. If `dst == 0` this is equivalent to `mpr_dlist_free(src)`, conceptually
 * discarding a reference to `src` by moving it into oblivion.
 * Otherwise, this is equivalent to `*dst = *src; *src = 0;`. If `*dst != 0`, `dst` is first freed.
 * The user should generally not need to call this; it is mainly exposed for testing. */
void mpr_dlist_move(mpr_dlist *dst, mpr_dlist *src);


/* Make a weak copy of a cell `src`.
 * This takes a reference to the data resource of `src`; a true copy is not made. However,
 * the copied cell can be incorporated into a new list without modifying the original.
 * I.e. the list links are copied, but the data is referenced. Be careful not to create
 * cyclic list structures, or memory may eventually be leaked. */
/* TODO: when all dlist data is stored with an rc, we can restore this function */
/* mpr_dlist  mpr_dlist_copy(mpr_dlist dlist); */

/* Methods for editing structures made of `mpr_dlist` cells. */
/* Note that these methods edit the underlying data structure, and this will be reflected by
 * any references held by the user, but not by copies/queries
 * (which have their own independent structure.)
 */

/* Allocates a new cell before/after the cell pointed to by `iter`.
 * If `dst != 0`, this allocates a new cell in `*dst` and inserts it into the list.
 * If `dst == 0`, a new list is still inserted into the list `iter`, but no reference is returned.
 * Consequently, if (`iter` is a null list OR `iter` is the front of a list in an `insert_before`
 * operation AND `dst == 0`), then this is a no-op. In these cases, the newly created cell has no
 * incoming references, so conceptually it is immediately freed. */
void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, void * data, mpr_dlist_destructor *destructor);
void mpr_dlist_insert_after (mpr_dlist *dst, mpr_dlist iter, void * data, mpr_dlist_destructor *destructor);

/* Add a new cell at the back of a list.
 * The list will be iterated from `*back` if it is not null, or otherwise from `*front`, to find
 * the back of the list. A new cell will be inserted after the back, and if `back != 0`
 * a reference to the back will be returned.
 * `*front` is passed by reference so that if an empty list is passed in, a reference to the new
 * cell can be returned. If `front == 0` this is a no-op. `front` has to be passed here since it
 * is assumed the user must keep a reference to the front of the list (otherwise it might be
 * garbage collected by the reference counting mechanism). `back` is included so that the scan
 * to find the back of the list can be avoided. */
void mpr_dlist_append(mpr_dlist *front, mpr_dlist *back, void * data, mpr_dlist_destructor *destructor);

/* Add a new cell at the front of a list.
 * The list will be iterated over from `*front` to find the front of the list.
 * A new cell will be inserted before the front, and `*front` will be updated to point to it.
 * This is a no-op if `front == 0`. */
void mpr_dlist_prepend(mpr_dlist *front, void * data, mpr_dlist_destructor *destructor);

/* Remove the cell `iter`, putting it into `dst`,
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

/* Determine the length of the list `dlist`.
 * `dlist` may refer to any cell in the list; the size of the whole list will be returned. */
size_t mpr_dlist_get_length(mpr_dlist dlist);

/* Iterate `dst` to the front/back of the list `iter`.
 * Returns the number of cells from `iter` to the front/back of the list inclusive of both,
 * i.e. `mpr_dlist_get_front(0, iter) + mpr_dlist_get_back(0, iter) - 1 == mpr_dlist_get_size(iter)`
 * If `dst` is a null pointer, this still iterates over the list to get its size. */
size_t mpr_dlist_get_front(mpr_dlist *dst, mpr_dlist iter);
size_t mpr_dlist_get_back (mpr_dlist *dst, mpr_dlist iter);

/* Tranform `iter` to a ref to the next/previous cell it refers to.
 * `iter`s current references are decremented, and its neighbour's are incremented.
 * If `iter` is a null list this is a no-op: conceptually, null's neighbours are also null */
void mpr_dlist_next(mpr_dlist *iter);
void mpr_dlist_prev(mpr_dlist *iter);

/* Access the data cared for or referred to by `dlist`. */
void * mpr_dlist_data(mpr_dlist dlist);

/* A helpful macro to access a cell's data and cast it as a particular type.
 * Example: `T *data = mpr_dlist_data_as(T*, dlist);` */
#define mpr_dlist_data_as(T, dlist) ((T)mpr_dlist_data(dlist))

/* Test helpers. These functions are mainly provided for testing purposes. */

size_t mpr_dlist_refcount(mpr_dlist dlist);

/* Check if two list cells are equivalent, e.g. one is a reference to another. */
int mpr_dlist_equals(mpr_dlist, mpr_dlist);

#endif // DLIST_H_INCLUDED
