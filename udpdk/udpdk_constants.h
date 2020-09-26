//
// Created by leoll2 on 9/26/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_CONSTANTS_H
#define UDPDK_CONSTANTS_H

#define NUM_SOCKETS_MAX     1024

/* Exchange memzone */
#define EXCH_MEMZONE_NAME   "UDPDK_exchange_desc"
#define EXCH_SLOTS_NAME     "UDPDK_exchange_slots"
#define EXCH_RING_SIZE      128
#define EXCH_RX_RING_NAME   "UDPDK_exchange_ring_%u_RX"
#define EXCH_TX_RING_NAME   "UDPDK_exchange_ring_%u_TX"

/* DPDK ports */
#define PORT_RX     0
#define PORT_TX     0
#define NUM_RX_DESC_DEFAULT 1024
#define NUM_TX_DESC_DEFAULT 1024
#define PKTMBUF_POOL_NAME   "UDPDK_mbuf_pool"
#define MBUF_CACHE_SIZE     512

#endif //UDPDK_CONSTANTS_H
