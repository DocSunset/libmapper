#include "array.h"
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

    /* process flags for -v verbose, -t terminate, -h help */
    for (i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] == '-') {
            int len = strlen(argv[i]);
            for (j = 1; j < len; j++) {
                switch (argv[i][j]) {
                    case 'h':
                        printf("testdatasetsiglist.c: possible arguments "
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

int main(int argc, char ** argv)
{
    parse_args(argc, argv);
    signal(SIGSEGV, segv);
    signal(SIGINT,  interrupt);
    signal(SIGTERM, interrupt);

    int fail = 0;

    eprintf("Allocating the array.\n");
    mpr_array array = mpr_array_new(3, sizeof(dummy_t));
    if (0 == array) {
        fail = 1;
        eprintf("Failed to allocate array. Header math problems?\n");
        goto done;
    }

    dummy_t data = {1,1.0};
    size_t n = 6;

    eprintf("Populating the array.\n");
    for (int i = 0; i < n; ++i) mpr_array_add(array, 1, &data);

    eprintf("Verifying the size of the array.\n");
    if (mpr_array_get_size(array) != n) {
        fail = 1;
        eprintf("Actual size %lu does not match expected size %lu\n", mpr_array_get_size(array), n);
        goto done;
    }

    eprintf("Verifying the contents of the array.\n");
    for (dummy_t * d = array; d - (dummy_t*)array < n; ++d) {
        if (0 != memcmp(d, &data, sizeof(dummy_t))) {
            fail = 1;
            eprintf("Contents of array do not match expected value.\n");
            goto done;
        }
    }

  done:
    if (array) {
        eprintf("Freeing array.\n");
        mpr_array_free(array);
    }

    printf("...................Test %s\x1B[0m.\n",
           fail ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return fail;
}
