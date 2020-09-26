//
// Created by leoll2 on 9/25/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#ifndef UDPDK_API_H
#define UDPDK_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "udpdk_types.h"

int udpdk_init(int argc, char *argv[]);

void udpdk_cleanup(void);

int udpdk_socket(int domain, int type, int protocol);

int udpdk_bind(int s, const struct sockaddr *addr, socklen_t addrlen);

ssize_t udpdk_sendto(int sockfd, const void *buf, size_t len, int flags,
        const struct sockaddr *dest_addr, socklen_t addrlen);

ssize_t udpdk_recvfrom(int s, void *buf, size_t len, int flags,
        struct sockaddr *src_addr, socklen_t *addrlen);

int udpdk_close(int s);

#ifdef __cplusplus
}
#endif

#endif //UDPDK_API_H
