#ifndef DLIST_H_INCLUDED
#define DLIST_H_INCLUDED

typedef void * mpr_dlist;

/* The `mpr_dlist` API provides a generic doubly linked list cell with methods for making
 * simple linear lists that can be iteratd over and/or treated as a FIFO, LIFO, or deque. */

/* List cells can be requested to allocate data, in which case they take ownership of the data, or
 * they can be passed a another list cell. When a `mpr_dlist` is copied or passed as data to a new
 * cell, rather than copying its data, a reference to it is used. In this special case, a reference
 * counting mechanism is used to ensure that the lifetime of the resource exceeds that of all
 * references to it. */

/* The user should never assign to or from a `mpr_dlist`, as doing so would circumvent the
 * reference counting mechanism. All functions that would require the user to receive a
 * value from the API should use a return variable in the form of a pointer to a `mpr_dlist`,
 * usually called `dst`. Defined behavior should be given for all values of `dst`
 * including a null pointer, a pointer to a null pointer, or a pointer to an existing list.
 * Obviously, undefined behavior will result if a pointer to something else is given. */

/* The void pointer (`mpr_dlist`) that the user interacts with, returned from and passed to API,
 * is the pointer to the list cell's data. It can be cast as a pointer to the data resource
 * (whether that is owned by the list or a pointer to an external resource) and manipulated
 * as such. */

/* A null pointer is always considered a valid list, i.e. a null list.
 * Note however that many methods require a pointer to a list; this pointer must not be null,
 * although the dereferenced value may be. */

/* Memory management */

/* Handler for freeing memory owned by a mpr_dlist.
 * This is called automatically if the reference count in a mpr_dlist cell drops to zero. */
typedef void mpr_dlist_data_destructor(void * data);

/* Allocate a new mpr_dlist cell pointing to `data` and/or move it into `dst`.
 * 
 * Memory semantics:
 * An optional size and destructor must be passed if the list should manage the memory of the data.
 * Otherwise, the list is treated purely as a reference to the data,
 * i.e. the data is assumed to be a pointer to a resource owned by another part of the program;
 * in this case the lifetime of the data must surpass that of the list.
 * Accessing the data after it has been freed or gone out of scope will lead to undefined behavior.
 * 
 * Null pointer handling:
 * This is a no-op if `dst == 0`, i.e. a null pointer is passed as `dst`.
 * If `*dst == 0`, a new cell is allocated.
 * Otherwise, the new data is moved into the existing cell,
 * overwriting and possibly freeing its contents. */
void mpr_dlist_new(mpr_dlist *dst, void * data, size_t size, mpr_dlist_data_destructor *destructor);

/* Free the memory for a list cell and decrement reference counts of cells it refers to.
 * If a null list is passed, this is a no-op:
 * conceptually, null lists have no outgoing references and are statically allocated on the heap. */
void mpr_dlist_free(mpr_dlist dst);

/* Move src into dst.
 * The user should generally not need to call this; it is mainly exposed for testing.
 * Dst takes ownership of references made by src, decrementing any references it already makes.
 * `src` is assumed to go out of scope (e.g. it is an rvalue) after this call;
 * otherwise use mpr_dlist_copy. 
 * If `dst == 0` this is equivalent to `mpr_dlist_free(src)`.
 * If `*dst == 0` then *dst is pointed to `src`. */
void mpr_dlist_move(mpr_dlist *dst, mpr_dlist src);

/* Make a copy of a dlist.
 * This increments the reference count to the resources referred to by the list.
 * If `dst == 0`, this is a no-op. You cannot copy a list into a memory slot that doesn't exist.
 * If `*dst == 0`, then a new cell will be allocated to hold the copy */
mpr_dlist mpr_dlist_copy(mpr_dlist *dst, mpr_dlist dlist);

/* Methods for building structures using mpr_dlist. */

/* Push a new cell at the front/back of the list `dlist` and create a reference to it in `dst`.
 * If `dlist` is a null list, this is equivalent to `mpr_dlist_new(dst, data, size, destructor)`.
 * Therefore, this is a no-op if `dlist` is a null list and `dst == 0`:
 * conceptually, because the new cell has no incoming references, it immediately goes out of scope.
 * Otherwise, if `dlist` is not a null list, then `dst == 0` still inserts a new cell,
 * but no reference is returned to it. */
void mpr_dlist_push_front(mpr_dlist *dst, mpr_dlist dlist, void * data, size_t size, mpr_dlist_data_destructor *destructor);
void mpr_dlist_push_back (mpr_dlist *dst, mpr_dlist dlist, void * data, size_t size, mpr_dlist_data_destructor *destructor);

/* Remove the cell at the front/back of `dlist`, putting it into `dst`.
 * If `dlist` is a null list, this is equivalent to `mpr_dlist_free(dst)`:
 * conceptually, the null list popped from the null list and moved into dst.
 * If `dst == 0`, then the popped list is simply freed.
 * It follows that if `dst == 0` and `dlist` is a null list, this is a no-op. */
void mpr_dlist_pop_front(mpr_dlist *dst, mpr_dlist dlist);
void mpr_dlist_pop_back (mpr_dlist *dst, mpr_dlist dlist);

/* Allocates a new cell before/after the cell pointed to by `iter`.
 * These are semantically equivalent to `mpr_dlist_push_front/back`,
 * only the new cell is inserted immediately before or after `iter`. */
void mpr_dlist_insert_before(mpr_dlist *dst, mpr_dlist iter, void * data, size_t size, mpr_dlist_data_destructor *destructor);
void mpr_dlist_insert_after (mpr_dlist *dst, mpr_dlist iter, void * data, size_t size, mpr_dlist_data_destructor *destructor);

/* Remove the cell before/after the cell `iter`, putting it into `dst`.
 * These are semantically equivalent to `mpr_dlist_pop_front/back`,
 * only the new cell is removed from immediately before or after `iter`. */
void mpr_dlist_remove_before(mpr_dlist *dst, mpr_dlist iter);
void mpr_dlist_remove_after (mpr_dlist *dst, mpr_dlist iter);

/* Methods for traversing mpr_dlists. */

/* Tranform `iter` to a copy of the next/previous cell it refers to.
 * `iter`s current references are decremented, and its neighbour's are incremented.
 * If `iter` is a null list this is a no-op: conceptually, null's neighbours are also null */
void mpr_dlist_next(mpr_dlist iter);
void mpr_dlist_prev(mpr_dlist iter);

#endif // DLIST_H_INCLUDED
