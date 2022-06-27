#include "dlist.h"
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
                        printf("testdlist.c: possible arguments "
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

int freed = 0;

void dummy_destructor(void * data)
{
    dummy_t *dummy = (dummy_t*)data;
    eprintf("Freeing data.\n");
    free(dummy);
    eprintf("Successfully freed data.\n");
    freed = 1;
}

int fail = 0;

int compare_data(dummy_t* data, mpr_dlist list)
{
    if (list == 0) {
        eprintf("List is unexpectedly a null pointer.\n");
        fail = 1;
        return 1;
    }
    if (memcmp(data, mpr_dlist_data(list), sizeof(dummy_t)) != 0) {
        eprintf("Data unexpectedly differ. %u,%f vs %u,%f\n",
                data->a, data->b,
                mpr_dlist_data_as(dummy_t*, list)->a,
                mpr_dlist_data_as(dummy_t*, list)->b);
        fail = 1;
        return 1;
    }
    return 0;
}

int confirm_refcount(mpr_dlist list, size_t expected)
{
    if (mpr_dlist_get_refcount(list) != expected) {
        eprintf("Actual refcount %lu does not match expected value %lu.\n"
                , mpr_dlist_get_refcount(list)
                , expected
                );
        fail = 1;
        return 1;
    }
    return 0;
}

int compare_lists(mpr_dlist a, mpr_dlist b)
{
    if (! mpr_dlist_equals(a, b)) {
        eprintf("Lists do not match\n");
        fail = 1;
        return 1;
    }
    return 0;
}

int check_null_list(mpr_dlist list)
{
    if (list != 0) {
        eprintf("List is not null.\n");
        fail = 1;
        return 1;
    }
    return 0;
}

int check_pointers_equal(mpr_dlist list, void * original_pointer)
{
    if (list != original_pointer) {
        eprintf("The moved list does not match the original pointer.\n");
        fail = 1;
        return 1;
    }
    return 0;
}

int try_to_free(mpr_dlist list, const char * name)
{
    eprintf("Attempting to free the list '%s'.\n", name);
    mpr_dlist_free(&list);
    if (list != 0) {
        eprintf("List apparently freed successfuly, but not set to nullptr\n");
        fail = 1;
        return 1;
    }
    return 0;
}


int main(int argc, char ** argv)
{
    parse_args(argc, argv);
    signal(SIGSEGV, segv);
    signal(SIGINT,  interrupt);
    signal(SIGTERM, interrupt);

    eprintf("Allocating some dummy data.\n");
    dummy_t * data = calloc(1, sizeof(dummy_t));
    data->a = 1;
    data->b = 2;

    eprintf("Allocating a new list.\n");
    mpr_dlist list = 0;
    mpr_dlist_new(&list, data, &dummy_destructor);
    if (list == 0) {
        eprintf("Failed to allocate list.\n");
        fail = 1;
        goto done;
    }

    eprintf("Confirming list contents.\n");
    if (compare_data(data, list)) goto done;

    eprintf("Confirming list's refcount.\n");
    if (confirm_refcount(list, 1)) goto done;

    eprintf("Making a copy.\n");
    mpr_dlist copy = 0;
    mpr_dlist_copy(&copy, list);

    eprintf("Confirming the copy matches the original.\n");
    if (compare_lists(copy, list)) goto done;

    eprintf("Confirming the copy's data.\n");
    if (compare_data(data, copy)) goto done;

    eprintf("Confirming taking a copy increased the original list's refcount.\n");
    if (confirm_refcount(list, 2)) goto done;

    eprintf("Confirming the copy's refcount.\n");
    if (confirm_refcount(copy, 1)) goto done;

    eprintf("Confirming original list contents were unaffected by the copy.\n");
    if (compare_data(data, list)) goto done;

    eprintf("Moving the original.\n");
    void * original_pointer = list;
    mpr_dlist move = 0;
    mpr_dlist_move(&move, &list);

    eprintf("Confirming that the original is now a nullptr.\n");
    if (check_null_list(list)) goto done;

    eprintf("Confirming the moved list matches the original pointer.\n");
    if (check_pointers_equal(move, original_pointer)) goto done;

    eprintf("Confirming the moved list matches the copy.\n");
    if (compare_lists(move, copy)) goto done;

    eprintf("Confirming the moved list's refcount is the same as the original.\n");
    if (confirm_refcount(move, 2)) goto done;

    eprintf("Confirming moved list contents.\n");
    if (compare_data(data, move)) goto done;

    eprintf("Moving the list back.\n");
    mpr_dlist_move(&list, &move);

    eprintf("Confirming the moved list is now a nullptr.\n");
    if (check_null_list(move)) goto done;

    eprintf("Confirming the original list matches the original pointer.\n");
    if (check_pointers_equal(list, original_pointer)) goto done;

    eprintf("Confirming list contents.\n");
    if (compare_data(data, list)) goto done;

    if (try_to_free(list, "list")) goto done;

    eprintf("Confirming the copy can still access the data.\n");
    float sum = mpr_dlist_data_as(dummy_t*,copy)->a + mpr_dlist_data_as(dummy_t*,copy)->b;
    if (3.0f != sum) {
        eprintf("%u + %f = %f\n"
                , mpr_dlist_data_as(dummy_t*,copy)->a
                , mpr_dlist_data_as(dummy_t*,copy)->b 
                , sum);
        fail = 1;
        goto done;
    }

    if (try_to_free(copy, "copy")) goto done;

  done:
    printf("...................Test %s\x1B[0m.\n",
           fail ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return fail;
}
