
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

#include "mapper_internal.h"
#include "types_internal.h"
#include <mapper/mapper.h>

#define MAX_INSTANCES 128

/* Function prototypes */
static void mapper_signal_update_internal(mapper_signal sig, int instance_index,
                                          const void *value, int count,
                                          mapper_timetag_t timetag);

static void *mapper_signal_instance_value_internal(mapper_signal sig,
                                                   int instance_index,
                                                   mapper_timetag_t *timetag);

static int mapper_signal_oldest_active_instance_internal(mapper_signal sig);

static int mapper_signal_newest_active_instance_internal(mapper_signal sig);

static int mapper_signal_find_instance_with_local_id(mapper_signal sig,
                                                     mapper_id id, int flags);

static int mapper_signal_add_id_map(mapper_signal sig, mapper_signal_instance si,
                                    mapper_id_map map);

static int compare_ids(const void *l, const void *r)
{
    return memcmp(&(*(mapper_signal_instance *)l)->id,
                  &(*(mapper_signal_instance *)r)->id,
                  sizeof(mapper_id));
}

static mapper_signal_instance find_instance_by_id(mapper_signal sig,
                                                  mapper_id id)
{
    if (!sig->num_instances)
        return 0;

    mapper_signal_instance_t si;
    mapper_signal_instance sip = &si;
    si.id = id;

    mapper_signal_instance *sipp = bsearch(&sip, sig->local->instances,
                                           sig->num_instances,
                                           sizeof(mapper_signal_instance),
                                           compare_ids);
    if (sipp && *sipp)
        return *sipp;
    return 0;
}

void mapper_signal_init(mapper_signal sig, const char *name, int length,
                        char type, mapper_direction direction, const char *unit,
                        const void *minimum, const void *maximum,
                        mapper_signal_update_handler *handler,
                        const void *user_data)
{
    int i;
    if (!name)
        return;

    name = skip_slash(name);
    int len = strlen(name)+2;
    sig->path = malloc(len);
    snprintf(sig->path, len, "/%s", name);
    sig->name = (char*)sig->path+1;

    sig->length = length;
    sig->type = type;
    sig->direction = direction;
    sig->unit = unit ? strdup(unit) : strdup("unknown");
    sig->minimum = sig->maximum = 0;

    sig->num_instances = 0;

    sig->user_data = (void*)user_data;

    if (sig->local) {
        sig->local->update_handler = handler;
        sig->local->has_complete_value = calloc(1, length / 8 + 1);
        for (i = 0; i < length; i++) {
            sig->local->has_complete_value[i/8] |= 1 << (i % 8);
        }

        // Reserve one instance to start
        mapper_signal_reserve_instances(sig, 1, 0, 0);

        // Reserve one instance id map
        sig->local->id_map_length = 1;
        sig->local->id_maps = calloc(1, sizeof(struct _mapper_signal_id_map));
    }

    sig->props = mapper_table_new();
    sig->staged_props = mapper_table_new();
    int flags = sig->local ? NON_MODIFIABLE : MODIFIABLE;

    // these properties need to be added in alphabetical order
    mapper_table_link_value(sig->props, AT_DESCRIPTION, 1, 's',
                            &sig->description, flags | INDIRECT);

    mapper_table_link_value(sig->props, AT_DIRECTION, 1, 'i', &sig->direction,
                            flags);

    mapper_table_link_value(sig->props, AT_ID, 1, 'h', &sig->id, flags);

    mapper_table_link_value(sig->props, AT_LENGTH, 1, 'i', &sig->length, flags);

    mapper_table_link_value(sig->props, AT_MAX, sig->length, sig->type,
                            &sig->maximum,
                            ((sig->local ? LOCAL_MODIFY : MODIFIABLE)
                             | INDIRECT | MUTABLE_LENGTH | MUTABLE_TYPE));

    mapper_table_link_value(sig->props, AT_MIN, sig->length, sig->type,
                            &sig->minimum,
                            ((sig->local ? LOCAL_MODIFY : MODIFIABLE)
                             | INDIRECT | MUTABLE_LENGTH | MUTABLE_TYPE));

    mapper_table_link_value(sig->props, AT_NAME, 1, 's', &sig->name,
                            flags | INDIRECT | LOCAL_ACCESS_ONLY);

    mapper_table_link_value(sig->props, AT_NUM_INCOMING_MAPS, 1, 'i',
                            &sig->num_incoming_maps, flags);

    mapper_table_link_value(sig->props, AT_NUM_INSTANCES, 1, 'i',
                            &sig->num_instances, flags);

    mapper_table_link_value(sig->props, AT_NUM_OUTGOING_MAPS, 1, 'i',
                            &sig->num_outgoing_maps, flags);

    // TODO: should rate be settable or only derived from calls to update()?
    mapper_table_link_value(sig->props, AT_RATE, 1, 'f', &sig->rate,
                            sig->local ? LOCAL_MODIFY : MODIFIABLE);

    mapper_table_link_value(sig->props, AT_TYPE, 1, 'c', &sig->type, flags);

    mapper_table_link_value(sig->props, AT_UNIT, 1, 's', &sig->unit,
                            (sig->local ? LOCAL_MODIFY : MODIFIABLE) | INDIRECT);

    mapper_table_link_value(sig->props, AT_USER_DATA, 1, 'v', &sig->user_data,
                            LOCAL_MODIFY | INDIRECT | LOCAL_ACCESS_ONLY);

    mapper_table_link_value(sig->props, AT_VERSION, 1, 'i', &sig->version,
                            flags);

    if (minimum)
        mapper_signal_set_minimum(sig, minimum);
    if (maximum)
        mapper_signal_set_maximum(sig, maximum);
}

void mapper_signal_free(mapper_signal sig)
{
    int i;
    if (!sig) return;

    if (sig->local) {
        // Free instances
        for (i = 0; i < sig->local->id_map_length; i++) {
            if (sig->local->id_maps[i].instance) {
                mapper_signal_instance_release_internal(sig, i, MAPPER_NOW);
            }
        }
        free(sig->local->id_maps);
        for (i = 0; i < sig->num_instances; i++) {
            if (sig->local->instances[i]->value)
                free(sig->local->instances[i]->value);
            if (sig->local->instances[i]->has_value_flags)
                free(sig->local->instances[i]->has_value_flags);
            free(sig->local->instances[i]);
        }
        free(sig->local->instances);
        if (sig->local->has_complete_value)
            free(sig->local->has_complete_value);
        free(sig->local);
    }

    if (sig->props)
        mapper_table_free(sig->props);
    if (sig->staged_props)
        mapper_table_free(sig->staged_props);
    if (sig->description)
        free(sig->description);
    if (sig->maximum)
        free(sig->maximum);
    if (sig->minimum)
        free(sig->minimum);
    if (sig->path)
        free(sig->path);
    if (sig->unit)
        free(sig->unit);
}

void mapper_signal_push(mapper_signal sig)
{
    if (!sig)
        return;
    mapper_network net = sig->device->database->network;

    if (sig->local) {
//        if (!sig->props->dirty)
//            return;
        // TODO: make direction flags compatible with MAPPER_OBJ flags
        mapper_object_type obj = ((sig->direction == MAPPER_DIR_OUTGOING)
                                  ? MAPPER_OBJ_OUTPUT_SIGNALS
                                  : MAPPER_OBJ_INPUT_SIGNALS);
        mapper_network_set_dest_subscribers(net, obj);
        mapper_signal_send_state(sig, MSG_SIGNAL);
    }
    else {
//        if (!sig->staged_props->dirty)
//            return;
        mapper_network_set_dest_bus(net);
        mapper_signal_send_state(sig, MSG_SIGNAL_SET_PROPS);

        // clear the staged properties
        mapper_table_clear(sig->staged_props);
    }
}

void mapper_signal_update(mapper_signal sig, const void *value, int count,
                          mapper_timetag_t tt)
{
    if (!sig || !sig->local)
        return;

    mapper_timetag_t *ttp;
    if (memcmp(&tt, &MAPPER_NOW, sizeof(mapper_timetag_t))==0) {
        ttp = &sig->device->database->network->clock.now;
        mapper_device_now(sig->device, ttp);
    }
    else
        ttp = &tt;

    int index = 0;
    if (!sig->local->id_maps[0].instance)
        index = mapper_signal_instance_with_local_id(sig, 0, 0, ttp);
    mapper_signal_update_internal(sig, index, value, count, *ttp);
}

void mapper_signal_update_int(mapper_signal sig, int value)
{
    if (!sig || !sig->local)
        return;

#ifdef DEBUG
    if (sig->type != 'i') {
        trace("called mapper_signal_update_int() on non-int signal!\n");
        return;
    }

    if (sig->length != 1) {
        trace("called mapper_signal_update_int() on non-scalar signal!\n");
        return;
    }

    if (!sig->device) {
        trace("signal does not have a device in mapper_signal_update_int().\n");
        return;
    }
#endif

    mapper_timetag_t *tt = &sig->device->database->network->clock.now;
    mapper_device_now(sig->device, tt);

    int index = 0;
    if (!sig->local->id_maps[0].instance)
        index = mapper_signal_instance_with_local_id(sig, 0, 0, tt);
    mapper_signal_update_internal(sig, index, &value, 1, *tt);
}

void mapper_signal_update_float(mapper_signal sig, float value)
{
    if (!sig || !sig->local)
        return;

#ifdef DEBUG
    if (sig->type != 'f') {
        trace("called mapper_signal_update_float() on non-float signal!\n");
        return;
    }

    if (sig->length != 1) {
        trace("called mapper_signal_update_float() on non-scalar signal!\n");
        return;
    }

    if (!sig->device) {
        trace("signal does not have a device in mapper_signal_update_float().\n");
        return;
    }
#endif

    mapper_timetag_t *tt = &sig->device->database->network->clock.now;
    mapper_device_now(sig->device, tt);

    int index = 0;
    if (!sig->local->id_maps[0].instance)
        index = mapper_signal_instance_with_local_id(sig, 0, 0, tt);
    mapper_signal_update_internal(sig, index, &value, 1, *tt);
}

void mapper_signal_update_double(mapper_signal sig, double value)
{
    if (!sig || !sig->local)
        return;

#ifdef DEBUG
    if (sig->type != 'd') {
        trace("called mapper_signal_update_double() on non-double signal!\n");
        return;
    }

    if (sig->length != 1) {
        trace("called mapper_signal_update_double() on non-scalar signal!\n");
        return;
    }

    if (!sig->device) {
        trace("signal does not have a device in mapper_signal_update_double().\n");
        return;
    }
#endif

    mapper_timetag_t *tt = &sig->device->database->network->clock.now;
    mapper_device_now(sig->device, tt);

    int index = 0;
    if (!sig->local->id_maps[0].instance)
        index = mapper_signal_instance_with_local_id(sig, 0, 0, tt);
    mapper_signal_update_internal(sig, index, &value, 1, *tt);
}

const void *mapper_signal_value(mapper_signal sig, mapper_timetag_t *timetag)
{
    if (!sig || !sig->local)
        return 0;
    int index = 0;
    if (!sig->local->id_maps[0].instance)
        index = mapper_signal_find_instance_with_local_id(sig, 0, 0);
    if (index < 0)
        return 0;
    return mapper_signal_instance_value_internal(sig, index, timetag);
}

/**** Instances ****/

static void mapper_signal_init_instance(mapper_signal_instance si)
{
    si->has_value = 0;
    lo_timetag_now(&si->created);
}

static int mapper_signal_find_instance_with_local_id(mapper_signal sig,
                                                     mapper_id id, int flags)
{
    int i;
    for (i = 0; i < sig->local->id_map_length; i++) {
        if (   sig->local->id_maps[i].instance
            && sig->local->id_maps[i].map->local == id) {
            if (sig->local->id_maps[i].status & ~flags)
                return -1;
            else
                return i;
        }
    }
    return -1;
}

int mapper_signal_find_instance_with_global_id(mapper_signal sig,
                                               mapper_id global_id,
                                               int flags)
{
    int i;
    for (i = 0; i < sig->local->id_map_length; i++) {
        if (!sig->local->id_maps[i].map)
            continue;
        if (sig->local->id_maps[i].map->global == global_id) {
            if (sig->local->id_maps[i].status & ~flags)
                return -1;
            else
                return i;
        }
    }
    return -1;
}

static mapper_signal_instance reserved_instance(mapper_signal sig)
{
    int i;
    for (i = 0; i < sig->num_instances; i++) {
        if (!sig->local->instances[i]->is_active) {
            return sig->local->instances[i];
        }
    }
    return 0;
}

int mapper_signal_instance_with_local_id(mapper_signal sig, mapper_id id,
                                         int flags, mapper_timetag_t *tt)
{
    if (!sig || !sig->local)
        return -1;

    mapper_signal_id_map_t *maps = sig->local->id_maps;
    mapper_signal_update_handler *update_h = sig->local->update_handler;
    mapper_instance_event_handler *event_h = sig->local->instance_event_handler;

    mapper_signal_instance si;
    int i;
    for (i = 0; i < sig->local->id_map_length; i++) {
        if (maps[i].instance && maps[i].map->local == id) {
            if (maps[i].status & ~flags)
                return -1;
            else {
                return i;
            }
        }
    }

    // check if device has record of id map
    mapper_id_map map = mapper_device_find_instance_id_map_by_local(sig->device,
                                                                    id);

    /* No instance with that id exists - need to try to activate instance and
     * create new id map. */
    if ((si = find_instance_by_id(sig, id))) {
        if (!map) {
            // Claim id map locally, add id map to device and link from signal
            map = mapper_device_add_instance_id_map(sig->device, id,
                                                    mapper_device_unique_id(sig->device));
        }
        else {
            ++map->refcount_local;
        }

        // store pointer to device map in a new signal map
        si->is_active = 1;
        mapper_signal_init_instance(si);
        i = mapper_signal_add_id_map(sig, si, map);
        if (event_h && (sig->local->instance_event_flags & MAPPER_NEW_INSTANCE)) {
            event_h(sig, id, MAPPER_NEW_INSTANCE, tt);
        }
        return i;
    }

    if (event_h && (sig->local->instance_event_flags & MAPPER_INSTANCE_OVERFLOW)) {
        // call instance event handler
        event_h(sig, 0, MAPPER_INSTANCE_OVERFLOW, tt);
    }
    else if (update_h) {
        if (sig->local->instance_stealing_mode == MAPPER_STEAL_OLDEST) {
            i = mapper_signal_oldest_active_instance_internal(sig);
            if (i < 0)
                return -1;
            update_h(sig, sig->local->id_maps[i].map->local, 0, 0, tt);
        }
        else if (sig->local->instance_stealing_mode == MAPPER_STEAL_NEWEST) {
            i = mapper_signal_newest_active_instance_internal(sig);
            if (i < 0)
                return -1;
            update_h(sig, sig->local->id_maps[i].map->local, 0, 0, tt);
        }
        else
            return -1;
    }
    else
        return -1;

    // try again
    if ((si = find_instance_by_id(sig, id))) {
        if (!map) {
            // Claim id map locally add id map to device and link from signal
            map = mapper_device_add_instance_id_map(sig->device, id,
                                                    mapper_device_unique_id(sig->device));
        }
        else {
            ++map->refcount_local;
        }
        si->is_active = 1;
        mapper_signal_init_instance(si);
        i = mapper_signal_add_id_map(sig, si, map);
        if (event_h && (sig->local->instance_event_flags & MAPPER_NEW_INSTANCE)) {
            event_h(sig, id, MAPPER_NEW_INSTANCE, tt);
        }
        return i;
    }
    return -1;
}

int mapper_signal_instance_with_global_id(mapper_signal sig, mapper_id global_id,
                                          int flags, mapper_timetag_t *tt)
{
    if (!sig || !sig->local)
        return -1;

    mapper_signal_id_map_t *maps = sig->local->id_maps;
    mapper_signal_update_handler *update_h = sig->local->update_handler;
    mapper_instance_event_handler *event_h = sig->local->instance_event_handler;

    mapper_signal_instance si;
    int i;
    for (i = 0; i < sig->local->id_map_length; i++) {
        if (maps[i].instance && maps[i].map->global == global_id) {
            if (maps[i].status & ~flags)
                return -1;
            else {
                return i;
            }
        }
    }

    // check if the device already has a map for this global id
    mapper_id_map map = mapper_device_find_instance_id_map_by_global(sig->device,
                                                                     global_id);
    if (!map) {
        /* Here we still risk creating conflicting maps if two signals are
         * updated asynchronously.  This is easy to avoid by not allowing a
         * local id to be used with multiple active remote maps, however users
         * may wish to create devices with multiple object classes which do not
         * require mutual instance id synchronization - e.g. instance 1 of
         * object class A is not related to instance 1 of object B. */
        // TODO: add object groups for explictly sharing id maps

        if ((si = reserved_instance(sig))) {
            map = mapper_device_add_instance_id_map(sig->device, si->id,
                                                    global_id);
            map->refcount_global = 1;

            si->is_active = 1;
            mapper_signal_init_instance(si);
            i = mapper_signal_add_id_map(sig, si, map);
            if (event_h && (sig->local->instance_event_flags & MAPPER_NEW_INSTANCE)) {
                event_h(sig, si->id, MAPPER_NEW_INSTANCE, tt);
            }
            return i;
        }
    }
    else {
        si = find_instance_by_id(sig, map->local);
        if (!si) {
            /* TODO: Once signal groups are explicit, allow re-mapping to
             * another instance if possible. */
            trace("Signal %s has no instance %llu available.\n", sig->name,
                  (unsigned long long)map->local);
            return -1;
        }
        else if (!si->is_active) {
            si->is_active = 1;
            mapper_signal_init_instance(si);
            i = mapper_signal_add_id_map(sig, si, map);
            ++map->refcount_local;
            ++map->refcount_global;

            if (event_h && (sig->local->instance_event_flags & MAPPER_NEW_INSTANCE)) {
                event_h(sig, si->id, MAPPER_NEW_INSTANCE, tt);
            }
            return i;
        }
    }

    // try releasing instance in use
    if (event_h && (sig->local->instance_event_flags & MAPPER_INSTANCE_OVERFLOW)) {
        // call instance event handler
        event_h(sig, 0, MAPPER_INSTANCE_OVERFLOW, tt);
    }
    else if (update_h) {
        if (sig->local->instance_stealing_mode == MAPPER_STEAL_OLDEST) {
            i = mapper_signal_oldest_active_instance_internal(sig);
            if (i < 0)
                return -1;
            update_h(sig, sig->local->id_maps[i].map->local, 0, 0, tt);
        }
        else if (sig->local->instance_stealing_mode == MAPPER_STEAL_NEWEST) {
            i = mapper_signal_newest_active_instance_internal(sig);
            if (i < 0)
                return -1;
            update_h(sig, sig->local->id_maps[i].map->local, 0, 0, tt);
        }
        else
            return -1;
    }
    else
        return -1;

    // try again
    if (!map) {
        if ((si = reserved_instance(sig))) {
            map = mapper_device_add_instance_id_map(sig->device, si->id,
                                                    global_id);
            map->refcount_global = 1;

            si->is_active = 1;
            mapper_signal_init_instance(si);
            i = mapper_signal_add_id_map(sig, si, map);
            if (event_h && (sig->local->instance_event_flags & MAPPER_NEW_INSTANCE)) {
                event_h(sig, si->id, MAPPER_NEW_INSTANCE, tt);
            }
            return i;
        }
    }
    else {
        si = find_instance_by_id(sig, map->local);
        if (!si) {
            trace("Signal %s has no instance %llu available.", sig->name,
                  (unsigned long long)map->local);
            return -1;
        }
        if (si) {
            if (si->is_active) {
                return -1;
            }
            si->is_active = 1;
            mapper_signal_init_instance(si);
            i = mapper_signal_add_id_map(sig, si, map);
            ++map->refcount_local;
            ++map->refcount_global;

            if (event_h && (sig->local->instance_event_flags & MAPPER_NEW_INSTANCE)) {
                event_h(sig, si->id, MAPPER_NEW_INSTANCE, tt);
            }
            return i;
        }
    }
    return -1;
}

static int reserve_instance_internal(mapper_signal sig, mapper_id *id,
                                     void *user_data)
{
    if (sig->num_instances >= MAX_INSTANCES)
        return -1;

    int i, lowest_index, cont;
    mapper_signal_instance si;

    // check if instance with this id already exists! If so, stop here.
    if (id && find_instance_by_id(sig, *id))
        return -1;

    // reallocate array of instances
    sig->local->instances = realloc(sig->local->instances,
                                    sizeof(mapper_signal_instance)
                                    * (sig->num_instances+1));
    sig->local->instances[sig->num_instances] =
        (mapper_signal_instance) calloc(1, sizeof(struct _mapper_signal_instance));
    si = sig->local->instances[sig->num_instances];
    si->value = calloc(1, mapper_signal_vector_bytes(sig));
    si->has_value_flags = calloc(1, sig->length / 8 + 1);
    si->has_value = 0;

    if (id)
        si->id = *id;
    else {
        // find lowest unused id
        mapper_id lowest_id = 0;
        cont = 1;
        while (cont) {
            cont = 0;
            for (i = 0; i < sig->num_instances; i++) {
                if (sig->local->instances[i]->id == lowest_id) {
                    cont = 1;
                    break;
                }
            }
            lowest_id += cont;
        }
        si->id = lowest_id;
    }
    // find lowest unused positive index
    lowest_index = 0;
    cont = 1;
    while (cont) {
        cont = 0;
        for (i = 0; i < sig->num_instances; i++) {
            if (sig->local->instances[i]->index == lowest_index) {
                cont = 1;
                break;
            }
        }
        lowest_index += cont;
    }
    si->index = lowest_index;

    mapper_signal_init_instance(si);
    si->user_data = user_data;

    ++sig->num_instances;
    qsort(sig->local->instances, sig->num_instances,
          sizeof(mapper_signal_instance), compare_ids);

    // return largest index
    int highest = -1;
    for (i = 0; i < sig->num_instances; i++) {
        if (sig->local->instances[i]->index > highest)
            highest = sig->local->instances[i]->index;
    }

    return highest;
}

int mapper_signal_reserve_instances(mapper_signal sig, int num, mapper_id *ids,
                                    void **user_data)
{
    if (!sig || !sig->local || !num)
        return 0;

    int i = 0, count = 0, highest = -1, result;
    if (sig->num_instances == 1 && !sig->local->instances[0]->id
        && !sig->local->instances[0]->user_data) {
        // we will overwite the default instance first
        if (ids)
            sig->local->instances[0]->id = ids[0];
        if (user_data)
            sig->local->instances[0]->user_data = user_data[0];
        ++i;
        ++count;
    }
    for (; i < num; i++) {
        result = reserve_instance_internal(sig, ids ? &ids[i] : 0,
                                           user_data ? user_data[i] : 0);
        if (result == -1)
            continue;
        highest = result;
        ++count;
    }
    if (highest != -1)
        mapper_device_num_instances_changed(sig->device, sig, highest + 1);
    return count;
}

void mapper_signal_instance_update(mapper_signal sig, mapper_id id,
                                   const void *value, int count,
                                   mapper_timetag_t timetag)
{
    if (!sig || !sig->local)
        return;

    if (!value) {
        mapper_signal_instance_release(sig, id, timetag);
        return;
    }

    int index = mapper_signal_instance_with_local_id(sig, id, 0, &timetag);
    if (index >= 0)
        mapper_signal_update_internal(sig, index, value, count, timetag);
}

int mapper_signal_instance_is_active(mapper_signal sig, mapper_id id)
{
    if (!sig)
        return 0;
    int index = mapper_signal_find_instance_with_local_id(sig, id, 0);
    if (index < 0)
        return 0;
    return sig->local->id_maps[index].instance->is_active;
}

mapper_id mapper_signal_oldest_active_instance(mapper_signal sig)
{
    if (!sig || !sig->local)
        return 0;

    int index = mapper_signal_oldest_active_instance_internal(sig);
    if (index < 0)
        return 0;
    else
        return sig->local->id_maps[index].map->local;
    return 0;
}

int mapper_signal_oldest_active_instance_internal(mapper_signal sig)
{
    int i;
    mapper_signal_instance si;
    for (i = 0; i < sig->local->id_map_length; i++) {
        if (sig->local->id_maps[i].instance)
            break;
    }
    if (i == sig->local->id_map_length) {
        // no active instances to steal!
        return -1;
    }
    int oldest = i;
    for (i = oldest+1; i < sig->local->id_map_length; i++) {
        if (!(si = sig->local->id_maps[i].instance))
            continue;
        if ((si->created.sec < sig->local->id_maps[oldest].instance->created.sec) ||
            (si->created.sec == sig->local->id_maps[oldest].instance->created.sec &&
             si->created.frac < sig->local->id_maps[oldest].instance->created.frac))
            oldest = i;
    }
    return oldest;
}

mapper_id mapper_signal_newest_active_instance(mapper_signal sig)
{
    if (!sig || !sig->local)
        return 0;

    int index = mapper_signal_newest_active_instance_internal(sig);
    if (index < 0)
        return 0;
    else
        return sig->local->id_maps[index].map->local;
    return 0;
}

int mapper_signal_newest_active_instance_internal(mapper_signal sig)
{
    int i;
    mapper_signal_instance si;
    for (i = 0; i < sig->local->id_map_length; i++) {
        if (sig->local->id_maps[i].instance)
            break;
    }
    if (i == sig->local->id_map_length) {
        // no active instances to steal!
        return -1;
    }
    int newest = i;
    for (i = newest+1; i < sig->local->id_map_length; i++) {
        if (!(si = sig->local->id_maps[i].instance))
            continue;
        if ((si->created.sec > sig->local->id_maps[newest].instance->created.sec) ||
            (si->created.sec == sig->local->id_maps[newest].instance->created.sec &&
             si->created.frac > sig->local->id_maps[newest].instance->created.frac))
            newest = i;
    }
    return newest;
}

static void mapper_signal_update_internal(mapper_signal sig, int instance_index,
                                          const void *value, int count,
                                          mapper_timetag_t tt)
{
    mapper_signal_instance si = sig->local->id_maps[instance_index].instance;

    /* We have to assume that value points to an array of correct type
     * and size. */

    if (value) {
        if (count<=0) count=1;
        size_t n = mapper_signal_vector_bytes(sig);
        memcpy(si->value, value + n*(count-1), n);
        si->has_value = 1;
    }
    else {
        si->has_value = 0;
    }

    if (memcmp(&tt, &MAPPER_NOW, sizeof(mapper_timetag_t))==0)
        mapper_device_now(sig->device, &si->timetag);
    else
        memcpy(&si->timetag, &tt, sizeof(mapper_timetag_t));

    mapper_device_route_signal(sig->device, sig, instance_index, value,
                               count, si->timetag);
}

void mapper_signal_instance_release(mapper_signal sig, mapper_id id,
                                    mapper_timetag_t timetag)
{
    if (!sig || !sig->local)
        return;

    int index = mapper_signal_find_instance_with_local_id(sig, id,
                                                          RELEASED_REMOTELY);
    if (index >= 0)
        mapper_signal_instance_release_internal(sig, index, timetag);
}

void mapper_signal_instance_release_internal(mapper_signal sig,
                                             int instance_index,
                                             mapper_timetag_t tt)
{
    mapper_signal_id_map_t *smap = &sig->local->id_maps[instance_index];
    if (!smap->instance)
        return;

    mapper_timetag_t *ttp;
    if (memcmp(&tt, &MAPPER_NOW, sizeof(mapper_timetag_t))==0) {
        ttp = &sig->device->database->network->clock.now;
        mapper_device_now(sig->device, ttp);
    }
    else
        ttp = &tt;

    mapper_device_route_signal(sig->device, sig, instance_index, 0, 0, *ttp);

    --smap->map->refcount_local;
    if (smap->map->refcount_local <= 0 && smap->map->refcount_global <= 0) {
        mapper_device_remove_instance_id_map(sig->device, smap->map);
        smap->map = 0;
    }
    else if ((sig->direction & MAPPER_DIR_OUTGOING)
             || smap->status & RELEASED_REMOTELY) {
        // TODO: consider multiple upstream source instances?
        smap->map = 0;
    }
    else {
        // mark map as locally-released but do not remove it
        sig->local->id_maps[instance_index].status |= RELEASED_LOCALLY;
    }

    // Put instance back in reserve list
    smap->instance->is_active = 0;
    smap->instance = 0;
}

void mapper_signal_remove_instance(mapper_signal sig, mapper_id id)
{
    if (!sig || !sig->local)
        return;

    int i;
    for (i = 0; i < sig->num_instances; i++) {
        if (sig->local->instances[i]->id == id) {
            if (sig->local->instances[i]->is_active) {
                // First release instance
                mapper_timetag_t tt = sig->device->database->network->clock.now;
                mapper_device_now(sig->device, &tt);
                mapper_signal_instance_release_internal(sig, i, tt);
            }
            break;
        }
    }

    if (i == sig->num_instances)
        return;

    if (sig->local->instances[i]->value)
        free(sig->local->instances[i]->value);
    if (sig->local->instances[i]->has_value_flags)
        free(sig->local->instances[i]->has_value_flags);
    free(sig->local->instances[i]);
    ++i;
    for (; i < sig->num_instances; i++) {
        sig->local->instances[i-1] = sig->local->instances[i];
    }
    --sig->num_instances;
    sig->local->instances = realloc(sig->local->instances,
                                    sizeof(mapper_signal_instance)
                                    * sig->num_instances);

    // TODO: could also realloc signal value histories
}

static void *mapper_signal_instance_value_internal(mapper_signal sig,
                                                   int instance_index,
                                                   mapper_timetag_t *timetag)
{
    mapper_signal_instance si = sig->local->id_maps[instance_index].instance;
    if (!si) return 0;
    if (!si->has_value)
        return 0;
    if (timetag)
        timetag = &si->timetag;
    return si->value;
}

const void *mapper_signal_instance_value(mapper_signal sig, mapper_id id,
                                         mapper_timetag_t *timetag)
{
    if (!sig || !sig->local)
        return 0;

    int index = mapper_signal_find_instance_with_local_id(sig, id,
                                                          RELEASED_REMOTELY);
    if (index < 0)
        return 0;
    return mapper_signal_instance_value_internal(sig, index, timetag);
}

int mapper_signal_num_instances(mapper_signal sig)
{
    return sig ? sig->num_instances : -1;
}

int mapper_signal_num_active_instances(mapper_signal sig)
{
    if (!sig || !sig->local)
        return -1;
    int i, j = 0;
    for (i = 0; i < sig->local->id_map_length; i++) {
        if (sig->local->id_maps[i].instance)
            ++j;
    }
    return j;
}

int mapper_signal_num_reserved_instances(mapper_signal sig)
{
    if (!sig || !sig->local)
        return -1;
    int i, j = 0;
    for (i = 0; i < sig->num_instances; i++) {
        if (!sig->local->instances[i]->is_active)
            ++j;
    }
    return j;
}

mapper_id mapper_signal_instance_id(mapper_signal sig, int index)
{
    if (!sig || !sig->local)
        return 0;
    int i;
    for (i = 0; i < sig->num_instances; i++) {
        if (i == index)
            return sig->local->instances[i]->id;
    }
    return 0;
}

mapper_id mapper_signal_active_instance_id(mapper_signal sig, int index)
{
    if (!sig || !sig->local)
        return 0;
    int i, j = -1;
    for (i = 0; i < sig->local->id_map_length; i++) {
        if (sig->local->id_maps[i].instance)
            ++j;
        if (j == index)
            return sig->local->id_maps[i].map->local;
    }
    return 0;
}

mapper_id mapper_signal_reserved_instance_id(mapper_signal sig, int index)
{
    if (!sig || !sig->local)
        return 0;
    int i, j = -1;
    for (i = 0; i < sig->num_instances; i++) {
        if (!sig->local->instances[i]->is_active)
            ++j;
        if (j == index)
            return sig->local->instances[i]->id;
    }
    return 0;
}

void mapper_signal_set_instance_stealing_mode(mapper_signal sig,
                                              mapper_instance_stealing_type mode)
{
    if (sig && sig->local)
        sig->local->instance_stealing_mode = mode;
}

mapper_instance_stealing_type mapper_signal_instance_stealing_mode(mapper_signal sig)
{
    if (sig && sig->local)
        return sig->local->instance_stealing_mode;
    return 0;
}

void mapper_signal_set_instance_event_callback(mapper_signal sig,
                                               mapper_instance_event_handler h,
                                               int flags)
{
    if (!sig || !sig->local)
        return;

    if (!h || !flags) {
        sig->local->instance_event_handler = 0;
        sig->local->instance_event_flags = 0;
        return;
    }

    sig->local->instance_event_handler = h;
    sig->local->instance_event_flags = flags;
}

void mapper_signal_set_user_data(mapper_signal sig, const void *user_data)
{
    if (sig)
        sig->user_data = (void*)user_data;
}

void *mapper_signal_user_data(mapper_signal sig)
{
    return sig ? sig->user_data : 0;
}

int mapper_signal_instance_activate(mapper_signal sig, mapper_id id)
{
    if (!sig || !sig->local)
        return 0;
    int index = mapper_signal_instance_with_local_id(sig, id, 0, 0);
    return index >= 0;
}

void mapper_signal_instance_set_user_data(mapper_signal sig, mapper_id id,
                                          const void *user_data)
{
    if (!sig || !sig->local)
        return;
    mapper_signal_instance si = find_instance_by_id(sig, id);
    if (si)
        si->user_data = (void*)user_data;
}

void *mapper_signal_instance_user_data(mapper_signal sig, mapper_id id)
{
    if (!sig || !sig->local)
        return 0;
    mapper_signal_instance si = find_instance_by_id(sig, id);
    if (si)
        return si->user_data;
    return 0;
}

/**** Queries ****/

void mapper_signal_set_callback(mapper_signal sig,
                                mapper_signal_update_handler *handler)
{
    if (!sig || !sig->local)
        return;
    if (!sig->local->update_handler && handler) {
        // Need to register a new liblo methods
        sig->local->update_handler = handler;
        mapper_device_add_signal_methods(sig->device, sig);
    }
    else if (sig->local->update_handler && !handler) {
        // Need to remove liblo methods
        sig->local->update_handler = 0;
        mapper_device_remove_signal_methods(sig->device, sig);
    }
}

int mapper_signal_query_remotes(mapper_signal sig, mapper_timetag_t tt)
{
    if (!sig || !sig->local)
        return -1;
    if (!sig->local->update_handler) {
        // no handler defined so we cannot process query responses
        return -1;
    }

    mapper_timetag_t *ttp;
    if (memcmp(&tt, &MAPPER_NOW, sizeof(mapper_timetag_t))==0) {
        ttp = &sig->device->database->network->clock.now;
        mapper_device_now(sig->device, ttp);
    }
    else
        ttp = &tt;
    return mapper_device_route_query(sig->device, sig, *ttp);
}

/**** Signal Properties ****/

// Internal function only
int mapper_signal_full_name(mapper_signal sig, char *name, int len)
{
    const char *dev_name = mapper_device_name(sig->device);
    if (!dev_name)
        return 0;

    int dev_name_len = strlen(dev_name);
    if (dev_name_len >= len)
        return 0;
    if ((dev_name_len + strlen(sig->name) + 1) > len)
        return 0;

    snprintf(name, len, "%s%s", dev_name, sig->path);
    return strlen(name);
}

mapper_device mapper_signal_device(mapper_signal sig)
{
    return sig->device;
}

const char *mapper_signal_description(mapper_signal sig)
{
    mapper_table_record_t *rec = mapper_table_record(sig->props, AT_DESCRIPTION, 0);
    if (rec && rec->type == 's')
        return (const char*)rec->value;
    return 0;
}

mapper_direction mapper_signal_direction(mapper_signal sig)
{
    return sig->direction;
}

mapper_id mapper_signal_id(mapper_signal sig)
{
    return sig->id;
}

int mapper_signal_is_local(mapper_signal sig)
{
    return sig && sig->local;
}

int mapper_signal_length(mapper_signal sig)
{
    return sig->length;
}

void *mapper_signal_maximum(mapper_signal sig)
{
    return sig->maximum;
}

void *mapper_signal_minimum(mapper_signal sig)
{
    return sig->minimum;
}

const char *mapper_signal_name(mapper_signal sig)
{
    return sig->name;
}

int mapper_signal_num_maps(mapper_signal sig, mapper_direction dir)
{
    if (!sig)
        return 0;
    if (!dir)
        return sig->num_incoming_maps + sig->num_outgoing_maps;
    return (  ((dir & MAPPER_DIR_INCOMING) ? sig->num_incoming_maps : 0)
            + ((dir & MAPPER_DIR_OUTGOING) ? sig->num_outgoing_maps : 0));
}

static int cmp_query_signal_maps(const void *context_data, mapper_map map)
{
    mapper_signal sig = *(mapper_signal *)context_data;
    int direction = *(int*)(context_data + sizeof(int64_t));
    if (!direction || (direction & MAPPER_DIR_OUTGOING)) {
        int i;
        for (i = 0; i < map->num_sources; i++) {
            if (map->sources[i]->signal == sig)
            return 1;
        }
    }
    if (!direction || (direction & MAPPER_DIR_INCOMING)) {
        if (map->destination.signal == sig)
        return 1;
    }
    return 0;
}

mapper_map *mapper_signal_maps(mapper_signal sig, mapper_direction dir)
{
    if (!sig || !sig->device->database->maps)
        return 0;
    return ((mapper_map *)
            mapper_list_new_query(sig->device->database->maps,
                                  cmp_query_signal_maps, "vi", &sig, dir));
}

float mapper_signal_rate(mapper_signal sig)
{
    return sig->rate;
}

char mapper_signal_type(mapper_signal sig)
{
    return sig->type;
}

const char *mapper_signal_unit(mapper_signal sig)
{
    return sig->unit;
}

int mapper_signal_num_properties(mapper_signal sig) {
    return mapper_table_num_records(sig->props);
}

int mapper_signal_property(mapper_signal sig, const char *name, int *length,
                           char *type, const void **value)
{
    return mapper_table_property(sig->props, name, length, type, value);
}

int mapper_signal_property_index(mapper_signal sig, unsigned int index,
                                 const char **name, int *length, char *type,
                                 const void **value)
{
    return mapper_table_property_index(sig->props, index, name, length, type,
                                       value);
}

void mapper_signal_set_description(mapper_signal sig, const char *description)
{
    if (!sig || !sig->local)
        return;
    mapper_table_set_record(sig->props, AT_DESCRIPTION, NULL, 1, 's',
                            description, LOCAL_MODIFY);
}

void mapper_signal_set_maximum(mapper_signal sig, const void *maximum)
{
    if (!sig || !sig->local)
        return;
    mapper_table_set_record(sig->props, AT_MAX, NULL, sig->length, sig->type,
                            maximum, LOCAL_MODIFY);
}

void mapper_signal_set_minimum(mapper_signal sig, const void *minimum)
{
    if (!sig || !sig->local)
        return;
    mapper_table_set_record(sig->props, AT_MIN, NULL, sig->length, sig->type,
                            minimum, LOCAL_MODIFY);
}

void mapper_signal_set_rate(mapper_signal sig, float rate)
{
    if (!sig || !sig->local)
        return;
    mapper_table_set_record(sig->props, AT_RATE, NULL, 1, 'f', &rate,
                            LOCAL_MODIFY);
}

void mapper_signal_set_unit(mapper_signal sig, const char *unit)
{
    if (!sig || !sig->local)
        return;
    mapper_table_set_record(sig->props, AT_UNIT, NULL, 1, 's', unit,
                            LOCAL_MODIFY);
}

int mapper_signal_set_property(mapper_signal sig, const char *name, int length,
                               char type, const void *value)
{
    mapper_property_t prop = mapper_property_from_string(name);
    if (prop == AT_USER_DATA) {
        if (sig->user_data != (void*)value) {
            sig->user_data = (void*)value;
            return 1;
        }
    }
    else if ((prop != AT_EXTRA) && !sig->local)
        return 0;
    else
        return mapper_table_set_record(sig->local ? sig->props : sig->staged_props,
                                       prop, name, length, type, value,
                                       sig->local ? LOCAL_MODIFY : REMOTE_MODIFY);
    return 0;
}

int mapper_signal_remove_property(mapper_signal sig, const char *name)
{
    mapper_property_t prop = mapper_property_from_string(name);
    if (prop == AT_USER_DATA) {
        if (sig->user_data) {
            sig->user_data = 0;
            return 1;
        }
    }
    else if (sig->local)
        return mapper_table_remove_record(sig->props, prop, name, 1,
                                          LOCAL_MODIFY);
    else if (prop == AT_EXTRA)
        return mapper_table_set_record(sig->staged_props, prop, name, 0, 0,
                                       NULL, REMOTE_MODIFY);
    return 0;
}

static int mapper_signal_add_id_map(mapper_signal sig, mapper_signal_instance si,
                                    mapper_id_map map)
{
    // find unused signal map
    int i;

    for (i = 0; i < sig->local->id_map_length; i++) {
        if (!sig->local->id_maps[i].map)
            break;
    }
    if (i == sig->local->id_map_length) {
        // need more memory
        if (sig->local->id_map_length >= MAX_INSTANCES) {
            // Arbitrary limit to number of tracked id_maps
            return -1;
        }
        sig->local->id_map_length *= 2;
        sig->local->id_maps = realloc(sig->local->id_maps,
                                      sig->local->id_map_length *
                                      sizeof(struct _mapper_signal_id_map));
        memset(sig->local->id_maps + i, 0,
               (sig->local->id_map_length - i)
               * sizeof(struct _mapper_signal_id_map));
    }
    sig->local->id_maps[i].map = map;
    sig->local->id_maps[i].instance = si;
    sig->local->id_maps[i].status = 0;

    return i;
}

void message_add_coerced_signal_instance_value(lo_message m,
                                               mapper_signal sig,
                                               mapper_signal_instance si,
                                               int length,
                                               const char type)
{
    int i;
    int min_length = length < sig->length ? length : sig->length;

    switch (sig->type) {
        case 'i': {
            int *v = (int*)si->value;
            for (i = 0; i < min_length; i++) {
                if (!si->has_value && !(si->has_value_flags[i/8] & 1 << (i % 8)))
                    lo_message_add_nil(m);
                else if (type == 'i')
                    lo_message_add_int32(m, v[i]);
                else if (type == 'f')
                    lo_message_add_float(m, (float)v[i]);
                else if (type == 'd')
                    lo_message_add_double(m, (double)v[i]);
            }
            break;
        }
        case 'f': {
            float *v = (float*)si->value;
            for (i = 0; i < min_length; i++) {
                if (!si->has_value && !(si->has_value_flags[i/8] & 1 << (i % 8)))
                    lo_message_add_nil(m);
                else if (type == 'f')
                    lo_message_add_float(m, v[i]);
                else if (type == 'i')
                    lo_message_add_int32(m, (int)v[i]);
                else if (type == 'd')
                    lo_message_add_double(m, (double)v[i]);
            }
            break;
        }
        case 'd': {
            double *v = (double*)si->value;
            for (i = 0; i < min_length; i++) {
                if (!si->has_value && !(si->has_value_flags[i/8] & 1 << (i % 8)))
                    lo_message_add_nil(m);
                else if (type == 'd')
                    lo_message_add_double(m, (int)v[i]);
                else if (type == 'i')
                    lo_message_add_int32(m, (int)v[i]);
                else if (type == 'f')
                    lo_message_add_float(m, (float)v[i]);
            }
            break;
        }
    }
    for (i = min_length; i < length; i++)
        lo_message_add_nil(m);
}

void mapper_signal_send_state(mapper_signal sig, network_message_t cmd)
{
    if (!sig)
        return;
    lo_message msg = lo_message_new();
    if (!msg) {
        trace("couldn't allocate lo_message\n");
        return;
    }

    /* signal name */
    char sig_name[1024];
    mapper_signal_full_name(sig, sig_name, 1024);
    lo_message_add_string(msg, sig_name);

    /* properties */
    mapper_table_add_to_message(cmd == MSG_SIGNAL
                                ? sig->props : sig->staged_props, msg);

    mapper_network_add_message(sig->device->database->network, 0, cmd, msg);
}

void mapper_signal_send_removed(mapper_signal sig)
{
    lo_message msg = lo_message_new();
    if (!msg) {
        trace("couldn't allocate lo_message\n");
        return;
    }
    char sig_name[1024];
    mapper_signal_full_name(sig, sig_name, 1024);
    lo_message_add_string(msg, sig_name);
    mapper_network_add_message(sig->device->database->network, 0,
                               MSG_SIGNAL_REMOVED, msg);
}

mapper_signal *mapper_signal_query_union(mapper_signal *query1,
                                         mapper_signal *query2)
{
    return (mapper_signal*)mapper_list_query_union((void**)query1,
                                                   (void**)query2);
}

mapper_signal *mapper_signal_query_intersection(mapper_signal *query1,
                                                mapper_signal *query2)
{
    return (mapper_signal*)mapper_list_query_intersection((void**)query1,
                                                          (void**)query2);
}

mapper_signal *mapper_signal_query_difference(mapper_signal *query1,
                                              mapper_signal *query2)
{
    return (mapper_signal*)mapper_list_query_difference((void**)query1,
                                                        (void**)query2);
}

mapper_signal mapper_signal_query_index(mapper_signal *sig, int index)
{
    return (mapper_signal)mapper_list_query_index((void**)sig, index);
}

mapper_signal *mapper_signal_query_next(mapper_signal *sig)
{
    return (mapper_signal*)mapper_list_query_next((void**)sig);
}

mapper_signal *mapper_signal_query_copy(mapper_signal *sig)
{
    return (mapper_signal*)mapper_list_query_copy((void**)sig);
}

void mapper_signal_query_done(mapper_signal *sig)
{
    mapper_list_query_done((void**)sig);
}

/*! Update information about a signal record based on message properties. */
int mapper_signal_set_from_message(mapper_signal sig, mapper_message_t *msg)
{
    mapper_message_atom atom;
    int i, updated = 0, len_type_diff = 0;

    if (!msg)
        return updated;

    for (i = 0; i < msg->num_atoms; i++) {
        atom = &msg->atoms[i];
        if (sig->local && atom->index != AT_EXTRA)
            continue;
        switch (atom->index) {
            case AT_DIRECTION: {
                int dir = 0;
                if (strcmp(&(*atom->values)->s, "output")==0)
                    dir = MAPPER_DIR_OUTGOING;
                else if (strcmp(&(*atom->values)->s, "input")==0)
                    dir = MAPPER_DIR_INCOMING;
                else
                    break;
                if (dir & MAPPER_DIR_INCOMING) {
                    if (!(sig->direction & MAPPER_DIR_INCOMING))
                        ++sig->device->num_inputs;
                }
                else if (sig->direction & MAPPER_DIR_INCOMING)
                    --sig->device->num_inputs;
                if (dir & MAPPER_DIR_OUTGOING) {
                    if (!(sig->direction & MAPPER_DIR_OUTGOING))
                        ++sig->device->num_outputs;
                }
                else if (sig->direction & MAPPER_DIR_OUTGOING)
                    --sig->device->num_outputs;
                mapper_table_set_record(sig->props, AT_DIRECTION, NULL, 1, 'i',
                                        &dir, REMOTE_MODIFY);
                break;
            }
            case AT_LENGTH:
            case AT_TYPE:
                len_type_diff += mapper_table_set_record_from_atom(sig->props,
                                                                   atom,
                                                                   REMOTE_MODIFY);
                break;
            default:
                updated += mapper_table_set_record_from_atom(sig->props, atom,
                                                             REMOTE_MODIFY);
                break;
        }
    }
    if (len_type_diff) {
        // may need to upgrade extrema props for associated map slots
        mapper_map *maps = mapper_signal_maps(sig, MAPPER_DIR_ANY);
        mapper_slot slot;
        while (maps) {
            slot = mapper_map_slot_by_signal(*maps, sig);
            mapper_slot_upgrade_extrema_memory(slot);
            maps = mapper_map_query_next(maps);
        }
    }
    return updated + len_type_diff;
}

void mapper_signal_print(mapper_signal sig, int include_device_name)
{
    if (include_device_name)
        printf("%s:%s, direction=", sig->device->name, sig->name);
    else
        printf("%s, direction=", sig->name);
    switch (sig->direction) {
        case MAPPER_DIR_OUTGOING:
            printf("output");
            break;
        case MAPPER_DIR_INCOMING:
            printf("input");
            break;
        default:
            printf("unknown");
            break;
    }

    int i=0;
    const char *key;
    char type;
    const void *val;
    int length;
    while(!mapper_signal_property_index(sig, i++, &key, &length, &type, &val)) {
        die_unless(val!=0, "returned zero value\n");

        // already printed these
        if (strcmp(key, "name")==0 || strcmp(key, "direction")==0)
            continue;

        if (length) {
            printf(", %s=", key);
            mapper_property_print(length, type, val);
        }
    }
    printf("\n");
}
