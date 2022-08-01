#include <mapper/dlist.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

int verbose = 1;
int done = 0;

static void eprintf(const char *format, ...)
{
    va_list args;
    if (!verbose) return;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void interrupt(int sig)
{
    done = 1;
}

void segv(int sig) {
    printf("\x1B[31m(SEGV)\n\x1B[0m");
    exit(1);
}

void parse_args(int argc, char ** argv)
{
    int i, j;

    /* process flags for -v verbose, -h help */
    for (i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] == '-') {
            int len = strlen(argv[i]);
            for (j = 1; j < len; j++) {
                switch (argv[i][j]) {
                    case 'h':
                        printf("testdlist_filter.c: possible arguments "
                               "-q quiet (suppress output), "
                               "-h help, "
                        );
                        exit(1);
                        break;
                    case 'q':
                        verbose = 0;
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

typedef struct {
    int a;
    float b;
} dummy_t;

size_t freed = 0;

void dummy_destructor(void * data)
{
    eprintf("Freeing data %d.\n", ((dummy_t*)data)->a);
    ++freed;
}

int fail = 0;

int try_to_free(mpr_dlist list, const char * name)
{
    eprintf("Attempting to free the list '%s'.\n", name);
    mpr_dlist_free(list);
    return 0;
}

int check_not_null_list(mpr_dlist list)
{
    if (list == 0) {
        eprintf("List is unexpectedly null.\n");
        fail = 1;
        return 1;
    }
    return 0;
}

int confirm_contents(mpr_dlist list, size_t i)
{
    if (list == 0) {
        eprintf("List is unexpectedly a null pointer.\n");
        fail = 1;
        return 1;
    }
    if (mpr_dlist_data_as(dummy_t*, list)->a != i) fail = 1;
    if (mpr_dlist_data_as(dummy_t*, list)->b != i) fail = 1;
    if (fail == 1) {
        eprintf("Data unexpectedly differ. %lu vs %u,%f\n",
                i,
                mpr_dlist_data_as(dummy_t*, list)->a,
                mpr_dlist_data_as(dummy_t*, list)->b);
        return 1;
    }
    return 0;
}

int confirm_length(mpr_dlist list, size_t expected)
{
    if (mpr_dlist_get_length(list) != expected) {
        eprintf("Actual length %lu does not match expected length %lu.\n"
               , mpr_dlist_get_length(list)
               , expected
               );
        fail = 1;
        return 1;
    }
    return 0;
}

int confirm_freed(size_t expected)
{
    if (freed != expected) {
        eprintf("Reported number freed %lu does not match expected number %lu.\n"
               , freed
               , expected
               );
        fail = 1;
        return 1;
    }
    freed = 0;
    return 0;
}

int predicate_equals(void * datum, const char * types, mpr_union *va)
{
    dummy_t * dumdum = (dummy_t*)datum;
    return dumdum->a == va[0].i && dumdum->b == va[1].f;
}

int predicate_greater_than(void * datum, const char * types, mpr_union *va)
{
    dummy_t * dumdum = (dummy_t*)datum;
    return dumdum->a > va[0].i && dumdum->b > va[1].f;
}

int main(int argc, char ** argv)
{
    parse_args(argc, argv);
    signal(SIGSEGV, segv);
    signal(SIGINT,  interrupt);
    signal(SIGTERM, interrupt);

    {
        eprintf("Populating list.\n");
        size_t initial_size = 5;
        mpr_dlist list = 0;
        for (size_t i = 0; i < initial_size; ++i) {
             mpr_dlist_prepend(&list, mpr_rc_new(sizeof(dummy_t), &dummy_destructor));
             mpr_dlist_data_as(dummy_t*, list)->a = i;
             mpr_dlist_data_as(dummy_t*, list)->b = i;
        }

        eprintf("Filtering for just one element.\n");
        mpr_dlist filt = mpr_dlist_new_filter(list, &predicate_equals, "if", 1, (float)1);

        eprintf("Confirming filtered list is not null.\n");
        if (check_not_null_list(filt)) goto done;

        eprintf("Confirming filtered list data matches expected value.\n");
        if (confirm_contents(filt, 1)) goto done;

        try_to_free(filt, "filt");

        eprintf("Filtering for multiple elements.\n");
        filt = mpr_dlist_new_filter(list, &predicate_greater_than, "if", 2, (float)2);
        mpr_dlist cache = mpr_dlist_make_ref(filt);

        eprintf("Confirming filtered list is not null.\n");
        if (check_not_null_list(filt)) goto done;

        eprintf("Confirming filtered list data matches expected value.\n");
        for (size_t i = initial_size-1; i > 2; --i) {
            if (confirm_contents(filt, i)) goto done;
            mpr_dlist_next(&filt);
        }

        eprintf("Confirming cached list has expected length.\n");
        if (confirm_length(cache, 2)) goto done;

        try_to_free(filt, "filt");
        try_to_free(cache, "cache");
        try_to_free(list, "list");

        confirm_freed(initial_size);
    }

  done:
    printf("...................Test %s\x1B[0m.\n",
           fail ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return fail;
}
