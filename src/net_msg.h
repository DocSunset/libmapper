#ifndef NET_MSG_H_INCLUDED
#define NET_MSG_H_INCLUDED

/*! Some useful strings for sending administrative messages. */
typedef enum {
    MSG_DEV,
    MSG_DEV_MOD,
    MSG_LOGOUT,
    MSG_MAP,
    MSG_MAP_TO,
    MSG_MAPPED,
    MSG_MAP_MOD,
    MSG_NAME_PROBE,
    MSG_NAME_REG,
    MSG_PING,
    MSG_SIG,
    MSG_SIG_REM,
    MSG_SIG_MOD,
    MSG_SUBSCRIBE,
    MSG_SYNC,
    MSG_UNMAP,
    MSG_UNMAPPED,
    MSG_WHO,
    MSG_DATA_SIG,
    MSG_DATA_MAP,
    MSG_DATA_MAP_TO,
    MSG_DATA_MAPPED,
    MSG_DATA_MAP_MOD,
    MSG_DATA_UNMAP,
    MSG_DATA_UNMAPPED,
    NUM_MSG_STRINGS
} net_msg_t;

#endif // NET_MSG_H_INCLUDED
