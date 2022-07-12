#define __MPR_DATASET_TYPES_H__
#include "types_internal.h"
#include <mapper/dlist.h>

typedef struct _mpr_data_record {
    mpr_sig sig;
    mpr_sig_evt evt;
    mpr_id instance;
    int length;
    mpr_type type;
    mpr_time time;
    const void * value;
} mpr_data_record_t, *mpr_data_record;

/* TODO: we will probably need to distinguish between datasets in general, and those that have been copied
 * to the local context */
typedef struct _mpr_dataset {
    mpr_obj_t obj;
    const char * name;
    struct {
        mpr_dlist front;
        mpr_dlist back;
    } recs;
    mpr_dlist sigs;
    unsigned int num_records;
    int is_local;
} mpr_dataset_t, *mpr_dataset;

typedef struct _mpr_data_recorder {
    mpr_dev dev;
    int mapped;
    int armed;
    int recording;
    unsigned int num_sigs;
    mpr_sig * remote_sigs; /* TODO: make the map in recorder_new so you don't need to track this */
    mpr_sig * sigs;
    mpr_dataset data;
} mpr_data_recorder_t, *mpr_data_recorder;
