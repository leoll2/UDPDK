//
// Created by leoll2 on 9/25/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#include "errno.h"
#include <netinet/in.h>

#include <rte_log.h>
#include <rte_random.h>

#include "udpdk_api.h"
#include "udpdk_lookup_table.h"

#define RTE_LOGTYPE_SYSCALL RTE_LOGTYPE_USER1

extern int interrupted;
extern configuration config;
extern htable_item *udp_port_table;
extern struct exch_zone_info *exch_zone_desc;
extern struct exch_slot *exch_slots;
extern struct rte_mempool *tx_pktmbuf_pool;

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

    // Check if the port is already being used
    port = addr_in->sin_port;
    ret = htable_lookup(udp_port_table, port);
    if (ret != -1) {
        errno = EINVAL;
        RTE_LOG(ERR, SYSCALL, "Failed to bind because port %d is already in use\n", port);
        return -1;
    }

    // Mark the slot as bound, and store the corresponding IP and port
    exch_zone_desc->slots[sockfd].bound = 1;
    exch_zone_desc->slots[sockfd].udp_port = (int)port;
    if (addr_in->sin_addr.s_addr == INADDR_ANY) {
        // If INADDR_ANY, use the address from the configuration file
        exch_zone_desc->slots[sockfd].ip_addr = config.src_ip_addr;
    } else {
        // If the address is explicitly set, bind to that
        exch_zone_desc->slots[sockfd].ip_addr = addr_in->sin_addr;
    }

    // Insert in the hashtable (port, sock_id)
    htable_insert(udp_port_table, (int)port, sockfd);
    RTE_LOG(INFO, SYSCALL, "Binding port %d to sock_id %d\n", port, sockfd);

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

// TODO move this elsewhere
static int get_free_udp_port(void)
{
    int port;
    if (exch_zone_desc->n_zones_active == NUM_SOCKETS_MAX) {
        // No port available
        return -1;
    }

    // Generate a random unused port
    do {
        port = (uint16_t)rte_rand();
    } while (htable_lookup(udp_port_table, port) != -1);

    return port;
}

ssize_t udpdk_sendto(int sockfd, const void *buf, size_t len, int flags,
                     const struct sockaddr *dest_addr, socklen_t addrlen)
{
    struct rte_mbuf *pkt;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
    void *udp_data;
    const struct sockaddr_in *dest_addr_in = (struct sockaddr_in *)dest_addr;

    // Validate the arguments
    if (sendto_validate_args(sockfd, buf, len, flags, dest_addr, addrlen) < 0) {
        return -1;
    }

    // If the socket was not explicitly bound, bind it when the first packet is sent
    if (unlikely(!exch_zone_desc->slots[sockfd].bound)) {
        struct sockaddr_in saddr_in;
        memset(&saddr_in, 0, sizeof(saddr_in));
        saddr_in.sin_family = AF_INET;
        saddr_in.sin_addr.s_addr = INADDR_ANY;
        saddr_in.sin_port = get_free_udp_port();
        if (udpdk_bind(sockfd, (const struct sockaddr *)&saddr_in, sizeof(saddr_in)) < 0) {
            RTE_LOG(ERR, SYSCALL, "Send failed to bind\n");
            return -1;
        }
    }

    // Allocate one mbuf for the packet (will be freed when effectively sent)
    pkt = rte_pktmbuf_alloc(tx_pktmbuf_pool);
    if (!pkt) {
        RTE_LOG(ERR, SYSCALL, "Sendto failed to allocate mbuf\n");
        errno = ENOMEM;
        return -1;
    }

    // Initialize the Ethernet header
    eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    rte_ether_addr_copy(&config.src_mac_addr, &eth_hdr->s_addr);
    rte_ether_addr_copy(&config.dst_mac_addr, &eth_hdr->d_addr);
    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    // Initialize the IP header
    ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
    memset(ip_hdr, 0, sizeof(*ip_hdr));
    ip_hdr->version_ihl = IP_VHL_DEF;
    ip_hdr->type_of_service = 0;
    ip_hdr->fragment_offset = 0;
    ip_hdr->time_to_live = IP_DEFTTL;
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->packet_id = 0;
    ip_hdr->src_addr = exch_zone_desc->slots[sockfd].ip_addr.s_addr;
    ip_hdr->dst_addr = dest_addr_in->sin_addr.s_addr;
    ip_hdr->total_length = rte_cpu_to_be_16(len + sizeof(*ip_hdr) + sizeof(*udp_hdr));
    ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);

    // Initialize the UDP header
    udp_hdr = (struct rte_udp_hdr *)(ip_hdr + 1);
    udp_hdr->src_port = exch_zone_desc->slots[sockfd].udp_port;
    udp_hdr->dst_port = dest_addr_in->sin_port;
    udp_hdr->dgram_cksum = 0;   // UDP checksum is optional
    udp_hdr->dgram_len = rte_cpu_to_be_16(len + sizeof(*udp_hdr));

    // Fill other DPDK metadata
    pkt->packet_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP;
    pkt->pkt_len = len + sizeof(*eth_hdr) + sizeof(*ip_hdr) + sizeof(*udp_hdr);
    pkt->data_len = pkt->pkt_len;
    pkt->l2_len = sizeof(struct rte_ether_hdr);
    pkt->l3_len = sizeof(struct rte_ipv4_hdr);
    pkt->l4_len = sizeof(struct rte_udp_hdr);

    // Write payload
    udp_data = (void *)(udp_hdr + 1);
    memcpy(udp_data, buf, len);

    // Put the packet in the tx_ring
    if (rte_ring_enqueue(exch_slots[sockfd].tx_q, (void *)pkt) < 0) {
        RTE_LOG(ERR, SYSCALL, "Sendto failed to put packet in the TX ring\n");
        errno = ENOBUFS;
        rte_pktmbuf_free(pkt);
        return -1;
    }

    return len;
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
    struct rte_mbuf *seg = NULL;
    uint32_t seg_len;           // number of bytes of payload in this segment
    uint32_t eff_len;           // number of bytes to read from this segment
    uint32_t eff_addrlen;
    uint32_t bytes_left = len;
    unsigned nb_segs;
    unsigned offset_payload;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;

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
    nb_segs = pkt->nb_segs;
    eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
    udp_hdr = (struct rte_udp_hdr *)(ip_hdr + 1);

    // Write source address (or part of it if addrlen is too short)
    if (src_addr != NULL) {
        struct sockaddr_in addr_in;
        memset(&addr_in, 0, sizeof(addr_in));
        addr_in.sin_family = AF_INET;
        addr_in.sin_port = udp_hdr->src_port;
        addr_in.sin_addr.s_addr = ip_hdr->src_addr;
        if (sizeof(addr_in) <= *addrlen) {
            eff_addrlen = sizeof(addr_in);
        } else {
            eff_addrlen = *addrlen;
        }
        memcpy((void *)src_addr, &addr_in, eff_addrlen);
        *addrlen = eff_addrlen;
    }

    seg = pkt;
    for (int s = 0; s < nb_segs; s++) {
        // The the first segment includes eth + ipv4 + udp headers before the payload
        offset_payload = (s == 0) ?
                sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) : 0;
        // Find how many bytes of data are in this segment
        seg_len = seg->data_len - offset_payload;
        // The amount of data to copy is the minimum between this segment length and the remaining requested bytes
        if (seg_len < bytes_left) {
            eff_len = seg_len;
        } else {
            eff_len = bytes_left;
        }
        // Copy payload into buffer
        memcpy(buf, rte_pktmbuf_mtod(seg, void *) + offset_payload, eff_len);
        // Adjust pointers and counters
        buf += eff_len;
        bytes_left -= eff_len;
        seg = seg->next;
        if (bytes_left == 0) {
            break;
        }
    }
    // Free the mbuf (with all the chained segments)
    rte_pktmbuf_free(pkt);

    // Return how many bytes read
    return len - bytes_left;
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
