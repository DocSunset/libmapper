#include <mapper/mapper.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

mpr_dev dev;
mpr_dataset data;
mpr_data_recorder rec;
mpr_sig sigs[24];

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

int setup_device(const char *iface)
{
    dev = mpr_dev_new("testrecorddataset", 0);
    if (!dev) goto error;
    if (iface) mpr_graph_set_interface(mpr_obj_get_graph(dev), iface);
    eprintf("device created using interface %s.\n",
            mpr_graph_get_interface(mpr_obj_get_graph(dev)));


    /* sigs: {input, output} {length1, length3} * {float, int, double} * {solo, instanced} = 24 */
    float  mnf[] = {0,0,0}, mxf[] = {127,127,127};
    int    mni[] = {0,0,0}, mxi[] = {127,127,127};
    double mnd[] = {0,0,0}, mxd[] = {127,127,127};
    int instances = 3;
    sigs[ 0]  = mpr_sig_new(dev, MPR_DIR_IN,  "in1f1",  1, MPR_FLT, NULL, mnf, mxf, NULL, NULL, 0);
    sigs[ 1]  = mpr_sig_new(dev, MPR_DIR_IN,  "in1f3",  1, MPR_FLT, NULL, mnf, mxf, &instances, NULL, 0);
    sigs[ 2]  = mpr_sig_new(dev, MPR_DIR_IN,  "in1i1",  1, MPR_INT32, NULL, mni, mxi, NULL, NULL, 0);
    sigs[ 3]  = mpr_sig_new(dev, MPR_DIR_IN,  "in1i3",  1, MPR_INT32, NULL, mni, mxi, &instances, NULL, 0);
    sigs[ 4]  = mpr_sig_new(dev, MPR_DIR_IN,  "in1d1",  1, MPR_DBL, NULL, mnd, mxd, NULL, NULL, 0);
    sigs[ 5]  = mpr_sig_new(dev, MPR_DIR_IN,  "in1d3",  1, MPR_DBL, NULL, mnd, mxd, &instances, NULL, 0);
    sigs[ 6]  = mpr_sig_new(dev, MPR_DIR_IN,  "in3f1",  3, MPR_FLT, NULL, mnf, mxf, NULL, NULL, 0);
    sigs[ 7]  = mpr_sig_new(dev, MPR_DIR_IN,  "in3f3",  3, MPR_FLT, NULL, mnf, mxf, &instances, NULL, 0);
    sigs[ 8]  = mpr_sig_new(dev, MPR_DIR_IN,  "in3i1",  3, MPR_INT32, NULL, mni, mxi, NULL, NULL, 0);
    sigs[ 9]  = mpr_sig_new(dev, MPR_DIR_IN,  "in3i3",  3, MPR_INT32, NULL, mni, mxi, &instances, NULL, 0);
    sigs[10]  = mpr_sig_new(dev, MPR_DIR_IN,  "in3d1",  3, MPR_DBL, NULL, mnd, mxd, NULL, NULL, 0);
    sigs[11]  = mpr_sig_new(dev, MPR_DIR_IN,  "in3d3",  3, MPR_DBL, NULL, mnd, mxd, &instances, NULL, 0);
    sigs[12] = mpr_sig_new(dev, MPR_DIR_OUT, "out1f1", 1, MPR_FLT, NULL, mnf, mxf, NULL, NULL, 0);
    sigs[13] = mpr_sig_new(dev, MPR_DIR_OUT, "out1f3", 1, MPR_FLT, NULL, mnf, mxf, &instances, NULL, 0);
    sigs[14] = mpr_sig_new(dev, MPR_DIR_OUT, "out1i1", 1, MPR_INT32, NULL, mni, mxi, NULL, NULL, 0);
    sigs[15] = mpr_sig_new(dev, MPR_DIR_OUT, "out1i3", 1, MPR_INT32, NULL, mni, mxi, &instances, NULL, 0);
    sigs[16] = mpr_sig_new(dev, MPR_DIR_OUT, "out1d1", 1, MPR_DBL, NULL, mnd, mxd, NULL, NULL, 0);
    sigs[17] = mpr_sig_new(dev, MPR_DIR_OUT, "out1d3", 1, MPR_DBL, NULL, mnd, mxd, &instances, NULL, 0);
    sigs[18] = mpr_sig_new(dev, MPR_DIR_OUT, "out3f1", 3, MPR_FLT, NULL, mnf, mxf, NULL, NULL, 0);
    sigs[19] = mpr_sig_new(dev, MPR_DIR_OUT, "out3f3", 3, MPR_FLT, NULL, mnf, mxf, &instances, NULL, 0);
    sigs[20] = mpr_sig_new(dev, MPR_DIR_OUT, "out3i1", 3, MPR_INT32, NULL, mni, mxi, NULL, NULL, 0);
    sigs[21] = mpr_sig_new(dev, MPR_DIR_OUT, "out3i3", 3, MPR_INT32, NULL, mni, mxi, &instances, NULL, 0);
    sigs[22] = mpr_sig_new(dev, MPR_DIR_OUT, "out3d1", 3, MPR_DBL, NULL, mnd, mxd, NULL, NULL, 0);
    sigs[23] = mpr_sig_new(dev, MPR_DIR_OUT, "out3d3", 3, MPR_DBL, NULL, mnd, mxd, &instances, NULL, 0);

    return 0;

  error:
    return 1;
}

void update_signals(int i)
{
    int j, k, prop_length, length, instances;
    const void * prop_val;
    mpr_type type;
    i = i % 128;
    int iout[]    = {i, i, i};
    float fout[]  = {i, i, i};
    double dout[] = {i, i, i};
    void * out;

    for (k = 0; k < 24; ++k) {
        mpr_sig sig = sigs[k];

        mpr_obj_get_prop_by_idx(sig, MPR_PROP_LEN, 0, &prop_length, &type, &prop_val, 0);
        length = *(int *)prop_val;
        mpr_obj_get_prop_by_idx(sig, MPR_PROP_NUM_INST, 0, &prop_length, &type, &prop_val, 0);
        instances = *(int *)prop_val;

        mpr_obj_get_prop_by_idx(sig, MPR_PROP_TYPE, 0, &prop_length, &type, &prop_val, 0);
        type = *(mpr_type *)prop_val;
        switch (type) {
            case MPR_FLT:
                out = fout;
                break;
            case MPR_INT32:
                out = iout;
                break;
            case MPR_DBL:
                out = dout;
                break;
            default:
                out = fout;
        }

        if (instances == 1) mpr_sig_set_value(sig, 0, length, type, out);
        else {
            if (i < 64) {
                for (j = 0; j < instances; ++j)
                    mpr_sig_set_value(sig, mpr_sig_get_inst_id(sig, j, (MPR_STATUS_ACTIVE | MPR_STATUS_RESERVED)),
                                      length, type, out);
            }
            else {
                for (j = 0; j < instances; ++j)
                    mpr_sig_release_inst(sig, mpr_sig_get_inst_id(sig, j, (MPR_STATUS_ACTIVE | MPR_STATUS_RESERVED)));
            }
        }
    }

    mpr_dev_poll(dev, 0);
}

void cleanup_device()
{
    eprintf("Freeing device.. ");
    fflush(stdout);
    mpr_dev_free(dev);
    eprintf("ok\n");
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

    if (setup_device(iface)) {
        eprintf("Error initializing device.\n");
        result = 1;
        goto done;
    }

    while(!done && !mpr_dev_get_is_ready(dev)) mpr_dev_poll(dev, 50);

    eprintf("Device ready, creating dataset.\n");
    data = mpr_dataset_new("test_dataset", NUM_SIGS, sigs);
    rec = mpr_data_recorder_new(data, 0);

    eprintf("Waiting for recorder device.\n");
    while(!done && !mpr_data_recorder_get_is_ready(rec)) mpr_data_recorder_poll(rec, 50);

    eprintf("Arming dataset.\n");
    mpr_data_recorder_arm(rec);

    eprintf("Waiting for record arm.\n");
    while(!done && !mpr_data_recorder_get_is_armed(rec)) mpr_data_recorder_poll(rec, 50);

    eprintf("Starting recording.\n");
    mpr_data_recorder_start(rec);

    eprintf("Entering loop.\n");
    int i = 0;
    while((!terminate || i < 100) && !done) {
        update_signals(i);
        mpr_data_recorder_poll(rec, 200);
        ++i;
    }

    mpr_data_recorder_stop(rec);
    mpr_data_recorder_disarm(rec);

    /* TODO TEST ASSERTION GOES HERE */

  done:
    cleanup_device();
    mpr_data_recorder_free(rec);
    mpr_dataset_free(data);

    printf("...................Test %s\x1B[0m.\n",
           result ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return result;
}
