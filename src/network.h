#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include "types_internal.h"

void mpr_net_add_dev(mpr_net n, mpr_local_dev d);

void mpr_net_remove_dev(mpr_net n, mpr_local_dev d);

void mpr_net_poll(mpr_net n);

void mpr_net_init(mpr_net n, const char *iface, const char *group, int port);

void mpr_net_use_bus(mpr_net n);

void mpr_net_use_mesh(mpr_net n, lo_address addr);

void mpr_net_use_subscribers(mpr_net net, mpr_local_dev dev, int type);

void mpr_net_add_msg(mpr_net n, const char *str, net_msg_t cmd, lo_message msg);

void mpr_net_handle_map(mpr_net net, mpr_local_map map, mpr_msg props);

void mpr_net_send(mpr_net n);

void mpr_net_free_msgs(mpr_net n);

void mpr_net_free(mpr_net n);

#endif // NETWORK_H_INCLUDED

