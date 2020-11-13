//
// Created by leoll2 on 11/13/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#include <unistd.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_mempool.h>

#include "udpdk_sync.h"

#define WAIT_MAX_CYCLES  100
extern struct rte_ring *ipc_app_to_pol;
extern struct rte_ring *ipc_pol_to_app;
extern struct rte_mempool *ipc_msg_pool;


/* Initialize IPC channel for the synchonization between app and poller processes */
int init_ipc_channel(void)
{
    ipc_app_to_pol = rte_ring_create("IPC_channel_app_to_pol", 1, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (ipc_app_to_pol == NULL) {
        return -1;
    }
    ipc_pol_to_app = rte_ring_create("IPC_channel_pol_to_app", 1, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (ipc_pol_to_app == NULL) {
        return -1;
    }
    ipc_msg_pool = rte_mempool_create("IPC_msg_pool", 5, 64, 0, 0, NULL, NULL, NULL, NULL, rte_socket_id(), 0);
    return 0;
}

/* Retrieve IPC channel for the synchonization between app and poller processes */
int retrieve_ipc_channel(void)
{
    ipc_app_to_pol = rte_ring_lookup("IPC_channel_app_to_pol");
    if (ipc_app_to_pol == NULL) {
        return -1;
    }
    ipc_pol_to_app = rte_ring_lookup("IPC_channel_pol_to_app");
    if (ipc_pol_to_app == NULL) {
        return -1;
    }
    ipc_msg_pool = rte_mempool_lookup("IPC_msg_pool");
    if (ipc_msg_pool == NULL) {
        return -1;
    }
    return 0;
}

/* Wait for a signal from the application to the poller */
int ipc_wait_for_app(void)
{
    void *sync_msg;
    int c = 0;

    while (rte_ring_dequeue(ipc_app_to_pol, &sync_msg) < 0) {
        usleep(50000);
        c++;
        if (c > WAIT_MAX_CYCLES) {
            return -1;
        }
    }
    rte_mempool_put(ipc_msg_pool, sync_msg);
    return 0;
}

/* Wait for a signal from the poller to the application */
int ipc_wait_for_poller(void)
{
    void *sync_msg;
    int c = 0;

    while (rte_ring_dequeue(ipc_pol_to_app, &sync_msg) < 0) {
        usleep(50000);
        c++;
        if (c > WAIT_MAX_CYCLES) {
            return -1;
        }
    }
    rte_mempool_put(ipc_msg_pool, sync_msg);
    return 0;
}

/* Send a notification signal from the poller to the application */
void ipc_notify_to_app(void)
{
    void *sync_msg;

    if (rte_mempool_get(ipc_msg_pool, &sync_msg) < 0) {
        rte_panic("Failed to get a sync message from the pool\n");
    }
    // NOTE: no need to write anything in the message
    if (rte_ring_enqueue(ipc_pol_to_app, sync_msg) < 0) {
        rte_mempool_put(ipc_msg_pool, sync_msg);
    }
}

/* Send a notification signal from the application to the poller */
void ipc_notify_to_poller(void)
{
    void *sync_msg;

    if (rte_mempool_get(ipc_msg_pool, &sync_msg) < 0) {
        rte_panic("Failed to get a sync message from the pool\n");
    }
    // NOTE: no need to write anything in the message
    if (rte_ring_enqueue(ipc_app_to_pol, sync_msg) < 0) {
        rte_mempool_put(ipc_msg_pool, sync_msg);
    }
}
