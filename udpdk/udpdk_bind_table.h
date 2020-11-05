//
// Created by leoll2 on 9/28/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_BIND_TABLE_H
#define UDPDK_BIND_TABLE_H

#include "udpdk_constants.h"
#include "udpdk_types.h"


void btable_init(void);

int btable_get_free_port(void);

int btable_add_binding(int s, struct in_addr ip, int port, int opts);

void btable_del_binding(int s, int port);

udpdk_list_t *btable_get_bindings(int port);

void btable_destroy(void);

#endif //UDPDK_BIND_TABLE_H
