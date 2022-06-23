#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mapper_internal.h"
#include "types_internal.h"
#include <mapper/mapper.h>

static char * data_recorder_dev_name(const char * name)
{
    size_t len = strlen(name);
    size_t dev_name_len = len + 8 + 1; /* 8 = length of "dataset_", 1 = length of null terminator */
    char * dev_name = malloc(dev_name_len);
    strncpy(dev_name, "dataset_", 9); /* copy the null terminator to silence warnings, even though we overwrite it */
    strncpy(dev_name+8, name, len + 1);

    /* transform slashes into underscores */
    for (char * c = dev_name + 8; c - dev_name < dev_name_len; ++c) if (*c == '/') *c = '_';

    return dev_name; /* remember to free this when you're done with it */
}

mpr_data_record mpr_data_record_new(mpr_sig sig, mpr_sig_evt evt, mpr_id instance,
                                    int length, mpr_type type, const void * value, mpr_time time)
{
    mpr_data_record record = malloc(sizeof(mpr_data_record_t));
    record->sig = sig;
    record->evt = evt;
    record->instance = instance;
    record->length = length;
    record->type = type;
    record->value = value;
    record->time = time;
    return record;
}

void mpr_data_record_free(mpr_data_record record)
{
    free(record);
}

mpr_sig      mpr_data_record_get_sig     (const mpr_data_record record) { return record->sig; }
mpr_sig_evt  mpr_data_record_get_evt     (const mpr_data_record record) { return record->evt; }
mpr_id       mpr_data_record_get_instance(const mpr_data_record record) { return record->instance; }
int          mpr_data_record_get_length  (const mpr_data_record record) { return record->length; }
mpr_type     mpr_data_record_get_type    (const mpr_data_record record) { return record->type; }
const void * mpr_data_record_get_value   (const mpr_data_record record) { return record->value; }
mpr_time     mpr_data_record_get_time    (const mpr_data_record record) { return record->time; }

#define DATASET_INITIAL_RECORDS 16
#define DATASET_INITIAL_VALUES sizeof(float) * DATASET_INITIAL_RECORDS
mpr_dataset mpr_dataset_new(const char * name, mpr_graph g)
{
    RETURN_ARG_UNLESS(name, 0);
    if (!g) {
        g = mpr_graph_new(0);
        g->own = 0;
    }

    /* TODO: the dataset should be allocated by the graph, and graphs should have a list of datasets:
     * data = (mpr_dataset)mpr_list_add_item((void**)&g->datasets, sizeof(mpr_dataset)); */
    mpr_dataset data = calloc(1, sizeof(mpr_dataset_t));

    data->obj.graph = g;
    /* TODO: determine ID; probably from network through name resolution, as with devices */
    /* TODO?: allow user data to be associated with the dataset? Is this handled generically through the property API? */
    /* TODO: set up data->obj.props _mpr_dict */
    /* TODO?: set up version? Not sure what this is for, but it seems to usually be linked to a mpr_tbl property */
    data->obj.type = MPR_DATASET;
    data->is_local = 1;

    /* TODO: should probably add name conflict resolution in the same way devices do so */
    data->name = strdup(name);
    die_unless(data->name, "Failed to malloc new dataset `%s` name string\n", name);

    return data;
}
#undef DATASET_INITIAL_VALUES
#undef DATASET_INITIAL_RECORDS

const char * mpr_dataset_get_name(mpr_dataset data)
{
    return data->name;
}

void mpr_dataset_free(mpr_dataset data)
{
    free(data);
}

void _deep_copy_sig(mpr_sig from, mpr_sig to, mpr_dataset data)
{
    *to = *from; /* copy the easy stuff */
    to->obj.graph = data->obj.graph;
    to->obj.data = 0;
    to->obj.props.synced = 0; /* TODO: save properties too? */
    to->obj.props.staged = 0;
    to->path = strdup(from->path);
    to->name = to->path + 1;
    to->unit = from->unit ? strdup(from->unit) : 0;
    if (from->min) {
        size_t bytes = mpr_type_get_size(from->type) * from->len;
        to->min = malloc(bytes);
        TRACE_RETURN_UNLESS(to->min, ;, "Failed to malloc memory for record sig copy min\n");
        memcpy(to->min, from->min, bytes);
    }
    if (from->max) {
        size_t bytes = mpr_type_get_size(from->type) * from->len;
        to->max = malloc(bytes);
        TRACE_RETURN_UNLESS(to->max, ;, "Failed to malloc memory for record sig copy max\n");
        memcpy(to->max, from->max, bytes);
    }
}

void mpr_dataset_add_record(mpr_dataset data, const mpr_data_record record)
{
    /* copy record metadata */
    mpr_data_record _record = (mpr_data_record)mpr_list_add_item((void**)&data->recs, sizeof(mpr_data_record_t));
    *_record = *record;

    /* copy values */
    size_t value_bytes = mpr_type_get_size(record->type) * record->length;
    void * value = malloc(value_bytes);
    TRACE_RETURN_UNLESS(value, ;, "Failed to malloc memory for storing a record value\n");
    memcpy(value, (void *)record->value, value_bytes);
    _record->value = value;

    /* if the signal in this record does not intersect with the signal list
     * i.e. we have never seen this signal before
     * then add it to the internal signal list */
    mpr_list lst = mpr_list_filter(mpr_graph_get_list(data->obj.graph, MPR_SIG), MPR_PROP_ID, 0, 1, MPR_INT64, 
                                   &_record->sig->obj.id, MPR_OP_EQ);
    lst = mpr_list_get_diff(lst, data->sigs);
    if (mpr_list_get_size(lst)) {
        data->sigs = mpr_list_get_union(lst, data->sigs);
        printf("Added signal, list size is %d\n", mpr_list_get_size(data->sigs));
    }
}

mpr_data_record mpr_dataset_get_record(mpr_dataset data, unsigned int idx)
{
    mpr_data_record record = (mpr_data_record)mpr_list_get_idx(mpr_list_from_data(data->recs), 
            mpr_list_get_size(mpr_list_from_data(data->recs)) - idx - 1);
    return record;
}

unsigned int mpr_dataset_get_num_records(mpr_dataset data)
{
    return mpr_list_get_size(mpr_list_from_data(data->recs));
}

mpr_list mpr_dataset_get_sigs(mpr_dataset data)
{
    RETURN_ARG_UNLESS(mpr_list_get_size(data->sigs) > 0, 0);
    return data->sigs;
}

mpr_data_recorder mpr_data_recorder_new(mpr_dataset data, unsigned int num_sigs, mpr_sig * sigs)
{
    RETURN_ARG_UNLESS(sigs, 0);
    RETURN_ARG_UNLESS(num_sigs > 0, 0);

    for (unsigned int i = 0; i < num_sigs; ++i) {
        TRACE_RETURN_UNLESS(sigs[i], NULL, "Cannot include uninitialized signal in dataset.\n");
        TRACE_RETURN_UNLESS(sigs[i]->dev->name, NULL, "Cannot include uninitialized device in dataset.\n");
    }
    /* TODO: (quietly?) ignore NULL signals */
    /* TODO: allow uninitialized devices into data recorders; just don't consider the recorder ready until all the devices are? */

    mpr_data_recorder rec = malloc(sizeof(mpr_data_recorder_t));

    rec->data = data;
    rec->num_sigs = num_sigs;
    rec->mapped = 0;
    rec->armed = 0;
    rec->recording = 0;
    rec->remote_sigs = sigs;
    rec->sigs = calloc(num_sigs, sizeof(mpr_sig *));

    /* set up device */
    {
        mpr_graph g = data->obj.graph;
        char * dev_name = data_recorder_dev_name(data->name);
        rec->dev = mpr_dev_new(dev_name, g);
        free(dev_name);
    }

    /* make a local destination for the signals to record */
    for (unsigned int i = 0; i < rec->num_sigs; ++i)
    {
        mpr_sig sig = sigs[i];
        size_t sig_name_len = strlen(sig->name);
        size_t dev_name_len = strlen(sig->dev->name);
        size_t name_len = sig_name_len + dev_name_len + 2; /* 2 = length of '/' + '\0' */
        char * name = malloc(name_len);
        snprintf(name, name_len, "%s/%s", sig->dev->name, sig->name);
        rec->sigs[i] = mpr_sig_new(rec->dev, MPR_DIR_IN, name, sig->len,
                                   sig->type, sig->unit, sig->min, sig->max, &sig->num_inst, 0, 0);
        mpr_obj_set_prop((mpr_obj)rec->sigs[i], MPR_PROP_DATA, 0, 1, MPR_PTR, rec, 0);
        /* TODO: mark the local recording destination as such for use by session managers */
        free(name);
    }

    /* ideally we would like to set up maps immediately here while we know all the signals; then we wouldn't have to store
     * the list and number of remote signals */

    return rec;
}

void mpr_data_recorder_free(mpr_data_recorder rec)
{
    mpr_dev_free(rec->dev);
    free(rec->sigs);
    free(rec);
}

int mpr_data_recorder_get_is_ready(mpr_data_recorder rec)
{
    return mpr_dev_get_is_ready(rec->dev);
    /* if we eventually are able to create the maps in recorder_new,
     * we should check here instead of in get_is_armed if the maps are ready as well as the device */
}

int mpr_data_recorder_poll(mpr_data_recorder rec, int block_ms)
{
    return mpr_dev_poll(rec->dev, block_ms);
}

void mpr_data_recorder_disarm(mpr_data_recorder rec)
{
    rec->armed = 0;
}

static inline int _recorder_is_mapped(mpr_data_recorder rec)
{
    return mpr_list_get_size(mpr_dev_get_maps(rec->dev, MPR_DIR_IN)) > 0;
}

static inline int _recorder_maps_are_ready(mpr_data_recorder rec)
{
    if (rec->mapped) return 1;
    RETURN_ARG_UNLESS(_recorder_is_mapped(rec), 0);
    for (mpr_list map_list = mpr_dev_get_maps(rec->dev, MPR_DIR_IN); map_list; map_list = mpr_list_get_next(map_list))
        RETURN_ARG_UNLESS(mpr_map_get_is_ready((mpr_map)*map_list), 0);
    return rec->mapped = 1;
}

int mpr_data_recorder_get_is_armed(mpr_data_recorder rec)
{
    return _recorder_maps_are_ready(rec) && rec->armed;
}

static void sig_handler(mpr_sig sig, mpr_sig_evt evt, mpr_id instance, int length,
        mpr_type type, const void * value, mpr_time time)
{
    mpr_data_recorder rec = (void*)mpr_obj_get_prop_as_ptr((mpr_obj)sig, MPR_PROP_DATA, 0);
    RETURN_UNLESS(mpr_data_recorder_get_is_recording(rec));

    mpr_data_record record = mpr_data_record_new(sig, evt, instance, length, type, value, time);
    mpr_dataset_add_record(rec->data, record);
    mpr_data_record_free(record);
}

void mpr_data_recorder_start(mpr_data_recorder rec)
{
    rec->recording = mpr_dev_get_is_ready(rec->dev) && mpr_data_recorder_get_is_armed(rec) && _recorder_maps_are_ready(rec);
}

void mpr_data_recorder_stop(mpr_data_recorder rec)
{
    rec->recording = 0;
}

int mpr_data_recorder_get_is_recording(mpr_data_recorder rec)
{
    return rec->recording;
}

void mpr_data_recorder_arm(mpr_data_recorder rec)
{
    RETURN_UNLESS(mpr_dev_get_is_ready(rec->dev));
    if (_recorder_is_mapped(rec) == 0) for (unsigned int i = 0; i < rec->num_sigs; ++i)
    {
        /* when the recorder is first armed, make maps */
        mpr_sig remote_sig = rec->remote_sigs[i];
        mpr_sig local_sig  = rec->sigs[i];
        mpr_map map = mpr_map_new(1, &remote_sig, 1, &local_sig);
        mpr_obj_set_prop((mpr_obj)map, MPR_PROP_EXPR, NULL, 1, MPR_STR, "y=x", 1);
        mpr_obj_push((mpr_obj)map);
        mpr_sig_set_cb(local_sig, sig_handler, MPR_SIG_ALL);
    }
    rec->armed = 1;
}
