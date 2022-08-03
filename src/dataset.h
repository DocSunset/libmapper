#ifndef __MPR_DATASET_TYPES_H__
#define __MPR_DATASET_TYPES_H__
/* mpr_obj_t, mpr_sig, mapper_constants, mpr_time, mpr_dev, mpr_dev_local */
#include "types_internal.h"
#include <mapper/dlist.h>

typedef struct _mpr_data_record {
    mpr_sig sig;
    mpr_sig_evt evt;
    mpr_id instance;
    int length;
    mpr_type type;
    mpr_time time;
    void * value;
} mpr_data_record_t, *mpr_data_record;

typedef struct _mpr_dataset {
    const char * name;
    int synced; /* The dataset is synchronized, meaning it is populated with data. This is true if the dataset is original, or if it is has been updated by an incoming data map to match the remote dataset it refers to. */
    struct {
        mpr_dlist front;
        mpr_dlist back;
    } recs;
    mpr_dlist sigs;
    /* TODO: track this metadata. Should datasets be `mpr_obj`s? */
    int num_records;
    double duration;
    struct _mpr_data_sig *pubr; /* pointer to publishing sig, or null if not published */
} mpr_dataset_t, *mpr_dataset;

#define MPR_DATA_SIG_STRUCT_ITEMS \
    mpr_obj_t obj;      /* always first (so that the sig can be safely cast as a mpr_obj) */\
    char * path;        /* OSC path of the sig, which must start with `/` */\
    char * name;        /* Name of the sig, i.e. path+1 */\
    mpr_dlist pubs;     /* Datasets that this signal publishes */\
    mpr_dlist subs;     /* Datasets this signal is subscribed to */\
    /* TODO: should these be mpr_props? */\
    int num_pubs;\
    int num_subs;\
    int is_local;

typedef struct _mpr_data_sig {
    MPR_DATA_SIG_STRUCT_ITEMS
    mpr_dev dev;
} mpr_data_sig_t, *mpr_data_sig;

typedef struct _mpr_local_data_sig {
    MPR_DATA_SIG_STRUCT_ITEMS
    mpr_local_dev dev;
    void *handler;
    int event_flags;
    mpr_dlist maps; /* list of mpr_local_data_maps where this signal is the source */
} mpr_local_data_sig_t, *mpr_local_data_sig;

#undef MPR_DATA_SIG_STRUCT_ITEMS

#define MPR_DATA_MAP_STRUCT_ITEMS \
    mpr_obj_t obj; \
    mpr_data_sig src; \
    mpr_data_sig dst; \
    char is_local; \
    int status;

typedef struct _mpr_data_map {
    MPR_DATA_MAP_STRUCT_ITEMS
} mpr_data_map_t, *mpr_data_map;

typedef struct _mpr_local_data_map {
    MPR_DATA_MAP_STRUCT_ITEMS
    mpr_link link;
} mpr_local_data_map_t, *mpr_local_data_map;

#undef MPR_DATA_MAP_STRUCT_ITEMS

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

/* Send the state of a data map to the network
 * 'cmd' should be one of the `MPR_DATA_MAP_` messages defined in net_msg.h
 * An OSC message will be sent to the admin bus with a description of the map. */
void mpr_data_map_send_state(mpr_data_map map, net_msg_t cmd);

/* Push a data map onto the network, possibly initiating the mapping handshake/linking procedure */
void mpr_data_map_push(mpr_data_map map);

/* dlist filter predicates */
int mpr_data_sigs_not_equal(mpr_rc datum, const char *types, mpr_union *va);
extern const char * mpr_data_sigs_not_equal_types;

int mpr_data_sig_by_full_name(mpr_rc datum, const char *types, mpr_union *va);
extern const char * mpr_data_sig_by_full_name_types;

int mpr_data_map_by_signals(mpr_rc datum, const char *types, mpr_union *va);
extern const char * mpr_data_map_by_signals_types;

#endif
