#include "array.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t item_size;
    size_t num_items;
    size_t num_reserved;
    char *next_byte;
    void *data; /* the incantation to treat this stub as the actual data array: (void*)&array->data */
} _mpr_array_t;

/* hopefully the compiler is smart enough to make this a compile-time constant */
static inline size_t mpr_array_header_size()
{
    _mpr_array_t *array;
    return (char*)&array->data - (char*)array;
}

static inline _mpr_array_t *_header_by_data(const mpr_array arr)
{
    return (_mpr_array_t*)( (char*)arr - mpr_array_header_size() );
}

static inline void * _reallocate(_mpr_array_t *array, size_t required_length)
{
    size_t new_length = array->num_reserved;
    while (new_length < required_length) new_length = new_length * 2; /* arbitrary growth factor */
    size_t new_bytes = new_length * array->item_size + mpr_array_header_size();
    void * new_array = realloc(array, new_bytes);
    if (new_array== 0) return new_array;
    array = new_array;
    array->num_reserved = required_length;
    return array;
}

void * mpr_array_new(size_t length, size_t size)
{
    _mpr_array_t *array;
    array = calloc(1, mpr_array_header_size() + size * length);
    array->item_size = size;
    array->num_items = 0;
    array->num_reserved = length;
    array->next_byte = (char*)&array->data;
    return (void*)&array->data;
}

void mpr_array_free(mpr_array arr)
{
    _mpr_array_t *array = _header_by_data(arr);
    free(array);
}

void * mpr_array_add(mpr_array arr, size_t length, void * value)
{
    _mpr_array_t *array = _header_by_data(arr);
    size_t required_length = array->num_items + length;
    if (array->num_reserved < required_length) {
        _mpr_array_t * new_array = _reallocate(array, required_length);
        if (new_array == 0) return new_array;
        else array = new_array;
    }
    size_t bytes = length * array->item_size;
    memcpy((void*)array->next_byte, value, bytes);
    array->num_items += length;
    array->next_byte += bytes;
    return (void*)&array->data;
}

size_t mpr_array_get_size(const mpr_array arr)
{
    _mpr_array_t *array = _header_by_data(arr);
    return array->num_items;
};
