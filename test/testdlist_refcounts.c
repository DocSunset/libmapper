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
                        printf("testdlist_refcounts.c: possible arguments "
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
    eprintf("Freeing data.\n");
    free(data);
    ++freed;
}

int fail = 0;

void set_contents(mpr_dlist list)
{
    mpr_dlist_data_as(dummy_t*, list)->a = 1;
    mpr_dlist_data_as(dummy_t*, list)->b = 2;
}

int confirm_contents(mpr_dlist list)
{
    if (list == 0) {
        eprintf("List is unexpectedly a null pointer.\n");
        fail = 1;
        return 1;
    }
    if (mpr_dlist_data_as(dummy_t*, list)->a != 1) fail = 1;
    if (mpr_dlist_data_as(dummy_t*, list)->b != 2) fail = 1;
    if (fail == 1) {
        eprintf("Data unexpectedly differ. 1,2.0f vs %u,%f\n",
                mpr_dlist_data_as(dummy_t*, list)->a,
                mpr_dlist_data_as(dummy_t*, list)->b);
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
        eprintf("List is unexpectedly not null.\n");
        fail = 1;
        return 1;
    }
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

    /* forward */
    {
        eprintf("Allocating a new list.\n");
        mpr_dlist front = 0;
        mpr_dlist_new(&front, 0, sizeof(dummy_t), &dummy_destructor);
        if (check_not_null_list(front)) goto done;

        eprintf("Confirming list's refcount.\n");
        if (confirm_refcount(front, 1)) goto done;

        eprintf("Inserting after with ref.\n");
        mpr_dlist back = 0;
        mpr_dlist_insert_after(&back, front, 0, sizeof(dummy_t), &dummy_destructor);
        if (check_not_null_list(back)) goto done;

        eprintf("Confirming refcounts.\n");
        if (confirm_refcount(front, 1)) goto done;
        if (confirm_refcount(back, 2)) goto done;

        eprintf("Confirming list length.\n");
        if (confirm_length(front, 2)) goto done;

        eprintf("Inserting after without ref.\n");
        mpr_dlist_insert_after(0, back, 0, sizeof(dummy_t), &dummy_destructor);

        eprintf("Confirming list length.\n");
        if (confirm_length(front, 3)) goto done;

        eprintf("Confirming refcounts.\n");
        if (confirm_refcount(front, 1)) goto done;
        if (confirm_refcount(back, 2)) goto done;

        eprintf("Advancing `back`.\n");
        mpr_dlist_next(&back);
        if (check_not_null_list(&back)) goto done;

        eprintf("Confirming refcounts.\n");
        if (confirm_refcount(front, 1)) goto done;
        if (confirm_refcount(back, 2)) goto done;

        eprintf("Advancing `back` to null list terminator.\n");
        mpr_dlist_next(&back);
        if (check_null_list(back)) goto done;

        eprintf("Confirming refcounts.\n");
        if (confirm_refcount(front, 1)) goto done;

        if (try_to_free(front, "front")) goto done;
        if (confirm_freed(3)) goto done;
    }

    /* backward */
    {
        eprintf("Allocating a new list.\n");
        mpr_dlist back = 0;
        mpr_dlist_new(&back, 0, sizeof(dummy_t), &dummy_destructor);
        if (check_not_null_list(back)) goto done;

        eprintf("Confirming list's refcount.\n");
        if (confirm_refcount(back, 1)) goto done;

        eprintf("Inserting before with ref.\n");
        mpr_dlist front = 0;
        mpr_dlist_insert_before(&front, back, 0, sizeof(dummy_t), &dummy_destructor);
        mpr_dlist_insert_before(&front, front, 0, sizeof(dummy_t), &dummy_destructor);
        mpr_dlist_insert_before(&front, front, 0, sizeof(dummy_t), &dummy_destructor);
        if (check_not_null_list(front)) goto done;

        eprintf("Confirming refcounts.\n");
        if (confirm_refcount(front, 1)) goto done;
        if (confirm_refcount(back, 2)) goto done;

        eprintf("Confirming list length.\n");
        if (confirm_length(back, 4)) goto done;

        eprintf("Inserting before without ref.\n");
        mpr_dlist_insert_before(0, front, 0, sizeof(dummy_t), &dummy_destructor);

        eprintf("Confirming list length is unchanged.\n");
        if (confirm_length(back, 4)) goto done;

        eprintf("Confirming refcounts.\n");
        if (confirm_refcount(front, 1)) goto done;
        if (confirm_refcount(back, 2)) goto done;

        eprintf("Advancing `front`.\n");
        mpr_dlist_next(&front);
        if (check_not_null_list(&front)) goto done;

        eprintf("Confirming one freed.\n");
        if (confirm_freed(1)) goto done;

        eprintf("Confirming refcounts.\n");
        if (confirm_refcount(front, 1)) goto done;
        if (confirm_refcount(back, 2)) goto done;

        eprintf("Confirming list length.\n");
        if (confirm_length(back, 3)) goto done;

        eprintf("Reversing `back`.\n");
        mpr_dlist_prev(&back);
        if (check_not_null_list(back)) goto done;

        eprintf("Confirming refcounts.\n");
        if (confirm_refcount(front, 1)) goto done;
        if (confirm_refcount(back, 2)) goto done;

        eprintf("Confirming list length is unchanged.\n");
        if (confirm_length(back, 3)) goto done;

        if (try_to_free(front, "front")) goto done;
        if (confirm_freed(1)) goto done;

        if (try_to_free(back, "back")) goto done;
        if (confirm_freed(2)) goto done;

    }

  done:
    printf("...................Test %s\x1B[0m.\n",
           fail ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return fail;
}
