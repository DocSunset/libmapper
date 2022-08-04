#include <mapper/mapper.h>
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
                        printf("testdatasetpubsub.c: possible arguments "
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

int got_dataset_insert = 0;
int got_dataset_remove = 0;
int dataset_length_matches = 0;
int dataset_value_matches = 0;
float value = 42.0;

void handler(mpr_data_sig sig, mpr_dataset data, mpr_data_record record, int event)
{
    eprintf("handler\n");
    if (event & MPR_DATASET_INSERT) {
        got_dataset_insert = 1;
        if (mpr_dataset_get_num_records(data) == 1) {
            dataset_length_matches = 1;
            mpr_data_record rec = *(mpr_data_record*)mpr_dataset_get_records(data);
            if (*(float*)mpr_data_record_get_value(rec) == value)
                dataset_value_matches = 1;
        }
    }
    if (event & MPR_DATASET_REMOVE) got_dataset_remove = 1;
}

int main(int argc, char ** argv)
{
    parse_args(argc, argv);
    signal(SIGSEGV, segv);
    signal(SIGINT,  interrupt);
    signal(SIGTERM, interrupt);

    /* TODO: test the shared graph case */
    eprintf("Setting up publisher.\n");
    mpr_dev pubber = mpr_dev_new("test_publisher", 0);
    mpr_data_sig pubsig = mpr_data_sig_new(pubber, "pubsig", 0, 0);
    mpr_sig sig = mpr_sig_new(pubber, MPR_DIR_OUT, "testsig", 1, MPR_FLT, 0, 0, 0, 0, 0, 0);

    eprintf("Setting up subscriber.\n");
    mpr_dev subber = mpr_dev_new("test_subscriber", 0);
    mpr_data_sig subsig = mpr_data_sig_new(subber, "subsig", handler, MPR_DATA_ALL);

    eprintf("Setting up dataset.\n");
    mpr_time time = {0,0};
    mpr_data_record record = mpr_data_record_new(sig, MPR_SIG_UPDATE, 0, 1, MPR_FLT, &value, time);
    mpr_dataset data = mpr_dataset_new("testdata", 0);
    mpr_dataset_add_record(data, record);

    eprintf("Waiting for devices.\n");
    while(!(mpr_dev_get_is_ready(pubber) && mpr_dev_get_is_ready(subber))) {
#define POLL \
        mpr_dev_poll(pubber, 10);\
        mpr_dev_poll(subber, 10);\

        POLL;
        if (done) goto done;
    }

    eprintf("Making data map.\n");
    mpr_data_map map = mpr_data_map_new(pubsig, subsig);
    mpr_obj_push(map);

    eprintf("Waiting for data map.\n");
    while(!mpr_data_map_get_is_ready(map)) {
        POLL;
        if (done) goto done;
    }

    eprintf("Publishing dataset.\n");
    mpr_data_sig_publish_dataset(pubsig, data);

    eprintf("Polling devices.\n");
    POLL;
    if (done) goto done;

    /* not necessarily all in this test file... */
    /* DATATODO: test adding and removing records from the dataset */
    /* DATATODO: test applying standardized filters to the dataset */
    /* DATATODO: test merging the dataset with another dataset */
    /* DATATODO: test the network case(s) (src requests map, dst requests map, monitor requests map) */

    eprintf("Withdrawing dataset.\n");
    mpr_dataset_withdraw(data);

    eprintf("Polling devices.\n");
    POLL;
    if (done) goto done;

  done:
    mpr_dev_free(pubber);
    mpr_dev_free(subber);
    mpr_data_record_free(record);
    mpr_dataset_free(data);

    int fail;
    eprintf("Insert: %s\nRemove: %s\nLength: %s\nValue:  %s\n"
           , got_dataset_insert ? "yes" : "no"
           , got_dataset_remove ? "yes" : "no"
           , dataset_length_matches ? "yes" : "no"
           , dataset_value_matches ? "yes" : "no"
           );
    if (got_dataset_insert && got_dataset_remove && dataset_length_matches && dataset_value_matches) fail = 0;
    else fail = 1;

    printf("...................Test %s\x1B[0m.\n",
           fail ? "\x1B[31mFAILED" : "\x1B[32mPASSED");
    return fail;
}
