//
// Created by leoll2 on 11/13/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_SYNC_H
#define UDPDK_SYNC_H

int init_ipc_channel(void);

int retrieve_ipc_channel(void);

int ipc_wait_for_app(void);

int ipc_wait_for_poller(void);

void ipc_notify_to_app(void);

void ipc_notify_to_poller(void);

#endif //UDPDK_SYNC_H
