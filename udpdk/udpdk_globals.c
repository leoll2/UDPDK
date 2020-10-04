//
// Created by leoll2 on 9/28/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#include "udpdk_lookup_table.h"
#include "udpdk_types.h"

volatile int interrupted = 0;

struct rte_mempool *rx_pktmbuf_pool = NULL;

struct rte_mempool *tx_pktmbuf_pool = NULL;

htable_item *udp_port_table = NULL;

struct exch_zone_info *exch_zone_desc = NULL;

struct exch_slot *exch_slots = NULL;