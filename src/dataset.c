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

    data->records = malloc(sizeof(mpr_data_record_t) * DATASET_INITIAL_RECORDS);
    data->num_records = 0;
    data->num_records_allocated = DATASET_INITIAL_RECORDS;
    die_unless(data->records, "Failed to malloc new dataset `%s` records array\n", data->name);

    data->values = malloc(DATASET_INITIAL_VALUES);
    data->values_write_position = data->values;
    data->values_bytes_allocated = DATASET_INITIAL_VALUES;
    die_unless(data->values, "Failed to malloc new dataset `%s` values array\n", data->name);

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

/* increase the allocated memory for data records in a dataset */
static void _dataset_realloc_records(mpr_dataset data)
{
    trace_dataset(data, "Reallocating from %lu to %lu records.\n", data->num_records_allocated, 2 * data->num_records_allocated);
    /* we should probably consider using someone else's better implementation of a dynamic array... */
    data->records = reallocarray(data->records, 2 * data->num_records_allocated, sizeof(mpr_data_record_t));
    die_unless(data->records, "Failed to reallocate dataset `%s` records array\n", data->name);
    data->num_records_allocated = 2 * data->num_records_allocated;
    trace_dataset(data, "Reallocation successful.\n");
}

static inline size_t _dataset_value_bytes_used(mpr_dataset data)
{
    return data->values_write_position - data->values;
}

static inline size_t _dataset_value_bytes_available(mpr_dataset data)
{
    return data->values_bytes_allocated - _dataset_value_bytes_used(data);
}

/* increase the allocated memory for data values in a dataset */
static void _dataset_realloc_values(mpr_dataset data, size_t minimum_additional_bytes)
{
    size_t new_values_array_size = 2 * data->values_bytes_allocated;
    size_t write_pointer_offset = _dataset_value_bytes_used(data);
    while (minimum_additional_bytes > new_values_array_size - data->values_bytes_allocated)
        new_values_array_size = 2 * new_values_array_size;
    trace_dataset(data, "%lu bytes requested. %lu bytes available. Reallocating from %lu to %lu value bytes.\n",
                  minimum_additional_bytes, _dataset_value_bytes_available(data),
                  data->values_bytes_allocated, new_values_array_size);
    data->values = realloc(data->values, new_values_array_size);
    die_unless(data->values, "Failed to reallocate dataset `%s` values array\n", data->name);
    data->values_write_position = data->values + write_pointer_offset;
    data->values_bytes_allocated = new_values_array_size;
    trace_dataset(data, "Reallocation successful.\n");
}

void mpr_dataset_add_record(mpr_dataset data, const mpr_data_record record)
{

	/* copy values */
    size_t value_bytes = mpr_type_get_size(record->type) * record->length;
    if (_dataset_value_bytes_available(data) < value_bytes) _dataset_realloc_values(data, value_bytes);
    memcpy(data->values_write_position, (void *)record->value, value_bytes);

	/* copy record metadata */
    if (data->num_records >= data->num_records_allocated) _dataset_realloc_records(data);
    data->records[data->num_records] = *record;
    data->records[data->num_records].value = data->values_write_position;

	/* adjust write positions */
	/* it's important to do this last, since the write positions are used above as the position of the current
     * record/values being recorded */
    data->values_write_position = data->values_write_position + value_bytes;
    ++data->num_records;
}

mpr_data_record mpr_dataset_get_record(mpr_dataset data, unsigned int idx)
{
    mpr_data_record record = data->records + idx;
    return record;
}

unsigned int mpr_dataset_get_num_records(mpr_dataset data)
{
    return data->num_records;
}

static int _cmp_sigs_equality(const void *context_data, mpr_sig sig)
{
    mpr_id sig_id = *(mpr_id*)context_data;
    mpr_id dev_id = *(mpr_id*)((char*)context_data + sizeof(mpr_id));
    return ((sig_id == sig->obj.id) && (dev_id == sig->dev->obj.id));
}

mpr_list mpr_dataset_get_sigs(mpr_dataset data)
{
    mpr_list sigs = 0;
    for (unsigned int i = 0; i < mpr_dataset_get_num_records(data); ++i) {
        mpr_data_record record = mpr_dataset_get_record(data, i);
        mpr_sig record_sig = mpr_data_record_get_sig(record);
        mpr_list seen = mpr_list_new_query((const void**)&sigs,
                                          (void*)_cmp_sigs_equality, "hh", record_sig->dev->obj.id, record_sig->obj.id);
        if (mpr_list_get_size(seen) == 0) /* we haven't seen record_sig */ {
            mpr_sig * s = mpr_list_add_item((void**)&sigs, sizeof(mpr_sig));
            *s = record_sig;
        }
    }
    return sigs;
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
	printf("data recorder signal handler completed\n");
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
