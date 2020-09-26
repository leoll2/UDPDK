//
// Created by leoll2 on 9/25/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#include "udpdk_api.h"


int udpdk_socket(int domain, int type, int protocol)
{
    // TODO implement
    return 0;
}

int udpdk_bind(int s, const struct sockaddr *addr, socklen_t addrlen)
{
    // TODO implement
    return 0;
}

ssize_t udpdk_sendto(int sockfd, const void *buf, size_t len, int flags,
                     const struct sockaddr *dest_addr, socklen_t addrlen)
{
    // TODO implement
    return 0;
}

ssize_t udpdk_recvfrom(int s, void *buf, size_t len, int flags,
                       struct sockaddr *src_addr, socklen_t *addrlen)
{
    // TODO implement
    return 0;
}

int udpdk_close(int s)
{
    // TODO implement
    return 0;
}