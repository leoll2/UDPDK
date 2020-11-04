//
// Created by leoll2 on 11/01/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#include <string.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>

#include "udpdk_shmalloc.h"

#define RTE_LOGTYPE_SHM  RTE_LOGTYPE_USER1

#define SetBit(A,k)     (A[(k / 32)] |= (1 << (k % 32)))
#define ClearBit(A,k)   (A[(k / 32)] &= ~(1 << (k % 32)))
#define TestBit(A,k)    (A[(k / 32)] & (1 << (k % 32)))

struct allocator {
    unsigned size;
    unsigned elem_size;     // size (byte) of each element
    unsigned n_free;
    unsigned next_free;     // index of next free
    unsigned pool_offset;   // offset (bytes) from the begin of memzone
};

const struct rte_memzone *udpdk_init_allocator(const char *name, unsigned size, unsigned elem_size)
{
    unsigned mem_needed;
    unsigned p_off;
    const struct rte_memzone *mz;
    struct allocator *all;
    const int *free_bitfield;

    //Round-up elem_size to cache line multiple (64 byte)
    elem_size = (elem_size + 64 - 1) / 64;

    // Determine how much memory is needed (pool size + bitfield of free elems + variables)
    mem_needed = sizeof(struct allocator) + (size / 8 + 1);
    mem_needed = (mem_needed + elem_size - 1) / elem_size;  // align
    p_off = mem_needed;
    mem_needed += (size * elem_size);

    // Allocate the memory for the allocator and its pool
    mz = rte_memzone_reserve(name, mem_needed, rte_socket_id(), 0);
    if (mz == NULL) {
        return NULL;
    }

    // Initialize the allocator internal variables
    all = (struct allocator *)(void *)mz->addr;
    all->size = size;
    all->elem_size = elem_size;
    all->n_free = size;
    all->next_free = 0;
    all->pool_offset = p_off;

    // Mark all the elements as free
    free_bitfield = (int *)(all + 1);
    memset((void *)free_bitfield, 0, (size / 8 + 1));

    return mz;
}

const struct rte_memzone *udpdk_retrieve_allocator(const char *name)
{
    return rte_memzone_lookup(name);
}

void *udpdk_shmalloc(const struct rte_memzone *mz)
{
    struct allocator *all;
    int *free_bitfield;
    unsigned size;
    unsigned p_off;
    unsigned i, j;
    void *ret;
    
    all = (struct allocator *)(void *)mz->addr;
    free_bitfield = (int *)(all + 1);

    if (all->n_free == 0) {
        RTE_LOG(WARNING, SHM, "shmalloc failed: out of memory\n");
        return NULL;
    }

    size = all->size;
    p_off = all->pool_offset;

    // Compute and store the pointer to return
    ret = ((void *)mz->addr + p_off + (all->next_free * all->elem_size));

    --all->n_free;

    // Update the free bitfield
    SetBit(free_bitfield, all->next_free);

    // Find the next free slot
    if (all->n_free != 0) {
        j = all->next_free + 1;
        for (int i = 0; i < size; i++) {
            if (j >= size) {
                j = 0;
            }
            if (!TestBit(free_bitfield, j)) {
                all->next_free = j;
                break;
            }
            ++j;
        }
    }

    return ret;
}

// NOTE: the memzone is only needed to check memory boundaries
void udpdk_shfree(const struct rte_memzone *mz, void *addr)
{
    struct allocator *all;
    int *free_bitfield;
    unsigned p_off;
    unsigned i;
    void *pool_start;
    void *pool_end;

    all = (struct allocator *)(void *)mz->addr;
    free_bitfield = (int *)(all + 1);
    p_off = all->pool_offset;

    // Validate the address
    pool_start = (void *)mz->addr + p_off;
    pool_end = ((void *)mz->addr + p_off + (all->size * all->elem_size));
    if ((addr < pool_start) || (addr >= pool_end)) {
        RTE_LOG(WARNING, SHM, "Double free\n");
        return;
    }

    // Check if the memory was really allocated
    i = (addr - pool_start) / all->elem_size;
    if (!TestBit(free_bitfield, i)) {
        RTE_LOG(WARNING, SHM, "Double free\n");
        return;
    }

    // Free
    ClearBit(free_bitfield, i);
    ++all->n_free;

    // If prevoiusly full, recompute next_free
    if (all->n_free == 1) {
        all->next_free = i;
    }
}

void udpdk_destroy_allocator(const struct rte_memzone *mz)
{
    const struct allocator *all;

    all = (struct allocator *)(void *)mz->addr;
    if (all->n_free != all->size) {
        RTE_LOG(WARNING, SHM, "Destroying shm allocator before all the elements were freed!\n");
    }

    rte_memzone_free(mz);
}
