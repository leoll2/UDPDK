//
// Created by leoll2 on 11/01/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_SHMALLOC_H
#define UDPDK_SHMALLOC_H

#include <rte_compat.h>
#include <rte_memory.h>
#include <rte_common.h>

const struct rte_memzone *udpdk_init_allocator(const char *name, unsigned size, unsigned elem_size);

const struct rte_memzone *udpdk_retrieve_allocator(const char *name);

void *udpdk_shmalloc(const struct rte_memzone *mz);

void udpdk_shfree(const struct rte_memzone *mz, void *addr);

void udpdk_destroy_allocator(const struct rte_memzone *mz);

#endif  // UDPDK_SHMALLOC_H
