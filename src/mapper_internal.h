
#ifndef __MAPPER_INTERNAL_H__
#define __MAPPER_INTERNAL_H__

#include "types_internal.h"
#include "util/mpr_inline.h"
#include <string.h>

#ifdef _MSC_VER
#include <malloc.h>
#endif

/* Structs that refer to things defined in mapper.h are declared here instead
   of in types_internal.h */

#include "util/func_if.h"
#define PROP(NAME) MPR_PROP_##NAME

#include "util/debug_macro.h"

/**** Subscriptions ****/
#ifdef DEBUG
void print_subscription_flags(int flags);
#endif

/**** Objects ****/
#include "object.h"

/* DATATODO: change this to something else, e.g. `l` */
#define MPR_LINK 0x20

/**** Networking ****/

#include "network.h"

#define NEW_LO_MSG(VARNAME, FAIL)                   \
lo_message VARNAME = lo_message_new();              \
if (!VARNAME) {                                     \
    trace_net("couldn't allocate lo_message\n");    \
    FAIL;                                           \
}

#include "device.h"

#include "graph.h"

/***** Router *****/

void mpr_rtr_remove_sig(mpr_rtr r, mpr_rtr_sig rs);

void mpr_rtr_num_inst_changed(mpr_rtr r, mpr_local_sig sig, int size);

void mpr_rtr_remove_inst(mpr_rtr rtr, mpr_local_sig sig, int idx);

/*! For a given signal instance, calculate mapping outputs and forward to
 *  destinations. */
void mpr_rtr_process_sig(mpr_rtr rtr, mpr_local_sig sig, int inst_idx, const void *val, mpr_time t);

void mpr_rtr_add_map(mpr_rtr rtr, mpr_local_map map);

void mpr_rtr_remove_link(mpr_rtr rtr, mpr_link lnk);

int mpr_rtr_remove_map(mpr_rtr rtr, mpr_local_map map);

mpr_local_slot mpr_rtr_get_slot(mpr_rtr rtr, mpr_local_sig sig, int slot_num);

int mpr_rtr_loop_check(mpr_rtr rtr, mpr_local_sig sig, int n_remote, const char **remote);

/**** Signals ****/

#define MPR_MAX_VECTOR_LEN 128

/*! Initialize an already-allocated mpr_sig structure. */
void mpr_sig_init(mpr_sig s, mpr_dir dir, const char *name, int len,
                  mpr_type type, const char *unit, const void *min,
                  const void *max, int *num_inst);

/*! Get the full OSC name of a signal, including device name prefix.
 *  \param sig  The signal value to query.
 *  \param name A string to accept the name.
 *  \param len  The length of string pointed to by name.
 *  \return     The number of characters used, or 0 if error.  Note that in some
 *              cases the name may not be available. */
int mpr_sig_get_full_name(mpr_sig sig, char *name, int len);

void mpr_sig_call_handler(mpr_local_sig sig, int evt, mpr_id inst, int len,
                          const void *val, mpr_time *time, float diff);

int mpr_sig_set_from_msg(mpr_sig sig, mpr_msg msg);

void mpr_sig_update_timing_stats(mpr_local_sig sig, float diff);

/*! Free memory used by a mpr_sig. Call this only for signals that are not
 *  registered with a device. Registered signals will be freed by mpr_sig_free().
 *  \param s        The signal to free. */
void mpr_sig_free_internal(mpr_sig sig);

void mpr_sig_send_state(mpr_sig sig, net_msg_t cmd);

void mpr_sig_send_removed(mpr_local_sig sig);

/**** Instances ****/

/*! Fetch a reserved (preallocated) signal instance using an instance id,
 *  activating it if necessary.
 *  \param s        The signal owning the desired instance.
 *  \param LID      The requested signal instance id.
 *  \param flags    Bitflags indicating if search should include released
 *                  instances.
 *  \param t        Time associated with this action.
 *  \param activate Set to 1 to activate a reserved instance if necessary.
 *  \return         The index of the retrieved instance id map, or -1 if no free
 *                  instances were available and allocation of a new instance
 *                  was unsuccessful according to the selected allocation
 *                  strategy. */
int mpr_sig_get_idmap_with_LID(mpr_local_sig sig, mpr_id LID, int flags, mpr_time t, int activate);

/*! Fetch a reserved (preallocated) signal instance using instance id map,
 *  activating it if necessary.
 *  \param s        The signal owning the desired instance.
 *  \param GID      Globally unique id of this instance.
 *  \param flags    Bitflags indicating if search should include released instances.
 *  \param t        Time associated with this action.
 *  \param activate Set to 1 to activate a reserved instance if necessary.
 *  \return         The index of the retrieved instance id map, or -1 if no free
 *                  instances were available and allocation of a new instance
 *                  was unsuccessful according to the selected allocation
 *                  strategy. */
int mpr_sig_get_idmap_with_GID(mpr_local_sig sig, mpr_id GID, int flags, mpr_time t, int activate);

/*! Release a specific signal instance. */
void mpr_sig_release_inst_internal(mpr_local_sig sig, int inst_idx);

/**** Links ****/

mpr_link mpr_link_new(mpr_local_dev local_dev, mpr_dev remote_dev);

/*! Return the list of maps associated with a given link.
 *  \param link         The link to check.
 *  \return             The list of results.  Use mpr_list_next() to iterate. */
mpr_list mpr_link_get_maps(mpr_link link);

void mpr_link_add_map(mpr_link link, int is_src);

void mpr_link_remove_map(mpr_link link, mpr_local_map rem);

void mpr_link_init(mpr_link link);
void mpr_link_connect(mpr_link link, const char *host, int admin_port,
                      int data_port);
void mpr_link_free(mpr_link link);
int mpr_link_process_bundles(mpr_link link, mpr_time t, int idx);
void mpr_link_add_msg(mpr_link link, mpr_sig dst, lo_message msg, mpr_time t, mpr_proto proto, int idx);

int mpr_link_get_is_local(mpr_link link);

/**** Maps ****/

void mpr_map_alloc_values(mpr_local_map map);

/*! Process the signal instance value according to mapping properties.
 *  The result of this operation should be sent to the destination.
 *  \param map          The mapping process to perform.
 *  \param time         Timestamp for this update. */
void mpr_map_send(mpr_local_map map, mpr_time time);

void mpr_map_receive(mpr_local_map map, mpr_time time);

lo_message mpr_map_build_msg(mpr_local_map map, mpr_local_slot slot, const void *val,
                             mpr_type *types, mpr_id_map idmap);

/*! Set a mapping's properties based on message parameters. */
int mpr_map_set_from_msg(mpr_map map, mpr_msg msg, int override);

const char *mpr_loc_as_str(mpr_loc loc);
mpr_loc mpr_loc_from_str(const char *string);

const char *mpr_protocol_as_str(mpr_proto pro);
mpr_proto mpr_protocol_from_str(const char *string);

const char *mpr_steal_as_str(mpr_steal_type stl);

int mpr_map_send_state(mpr_map map, int slot, net_msg_t cmd);

void mpr_map_init(mpr_map map);

void mpr_map_free(mpr_map map);

/**** Slot ****/

mpr_slot mpr_slot_new(mpr_map map, mpr_sig sig, unsigned char is_local, unsigned char is_src);

void mpr_slot_alloc_values(mpr_local_slot slot, int num_inst, int hist_size);

void mpr_slot_free(mpr_slot slot);

void mpr_slot_free_value(mpr_local_slot slot);

int mpr_slot_set_from_msg(mpr_slot slot, mpr_msg msg);

void mpr_slot_add_props_to_msg(lo_message msg, mpr_slot slot, int is_dest);

void mpr_slot_print(mpr_slot slot, int is_dest);

int mpr_slot_match_full_name(mpr_slot slot, const char *full_name);

void mpr_slot_remove_inst(mpr_local_slot slot, int idx);

/**** Messages ****/
/*! Parse the device and signal names from an OSC path. */
int mpr_parse_names(const char *string, char **devnameptr, char **signameptr);

/*! Parse a message based on an OSC path and named properties.
 *  \param argc     Number of arguments in the argv array.
 *  \param types    String containing message parameter types.
 *  \param argv     Vector of lo_arg structures.
 *  \return         A mpr_msg structure. Free when done using mpr_msg_free. */
mpr_msg mpr_msg_parse_props(int argc, const mpr_type *types, lo_arg **argv);

void mpr_msg_free(mpr_msg msg);

/*! Look up the value of a message parameter by symbolic identifier.
 *  \param msg      Structure containing parameter info.
 *  \param prop     Symbolic identifier of the property to look for.
 *  \return         Pointer to mpr_msg_atom, or zero if not found. */
mpr_msg_atom mpr_msg_get_prop(mpr_msg msg, int prop);

void mpr_msg_add_typed_val(lo_message msg, int len, mpr_type type, const void *val);

/*! Prepare a lo_message for sending based on a map struct. */
const char *mpr_map_prepare_msg(mpr_map map, lo_message msg, int slot_idx);

/*! Helper for setting property value from different lo_arg types. */
int set_coerced_val(int src_len, mpr_type src_type, const void *src_val,
                    int dst_len, mpr_type dst_type, void *dst_val);

/**** Expression parser/evaluator ****/

mpr_expr mpr_expr_new_from_str(mpr_expr_stack eval_stk, const char *str, int num_in,
                               const mpr_type *in_types, const int *in_vec_lens, mpr_type out_type,
                               int out_vec_len);

int mpr_expr_get_in_hist_size(mpr_expr expr, int idx);

int mpr_expr_get_out_hist_size(mpr_expr expr);

int mpr_expr_get_num_vars(mpr_expr expr);

int mpr_expr_get_var_vec_len(mpr_expr expr, int idx);

int mpr_expr_get_var_type(mpr_expr expr, int idx);

int mpr_expr_get_var_is_instanced(mpr_expr expr, int idx);

int mpr_expr_get_src_is_muted(mpr_expr expr, int idx);

const char *mpr_expr_get_var_name(mpr_expr expr, int idx);

int mpr_expr_get_manages_inst(mpr_expr expr);

void mpr_expr_var_updated(mpr_expr expr, int var_idx);

#ifdef DEBUG
void printexpr(const char*, mpr_expr);
#endif

/*! Evaluate the given inputs using the compiled expression.
 *  \param stk          A preallocated expression eval stack.
 *  \param expr         The expression to use.
 *  \param srcs         An array of mpr_value structures for sources.
 *  \param expr_vars    An array of mpr_value structures for user variables.
 *  \param result       A mpr_value structure for the destination.
 *  \param t            A pointer to a timetag structure for storing the time
 *                      associated with the result.
 *  \param types        An array of mpr_type for storing the output type per
 *                      vector element
 *  \param inst_idx     Index of the instance being updated.
 *  \result             0 if the expression evaluation caused no change, or a
 *                      bitwise OR of MPR_SIG_UPDATE (if an update was
 *                      generated), MPR_SIG_REL_UPSTRM (if the expression
 *                      generated an instance release before the update), and
 *                      MPR_SIG_REL_DNSRTM (if the expression generated an
 *                      instance release after an update). */
int mpr_expr_eval(mpr_expr_stack stk, mpr_expr expr, mpr_value *srcs, mpr_value *expr_vars,
                  mpr_value result, mpr_time *t, mpr_type *types, int inst_idx);

int mpr_expr_get_num_input_slots(mpr_expr expr);

void mpr_expr_free(mpr_expr expr);

mpr_expr_stack mpr_expr_stack_new();
void mpr_expr_stack_free(mpr_expr_stack stk);

/**** String tables ****/

#include "table.h"

/**** Lists ****/

void *mpr_list_from_data(const void *data);

void *mpr_list_add_item(void **list, size_t size);

void mpr_list_remove_item(void **list, void *item);

void mpr_list_free_item(void *item);

mpr_list mpr_list_new_query(const void **list, const void *func,
                            const mpr_type *types, ...);

mpr_list mpr_list_start(mpr_list list);

/**** Time ****/

#include "mpr_time.h"

/**** Properties ****/

/*! Helper for printing typed values.
 *  \param len          The vector length of the value.
 *  \param type         The value type.
 *  \param val          A pointer to the property value to print. */
void mpr_prop_print(int len, mpr_type type, const void *val);

mpr_prop mpr_prop_from_str(const char *str);

const char *mpr_prop_as_str(mpr_prop prop, int skip_slash);

#include "util/mpr_type_get_size.h"

/**** Values ****/

void mpr_value_realloc(mpr_value val, unsigned int vec_len, mpr_type type,
                       unsigned int mem_len, unsigned int num_inst, int is_output);

void mpr_value_reset_inst(mpr_value v, int idx);

int mpr_value_remove_inst(mpr_value v, int idx);

void mpr_value_set_samp(mpr_value v, int idx, void *s, mpr_time t);

/*! Helper to find the pointer to the current value in a mpr_value_t. */
MPR_INLINE static void* mpr_value_get_samp(mpr_value v, int idx)
{
    mpr_value_buffer b = &v->inst[idx % v->num_inst];
    return (char*)b->samps + b->pos * v->vlen * mpr_type_get_size(v->type);
}

MPR_INLINE static void* mpr_value_get_samp_hist(mpr_value v, int inst_idx, int hist_idx)
{
    mpr_value_buffer b = &v->inst[inst_idx % v->num_inst];
    int idx = (b->pos + v->mlen + hist_idx) % v->mlen;
    if (idx < 0)
        idx += v->mlen;
    return (char*)b->samps + idx * v->vlen * mpr_type_get_size(v->type);
}

/*! Helper to find the pointer to the current time in a mpr_value_t. */
MPR_INLINE static mpr_time* mpr_value_get_time(mpr_value v, int idx)
{
    mpr_value_buffer b = &v->inst[idx % v->num_inst];
    return &b->times[b->pos];
}

MPR_INLINE static mpr_time* mpr_value_get_time_hist(mpr_value v, int inst_idx, int hist_idx)
{
    mpr_value_buffer b = &v->inst[inst_idx % v->num_inst];
    int idx = (b->pos + v->mlen + hist_idx) % v->mlen;
    if (idx < 0)
        idx += v->mlen;
    return &b->times[idx];
}

void mpr_value_free(mpr_value v);

#ifdef DEBUG
void mpr_value_print(mpr_value v, int inst_idx);
void mpr_value_print_hist(mpr_value v, int inst_idx);
#endif

/*! Helper to find the size in bytes of a signal's full vector. */
MPR_INLINE static size_t mpr_sig_get_vector_bytes(mpr_sig sig)
{
    return mpr_type_get_size(sig->type) * sig->len;
}

/*! Helper to check if a type character is valid. */
MPR_INLINE static int check_sig_length(int length)
{
    return (length < 1 || length > MPR_MAX_VECTOR_LEN);
}

/*! Helper to check if bitfields match completely. */
MPR_INLINE static int bitmatch(unsigned int a, unsigned int b)
{
    return (a & b) == b;
}

/*! Helper to check if type is a number. */
MPR_INLINE static int mpr_type_get_is_num(mpr_type type)
{
    switch (type) {
        case MPR_INT32:
        case MPR_FLT:
        case MPR_DBL:
            return 1;
        default:    return 0;
    }
}

/*! Helper to check if type is a boolean. */
MPR_INLINE static int mpr_type_get_is_bool(mpr_type type)
{
    return 'T' == type || 'F' == type;
}

/*! Helper to check if type is a string. */
MPR_INLINE static int mpr_type_get_is_str(mpr_type type)
{
    return MPR_STR == type;
}

/*! Helper to check if type is a string or void* */
MPR_INLINE static int mpr_type_get_is_ptr(mpr_type type)
{
    return MPR_PTR == type || MPR_STR == type;
}

/*! Helper to check if data type matches, but allowing 'T' and 'F' for bool. */
MPR_INLINE static int type_match(const mpr_type l, const mpr_type r)
{
    return (l == r) || (strchr("bTF", l) && strchr("bTF", r));
}

#include "util/skip_slash.h"

MPR_INLINE static void set_bitflag(char *bytearray, int idx)
{
    bytearray[idx / 8] |= 1 << (idx % 8);
}

MPR_INLINE static int get_bitflag(char *bytearray, int idx)
{
    return bytearray[idx / 8] & 1 << (idx % 8);
}

MPR_INLINE static int compare_bitflags(char *l, char *r, int num_flags)
{
    return memcmp(l, r, num_flags / 8 + 1);
}

MPR_INLINE static void clear_bitflags(char *bytearray, int num_flags)
{
    memset(bytearray, 0, num_flags / 8 + 1);
}

#include <mapper/mapper.h>

#endif /* __MAPPER_INTERNAL_H__ */
