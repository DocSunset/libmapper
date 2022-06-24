#include "dlist.h"

typedef union {
    size_t count;
    size_t *ptr;
} refs;

typedef struct _mpr_dlist_t {
    struct _mpr_dlist_t *front;
    struct _mpr_dlist_t *prev;
    struct _mpr_dlist_t *next;
    mpr_dlist_data_destructor *destructor;
    refs refcount;
    void * data;
} mpr_dlist_t;

static inline bool _dlist

