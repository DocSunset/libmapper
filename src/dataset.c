#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dataset.h"
#include "object.h"
#include "table.h"
#include "device.h"
#include "network.h"
#include "graph.h"
#include "util/func_if.h"
#include "util/mpr_type_get_size.h"
#include "util/debug_macro.h"
#include "util/skip_slash.h"
#include <mapper/rc.h>
#include <mapper/mapper.h>

const char * mpr_data_sigs_not_equal_types = "v";
const char * mpr_data_sig_by_full_name_types = "s";
const char * mpr_data_map_by_signals_types = "vv";

/* dlist filter predicates */
int mpr_data_sigs_not_equal(mpr_rc datum, const char *types, void **va)
{
    return 1;
}

int mpr_data_sig_by_full_name(mpr_rc datum, const char *types, void **va)
{
    return 1;
}

int mpr_data_map_by_signals(mpr_rc datum, const char *types, void **va)
{
    return 1;
}

/* Data records */

void mpr_data_record_destructor(mpr_rc rc)
{
    mpr_data_record record = (mpr_data_record)rc;
    if (record->value) free(record->value);
}

mpr_data_record mpr_data_record_new(mpr_sig sig, mpr_sig_evt evt, mpr_id instance,
                                    int length, mpr_type type, const void * value, mpr_time time)
{
    mpr_data_record record = (mpr_data_record)mpr_rc_new(sizeof(mpr_data_record_t), &mpr_data_record_destructor);
    if (0 == record) return 0;
    record->sig = sig;
    record->evt = evt;
    record->instance = instance;
    record->length = length;
    record->type = type;
    record->value = calloc(length, mpr_type_get_size(type));
    if (0 == record->value) {
        mpr_rc_free(record);
        return 0;
    }
    else memcpy(record->value, (void *)value, length * mpr_type_get_size(type));
    record->time = time;
    return record;
}

/* this should be strictly an alias to mpr_rc_free */
void mpr_data_record_free(mpr_data_record data)
{
    mpr_rc_free((mpr_rc)data);
}

mpr_sig      mpr_data_record_get_sig     (const mpr_data_record record) { return record->sig; }
mpr_sig_evt  mpr_data_record_get_evt     (const mpr_data_record record) { return record->evt; }
mpr_id       mpr_data_record_get_instance(const mpr_data_record record) { return record->instance; }
int          mpr_data_record_get_length  (const mpr_data_record record) { return record->length; }
mpr_type     mpr_data_record_get_type    (const mpr_data_record record) { return record->type; }
const void * mpr_data_record_get_value   (const mpr_data_record record) { return record->value; }
mpr_time     mpr_data_record_get_time    (const mpr_data_record record) { return record->time; }

void mpr_dataset_destructor(mpr_rc rc)
{
    mpr_dataset data = (mpr_dataset)rc;
    mpr_dlist_free(data->recs.back);
    mpr_dlist_free(data->recs.front);
    mpr_dlist_free(data->sigs);
    free((void*)data->name);
}

mpr_dataset mpr_dataset_new(const char * name, mpr_data_sig parent)
{
    RETURN_ARG_UNLESS(name, 0);
    mpr_dataset data = (mpr_dataset)mpr_rc_new(sizeof(mpr_dataset_t), &mpr_dataset_destructor);

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

/* this strictly must be an alias of mpr_rc_free */
void mpr_dataset_free(mpr_dataset data)
{
    mpr_rc_free((mpr_rc)data);
}

void mpr_dataset_add_record(mpr_dataset data, const mpr_data_record record)
{
    RETURN_UNLESS(data);
    /* copy record metadata */
    mpr_dlist_append(&data->recs.front, &data->recs.back, (mpr_rc)record);

    mpr_dlist iter;
    for (iter = mpr_dlist_make_ref(data->sigs); iter; mpr_dlist_next(&iter)) {
        mpr_sig iter_sig = mpr_dlist_data_as(mpr_sig, iter);
        if (record->sig->obj.id == iter_sig->obj.id) break;
    }
    if (iter == 0) {
        /* DATATODO: the signal refs stored in a dataset may become invalidated. Should signals be immutable? */
        mpr_rc sigrc = mpr_rc_new(sizeof(mpr_sig), &mpr_rc_no_destructor);
        *(mpr_sig*)sigrc = record->sig;
        mpr_dlist_prepend(&data->sigs, sigrc);
        printf("Added signal, list size is %lu\n", mpr_dlist_get_length(data->sigs));
    }
    else mpr_dlist_free(&iter);
}

mpr_data_record mpr_dataset_get_record(mpr_dataset data, unsigned int idx)
{
    RETURN_ARG_UNLESS(data && data->recs.front, 0);
    mpr_dlist iter = mpr_dlist_make_ref(data->recs.front);
    unsigned int i = 0;
    while(i < idx && iter) {
        ++i;
        mpr_dlist_next(&iter);
    }
    mpr_data_record ret = mpr_dlist_data_as(mpr_data_record, iter);
    mpr_dlist_free(&iter);
    return ret;
}

mpr_dlist mpr_dataset_get_records(mpr_dataset data)
{
    RETURN_ARG_UNLESS(data, 0);
    return mpr_dlist_make_ref(data->recs.front);
}

unsigned int mpr_dataset_get_num_records(mpr_dataset data)
{
    RETURN_ARG_UNLESS(data, 0);
    return mpr_dlist_get_length(data->recs.front);
}

mpr_dlist mpr_dataset_get_sigs(mpr_dataset data)
{
    RETURN_ARG_UNLESS(data, 0);
    return mpr_dlist_make_ref(data->sigs);
}

/* DATATODO: implement publish methods */
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
    /* DATATODO: (quietly?) ignore NULL signals */
    /* DATATODO: allow uninitialized devices into data recorders; just don't consider the recorder ready until all the devices are? */

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
        /* DATATODO: mark the local recording destination as such for use by session managers */
        free(name);
    }

    /* DATATODO: ideally we would like to set up maps immediately here while we know all the signals; then we wouldn't have to store
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

    /* DATATODO: for now the time is set here, locally, to ensure that records are recorded chronologically in some sense.
     * in the future it would probably be better to convert the remote timestamp to the local timebase
     * and then insert it into the record list at the appropriate place */
    mpr_time t;
    mpr_time_set(&t, MPR_NOW);
    mpr_data_record record = mpr_data_record_new(sig, evt, instance, length, type, value, t);
    if (0 == record) return;
    mpr_dataset_add_record(rec->data, record);
}

static inline void _maybe_add_recording(mpr_data_recorder rec)
{
    if (rec->recording) {
        /* DATATODO: provide better generic names */
        mpr_dataset data = mpr_dataset_new("recording", 0);
        /* set the dataset's list to the bounds of the recorder's data buffer */
        data->recs.front = mpr_dlist_make_ref(rec->data->recs.front);
        data->recs.back  = mpr_dlist_make_ref(rec->data->recs.back);
        mpr_dlist_prepend(&rec->recordings, (mpr_rc)data);
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

mpr_dlist mpr_data_recorder_get_recordings(mpr_data_recorder rec)
{
    RETURN_ARG_UNLESS(rec, 0);
    return mpr_dlist_make_ref(rec->recordings);
}

/* Dataset signals */

void mpr_data_sig_init(mpr_data_sig sig, const char *name)
{
    name = skip_slash(name);
    {
        int str_len = strlen(name)+2;
        sig->path = malloc(str_len);
        snprintf(sig->path, str_len, "/%s", name);
    }
    sig->name = sig->path+1;
    sig->obj.type = MPR_DATA_SIG;
    {
        mpr_tbl tbl = sig->obj.props.synced = mpr_tbl_new();
        int rem_mod = sig->is_local ? NON_MODIFIABLE : MODIFIABLE;
        mpr_tbl_link(tbl, MPR_PROP_DATA, 1, MPR_PTR, &sig->obj.data,
                     LOCAL_MODIFY | INDIRECT | LOCAL_ACCESS_ONLY);
        mpr_tbl_link(tbl, MPR_PROP_DEV, 1, MPR_DEV, &sig->dev,
                     NON_MODIFIABLE | INDIRECT | LOCAL_ACCESS_ONLY);
        mpr_tbl_link(tbl, MPR_PROP_ID, 1, MPR_INT64, &sig->obj.id, rem_mod);
        mpr_tbl_link(tbl, MPR_PROP_VERSION, 1, MPR_INT32, &sig->obj.version, NON_MODIFIABLE);
        mpr_tbl_set(tbl, MPR_PROP_IS_LOCAL, NULL, 1, MPR_BOOL, &sig->is_local, LOCAL_ACCESS_ONLY | NON_MODIFIABLE);
    }
}

/* Free a data signal's memory resources */
void mpr_data_sig_destructor(mpr_rc rc)
{
    mpr_data_sig sig = (mpr_data_sig)rc;
    free(sig->path);
    mpr_dlist_free(sig->pubs);
    mpr_dlist_free(sig->subs);
    FUNC_IF(mpr_tbl_free, sig->obj.props.synced);
    FUNC_IF(mpr_tbl_free, sig->obj.props.staged);
    /* DATATODO: what is the purpose of increment version? */
    mpr_obj_increment_version((mpr_obj)sig);
    if (sig->is_local) {
        mpr_local_data_sig lsig = (mpr_local_data_sig)sig;
        mpr_dlist_free(lsig->maps);
    }
}


mpr_data_sig mpr_data_sig_new(mpr_dev dev, const char *name,
                              mpr_data_sig_handler *handler, int events)
{
    mpr_local_data_sig lsig;
    RETURN_ARG_UNLESS(dev && name, 0);
    mpr_graph g = mpr_obj_get_graph((mpr_obj)dev);

    const char * full_name; /* DATATODO: malloc and snprintf the full name, and then free. */

    mpr_dlist already_exists = mpr_dlist_new_filter(g->dsigs,
                                                    &mpr_data_sig_by_full_name,
                                                    mpr_data_sig_by_full_name_types,
                                                    full_name);
    if (already_exists) {
        mpr_data_sig ret = mpr_dlist_data_as(mpr_data_sig, already_exists);
        mpr_dlist_free(already_exists);
        return ret;
    }

    lsig = mpr_rc_new(sizeof(mpr_local_data_sig_t), &mpr_data_sig_destructor);
    mpr_dlist_prepend(&g->dsigs, lsig);

    lsig->dev = (mpr_local_dev)dev;
    lsig->obj.id = mpr_dev_get_unused_sig_id((mpr_local_dev)dev);
    lsig->obj.graph = g;
    lsig->handler = handler;
    lsig->event_flags = events;
    lsig->is_local = 1;

    mpr_data_sig_init((mpr_data_sig)lsig, name);

    mpr_obj_increment_version((mpr_obj)dev); /* DATATODO: what does this do? */

    mpr_dev_add_data_sig_methods(lsig->dev, lsig);

    /* DATATODO: if the device is already registered, tell subscribers about the dataset lsig */
    /*
    if (((mpr_local_dev)dev)->registered) {
        mpr_net_use_subscribers(&g->net, (mpr_local_dev)dev,
                                ((dir == MPR_DIR_IN) ? MPR_SIG_IN : MPR_SIG_OUT));
        mpr_sig_send_state((mpr_sig)lsig, MSG_SIG);
    }
    */

    return (mpr_data_sig)lsig;
}

/* DATATODO: 
 * void mpr_data_sig_send_state(mpr_data_sig sig, mpr_msg cmd)
 * {
 * }
 */ 

void mpr_data_sig_free(mpr_data_sig sig)
{
    RETURN_UNLESS(sig && sig->is_local);
    mpr_graph g = mpr_obj_get_graph((mpr_obj)sig);
    mpr_graph_remove_data_sig(g, sig, MPR_OBJ_REM);
}

mpr_dlist mpr_data_sig_get_pubs(mpr_data_sig sig)
{
    return sig ? mpr_dlist_make_ref(sig->pubs) : 0;
}

mpr_dlist mpr_data_sig_get_subs(mpr_data_sig sig)
{
    return sig ? mpr_dlist_make_ref(sig->subs) : 0;
}

mpr_dev mpr_data_sig_get_dev(mpr_data_sig sig)
{
    RETURN_ARG_UNLESS(sig, 0);
    return sig->dev;
}

void mpr_data_sig_set_cb(mpr_data_sig sig, mpr_data_sig_handler* h, int events)
{
    RETURN_UNLESS(sig && sig->is_local);
    mpr_local_data_sig lsig = (mpr_local_data_sig)sig;
    lsig->handler = (void*)h;
    lsig->event_flags = events;
}

/* Dataset mappings */

void mpr_data_map_init(mpr_data_map m)
{
    mpr_tbl t = m->obj.props.synced = mpr_tbl_new();
    m->obj.props.staged = mpr_tbl_new();

    /* these properties need to be added in alphabetical order */
    mpr_tbl_link(t, MPR_PROP_DATA, 1, MPR_PTR, &m->obj.data,
                 MODIFIABLE | INDIRECT | LOCAL_ACCESS_ONLY);
    mpr_tbl_link(t, MPR_PROP_ID, 1, MPR_INT64, &m->obj.id, NON_MODIFIABLE | LOCAL_ACCESS_ONLY);
    mpr_tbl_link(t, MPR_PROP_STATUS, 1, MPR_INT32, &m->status, NON_MODIFIABLE);
    mpr_tbl_link(t, MPR_PROP_VERSION, 1, MPR_INT32, &m->obj.version, REMOTE_MODIFY);

    mpr_tbl_set(t, MPR_PROP_IS_LOCAL, NULL, 1, MPR_BOOL, &m->is_local,
                LOCAL_ACCESS_ONLY | NON_MODIFIABLE);
    m->status = MPR_STATUS_STAGED;
}

mpr_data_map mpr_data_map_new(mpr_data_sig src, mpr_data_sig dst)
{
    RETURN_ARG_UNLESS(src && dst, 0);

    mpr_dlist already_exists = mpr_dlist_new_filter(src->obj.graph->dmaps,
                                                    &mpr_data_map_by_signals, mpr_data_map_by_signals_types,
                                                    src, dst);
    if (already_exists) {
        mpr_data_map ret = mpr_dlist_data_as(mpr_data_map, already_exists);
        mpr_dlist_free(already_exists);
        return ret;
    }

    /* If the signals are both local:
         * if they are the same signal, they will be pointers to the same location, we can't map this
         * if they are not the same signal, we can map them immediately
     * else, if one of the signals is remote:
         * if the local signal is uninitialized, stage the map to be completed when the local dev is registered
         * else we can make the map
     * else, if both signals are remote:
         * we know they must both be initialized, since we wouldn't know about them otherwise
         * if the signals are the same (they have the same dev name and sig name), we can't make this map
         * else we can make the map */
    if (src->is_local && dst->is_local) {
        if (src == dst) {
            trace("Cannot connect signal '%s:%s' to itself.\n"
                  mpr_dev_get_name(src->dev), src->name);
            return 0;
        }
        /* DATATODO: else immediately establish local-only link */
    }
    else if (src->is_local || dst->is_local) {
        mpr_local_data_sig lsig = src->is_local ? (mpr_local_data_sig)src : (mpr_local_data_sig)dst;
        if (!mpr_dev_get_is_ready((mpr_dev)lsig->dev)) {
            /* DATATODO: stage the map for when the local device is ready */
            /* it may be enough to just continue on, and eventually push the map when the device is registered... */
            return 0;
        }
    }
    else if (strcmp(src->name, dst->name) == 0 && strcmp(src->dev->name, dst->dev->name) == 0) {
        trace("Cannot connect signal '%s:%s' to itself.\n",
              mpr_dev_get_name(src[i]->dev), src[i]->name);
        return 0;
    }

    mpr_data_map m;
    if (src->is_local) {
        m = mpr_rc_new(sizeof(mpr_local_data_map_t), &mpr_rc_no_destructor); /* map has no memory resources to free */
        m->is_local = 1;
    }
    else m = mpr_rc_new(sizeof(mpr_data_map_t), &mpr_rc_no_destructor);

    mpr_graph g = mpr_obj_get_graph((mpr_obj)src);
    mpr_dlist_prepend(&g->dmaps, m);

    m->obj.type = MPR_DATA_MAP;
    m->obj.graph = src->obj.graph;
    m->obj.id = mpr_dev_generate_unique_id(src->dev);
    m->src = src;
    m->dst = dst;
    mpr_data_map_init(m);
    /* DATATODO: increment graph count of staged data maps so that they will be pushed automatically on poll */
    return m;
}

void mpr_data_map_push(mpr_data_map m)
{
    mpr_net n = &m->obj.graph->net;
    mpr_data_sig src = m->src;
    mpr_data_sig dst = m->dst;
    RETURN_UNLESS(mpr_dev_get_is_ready(src->dev) && mpr_dev_get_is_ready(dst->dev));
    if (src->is_local && dst->is_local) {
        /* DATATODO: local-only link should already be established, start syncing */
    } else if (src->is_local) {
        /* DATATODO: immediately start linking procedure */
    } else if (dst->is_local) {
        /* DATATODO: send /data/map including dst address */
    }
    mpr_net_use_bus(n);
    mpr_data_map_send_state(m, MSG_DATA_MAP);
}

void mpr_data_map_send_state(mpr_data_map map, net_msg_t cmd)
{
}

void mpr_data_map_release(mpr_data_map map)
{
}

int mpr_data_map_get_is_ready(mpr_data_map map)
{
    return 1;
}

mpr_data_sig mpr_data_map_get_src(mpr_data_map map)
{
    return 0;
}

mpr_data_sig mpr_data_map_get_dst(mpr_data_map map)
{
    return 0;
}
