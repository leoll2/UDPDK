//
// Created by leoll2 on 11/19/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
// The following code derives in part from netmap pkt-gen.c
//

#ifndef UDPDK_DUMP_H
#define UDPDK_DUMP_H

void udpdk_dump_payload(const char *payload, int len);

void udpdk_dump_mbuf(struct rte_mbuf *m);

#endif  // UDPDK_DUMP_H
