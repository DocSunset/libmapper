#include <stdlib.h>
#include <string.h>

#include "mapper_internal.h"
#include "types_internal.h"
#include <mapper/mapper.h>

mpr_dataset mpr_dataset_new(const char * name, unsigned int num_sig, mpr_sig * sig)
{
    unsigned int i;
    RETURN_ARG_UNLESS(name, 0);
    RETURN_ARG_UNLESS(sig, 0);
    RETURN_ARG_UNLESS(num_sig > 0, 0);
    for (i = 0; i < num_sig; ++i) {
        TRACE_RETURN_UNLESS(sig[i], NULL, "Cannot include uninitialized signal in dataset.\n");
        TRACE_RETURN_UNLESS(sig[i]->dev->name, NULL, "Cannot include uninitialized device in dataset.\n");
    }

    size_t len = strlen(name);
    char * dev_name = malloc(len + 8 + 1); /* 8 = length of "dataset_", 1 = length of null terminator */
    strncpy(dev_name, "dataset_", 9); /* copy the null terminator to silence warnings, even though we overwrite it */
    strncpy(dev_name+8, name, len + 1);

    /* transform slashes into underscores */
    for (char * c = dev_name + 8; *c != '\0'; ++c) if (*c == '/') *c = '_';

    mpr_dataset data = malloc(sizeof(mpr_dataset_t));
    data->name = name;
    data->dev_name = dev_name;

    return data;
}

void mpr_dataset_free(mpr_dataset data)
{
    free(data->dev_name);
    free(data);
}

mpr_data_recorder mpr_data_recorder_new(mpr_dataset data, mpr_graph g)
{
    mpr_data_recorder rec = malloc(sizeof(mpr_data_recorder_t));
    rec->data = data;
    rec->dev = mpr_dev_new(data->dev_name, g);
    return rec;
}

void mpr_data_recorder_free(mpr_data_recorder rec)
{
    mpr_dev_free(rec->dev);
    free(rec);
}

int mpr_data_recorder_poll(mpr_data_recorder rec, int block_ms)
{
    return mpr_dev_poll(rec->dev, block_ms);
}

void mpr_data_recorder_arm(mpr_data_recorder rec)
{
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

void mpr_data_recorder_start(mpr_data_recorder rec)
{
    rec->recording = 1;
}

void mpr_data_recorder_stop(mpr_data_recorder rec)
{
    rec->recording = 0;
}
