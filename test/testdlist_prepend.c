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
                        printf("testdlist_prepend.c: possible arguments "
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
    ++freed;
}

int fail = 0;

int main(int argc, char ** argv)
{
    parse_args(argc, argv);
    signal(SIGSEGV, segv);
    signal(SIGINT,  interrupt);
    signal(SIGTERM, interrupt);

    eprintf("Populating list.\n");
    size_t initial_size = 5;
    mpr_dlist front = 0;
    for (size_t i = 0; i < initial_size; ++i) {
         mpr_dlist_prepend(&front, mpr_rc_new(sizeof(dummy_t), &dummy_destructor));
    }

    if (mpr_dlist_get_length(front) != initial_size) {
        eprintf("Actual length %lu does not match expected length %lu.\n"
               , mpr_dlist_get_length(front)
               , initial_size
               );
        fail = 1;
    }

    mpr_dlist_free(front);
    printf("...................Test %s\x1B[0m.\n",
           fail ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return fail;
}
