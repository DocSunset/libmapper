/*
 * \defgroup rc Reference Counted Memory Cell
 * @{
 * The `mpr_rc` API provides a very simple reference counted pointer.
 *
 * A reference counting mechanism is used to ensure that the lifetime of rc managed memory
 * exceeds that of all references to it. This requires that the user explicitly calls
 * mpr_rc_make_ref to increment the reference count and mpr_rc_free to decrement it, i.e.
 * the reference counting is not automatic. However, the library itself handles the majority
 * of this; in general, the lifetime of any mpr_obj constructed by the library is at least
 * as long as the lifetime of the graph associated with it.
 * 
 * All `mpr_data_*` objects and all `mpr_dlist`s are wrapped in `mpr_rc` cells, so the user can
 * pass them to `mpr_rc_make_ref` to increment the reference count and prevent these resources
 * from being freed while the user is using them. Other objects in the library are not currently
 * using the rc mechanism, but may one day also be managed in this way.
 */

#ifndef RC_H_INCLUDED
#define RC_H_INCLUDED
#include <stddef.h>

typedef void * mpr_rc;

/*! Handler function type for side effects before freeing memory cared for by a `mpr_dlist`.
 * This is called automatically if the reference count in an rc cell drops to zero.
 * Note: this should not call `free` passing `data`, since `data` is actually not a pointer
 * to memory allocated with `malloc` or its ilk. The rc API will free the data after the
 * destructor has been called.
 * \param data  The mpr_rc cell whose refcount has dropped to zero and needs to be cleaned up. */
typedef void mpr_rc_data_destructor(mpr_rc data);

/*! A convenience for when you don't want rc to call the destructor, e.g. if its resource is plain
 * old data and doesn't need side-effects when it is freed.
 * \param  The mpr_rc cell who doesn't need to do any cleanup. */
void mpr_rc_no_destructor(mpr_rc data);

/*! Allocate a new rc cell.
 * \param size  The size in bytes of the resource to be stored by the rc.
 * \param destructor  A callback function to clean up and free resources managed by the object in
 * the rc. This must be given or undefined behavior will arise when the refcount drops to zero.
 * Use &mpr_rc_no_destructor if your data doesn't need to do any cleanup or free its own resources.
 * \return  The newly allocated rc cell. */
mpr_rc mpr_rc_new(size_t size, mpr_rc_data_destructor *destructor);

/* Increment the reference counter of an rc cell and return a new reference to it.
 * \param rc  The rc cell to make a reference to.
 * \return  A reference to `rc`. The returned `mpr_rc` is guaranteed to be the same as the argument. */
mpr_rc mpr_rc_make_ref(mpr_rc rc);

/* Decrement the reference count of an rc cell and possibly free its contents.
 * \param rc  The rc cell to possibly free. */
void mpr_rc_free(mpr_rc rc);

/* Query the reference count of an rc cell. Useful for internal testing and some internal features.
 * \param rc  The rc cell to query.
 * \return  The current number of references to `rc`. */
size_t mpr_rc_refcount(mpr_rc rc);

/*! @}@} */ /* end rc documentation group */
#endif // RC_H_INCLUDED
