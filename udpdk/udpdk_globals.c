//
// Created by leoll2 on 9/28/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#include "udpdk_constants.h"
#include "udpdk_types.h"

volatile int interrupted = 0;

configuration config;

int primary_argc = 0;

int secondary_argc = 0;

char *primary_argv[MAX_ARGC];

char *secondary_argv[MAX_ARGC];

struct rte_mempool *rx_pktmbuf_pool = NULL;

struct rte_mempool *tx_pktmbuf_pool = NULL;

struct rte_mempool *tx_pktmbuf_direct_pool = NULL;

struct rte_mempool *tx_pktmbuf_indirect_pool = NULL;

struct exch_zone_info *exch_zone_desc = NULL;

struct exch_slot *exch_slots = NULL;
