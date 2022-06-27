#ifndef DLIST_H_INCLUDED
#define DLIST_H_INCLUDED

#include <stddef.h>

typedef void * mpr_dlist;

/* The `mpr_dlist` API provides a generic list cell */

/* List cells can be requested to allocate data, in which case they take ownership of the data, or
 * they can be passed another list cell. When a `mpr_dlist` is copied or passed as data to a new
 * cell, rather than copying its data, a reference to it is used. In this special case, a reference
 * counting mechanism is used to ensure that the lifetime of the resource exceeds that of all
 * references to it. */

/* The user should never assign to or from a `mpr_dlist`, as doing so would circumvent the
 * reference counting mechanism. All functions that would require the user to receive a
 * value from the API should use a return variable in the form of a pointer to a `mpr_dlist`,
 * usually called `dst`. Defined behavior should be given for all values of `dst`
 * including a null pointer, a pointer to a null pointer, or a pointer to an existing list.
 * Obviously, undefined behavior will result if a pointer to something else is given. */

/* A null pointer is always considered a valid list, i.e. a null list.
 * Note however that many methods require a pointer to a list; this pointer must not be null,
 * although the dereferenced value may be. */

/* Memory handling */

/* Handler function type for freeing memory cared for by a `mpr_dlist`.
 * This is called automatically if the reference count in a `mpr_dlist` cell drops to zero. */
typedef void mpr_dlist_data_destructor(void * data);

/* Allocate a new mpr_dlist cell pointing to `data` and direct `dst` toward it.
 * 
 * This is a no-op if any arguments are null pointers.
 * If `*dst == 0`, a new cell is allocated.
 * Otherwise, `*dst` is interpreted as an existing `mpr_dlist`, which is overwritten by this
 * operation, possibly freeing its contents.
 * 
 * A destructor must be given. `data` is interpreted as a memory resource. The new list cell
 * takes responsibility for this resource and will free it at the appropriate time by calling
 * the destructor, passing `data` as an argument. */
void mpr_dlist_new(mpr_dlist *dst, void * data, mpr_dlist_data_destructor *destructor);

/* Free the memory for a list cell and decrement reference counts of cells it refers to.
 * If a null pointer or pointer to null list is passed, this is a no-op:
 * conceptually, null lists have no outgoing references and are statically allocated on the heap. */
void mpr_dlist_free(mpr_dlist *dlist);

/* Move src into dst.
 * If `dst == 0 || src == 0` this is a no-op.
 * Otherwise, this is equivalent to `*dst = *src; *src = 0;`. If `*dst != 0`, `dst` is first freed.
 * The user should generally not need to call this; it is mainly exposed for testing. */
void mpr_dlist_move(mpr_dlist *dst, mpr_dlist *src);

/* Swap the resources of two list cells. */
void mpr_dlist_swap(mpr_dlist *a, mpr_dlist *b);

/* Make a copy of the cell `src`.
 * In reality, `*dst` is not a true copy, but simply refers to the same resource as `src`.
 * This increments the reference count of the resources referred to or cared for by `src`.
 * If `dst == 0`, this is a no-op. You cannot copy a list into a memory slot that doesn't exist.
 * If `*dst == 0`, then a new cell will be allocated to hold the copy */
void mpr_dlist_copy(mpr_dlist *dst, mpr_dlist iter);

/* Methods for editing structures made of `mpr_dlist` cells. */

/* Allocates a new cell before/after the cell pointed to by `iter`.
 * If `dlist` is a null list, this is equivalent to `mpr_dlist_new(dst, data, size, destructor)`.
 * Otherwise, if `dlist` is not a null list, then `dst == 0` still inserts a new cell,
 * but no reference is returned to it.
 * Therefore, this is a no-op if `dlist` is a null list and `dst == 0`: conceptually, 
 * because the new cell has no incoming references, it immediately goes out of scope. */
void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, void * data, size_t size, mpr_dlist_data_destructor *destructor);
void mpr_dlist_insert_after (mpr_dlist *dst, mpr_dlist iter, void * data, size_t size, mpr_dlist_data_destructor *destructor);

/* Remove the cell `iter`, putting it into `dst`,
 * and advancing/reversing `iter` to the next/previous cell.
 * If `iter` is a null list, this is equivalent to `mpr_dlist_free(dst)`:
 * conceptually, the null list popped from the null list and moved into `dst`.
 * If `dst == 0`, then the popped list is simply freed.
 * It follows that if `dst == 0` and `iter` is a null list, this is a no-op. */
void mpr_dlist_pop (mpr_dlist *dst, mpr_dlist iter);
void mpr_dlist_rpop(mpr_dlist *dst, mpr_dlist iter);

/* Methods for (immutably) traversing and inspecting `mpr_dlist` structures. */

/* Determine the length of the list `dlist`.
 * `dlist` may refer to any cell in the list; the size of the whole list will be returned. */
size_t mpr_dlist_get_length(mpr_dlist dlist);

/* Push a new cell at the front/back of the list `dlist` and create a reference to it in `dst`.
 * If `dst` is a null pointer, this still iterates over the list to get its size.
 * Returns the number of cells from `iter` to the front/back of the list,
 * excluding`iter` and including the front/back, i.e.
 * mpr_dlist_get_front(0, iter) + mpr_dlist_get_back(0, iter) == mpr_dlist_get_size(iter) - 1 */
size_t mpr_dlist_get_front(mpr_dlist *dst, mpr_dlist iter);
size_t mpr_dlist_get_back (mpr_dlist *dst, mpr_dlist iter);

/* Tranform `iter` to a copy of the next/previous cell it refers to.
 * `iter`s current references are decremented, and its neighbour's are incremented.
 * If `iter` is a null list this is a no-op: conceptually, null's neighbours are also null */
void mpr_dlist_next(mpr_dlist iter);
void mpr_dlist_prev(mpr_dlist iter);

/* Access the data cared for or referred to by `dlist`. */
const void * mpr_dlist_data(mpr_dlist dlist);

/* A helpful macro to access a cell's data and cast it as a particular type. */
#define mpr_dlist_data_as(T, dlist) ((T)mpr_dlist_data(dlist))

/* Test helpers. These functions are mainly provided for testing purposes. */

/* Check the reference count of a list cell. */
size_t mpr_dlist_get_refcount(mpr_dlist dlist);

/* Check if two list cells are equivalent, e.g. one is a copy of another. */
int mpr_dlist_equals(mpr_dlist, mpr_dlist);

#endif // DLIST_H_INCLUDED
