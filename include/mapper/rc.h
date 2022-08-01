#ifndef RC_H_INCLUDED
#define RC_H_INCLUDED
#include <stddef.h>

/* The `mpr_rc` API provides a very simple reference counted pointer. */

/* A reference counting mechanism is used to ensure that the lifetime of rc managed memory
 * exceeds that of all references to it. This requires that the user explicitly calls
 * mpr_rc_make_ref to increment the reference count and mpr_rc_free to decrement it, i.e.
 * the reference counting is not automatic. However, the library itself handles the majority
 * of this; in general, the lifetime of any mpr_obj constructed by the library is at least
 * as long as the lifetime of the graph associated with it. */

typedef void * mpr_rc;

/* Handler function type for side effects before freeing memory cared for by a `mpr_dlist`.
 * This is called automatically if the reference count in an rc cell drops to zero.
 * Note: this should not call `free` passing `data`, since `data` is actually not a pointer
 * to memory allocated with `malloc` or its ilk. The rc API will free the data after the
 * destructor has been called. */
typedef void mpr_rc_data_destructor(mpr_rc data);

/* A convenience for when you don't want rc to call the destructor, e.g. if its resource is plain
 * old data and doesn't need side-effects when it is freed. */
void mpr_rc_no_destructor(mpr_rc data);

/* Allocate a new rc cell.
 * size is the size in bytes of the resource to be stored by the rc.
 * destructor must be given or undefined behavior will arise when the refcount drops to zero.
 * Use &mpr_rc_no_destructor if your data doesn't need to do any cleanup or free its own resources.
 */
mpr_rc mpr_rc_new(size_t size, mpr_rc_data_destructor *destructor);

/* Increment the reference counter of an rc cell and return a new reference to it.
 * The returned mpr_rc is guaranteed to be the same as the argument. */
mpr_rc mpr_rc_make_ref(mpr_rc rc);

/* Decrement the reference count of an rc cell and possibly free its contents. */
void mpr_rc_free(mpr_rc rc);

/* Query the reference count of an rc cell. Useful for internal testing and some internal features. */
size_t mpr_rc_refcount(mpr_rc);

#endif // RC_H_INCLUDED

