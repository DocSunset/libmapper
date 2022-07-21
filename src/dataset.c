#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dataset.h"
#include <mapper/mapper.h>
#include "type.h"
#include "debug_macro.h"

/* Data records */

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

mpr_dataset mpr_dataset_new(const char * name, mpr_data_sig parent)
{
    RETURN_ARG_UNLESS(name, 0);
    /* TODO: the dataset should be allocated by the graph, and graphs should have a list of datasets:
     * data = (mpr_dataset)mpr_list_add_item((void**)&g->datasets, sizeof(mpr_dataset)); */
    mpr_dataset data = calloc(1, sizeof(mpr_dataset_t));

    /* TODO: should probably add name conflict resolution in the same way devices do so */
    data->name = strdup(name);
    die_unless(data->name, "Failed to malloc new dataset `%s` name string\n", name);

    data->synced = 0;

    return data;
}

/* Datasets */

const char * mpr_dataset_get_name(mpr_dataset data)
{
    RETURN_ARG_UNLESS(data, 0);
    return data->name;
}

void mpr_dataset_free(mpr_dataset data)
{
    RETURN_UNLESS(data);
    mpr_dlist_free(data->recs.back);
    mpr_dlist_free(data->recs.front);
    mpr_dlist_free(data->sigs);
    free((void*)data->name);
    free(data);
}

void mpr_dataset_add_record(mpr_dataset data, const mpr_data_record record)
{
    RETURN_UNLESS(data);
    /* copy record metadata */
    mpr_data_record _record = 0;
    mpr_dlist_append(&data->recs.front, &data->recs.back, (void**)&_record, sizeof(mpr_data_record_t), &free);
    TRACE_RETURN_UNLESS(_record, ;, "Failed to calloc memory for storing a record.\n");
    *_record = *record;

    /* copy values */
    size_t value_bytes = mpr_type_get_size(record->type) * record->length;
    void * value = malloc(value_bytes);
    TRACE_RETURN_UNLESS(value, ;, "Failed to malloc memory for storing a record value\n");
    memcpy(value, (void *)record->value, value_bytes);
    _record->value = value;

    /* TODO: fully convert to using mpr_dlist for everything so that this is easier... */
    /* if the signal in this record does not intersect with the signal list
     * i.e. we have never seen this signal before
     * then add it to the internal signal list */
    mpr_dlist iter = 0;
    mpr_sig rec_sig = _record->sig;
    for (mpr_dlist_make_ref(&iter, data->sigs); iter; mpr_dlist_next(&iter)) {
        mpr_sig iter_sig = *mpr_dlist_data_as(mpr_sig*, iter);
        if (rec_sig->obj.id == iter_sig->obj.id) break;
    }
    if (iter == 0) {
        mpr_dlist_prepend(&data->sigs, (void**)&rec_sig, 0, &mpr_dlist_no_destructor);
        printf("Added signal, list size is %lu\n", mpr_dlist_get_length(data->sigs));
    }
    mpr_dlist_free(&iter);
}

mpr_data_record mpr_dataset_get_record(mpr_dataset data, unsigned int idx)
{
    RETURN_ARG_UNLESS(data && data->recs.front, 0);
    mpr_dlist iter = 0; mpr_dlist_make_ref(&iter, data->recs.front);
    unsigned int i = 0;
    while(i < idx && iter) {
        ++i;
        mpr_dlist_next(&iter);
    }
    mpr_data_record ret = mpr_dlist_data_as(mpr_data_record, iter);
    mpr_dlist_free(&iter);
    return ret;
}

void mpr_dataset_get_records(mpr_dataset data, mpr_dlist *records)
{
    RETURN_UNLESS(data);
    mpr_dlist_make_ref(records, data->recs.front);
}

unsigned int mpr_dataset_get_num_records(mpr_dataset data)
{
    RETURN_ARG_UNLESS(data, 0);
    return mpr_dlist_get_length(data->recs.front);
}

void mpr_dataset_get_sigs(mpr_dataset data, mpr_dlist *sigs)
{
    RETURN_UNLESS(data);
    mpr_dlist_make_ref(sigs, data->sigs);
}

mpr_data_sig mpr_dataset_publish(mpr_dataset data, mpr_dev dev, const char * name,
                         mpr_data_sig_handler handler, int events)
{
    return 0;
}

void mpr_dataset_publish_with_sig(mpr_dataset data, mpr_data_sig sig)
{
}

void mpr_dataset_withdraw(mpr_dataset data)
{
}

/* Data recorders */

mpr_data_recorder mpr_data_recorder_new(const char * name, mpr_graph g, unsigned int num_sigs, mpr_sig * sigs)
{
    RETURN_ARG_UNLESS(sigs, 0);
    RETURN_ARG_UNLESS(num_sigs > 0, 0);

    for (unsigned int i = 0; i < num_sigs; ++i) {
        TRACE_RETURN_UNLESS(sigs[i], NULL, "Cannot include uninitialized signal in dataset.\n");
        TRACE_RETURN_UNLESS(sigs[i]->dev->name, NULL, "Cannot include uninitialized device in dataset.\n");
    }
    /* TODO: (quietly?) ignore NULL signals */
    /* TODO: allow uninitialized devices into data recorders; just don't consider the recorder ready until all the devices are? */

    /* set up device */
    const char * dev_name = name ? name : "data_recorder";
    mpr_dev dev = mpr_dev_new(dev_name, g);
    RETURN_ARG_UNLESS(dev, 0);

    mpr_data_recorder rec = malloc(sizeof(mpr_data_recorder_t));

    rec->dev = dev;
    rec->data = mpr_dataset_new(dev_name, 0);
    rec->num_sigs = num_sigs;
    rec->mapped = 0;
    rec->armed = 0;
    rec->recording = 0;
    rec->remote_sigs = sigs;
    rec->sigs = calloc(num_sigs, sizeof(mpr_sig *));

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

    /* TODO: ideally we would like to set up maps immediately here while we know all the signals; then we wouldn't have to store
     * the list and number of remote signals */

    return rec;
}

void mpr_data_recorder_free(mpr_data_recorder rec)
{
    RETURN_UNLESS(rec);
    mpr_dev_free(rec->dev);
    free(rec->sigs);
    free(rec);
}

int mpr_data_recorder_get_is_ready(mpr_data_recorder rec)
{
    RETURN_ARG_UNLESS(rec, 0);
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
    RETURN_UNLESS(rec);
    rec->armed = 0;
}

static inline int _recorder_is_mapped(mpr_data_recorder rec)
{
    RETURN_ARG_UNLESS(rec, 0);
    return mpr_list_get_size(mpr_dev_get_maps(rec->dev, MPR_DIR_IN)) > 0;
}

static inline int _recorder_maps_are_ready(mpr_data_recorder rec)
{
    RETURN_ARG_UNLESS(rec, 0);
    if (rec->mapped) return 1;
    RETURN_ARG_UNLESS(_recorder_is_mapped(rec), 0);
    for (mpr_list map_list = mpr_dev_get_maps(rec->dev, MPR_DIR_IN); map_list; map_list = mpr_list_get_next(map_list))
        RETURN_ARG_UNLESS(mpr_map_get_is_ready((mpr_map)*map_list), 0);
    return rec->mapped = 1;
}

int mpr_data_recorder_get_is_armed(mpr_data_recorder rec)
{
    RETURN_ARG_UNLESS(rec, 0);
    return _recorder_maps_are_ready(rec) && rec->armed;
}

static void sig_handler(mpr_sig sig, mpr_sig_evt evt, mpr_id instance, int length,
        mpr_type type, const void * value, mpr_time time)
{
    mpr_data_recorder rec = (void*)mpr_obj_get_prop_as_ptr((mpr_obj)sig, MPR_PROP_DATA, 0);
    RETURN_UNLESS(mpr_data_recorder_get_is_recording(rec));

    /* TODO: for now the time is set here, locally, to ensure that records are recorded chronologically in some sense.
     * in the future it would probably be better to convert the remote timestamp to the local timebase
     * and then insert it into the record list at the appropriate place */
    mpr_time t;
    mpr_time_set(&t, MPR_NOW);
    mpr_data_record record = mpr_data_record_new(sig, evt, instance, length, type, value, t);
    mpr_dataset_add_record(rec->data, record);
    mpr_data_record_free(record);
}

static inline void _maybe_add_recording(mpr_data_recorder rec)
{
    if (rec->recording) {
        /* TODO: provide better generic names */
        mpr_dataset data = mpr_dataset_new("recording", 0);
        mpr_dlist_make_ref(&data->recs.front, rec->data->recs.front);
        mpr_dlist_make_ref(&data->recs.back, rec->data->recs.back);
        mpr_dlist_insert_before(&rec->recordings, rec->recordings, &data, 0, &mpr_dataset_free);
    }
}

void mpr_data_recorder_start(mpr_data_recorder rec)
{
    RETURN_UNLESS(rec);
    _maybe_add_recording(rec);
    rec->recording = mpr_dev_get_is_ready(rec->dev) && mpr_data_recorder_get_is_armed(rec) && _recorder_maps_are_ready(rec);
}

void mpr_data_recorder_stop(mpr_data_recorder rec)
{
    RETURN_UNLESS(rec);
    _maybe_add_recording(rec);
    rec->recording = 0;
}

int mpr_data_recorder_get_is_recording(mpr_data_recorder rec)
{
    RETURN_ARG_UNLESS(rec, 0);
    return rec->recording;
}

void mpr_data_recorder_arm(mpr_data_recorder rec)
{
    RETURN_UNLESS(rec);
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

void mpr_data_recorder_get_recordings(mpr_data_recorder rec, mpr_dlist *datasets)
{
    RETURN_UNLESS(rec);
    mpr_dlist_make_ref(datasets, rec->recordings);
}

/* Dataset signals */

mpr_data_sig mpr_data_sig_new(mpr_dev parent, const char *name,
                              mpr_data_sig_handler *handler, int events)
{
    return 0;
}

void mpr_data_sig_free(mpr_data_sig)
{
}

void mpr_data_sig_get_pubs(mpr_dlist*, mpr_data_sig)
{
}

void mpr_data_sig_get_subs(mpr_dlist*, mpr_data_sig)
{
}

mpr_dev mpr_data_sig_get_dev(mpr_data_sig)
{
    return 0;
}

void mpr_data_sig_set_cb(mpr_data_sig, mpr_data_sig_handler*)
{
}

/* Dataset mappings */

mpr_data_map mpr_data_map_new(mpr_data_sig src, mpr_data_sig dst)
{
    return 0;
}

void mpr_data_map_release(mpr_data_map)
{
}

int mpr_data_map_get_is_ready(mpr_data_map)
{
    return 0;
}

mpr_data_sig mpr_data_map_get_src(mpr_data_map)
{
    return 0;
}

mpr_data_sig mpr_data_map_get_dst(mpr_data_map)
{
    return 0;
}
