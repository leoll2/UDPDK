//
// Created by leoll2 on 9/26/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_CONSTANTS_H
#define UDPDK_CONSTANTS_H

#define MAX(a,b) ((a) > (b) ? a : b)
#define MIN(a,b) ((a) < (b) ? a : b)

#define NUM_SOCKETS_MAX     1024
#define UDP_MAX_PORT        65536

/* DPDK ports */
#define PORT_RX     0
#define PORT_TX     0
#define QUEUE_RX    0
#define QUEUE_TX    0
#define NUM_RX_DESC_DEFAULT 2048 
#define NUM_TX_DESC_DEFAULT 2048 
#define MBUF_CACHE_SIZE     512
#define PKTMBUF_POOL_RX_NAME            "UDPDK_mbuf_pool_RX"
#define PKTMBUF_POOL_TX_NAME            "UDPDK_mbuf_pool_TX"
#define PKTMBUF_POOL_DIRECT_TX_NAME     "UDPDK_mbuf_pool_direct_TX"
#define PKTMBUF_POOL_INDIRECT_TX_NAME   "UDPDK_mbuf_pool_indir_TX"

/* Ethernet */
#define JUMBO_FRAME_MAX_SIZE    0x2600

/* IPv4 Fragmentation */
#define NUM_FLOWS_DEF       0x1000
#define NUM_FLOWS_MIN       1
#define NUM_FLOWS_MAX       UINT16_MAX
#define MAX_FLOW_TTL        MS_PER_S
#define IP_FRAG_TBL_BUCKET_ENTRIES  16
#define IPV4_MTU_DEFAULT    RTE_ETHER_MTU
#define MAX_PACKET_FRAG     RTE_LIBRTE_IP_FRAG_MAX_FRAG

/* Packet poller */
#define BURST_SIZE          128
#define RX_MBUF_TABLE_SIZE  BURST_SIZE
#define TX_MBUF_TABLE_SIZE  (2 * MAX(BURST_SIZE, MAX_PACKET_FRAG))
#define PREFETCH_OFFSET     4

/* Exchange memzone */
#define EXCH_MEMZONE_NAME   "UDPDK_exchange_desc"
#define EXCH_SLOTS_NAME     "UDPDK_exchange_slots"
#define EXCH_RING_SIZE      2048
#define EXCH_RX_RING_NAME   "UDPDK_exchange_ring_%u_RX"
#define EXCH_TX_RING_NAME   "UDPDK_exchange_ring_%u_TX"
#define EXCH_BUF_SIZE       BURST_SIZE

/* L4 port switching */
#define UDP_BIND_TABLE_NAME "UDPDK_btable"

/* IPv4 header */
#define IP_DEFTTL       64
#define IP_VERSION      0x40
#define IP_HDRLEN       0x05
#define IP_VHL_DEF      (IP_VERSION | IP_HDRLEN)

/* Parser */
#define MAX_ARGC        64
#define MAX_ARG_LEN     256

#endif //UDPDK_CONSTANTS_H
