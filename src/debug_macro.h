#ifndef DEBUG_MACRO_H_INCLUDED
#define DEBUG_MACRO_H_INCLUDED

#define RETURN_UNLESS(condition) { if (!(condition)) { return; }}
#define RETURN_ARG_UNLESS(condition, arg) { if (!(condition)) { return arg; }}
#define DONE_UNLESS(condition) { if (!(condition)) { goto done; }}

/* TODO: module specific tracers (trace_dev etc.) should be module specific */
#if DEBUG
#define TRACE_RETURN_UNLESS(a, ret, ...) \
if (!(a)) { trace(__VA_ARGS__); return ret; }
#define TRACE_DEV_RETURN_UNLESS(a, ret, ...) \
if (!(a)) { trace_dev(dev, __VA_ARGS__); return ret; }
#define TRACE_NET_RETURN_UNLESS(a, ret, ...) \
if (!(a)) { trace_net(__VA_ARGS__); return ret; }
#else
#define TRACE_RETURN_UNLESS(a, ret, ...) if (!(a)) { return ret; }
#define TRACE_DEV_RETURN_UNLESS(a, ret, ...) if (!(a)) { return ret; }
#define TRACE_NET_RETURN_UNLESS(a, ret, ...) if (!(a)) { return ret; }
#endif

/*! Debug tracer */
#ifdef __GNUC__
#ifdef DEBUG
#include <stdio.h>
#include <assert.h>
#define trace(...) { printf("-- " __VA_ARGS__); }
#define trace_graph(...)  { printf("\x1B[31m-- <graph>\x1B[0m " __VA_ARGS__);}
#define trace_dev(DEV, ...)                                                         \
{                                                                                   \
    if (!DEV)                                                                       \
        printf("\x1B[32m-- <device>\x1B[0m ");                                      \
    else if (DEV->is_local && ((mpr_local_dev)DEV)->registered)                     \
        printf("\x1B[32m-- <device '%s'>\x1B[0m ", mpr_dev_get_name((mpr_dev)DEV)); \
    else                                                                            \
        printf("\x1B[32m-- <device '%s.?'::%p>\x1B[0m ", DEV->prefix, DEV);         \
    printf(__VA_ARGS__);                                                            \
}
#define trace_net(...)  { printf("\x1B[33m-- <network>\x1B[0m  " __VA_ARGS__);}
#define die_unless(a, ...) { if (!(a)) { printf("-- " __VA_ARGS__); assert(a); } }
#else /* !DEBUG */
#define trace(...) {}
#define trace_graph(...) {}
#define trace_dev(...) {}
#define trace_dataset(...) {};
#define trace_net(...) {}
#define die_unless(...) {}
#endif /* DEBUG */
#else /* !__GNUC__ */
#define trace(...) {};
#define trace_graph(...) {};
#define trace_dev(...) {};
#define trace_dataset(...) {};
#define trace_net(...) {};
#define die_unless(...) {};
#endif /* __GNUC__ */


#endif // debug_macro_h_INCLUDED

