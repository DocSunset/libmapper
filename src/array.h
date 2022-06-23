#ifndef ARRAY_H_INCLUDED
#define ARRAY_H_INCLUDED

#include <stddef.h>

typedef void* mpr_array;

/* Reserves zero-initialized memory for `length` elements of `size` bytes each (does not check for integer overflow),
 * but also allocates a secrete header with metadata about the state of the array
 * stored just in front of the returned pointer. */
mpr_array mpr_array_new(size_t length, size_t size);

/* Frees the array and its secret header */
void mpr_array_free(mpr_array array);

/* Adds `length` elements stored in the location pointed to by `value` to the back of `array`,
 * assuming that each element has the same size in bytes as was passed when allocating the array.
 * If there is not enough room, this will attempt to realloc. Returns the current location of the array, or 0 if there
 * is an error while reallocating. */
void * mpr_array_add(mpr_array array, size_t length, void * value);

/* Returns the size in elements of the array */
size_t mpr_array_get_size(const mpr_array array);

/* For accessing elements in the array the user is advised to cast the void* to a pointer of the type of elements in the array. However, the same method should not be used to add elements to the array. */

#endif // ARRAY_H_INCLUDED

