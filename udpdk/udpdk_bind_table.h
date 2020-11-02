//
// Created by leoll2 on 9/28/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_BIND_TABLE_H
#define UDPDK_BIND_TABLE_H

#include "udpdk_constants.h"
#include "udpdk_types.h"

/*
void htable_init(htable_item *table);

int htable_insert(htable_item *table, int key, int val);

int htable_lookup(htable_item *table, int key);

int htable_delete(htable_item *table, int key);
*/

void btable_init(void);

int btable_get_free_port(void);

int btable_add_binding(int s, struct in_addr ip, int port, int opts);

void btable_del_binding(int s, int port);

list_t *btable_get_bindings(int port);

#endif //UDPDK_BIND_TABLE_H
