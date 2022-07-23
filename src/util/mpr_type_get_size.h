#ifndef TYPE_H_INCLUDED
#define TYPE_H_INCLUDED

#include "util/mpr_inline.h"
#include "util/debug_macro.h"
#include <mapper/mapper_constants.h>

/*! Helper to find size of signal value types. */
MPR_INLINE static int mpr_type_get_size(mpr_type type)
{
    if (type <= MPR_LIST)   return sizeof(void*);
    switch (type) {
        case MPR_INT32:
        case MPR_BOOL:
        case 'T':
        case 'F':           return sizeof(int);
        case MPR_FLT:       return sizeof(float);
        case MPR_DBL:       return sizeof(double);
        case MPR_PTR:       return sizeof(void*);
        case MPR_STR:       return sizeof(char*);
        case MPR_INT64:     return sizeof(int64_t);
        case MPR_TIME:      return sizeof(mpr_time);
        case MPR_TYPE:      return sizeof(mpr_type);
        default:
            die_unless(0, "Unknown type '%c' in mpr_type_get_size().\n", type);
            return 0;
    }
}

#endif // TYPE_H_INCLUDED
