#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mapper_internal.h"
#include "types_internal.h"
#include <mapper/mapper.h>

static char * dataset_dev_name(const char * name)
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

    mpr_dataset data = malloc(sizeof(mpr_dataset_t));
    /* TODO: initialize mpr_obj, properties, etc. */
    data->name = name;
    data->sigs = malloc(num_sigs * sizeof(mpr_sig)); /* but if they go offline? Use after free is likely? */
    memcpy(data->sigs, sigs, num_sigs * sizeof(mpr_sig)); /* so we copy the sigs themselves. Probably still issues though... */
    /* arguably we should just copy the properties of the signals? */
    /* the dataset needs to be able to assume that the signal properties remain the same; if not, there would have to be some way
     * to automatically update the dataset based on the change in properties, which is not trivial */
    data->num_sigs = num_sigs;

    return data;
}

void mpr_dataset_free(mpr_dataset data)
{
    free(data->sigs);
    free(data);
}

mpr_data_recorder mpr_data_recorder_new(mpr_dataset data, mpr_graph g)
{
    mpr_data_recorder rec = malloc(sizeof(mpr_data_recorder_t));
    rec->data = data;
    {
        char * dev_name = dataset_dev_name(data->name);
        rec->dev = mpr_dev_new(dev_name, g);
        free(dev_name);
    }
    rec->sigs = calloc(data->num_sigs, sizeof(mpr_sig));
    rec->maps = calloc(data->num_sigs, sizeof(mpr_map));

    for (unsigned int i = 0; i < rec->data->num_sigs; ++i)
    {
        /* make a local copy of the signal */
        mpr_sig sig = rec->data->sigs[i];
        size_t sig_name_len = strlen(sig->name);
        size_t dev_name_len = strlen(sig->dev->name);
        size_t name_len = sig_name_len + dev_name_len + 2; /* 2 = length of '/' + '\0' */
        char * name = malloc(name_len);
        snprintf(name, name_len, "%s/%s", sig->name, sig->dev->name);
        rec->sigs[i] = mpr_sig_new(rec->dev, MPR_DIR_IN, name, sig->len,
                                   sig->type, sig->unit, sig->min, sig->max, &sig->num_inst, 0, 0);
        mpr_obj_set_prop((mpr_obj)rec->sigs[i], MPR_PROP_DATA, NULL, 1, MPR_PTR, &rec->data, 0);
        /* TODO: mark the local copy as such */
        free(name);
    }
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
    if (rec->armed) return;
    for (unsigned int i = 0; i < rec->data->num_sigs; ++i)
    {
        mpr_sig remote_sig = rec->data->sigs[i];
        mpr_sig local_sig  = rec->sigs[i];
        /* make a map to the copy */
        rec->maps[i] = mpr_map_new(1, &remote_sig, 1, &local_sig);
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
