#include <mapper/mapper.h>
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

int main(int argc, char ** argv)
{
    parse_args(argc, argv);
    signal(SIGSEGV, segv);
    signal(SIGINT,  interrupt);
    signal(SIGTERM, interrupt);

    eprintf("Setting up signals.\n");
    mpr_dev dev = mpr_dev_new("testdatasetsiglistdev", 0);

    mpr_sig sig[3];
    sig[0] = mpr_sig_new(dev, MPR_DIR_OUT, "sig0", 1, MPR_INT32, 0, 0, 0, 0, 0, 0);
    int upd0[] = {0};
    sig[1] = mpr_sig_new(dev, MPR_DIR_OUT, "sig1", 3, MPR_INT32, 0, 0, 0, 0, 0, 0);
    int upd1[] = {1,2,3};
    sig[2] = mpr_sig_new(dev, MPR_DIR_OUT, "sig2", 1, MPR_FLT,   0, 0, 0, 0, 0, 0);
    float upd2[] = {4.5};
    mpr_time time = {0,0};

    eprintf("Setting up dataset.\n");
    mpr_dataset data = mpr_dataset_new("testdatasetsiglist", mpr_obj_get_graph(dev));

    eprintf("Setting up records.\n");
    mpr_data_record in[3];
    in[0] = mpr_data_record_new(sig[0], MPR_SIG_UPDATE, 0, 1, MPR_INT32, (const void*)upd0, time);
    in[1] = mpr_data_record_new(sig[1], MPR_SIG_UPDATE, 0, 3, MPR_INT32, (const void*)upd1, time);
    in[2] = mpr_data_record_new(sig[2], MPR_SIG_UPDATE, 0, 1, MPR_FLT,   (const void*)upd2, time);

    eprintf("Populating dataset.\n");
    for (unsigned int i = 0; i < 3; ++i)
        mpr_dataset_add_record(data, in[i]);

    eprintf("Getting signals.\n");
    int result = 1;
    mpr_dlist sigs = mpr_dataset_get_sigs(data);
    eprintf("Checking list size.\n");
    if (mpr_dlist_get_length(sigs) != 3) {
        eprintf("Signal list size %d != 3.\n", mpr_dlist_get_length(sigs));
        goto done;
    }
    eprintf("Signal list size matches.\n");
    for (; sigs; mpr_dlist_next(&sigs)) {
        if(!( mpr_dlist_data_as(mpr_sig, sigs) == sig[0]
           || mpr_dlist_data_as(mpr_sig, sigs) == sig[1]
           || mpr_dlist_data_as(mpr_sig, sigs) == sig[2] ))
        {
            eprintf("Signal list result not found.\n");
            goto done;
        }
    }
    eprintf("All signals found.\n");
    result = 0;

  done:

    for (unsigned int i = 0; i < 3; ++i)
        mpr_data_record_free(in[i]);

    mpr_dataset_free(data);

    mpr_dev_free(dev);

    printf("...................Test %s\x1B[0m.\n",
           result ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return result;
}
