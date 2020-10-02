//
// Created by leoll2 on 9/25/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#include "errno.h"
#include <netinet/in.h>

#include <rte_log.h>

#include "udpdk_api.h"
#include "udpdk_lookup_table.h"

#define RTE_LOGTYPE_SYSCALL RTE_LOGTYPE_USER1

extern int interrupted;
extern htable_item *udp_port_table;
extern struct exch_zone_info *exch_zone_desc;
extern struct exch_slot *exch_slots;

static int socket_validate_args(int domain, int type, int protocol)
{
    // Domain must be AF_INET (IPv4)
    if (domain != AF_INET) {
        errno = EAFNOSUPPORT;
        RTE_LOG(ERR, SYSCALL, "Attemp to create UDPDK socket of unsupported domain (%d)\n", domain);
        return -1;
    }

    // Type must be DGRAM (UDP)
    if (type != SOCK_DGRAM) {
        errno = EPROTONOSUPPORT;
        RTE_LOG(ERR, SYSCALL, "Attemp to create UDPDK socket of unsupported type (%d)\n", type);
        return -1;
    }

    // Protocol must be 0
    if (protocol != 0) {
        errno = EINVAL;
        RTE_LOG(ERR, SYSCALL, "Attemp to create UDPDK socket of unsupported protocol (%d)\n", protocol);
        return -1;
    }
    return 0;
}

int udpdk_socket(int domain, int type, int protocol)
{
    int sock_id;

    // Validate the arguments
    if (socket_validate_args(domain, type, protocol) < 0) {
        return -1;
    }
    // Fail if reached the maximum number of open sockets
    if (exch_zone_desc->n_zones_active > NUM_SOCKETS_MAX) {
        errno = ENOBUFS;
        return -1;
    }
    // Allocate a free sock_id
    for (sock_id = 0; sock_id < NUM_SOCKETS_MAX; sock_id++) {
        if (!exch_zone_desc->slots[sock_id].used) {
            exch_zone_desc->slots[sock_id].used = 1;
            exch_zone_desc->slots[sock_id].bound = 0;
            exch_zone_desc->slots[sock_id].sockfd = sock_id;
            break;
        }
    }
    if (sock_id == NUM_SOCKETS_MAX) {
        // Could not find a free slot
        errno = ENOBUFS;
        RTE_LOG(ERR, SYSCALL, "Failed to allocate a descriptor for socket (%d)\n", sock_id);
        return -1;
    }
    // Increment counter in exch_zone_desc
    exch_zone_desc->n_zones_active++;

    return sock_id;
}

static int bind_validate_args(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    // Check if the sockfd is valid
    if (!exch_zone_desc->slots[sockfd].used) {
        errno = EBADF;
        return -1;
    }
    // Check if already bound
    if (exch_zone_desc->slots[sockfd].bound) {
        errno = EINVAL;
        return -1;
    }
    // Validate addr
    if (addr->sa_family != AF_INET) {
        errno = EINVAL;
        return -1;
    }
    // Validate addr len
    if (addrlen != sizeof(struct sockaddr_in)) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int udpdk_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret;
    unsigned short port;
    const struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;

    // Validate the arguments
    if (bind_validate_args(sockfd, addr, addrlen) < 0) {
        return -1;
    }

    port = addr_in->sin_port;
    ret = htable_lookup(udp_port_table, port);
    if (ret != -1) {
        errno = EINVAL;
        RTE_LOG(ERR, SYSCALL, "Failed to bind because port %d is already in use\n", port);
        return -1;
    }

    // Mark the slot as bound
    exch_zone_desc->slots[sockfd].bound = 1;

    // Insert in the hashtable (port, sock_id)
    htable_insert(udp_port_table, (int)port, sockfd);
    return 0;
}

static int sendto_validate_args(int sockfd, const void *buf, size_t len, int flags,
                                const struct sockaddr *dest_addr, socklen_t addrlen)
{
    // Ensure sockfd is not beyond max limit
    if (sockfd >= NUM_SOCKETS_MAX) {
        errno = ENOTSOCK;
        return -1;
    }

    // Check if the sockfd is valid
    if (!exch_zone_desc->slots[sockfd].used) {
        errno = EBADF;
        return -1;
    }

    // TODO check if buf is a legit address

    // Check if flags are supported (atm none is supported)
    if (flags != 0) {
        errno = EINVAL;
        return -1;
    }

    // Check if the sender is specified
    if (dest_addr == NULL || addrlen == 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

ssize_t udpdk_sendto(int sockfd, const void *buf, size_t len, int flags,
                     const struct sockaddr *dest_addr, socklen_t addrlen)
{
    // Validate the arguments
    if (sendto_validate_args(sockfd, buf, len, flags, dest_addr, addrlen) < 0) {
        return -1;
    }

    // TODO implement core
    return 0;
}

static int recvfrom_validate_args(int sockfd, void *buf, size_t len, int flags,
                                  struct sockaddr *src_addr, socklen_t *addrlen)
{
    // Ensure sockfd is not beyond max limit
    if (sockfd >= NUM_SOCKETS_MAX) {
        errno = ENOTSOCK;
        return -1;
    }

    // Check if the sockfd is valid
    if (!exch_zone_desc->slots[sockfd].used) {
        errno = EBADF;
        return -1;
    }

    // TODO check if buf is a legit address

    // Check if flags are supported (atm none is supported)
    if (flags != 0) {
        errno = EINVAL;
        return -1;
    }

    // If buf is null, then addrlen must be null too
    if (buf == NULL && addrlen != NULL) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

ssize_t udpdk_recvfrom(int sockfd, void *buf, size_t len, int flags,
                       struct sockaddr *src_addr, socklen_t *addrlen)
{
    int ret = -1;
    struct rte_mbuf *pkt = NULL;
    uint32_t pkt_len;
    uint32_t udp_data_len;
    uint32_t eff_len;
    uint32_t eff_addrlen;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
    void *udp_data;

    printf("Inside recvfrom\n");
    // Validate the arguments
    if (recvfrom_validate_args(sockfd, buf, len, flags, src_addr, addrlen) < 0) {
        return -1;
    }

    // Dequeue one packet (busy wait until one is available)
    while (ret < 0 && !interrupted) {
        ret = rte_ring_dequeue(exch_slots[sockfd].rx_q, (void **)&pkt);
    }
    if (interrupted) {
        RTE_LOG(INFO, SYSCALL, "Recvfrom returning due to signal\n");
        errno = EINTR;
        return -1;
    }
    // Get some useful pointers to headers and data
    pkt_len = pkt->pkt_len;
    eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
    udp_hdr = (struct rte_udp_hdr *)(ip_hdr + 1);
    udp_data = (void *)(udp_hdr + 1);
    udp_data_len = pkt_len - sizeof(struct rte_ipv4_hdr) - sizeof(struct rte_udp_hdr);
    printf("recfrom pktlen: %d\n", pkt_len);
    printf("recfrom udp_data_len: %d\n", udp_data_len);

    // If the provided buffer is large enough to store it, then copy the whole packet, else only part of it
    if (udp_data_len >= len) {
        eff_len = udp_data_len;
    } else {
        eff_len = len;
    }

    // Copy the data to the buffer provided by the user
    memcpy(buf, udp_data, eff_len);

    // Write source address (or part of it if addrlen is too short)
    if (src_addr != NULL) {
        struct sockaddr_in addr_in;
        memset(&addr_in, 0, sizeof(addr_in));
        addr_in.sin_family = AF_INET;
        addr_in.sin_port = rte_be_to_cpu_16(udp_hdr->src_port);
        addr_in.sin_addr.s_addr = rte_be_to_cpu_32(ip_hdr->src_addr);
        if (sizeof(addr_in) <= *addrlen) {
            eff_addrlen = sizeof(addr_in);
        } else {
            eff_addrlen = *addrlen;
        }
        memcpy((void *)src_addr, &addr_in, eff_addrlen);
        *addrlen = eff_addrlen;
    }

    // Return how many bytes read
    return eff_len;
}

static int close_validate_args(int s)
{
    // Check if the socket is open
    if (!exch_zone_desc->slots[s].used) {
        errno = EBADF;
        RTE_LOG(ERR, SYSCALL, "Failed to close socket %d because it was not open\n", s);
        return -1;
    }
    return 0;
}

int udpdk_close(int s)
{
    // Validate the arguments
    if (close_validate_args(s)) {
        return -1;
    }

    // Reset slot
    exch_zone_desc->slots[s].bound = 0;
    exch_zone_desc->slots[s].used = 0;

    // Decrement counter of active slots
    exch_zone_desc->n_zones_active++;
    return 0;
}
