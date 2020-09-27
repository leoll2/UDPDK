//
// Created by leoll2 on 9/25/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_TYPES_H
#define UDPDK_TYPES_H

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memzone.h>

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "udpdk_constants.h"

enum exch_ring_func {EXCH_RING_RX, EXCH_RING_TX};

struct exch_slot_info {
    int active;
    int sockfd;
} __rte_cache_aligned;

struct exch_zone_info {
    uint64_t n_zones_active;
    struct exch_slot_info slots[NUM_SOCKETS_MAX];
};

struct exch_slot {
    struct rte_ring *rx_q;                      // RX queue
    struct rte_ring *tx_q;                      // TX queue
    struct rte_mbuf *rx_buffer[EXCH_BUF_SIZE];  // buffers storing rx packets before flushing to rt_ring
    uint16_t rx_count;                          // current number of packets in the rx buffer
};

#endif //UDPDK_TYPES_H
