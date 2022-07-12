#ifndef DLIST_H_INCLUDED
#define DLIST_H_INCLUDED

#include <stddef.h>

#ifndef DLIST_TYPES_INTERNAL
typedef void * mpr_dlist;

/* Handler function type for freeing memory cared for by a `mpr_dlist`.
 * This is called automatically if the reference count in a `mpr_dlist` cell drops to zero. */
typedef void mpr_dlist_data_destructor(void * data);
#endif

/* helper destructor in case a `mpr_dlist` should not manage its memory resource. */
/* TODO: eventually all memory resources could be managed by `mpr_dlist`
 * so that this would not be useful. */
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

/* List cells can be requested to allocate data, in which case they take ownership of the data, or
 * they can be passed another list cell. When a `mpr_dlist` is copied or passed as data to a new
 * cell, rather than copying its data, a reference to it is used. In this special case, a reference
 * counting mechanism is used to ensure that the lifetime of the resource exceeds that of all
 * references to it. The counting mechanism tracks cells that directly refer to other cells,
 * and forward links within lists of cells; backwards links are not counted so as to avoid
 * creating reference cycles. Because of this, if the user does not keep a reference to the
 * front of the list, e.g. while iterating over a list, then the list will be progressively
 * freed as the iterator advances through the list. */

/* The user should never assign to or from a `mpr_dlist`, as doing so would circumvent the
 * reference counting mechanism. All functions that would require the user to receive a
 * value from the API should use a return variable in the form of a pointer to a `mpr_dlist`,
 * usually called `dst`. Defined behavior should be given for all values of `dst`
 * including a null pointer, a pointer to a null pointer, or a pointer to an existing list.
 * Obviously, undefined behavior will result if a pointer to something else is given. */

/* A null pointer is always considered a valid list, i.e. a null list.
 * Note however that many methods require a pointer to a list; this pointer must not be null,
 * although the dereferenced value may be. */

/* TODO: should the list functionality be distinct from the reference counted smart pointer
 * functionality? It probably should, shouldn't it... */

/* Memory handling */

/* Handler function for queries */

/* Allocate a new mpr_dlist cell pointing to `data` and direct `dst` toward it.
 * 
 * This is a no-op if any arguments are null pointers.
 * If `*dst == 0`, a new cell is allocated.
 * Otherwise, `*dst` is interpreted as an existing `mpr_dlist`, which is overwritten by this
 * operation, possibly freeing its contents.
 * 
 * A destructor must be given. `data` is interpreted as a memory resource. The new list cell
 * takes responsibility for this resource and will free it at the appropriate time by calling
 * the destructor, passing `data` as an argument. If a size if given, `mpr_dlist_new` will
 * allocate memory for the resource by calling `calloc`; otherwise, `data` is assumed to be
 * pre-allocated. */
void mpr_dlist_new(mpr_dlist *dst, void ** data, size_t size, mpr_dlist_data_destructor *destructor);

/* Free the memory for a list cell and decrement reference counts of cells it refers to.
 * If a null pointer or pointer to null list is passed, this is a no-op:
 * conceptually, null lists have no outgoing references and are statically allocated on the heap. */
void mpr_dlist_free(mpr_dlist *dlist);

/* Make a reference to the cell `src`.
 * This increments the reference count of the resources referred to or cared for by `src` and returns
 * an additional pointer to `src`. If you make a reference, remember you have to free it.
 * If `dst == 0`, this is a no-op. You cannot make a reference in a memory slot that doesn't exist.
 * If `*dst != 0`, then the existing cell will be overwritten. */
void mpr_dlist_make_ref(mpr_dlist *dst, mpr_dlist src);

/* Move src into dst.
 * If `src == 0` this is a no-op.
 * If `*src` is a null list, this is equivalent to `mpr_dlist_free(dst)`, conceptually moving the
 * null list into `dst`. If `dst == 0` this is equivalent to `mpr_dlist_free(src)`, conceptually
 * discarding a reference to `src` by moving it into oblivion.
 * Otherwise, this is equivalent to `*dst = *src; *src = 0;`. If `*dst != 0`, `dst` is first freed.
 * The user should generally not need to call this; it is mainly exposed for testing. */
void mpr_dlist_move(mpr_dlist *dst, mpr_dlist *src);

/* Swap the resources of two list cells. */
void mpr_dlist_swap(mpr_dlist *a, mpr_dlist *b);

/* Make a weak copy of a cell `src`.
 * This takes a reference to the data resource of `src`; a true copy is not made. However,
 * the copied cell can be incorporated into a new list without modifying the original.
 * I.e. the list links are copied, but the data is referenced. Be careful not to create
 * cyclic list structures, or memory may eventually be leaked.
 * If `dst == 0`, this is a no-op. You cannot make a copy in a memory slot that doesn't exist.
 * If `*dst != 0`, then the existing cell will be overwritten. */
void mpr_dlist_copy(mpr_dlist *dst, mpr_dlist src);

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
void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, void **data, size_t size, mpr_dlist_data_destructor *destructor);
void mpr_dlist_insert_after (mpr_dlist *dst, mpr_dlist iter, void **data, size_t size, mpr_dlist_data_destructor *destructor);

/* Add a new cell at the back of a list.
 * The list will be iterated from `*back` if it is not null, or otherwise from `*front`, to find
 * the back of the list. A new cell will be inserted after the back, and if `back != 0`
 * a reference to the back will be returned.
 * `*front` is passed by reference so that if an empty list is passed in, a reference to the new
 * cell can be returned. If `front == 0` this is a no-op. `front` has to be passed here since it
 * is assumed the user must keep a reference to the front of the list (otherwise it might be
 * garbage collected by the reference counting mechanism). `back` is included so that the scan
 * to find the back of the list can be avoided. */
void mpr_dlist_append(mpr_dlist *front, mpr_dlist *back, void **data, size_t size, mpr_dlist_data_destructor *destructor);

/* Add a new cell at the front of a list.
 * The list will be iterated over from `*front` to find the front of the list.
 * A new cell will be inserted before the front, and `*front` will be updated to point to it.
 * This is a no-op if `front == 0`. */
void mpr_dlist_prepend(mpr_dlist *front, void **data, size_t size, mpr_dlist_data_destructor *destructor);

/* Remove the cell `iter`, putting it into `dst`,
 * and advancing/reversing `iter` to the next/previous cell.
 * This is a no-op if `iter == 0`.
 * If `*iter` is a null list, this is equivalent to `mpr_dlist_free(dst)`:
 * conceptually, the null list popped from the null list and moved into `dst`.
 * If `dst == 0`, then the popped list is simply freed.
 * It follows that if `dst == 0` and `iter` is a null list, this is a no-op. */
void mpr_dlist_pop (mpr_dlist *dst, mpr_dlist *iter);
void mpr_dlist_rpop(mpr_dlist *dst, mpr_dlist *iter);

/* Insert an existing list before/after the cell pointed to by `iter`.
 * If both `splice_front` and `splice_back` are null lists, this is a no-op.
 * If only one is a non-null list, then this internally traverses the list to add to find the
 * other end. If both a given, they are assumed to refer to the two ends of an existing list.
 * If this is not the case, the behavior is defined as equivalent to splitting before/after
 * `iter` and joining left split with `splice_front` and the right split with `splice_back`;
 * This behavior should not be exploited and may change without notice. */
/*void mpr_dlist_splice_before(mpr_dlist iter, mpr_dlist splice_front, mpr_dlist splice_back);
void mpr_dlist_splice_after (mpr_dlist iter, mpr_dlist splice_front, mpr_dlist splice_back);
*/

/* Split a list in two before/after the cell pointed to by `*right`.
 * After the split, `*left` is made to point to the back of the left side of the split,
 * and `right` is made to refer to the front of the right side of the split.
 * If `*left` is an existing list, it wll be overwritten.
 * If `left` is a null pointer, no reference to the left list is returned.
 * If `right` is a null list, this is equivalent to `mpr_dlist_free(left)`; conceptually, the null
 * list is split into two null lists, and left is made to refer to one. */
/*
void mpr_dlist_split_before(mpr_dlist *left, mpr_dlist *right);
void mpr_dlist_split_after (mpr_dlist *left, mpr_dlist *right);
*/

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
const void * mpr_dlist_data(mpr_dlist dlist);

/* A helpful macro to access a cell's data and cast it as a particular type.
 * Example: `T *data = mpr_dlist_data_as(T*, dlist);` */
#define mpr_dlist_data_as(T, dlist) ((T)mpr_dlist_data(dlist))

/* A helpful macro to cast a cell's data as a pointer to a particular type and dereference it.
 * Example: `mpr_sig sig = mpr_dlist_deref_as(mpr_sig, dlist);` */
#define mpr_dlist_deref_as(T, dlist) (dlist ? *(T*)mpr_dlist_data(dlist) : 0)

/* Test helpers. These functions are mainly provided for testing purposes. */

/* Check the reference count of a list cell. */
size_t mpr_dlist_get_refcount(mpr_dlist dlist);

/* Check if two list cells are equivalent, e.g. one is a reference to another. */
int mpr_dlist_equals(mpr_dlist, mpr_dlist);

#endif // DLIST_H_INCLUDED
