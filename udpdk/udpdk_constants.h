//
// Created by leoll2 on 9/26/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_CONSTANTS_H
#define UDPDK_CONSTANTS_H

#define NUM_SOCKETS_MAX     1024

/* DPDK ports */
#define PORT_RX     0
#define PORT_TX     0
#define QUEUE_RX    0
#define NUM_RX_DESC_DEFAULT 1024
#define NUM_TX_DESC_DEFAULT 1024
#define PKTMBUF_POOL_NAME   "UDPDK_mbuf_pool"
#define MBUF_CACHE_SIZE     512

/* Packet poller */
#define PKT_READ_SIZE       32
#define PREFETCH_OFFSET     4
#define NUM_FLOWS_DEF       0x1000
#define NUM_FLOWS_MIN       1
#define NUM_FLOWS_MAX       UINT16_MAX
#define MAX_FLOW_TTL        MS_PER_S
#define IP_FRAG_TBL_BUCKET_ENTRIES  16

/* Exchange memzone */
#define EXCH_MEMZONE_NAME   "UDPDK_exchange_desc"
#define EXCH_SLOTS_NAME     "UDPDK_exchange_slots"
#define EXCH_RING_SIZE      128
#define EXCH_RX_RING_NAME   "UDPDK_exchange_ring_%u_RX"
#define EXCH_TX_RING_NAME   "UDPDK_exchange_ring_%u_TX"
#define EXCH_BUF_SIZE       32

/* L4 port switching */
#define UDP_PORT_TABLE_NAME     "UDPDK_UDP_port_table"

#endif //UDPDK_CONSTANTS_H
