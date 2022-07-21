#ifndef __MPR_DATASET_TYPES_H__
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

typedef struct _mpr_dataset {
    const char * name;
    int synced; /* The dataset is synchronized, meaning it is populated with data. This is true if the dataset is original, or if it is has been updated by an incoming data map to match a remote signal. */
    struct {
        mpr_dlist front;
        mpr_dlist back;
    } recs;
    mpr_dlist sigs;
    struct _mpr_data_sig *pubr; /* pointer to publishing sig, or null if not published */
} mpr_dataset_t, *mpr_dataset;

typedef struct _mpr_data_sig {
    mpr_obj_t obj;      /* always first (so that the sig can be safely cast as a mpr_obj) */
    const char * path;  /* OSC path of the sig, which must start with `/` */
    const char * name;  /* Name of the sig, i.e. path+1 */
    char dir;           /* mapper direction, either incoming or outgoing */
    mpr_dlist pubs; /* Datasets that this signal publishes */
    mpr_dlist subs; /* Datasets this signal is subscribed to */
    mpr_dev dev;        /* Pointer back to parent device */
} mpr_data_sig_t, *mpr_data_sig;

#define MPR_DATA_MAP_ELEMENTS mpr_obj_t obj; \
    mpr_data_sig src; \
    mpr_data_sig dst; \
    char is_local; \
    char is_local_only;

typedef struct _mpr_data_map {
    MPR_DATA_MAP_ELEMENTS
} mpr_data_map_t, *mpr_data_map;

typedef struct _mpr_local_data_map {
    MPR_DATA_MAP_ELEMENTS
    mpr_link link;
} mpr_local_data_map_t, *mpr_local_data_map;

#undef MPR_DATA_MAP_ELEMENTS

typedef struct _mpr_data_recorder {
    mpr_dev dev;
    int mapped;
    int armed;
    int recording;
    unsigned int num_sigs;
    mpr_sig * remote_sigs;
    mpr_sig * sigs;
    mpr_dataset data;
    mpr_dlist recordings;
} mpr_data_recorder_t, *mpr_data_recorder;

/* Make a copy of a dataset.
 * Since datasets are just a struct of pointers, this is reasonably inexpensive.
 * Furthermore, since the data itself is managed with mpr_dlist, copies can be
 * make indescriminately without fear of use after free errors.
 * Copies still need to be freed appropriately. */
mpr_dataset mpr_dataset_copy(mpr_dataset data);

#endif
