#include <mapper/mapper.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

mpr_dataset data;

#define NUM_SIGS (sizeof(sigs)/sizeof(sigs[0]))

int verbose = 1;
int terminate = 0;
int done = 0;
char * iface = 0;

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
                        printf("testmapinput.c: possible arguments "
                               "-q quiet (suppress output), "
                               "-t terminate automatically, "
                               "-h help, "
                               "--iface network interface\n");
                        exit(1);
                        break;
                    case 'q':
                        verbose = 0;
                        break;
                    case 't':
                        terminate = 1;
                        break;
                    case '-':
                        if (strcmp(argv[i], "--iface")==0 && argc>i+1) {
                            i++;
                            iface = argv[i];
                            j = 1;
                        }
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
    int result = 0;
    parse_args(argc, argv);
    signal(SIGSEGV, segv);
    signal(SIGINT,  interrupt);
    signal(SIGTERM, interrupt);

	mpr_dev dev = mpr_dev_new("testdatasetaddrecord", 0);
	mpr_sig sig1 = mpr_sig_new(dev, MPR_DIR_OUT, "sig1", 1, MPR_INT32, 0, 0, 0, 0, 0, 0);
	int upd1[] = {0};
	mpr_sig sig2 = mpr_sig_new(dev, MPR_DIR_OUT, "sig2", 3, MPR_INT32, 0, 0, 0, 0, 0, 0);
	int upd2[] = {1,2,3};
	mpr_sig sig3 = mpr_sig_new(dev, MPR_DIR_OUT, "sig3", 1, MPR_FLT,   0, 0, 0, 0, 0, 0);
	float upd3[] = {4.5};
	mpr_time time;

    data = mpr_dataset_new("testdatasetaddrecord");
    mpr_dataset_add_record(data, sig1, MPR_SIG_UPDATE, 0, 1, MPR_INT32, (const void*)upd1, time);
    mpr_dataset_add_record(data, sig2, MPR_SIG_UPDATE, 0, 3, MPR_INT32, (const void*)upd2, time);
    mpr_dataset_add_record(data, sig3, MPR_SIG_UPDATE, 0, 1, MPR_FLT,   (const void*)upd3, time);

    /* TODO: check that the binary representation is as expected */

    printf("...................Test %s\x1B[0m.\n",
           result ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return result;
}
