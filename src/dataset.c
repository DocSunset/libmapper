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
mpr_dataset mpr_dataset_new(const char * name)
{
    RETURN_ARG_UNLESS(name, 0);
    mpr_dataset data = malloc(sizeof(mpr_dataset_t));
    /* TODO: initialize mpr_obj, properties, etc. */
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

void mpr_dataset_free(mpr_dataset data)
{
    free(data);
}

/* increase the allocated memory for data records in a dataset */
static void _dataset_realloc_records(mpr_dataset data)
{
    /* we should probably consider using someone else's better implementation of a dynamic array... */
    data->records = reallocarray(data->records, 2 * data->num_records_allocated, sizeof(mpr_data_record_t));
    die_unless(data->records, "Failed to reallocate dataset `%s` records array\n", data->name);
    data->num_records_allocated = 2 * data->num_records_allocated;
}

/* increase the allocated memory for data values in a dataset */
static void _dataset_realloc_values(mpr_dataset data, size_t minimum_additional_bytes)
{
    size_t new_values_array_size = 2 * data->values_bytes_allocated;
    while (minimum_additional_bytes > new_values_array_size) new_values_array_size = 2 * new_values_array_size;
    data->values = realloc(data->values, new_values_array_size);
    die_unless(data->values, "Failed to reallocate dataset `%s` values array\n", data->name);
    data->values_bytes_allocated = new_values_array_size;
}

void mpr_dataset_add_record(mpr_dataset data, const mpr_data_record record)
{
    /* we assume there is always room for one more record */
    data->records[data->num_records] = *record;

	/* we make sure there is enough room for the additional values */
    size_t additional_value_bytes = mpr_type_get_size(record->type) * record->length;
    if (data->values_bytes_allocated - (data->values_write_position - data->values) < additional_value_bytes)
        _dataset_realloc_values(data, additional_value_bytes);
    memcpy(data->values_write_position, record->value, additional_value_bytes);
    data->records[data->num_records].value = data->values_write_position;

	/* increment and realloc if necessary */
    ++data->num_records;
    if (data->num_records > data->num_records_allocated)
        _dataset_realloc_records(data);

    data->values_write_position = data->values_write_position + additional_value_bytes;
    if (data->values_write_position - data->values > data->values_bytes_allocated)
        _dataset_realloc_values(data, additional_value_bytes);
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

mpr_data_recorder mpr_data_recorder_new(mpr_dataset data, unsigned int num_sigs, mpr_sig * sigs, mpr_graph g)
{
    RETURN_ARG_UNLESS(sigs, 0);
    RETURN_ARG_UNLESS(num_sigs > 0, 0);

    for (unsigned int i = 0; i < num_sigs; ++i) {
        TRACE_RETURN_UNLESS(sigs[i], NULL, "Cannot include uninitialized signal in dataset.\n");
        TRACE_RETURN_UNLESS(sigs[i]->dev->name, NULL, "Cannot include uninitialized device in dataset.\n");
    }
    /* TODO: (quietly?) ignore NULL signals */
    /* TODO: allow uninitialized devices into data recorders; just don't consider the recorder ready until all the devices are */

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
        mpr_obj_set_prop((mpr_obj)rec->sigs[i], MPR_PROP_DATA, 0, 1, MPR_PTR, &rec, 0);
        /* TODO: mark the local recording destination as such for use by session managers */
        free(name);
    }

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
}

int mpr_data_recorder_poll(mpr_data_recorder rec, int block_ms)
{
    return mpr_dev_poll(rec->dev, block_ms);
}

void mpr_data_recorder_disarm(mpr_data_recorder rec)
{
    rec->armed = 0;
}

int mpr_data_recorder_get_is_armed(mpr_data_recorder rec)
{
    return rec->armed;
}

static void sig_handler(mpr_sig sig, mpr_sig_evt evt, mpr_id instance, int length,
        mpr_type type, const void * value, mpr_time time)
{
    mpr_data_recorder rec = mpr_obj_get_prop_as_ptr((mpr_obj)sig, MPR_PROP_DATA, 0);
    RETURN_UNLESS(mpr_data_recorder_get_is_recording(rec));

    mpr_data_record record = mpr_data_record_new(sig, evt, instance, length, type, value, time);
    mpr_dataset_add_record(rec->data, record);
    mpr_data_record_free(record);
}

void mpr_data_recorder_start(mpr_data_recorder rec)
{
    RETURN_UNLESS(mpr_dev_get_is_ready(rec->dev));
    if (!mpr_data_recorder_get_is_armed(rec)) mpr_data_recorder_arm(rec);
    rec->recording = 1;
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
    if (rec->mapped == 0) for (unsigned int i = 0; i < rec->num_sigs; ++i)
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
