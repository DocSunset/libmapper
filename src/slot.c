#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <limits.h>

#include "mapper_internal.h"
#include "types_internal.h"
#include <mapper/mapper.h>

mpr_slot mpr_slot_new(mpr_map map, mpr_sig sig, unsigned char is_local, unsigned char is_src)
{
    size_t size = is_local ? sizeof(struct _mpr_local_slot) : sizeof(struct _mpr_slot);
    mpr_slot slot = (mpr_slot)calloc(1, size);
    slot->map = map;
    slot->sig = sig;
    slot->is_local = is_local ? 1 : 0;
    slot->num_inst = 1;
    slot->dir = (is_src == sig->is_local) ? MPR_DIR_OUT : MPR_DIR_IN;
    slot->causes_update = 1; /* default */
    return slot;
}

static int slot_mask(mpr_slot slot)
{
    return slot == slot->map->dst ? DST_SLOT_PROP : SRC_SLOT_PROP(slot->id);
}

void mpr_slot_free(mpr_slot slot)
{
    free(slot);
}

void mpr_slot_free_value(mpr_local_slot slot)
{
    /* TODO: use rtr_sig for holding memory of local slots for effiency */
    mpr_value_free(&slot->val);
}

int mpr_slot_set_from_msg(mpr_slot slot, mpr_msg msg)
{
    int updated = 0, mask;
    mpr_msg_atom a;
    RETURN_ARG_UNLESS(slot && (!slot->is_local || !((mpr_local_slot)slot)->rsig), 0);
    mask = slot_mask(slot);

    a = mpr_msg_get_prop(msg, MPR_PROP_LEN | mask);
    if (a) {
        mpr_prop prop = a->prop;
        a->prop &= ~mask;
        if (mpr_tbl_set_from_atom(slot->sig->obj.props.synced, a, REMOTE_MODIFY))
            ++updated;
        a->prop = prop;
    }
    a = mpr_msg_get_prop(msg, MPR_PROP_TYPE | mask);
    if (a) {
        mpr_prop prop = a->prop;
        a->prop &= ~mask;
        if (mpr_tbl_set_from_atom(slot->sig->obj.props.synced, a, REMOTE_MODIFY))
            ++updated;
        a->prop = prop;
    }
    RETURN_ARG_UNLESS(!slot->is_local, 0);
    a = mpr_msg_get_prop(msg, MPR_PROP_DIR | mask);
    if (a && mpr_type_get_is_str(a->types[0])) {
        int dir = 0;
        if (strcmp(&(*a->vals)->s, "output")==0)
            dir = MPR_DIR_OUT;
        else if (strcmp(&(*a->vals)->s, "input")==0)
            dir = MPR_DIR_IN;
        if (dir)
            updated += mpr_tbl_set(slot->sig->obj.props.synced, PROP(DIR), NULL, 1, MPR_INT32,
                                   &dir, REMOTE_MODIFY);
    }
    a = mpr_msg_get_prop(msg, MPR_PROP_NUM_INST | mask);
    if (a && MPR_INT32 == a->types[0]) {
        int num_inst = a->vals[0]->i;
        if (slot->is_local && !slot->sig->is_local && ((mpr_local_map)slot->map)->expr) {
            mpr_local_map map = (mpr_local_map)slot->map;
            int hist_size = 0;
            if (map->dst == (mpr_local_slot)slot)
                hist_size = mpr_expr_get_out_hist_size(map->expr);
            else {
                int i;
                for (i = 0; i < map->num_src; i++) {
                    if (map->src[i] == (mpr_local_slot)slot)
                        hist_size = mpr_expr_get_in_hist_size(map->expr, i);
                }
            }
            mpr_slot_alloc_values((mpr_local_slot)slot, num_inst, hist_size);
        }
        else
            slot->num_inst = num_inst;
    }
    return updated;
}

void mpr_slot_add_props_to_msg(lo_message msg, mpr_slot slot, int is_dst)
{
    int len;
    char temp[32];
    if (is_dst)
        snprintf(temp, 32, "@dst");
    else if (0 == (int)slot->id)
        snprintf(temp, 32, "@src");
    else
        snprintf(temp, 32, "@src.%d", (int)slot->id);
    len = strlen(temp);

    if (slot->sig->is_local) {
        /* include length from associated signal */
        snprintf(temp+len, 32-len, "%s", mpr_prop_as_str(MPR_PROP_LEN, 0));
        lo_message_add_string(msg, temp);
        lo_message_add_int32(msg, slot->sig->len);

        /* include type from associated signal */
        snprintf(temp+len, 32-len, "%s", mpr_prop_as_str(MPR_PROP_TYPE, 0));
        lo_message_add_string(msg, temp);
        lo_message_add_char(msg, slot->sig->type);

        /* include direction from associated signal */
        snprintf(temp+len, 32-len, "%s", mpr_prop_as_str(MPR_PROP_DIR, 0));
        lo_message_add_string(msg, temp);
        lo_message_add_string(msg, slot->sig->dir == MPR_DIR_OUT ? "output" : "input");

        /* include num_inst property */
        snprintf(temp+len, 32-len, "%s", mpr_prop_as_str(MPR_PROP_NUM_INST, 0));
        lo_message_add_string(msg, temp);
        lo_message_add_int32(msg, slot->num_inst);
    }
}

void mpr_slot_print(mpr_slot slot, int is_dst)
{
    char temp[16];
    if (is_dst)
        snprintf(temp, 16, "@dst");
    else if (0 == (int)slot->id)
        snprintf(temp, 16, "@src");
    else
        snprintf(temp, 16, "@src.%d", (int)slot->id);

    printf(", %s%s=%d", temp, mpr_prop_as_str(MPR_PROP_LEN, 0), slot->sig->type);
    printf(", %s%s=%c", temp, mpr_prop_as_str(MPR_PROP_TYPE, 0), slot->sig->type);
    printf(", %s%s=%d", temp, mpr_prop_as_str(MPR_PROP_NUM_INST, 0), slot->num_inst);
}

int mpr_slot_match_full_name(mpr_slot slot, const char *full_name)
{
    int len;
    const char *sig_name, *dev_name;
    RETURN_ARG_UNLESS(full_name, 1);
    full_name += (full_name[0]=='/');
    sig_name = strchr(full_name+1, '/');
    RETURN_ARG_UNLESS(sig_name, 1);
    len = sig_name - full_name;
    dev_name = slot->sig->dev->name;
    return (strlen(dev_name) != len || strncmp(full_name, dev_name, len)
            || strcmp(sig_name+1, slot->sig->name)) ? 1 : 0;
}

void mpr_slot_alloc_values(mpr_local_slot slot, int num_inst, int hist_size)
{
    RETURN_UNLESS(num_inst && hist_size && slot->sig->type && slot->sig->len);
    if (slot->sig->is_local)
        num_inst = slot->sig->num_inst;

    /* reallocate memory */
    mpr_value_realloc(&slot->val, slot->sig->len, slot->sig->type,
                      hist_size, num_inst, slot == slot->map->dst);

    slot->num_inst = num_inst;
}

void mpr_slot_remove_inst(mpr_local_slot slot, int idx)
{
    RETURN_UNLESS(slot && idx >= 0 && idx < slot->num_inst);
    /* TODO: remove slot->num_inst property */
    slot->num_inst = mpr_value_remove_inst(&slot->val, idx);
}
