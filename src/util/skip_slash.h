#ifndef SKIP_SLASH_H_INCLUDED
#define SKIP_SLASH_H_INCLUDED

#include "util/mpr_inline.h"
/*! Helper to remove a leading slash '/' from a string. */
MPR_INLINE static const char *skip_slash(const char *string)
{
    return string + (string && string[0]=='/');
}

#endif // SKIP_SLASH_H_INCLUDED

