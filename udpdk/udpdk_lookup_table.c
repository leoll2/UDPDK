//
// Created by leoll2 on 9/27/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
// Simple hashmap with fixed size, useful to implement L4 port switching.
// Keys and values must be non-negative integers (-1 is reserved for 'empty')
//

#include "udpdk_lookup_table.h"

#define h(x) (9649 * x % NUM_SOCKETS_MAX)

void htable_init(htable_item *table)
{
    for (int i = 0; i < NUM_SOCKETS_MAX; i++) {
        table[i].key = -1;
    }
}

static inline int htable_get_idx(htable_item *table, int key)
{
    int i = h(key);
    int free_idx = -1;
    int scanned = 0;

    // find a free slot (linear probing starting from hashed index)
    while ((table[i].key != key) && (++scanned < NUM_SOCKETS_MAX)) {
        if (free_idx == -1 && table[i].key == -1) {
            // store the first free index
            free_idx = i;
        }
        i++;
        if (i == NUM_SOCKETS_MAX) {
            i = 0;
        }
    }
    // table is full
    if (scanned == NUM_SOCKETS_MAX) {
        return free_idx;
    }
    return i;
}

inline int htable_insert(htable_item *table, int key, int val)
{

    int i = htable_get_idx(table, key);
    if (i == -1) {
        // full
        return -1;
    }
    table[i].key = key;
    table[i].val = val;
    return 0;
}

inline int htable_delete(htable_item *table, int key)
{
    int i = htable_get_idx(table, key);
    if (i == -1) {
        // not found (table full)
        return -1;
    } else if (table[i].key == -1) {
        // not found (table not full)
        return -1;
    } else {
        // remove
        table[i].key = -1;
        return 0;
    }
}

inline int htable_lookup(htable_item *table, int key)
{
    int i = htable_get_idx(table, key);
    if (i == -1) {
        // not found (table full)
        return -1;
    } else if (table[i].key == -1) {
        // not found (table not full)
        return -1;
    } else {
        return table[i].val;
    }
}
