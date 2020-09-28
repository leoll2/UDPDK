//
// Created by leoll2 on 9/28/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_LOOKUP_TABLE_H
#define UDPDK_LOOKUP_TABLE_H

#include "udpdk_constants.h"

typedef struct htable_item {
    int key;
    int val;
} htable_item;

void htable_init(htable_item *table);

int htable_insert(htable_item *table, int key, int val);

int htable_lookup(htable_item *table, int key);

int htable_delete(htable_item *table, int key);

#endif //UDPDK_LOOKUP_TABLE_H
