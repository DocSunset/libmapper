#include "mapper_internal.h"
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
    parse_args(argc, argv);
    signal(SIGSEGV, segv);
    signal(SIGINT,  interrupt);
    signal(SIGTERM, interrupt);

    mpr_dev dev = mpr_dev_new("testdatasetaddrecord", 0);
    mpr_sig sig[3];
    sig[0] = mpr_sig_new(dev, MPR_DIR_OUT, "sig0", 1, MPR_INT32, 0, 0, 0, 0, 0, 0);
    int upd0[] = {0};
    sig[1] = mpr_sig_new(dev, MPR_DIR_OUT, "sig1", 3, MPR_INT32, 0, 0, 0, 0, 0, 0);
    int upd1[] = {1,2,3};
    sig[2] = mpr_sig_new(dev, MPR_DIR_OUT, "sig2", 1, MPR_FLT,   0, 0, 0, 0, 0, 0);
    float upd2[] = {4.5};
    mpr_time time = {0,0};

    mpr_dataset data = mpr_dataset_new("testdatasetaddrecord", mpr_obj_get_graph((mpr_obj)dev));

    mpr_data_record in[3];
    in[0] = mpr_data_record_new(sig[0], MPR_SIG_UPDATE, 0, 1, MPR_INT32, (const void*)upd0, time);
    in[1] = mpr_data_record_new(sig[1], MPR_SIG_UPDATE, 0, 3, MPR_INT32, (const void*)upd1, time);
    in[2] = mpr_data_record_new(sig[2], MPR_SIG_UPDATE, 0, 1, MPR_FLT,   (const void*)upd2, time);

    mpr_dataset_add_record(data, in[0]);
    mpr_dataset_add_record(data, in[1]);
    mpr_dataset_add_record(data, in[2]);

	mpr_data_record out[3];
    out[0] = mpr_dataset_get_record(data, 0);
    out[1] = mpr_dataset_get_record(data, 1);
    out[2] = mpr_dataset_get_record(data, 2);

    int result = 0;
    for (int i = 0; i < 3; ++i) {
        mpr_sig sig_in  = mpr_data_record_get_sig(in[i]);
        mpr_sig sig_out = mpr_data_record_get_sig(out[i]);
        if (sig_in != sig_out) {
            eprintf("%dth signals differ\n", i);
            eprintf("%x vs %x\n", sig_in, sig_out);
            result = result || 1;
        }

        mpr_sig_evt evt_in  = mpr_data_record_get_evt(in[i]);
        mpr_sig_evt evt_out = mpr_data_record_get_evt(out[i]);
        if (evt_in != evt_out) {
            eprintf("%dth events differ\n", i);
            eprintf("%d vs %d\n", evt_in, evt_out);
            result = result || 1;
        }

        mpr_type type_in  = mpr_data_record_get_type(in[i]);
        mpr_type type_out = mpr_data_record_get_type(out[i]);
        if (type_in != type_out) {
            eprintf("%dth types differ\n", i);
            eprintf("%d vs %d\n", type_in, type_out);
            result = result || 1;
        }

        mpr_time time_in  = mpr_data_record_get_time(in[i]);
        mpr_time time_out = mpr_data_record_get_time(out[i]);
        if (mpr_time_cmp(time_in, time_out) != 0) {
            eprintf("%dth times differ\n", i);
            eprintf("%d.%d vs %d.%d\n", time_in.sec, time_in.frac, time_out.sec, time_out.frac);
            result = result || 1;
        }

        int length_in  = mpr_data_record_get_length(in[i]);
        int length_out = mpr_data_record_get_length(out[i]);
        if (length_in != length_out) {
            eprintf("%dth lengths differ\n", i);
            eprintf("%d vs %d\n", length_in, length_out);
            result = result || 1;
        }

        mpr_id id_in  = mpr_data_record_get_instance(in[i]);
        mpr_id id_out = mpr_data_record_get_instance(out[i]);
        if (id_in != id_out) {
            eprintf("%dth ids differ\n", i);
            eprintf("%d vs %d\n", id_in, id_out);
            result = result || 1;
        }

        const void * value_in  = mpr_data_record_get_value(in[i]);
        const void * value_out = mpr_data_record_get_value(out[i]);
        if (0 != memcmp(value_in, value_out, length_out * mpr_type_get_size(type_out))) {
            eprintf("%dth values differ\n", i);
            result = result || 1;
        }
    }

    mpr_data_record_free(in[0]);
    mpr_data_record_free(in[1]);
    mpr_data_record_free(in[2]);
    mpr_dataset_free(data);

    printf("...................Test %s\x1B[0m.\n",
           result ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return result;
}
