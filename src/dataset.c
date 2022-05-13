#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mapper_internal.h"
#include "types_internal.h"
#include <mapper/mapper.h>

mpr_dataset mpr_dataset_new(const char * name, unsigned int num_sigs, mpr_sig * sigs)
{
    unsigned int i;
    RETURN_ARG_UNLESS(name, 0);
    RETURN_ARG_UNLESS(sigs, 0);
    RETURN_ARG_UNLESS(num_sigs > 0, 0);
    for (i = 0; i < num_sigs; ++i) {
        TRACE_RETURN_UNLESS(sigs[i], NULL, "Cannot include uninitialized signal in dataset.\n");
        TRACE_RETURN_UNLESS(sigs[i]->dev->name, NULL, "Cannot include uninitialized device in dataset.\n");
    }

    size_t len = strlen(name);
    size_t dev_name_len = len + 8 + 1; /* 8 = length of "dataset_", 1 = length of null terminator */
    char * dev_name = malloc(dev_name_len);
    strncpy(dev_name, "dataset_", 9); /* copy the null terminator to silence warnings, even though we overwrite it */
    strncpy(dev_name+8, name, len + 1);

    /* transform slashes into underscores */
    for (char * c = dev_name + 8; *c != '\0'; ++c) if (*c == '/') *c = '_';

    mpr_dataset data = malloc(sizeof(mpr_dataset_t));
    data->name = name;
    data->dev_name = dev_name;
    data->dev_name_len = dev_name_len;
    data->sigs = malloc(num_sigs * sizeof(mpr_sig));
    memcpy(data->sigs, sigs, num_sigs * sizeof(mpr_sig));
    data->num_sigs = num_sigs;

    return data;
}

void mpr_dataset_free(mpr_dataset data)
{
    free(data->dev_name);
    free(data->sigs);
    free(data);
}

mpr_data_recorder mpr_data_recorder_new(mpr_dataset data, mpr_graph g)
{
    mpr_data_recorder rec = malloc(sizeof(mpr_data_recorder_t));
    rec->data = data;
    rec->dev = mpr_dev_new(data->dev_name, g);
    rec->sigs = calloc(data->num_sigs, sizeof(mpr_sig));
    rec->maps = calloc(data->num_sigs, sizeof(mpr_map));
    return rec;
}

void mpr_data_recorder_free(mpr_data_recorder rec)
{
    mpr_dev_free(rec->dev);
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

void mpr_data_recorder_arm(mpr_data_recorder rec)
{
    RETURN_UNLESS(mpr_dev_get_is_ready(rec->dev));
    for (unsigned int i = 0; i < rec->data->num_sigs; ++i)
    {
        mpr_sig sig = rec->data->sigs[i];
        {
            /* make a local copy of the signal */
            size_t sig_name_len = strlen(sig->name);
            size_t dev_name_len = strlen(sig->dev->name);
            size_t name_len = sig_name_len + dev_name_len + 2;
            char * name = malloc(name_len);
            snprintf(name, name_len, "%s/%s", sig->name, sig->dev->name);
            rec->sigs[i] = mpr_sig_new(rec->dev, MPR_DIR_IN, name, sig->len,
                                       sig->type, sig->unit, sig->min, sig->max, &sig->num_inst, 0, 0);
            mpr_obj_set_prop((mpr_obj)rec->sigs[i], MPR_PROP_DATA, NULL, 1, MPR_PTR, &rec->data, 0);
            /* TODO: mark the local copy as such */
            free(name);
        }
        /* make a map to the copy */
        rec->maps[i] = mpr_map_new(1, &sig, 1, &rec->sigs[i]);
        mpr_obj_set_prop((mpr_obj)rec->maps[i], MPR_PROP_EXPR, NULL, 1, MPR_STR, "y=x", 1);
        mpr_obj_push((mpr_obj)rec->maps[i]);
    }
    rec->armed = 1;
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
    mpr_dataset data = mpr_obj_get_prop_as_ptr(sig, MPR_PROP_DATA, 0);
    printf("handler for %s\n", sig->name);
}

void mpr_data_recorder_start(mpr_data_recorder rec)
{
    RETURN_UNLESS(mpr_dev_get_is_ready(rec->dev));
    if (!mpr_data_recorder_get_is_armed(rec)) mpr_data_recorder_arm(rec);
    for (unsigned int i = 0; i < rec->data->num_sigs; ++i)
    {
        mpr_sig_set_cb(rec->sigs[i], sig_handler, MPR_SIG_ALL);
    }
    rec->recording = 1;
}

void mpr_data_recorder_stop(mpr_data_recorder rec)
{
    rec->recording = 0;
}
