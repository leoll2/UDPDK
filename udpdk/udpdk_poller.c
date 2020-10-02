//
// Created by leoll2 on 9/27/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip_frag.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_per_lcore.h>
#include <rte_ring.h>
#include <rte_string_fns.h>

#include "udpdk_constants.h"
#include "udpdk_lookup_table.h"
#include "udpdk_types.h"

#define RTE_LOGTYPE_POLLBODY RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_POLLINIT RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_POLLINTR RTE_LOGTYPE_USER1

static volatile int poller_alive = 1;

extern struct exch_zone_info *exch_zone_desc;
extern struct exch_slot *exch_slots;
extern htable_item *udp_port_table;

/* Descriptor of a RX queue */
struct rx_queue {
    struct rte_ip_frag_tbl *frag_tbl;       // table to store incoming packet fragments
    struct rte_mempool *pool;               // pool of mbufs
    uint16_t portid;
};

/* Descriptor of each lcore (queue configuration) */
struct lcore_queue_conf {
    struct rx_queue rx_queue;
    struct rte_ip_frag_death_row death_row;
} __rte_cache_aligned;

static struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];


/* Poller signal handler */
static void poller_sighandler(int sig)
{
    RTE_LOG(INFO, POLLINTR, "Received request to stop\n");
    poller_alive = 0;
}

/* Get the name of the rings of exchange slots */
static inline const char * get_exch_ring_name(unsigned id, enum exch_ring_func func)
{
    static char buffer[sizeof(EXCH_RX_RING_NAME) + 8];

    if (func == EXCH_RING_RX) {
        snprintf(buffer, sizeof(buffer), EXCH_RX_RING_NAME, id);
    } else {
        snprintf(buffer, sizeof(buffer), EXCH_TX_RING_NAME, id);
    }
    return buffer;
}

/* Initialize the queues for this lcore */
static int setup_queues(void)
{
    unsigned lcore_id;
    unsigned socket_id;
    uint64_t frag_cycles;
    struct lcore_queue_conf *qconf;

    lcore_id = rte_lcore_id();
    socket_id = rte_socket_id();
    qconf = &lcore_queue_conf[lcore_id];
    frag_cycles = (rte_get_tsc_hz() + MS_PER_S - 1) / MS_PER_S * MAX_FLOW_TTL;

    // Memory pool for mbufs
    // TODO actually unused because pool is needed only to initialize a queue, which is done in 'application' anyway
    qconf->rx_queue.pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
    if (qconf->rx_queue.pool == NULL) {
        RTE_LOG(ERR, POLLINIT, "Cannot retrieve mempool for mbufs\n");
        return -1;
    }

    // Fragment table
    qconf->rx_queue.frag_tbl = rte_ip_frag_table_create(NUM_FLOWS_DEF, IP_FRAG_TBL_BUCKET_ENTRIES,
            NUM_FLOWS_MAX, frag_cycles, socket_id);
    if (qconf->rx_queue.frag_tbl == NULL) {
        RTE_LOG(ERR, POLLINIT, "ip_frag_tbl_create(%u) on lcore %u failed\n", NUM_FLOWS_DEF, lcore_id);
        return -1;
    }
    RTE_LOG(INFO, POLLINIT, "Created IP fragmentation table\n");

    return 0;
}

static int setup_exch_zone(void)
{
    uint16_t i;
    const struct rte_memzone *mz;

    // Retrieve the exchange zone descriptor in shared memory
    mz = rte_memzone_lookup(EXCH_MEMZONE_NAME);
    if (mz == NULL) {
        RTE_LOG(ERR, POLLINIT, "Cannot retrieve exchange memzone descriptor\n");
        return -1;
    }
    exch_zone_desc = mz->addr;

    // Allocate enough memory to store the exchange slots
    exch_slots = rte_zmalloc(EXCH_SLOTS_NAME, sizeof(*exch_slots) * NUM_SOCKETS_MAX, 0);
    if (exch_slots == NULL) {
        RTE_LOG(ERR, POLLINIT, "Cannot allocate memory for exchange slots\n");
        return -1;
    }

    for (i = 0; i < NUM_SOCKETS_MAX; i++) {
        // Retrieve the RX queue for each slot
        exch_slots[i].rx_q = rte_ring_lookup(get_exch_ring_name(i, EXCH_RING_RX));
        if (exch_slots[i].rx_q == NULL) {
            RTE_LOG(ERR, POLLINIT, "Failed to retrieve rx ring queue for exchanger %u\n", i);
            return -1;
        }
        // Retrieve the TX queue for each slot
        exch_slots[i].tx_q = rte_ring_lookup(get_exch_ring_name(i, EXCH_RING_TX));
        if (exch_slots[i].tx_q == NULL) {
            RTE_LOG(ERR, POLLINIT, "Failed to retrieve tx ring queue for exchanger %u\n", i);
            return -1;
        }
        // rx_buffer and rx_count are already zeroed thanks to zmalloc
    }

    return 0;
}

static int setup_udp_table(void)
{
    const struct rte_memzone *udp_port_table_mz;

    udp_port_table_mz = rte_memzone_lookup(UDP_PORT_TABLE_NAME);
    if (udp_port_table_mz == NULL) {
        RTE_LOG(ERR, POLLINIT, "Cannot retrieve exchange memzone descriptor\n");
        return -1;
    }
    udp_port_table = udp_port_table_mz->addr;

    return 0;
}

/* Initialize UDPDK packet poller (runs in a separate process) */
int poller_init(int argc, char *argv[])
{
    int retval;

    // Initialize EAL
    retval = rte_eal_init(argc, argv);
    if (retval < 0) {
        RTE_LOG(ERR, POLLINIT, "Cannot initialize EAL for poller\n");
        return -1;
    }

    // Setup RX/TX queues
    retval = setup_queues();
    if (retval < 0) {
        RTE_LOG(ERR, POLLINIT, "Cannot setup queues for poller\n");
        return -1;
    }

    // Setup buffers for exchange slots
    retval = setup_exch_zone();
    if (retval < 0) {
        RTE_LOG(ERR, POLLINIT, "Cannot setup exchange zone for poller\n");
        return -1;
    }

    // Setup table for UDP port switching
    retval = setup_udp_table();
    if (retval < 0) {
        RTE_LOG(ERR, POLLINIT, "Cannot setup table for UDP port switching\n");
        return -1;
    }

    // Setup signals for termination
    signal(SIGINT, poller_sighandler);
    signal(SIGTERM, poller_sighandler);

    return 0;
}

static void ipv4_int_to_str(unsigned int ip, char *buf)
{
    unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
    snprintf(buf, 16, "%d.%d.%d.%d", bytes[3], bytes[2], bytes[1], bytes[0]);
}

static void flush_rx_queue(uint16_t idx)
{
    uint16_t j;
    struct rte_ring *rx_q;

    // Skip if no packets received
    if (exch_slots[idx].rx_count == 0)
        return;

    // Get a reference to the appropriate ring in shared memory (lookup by name)
    rx_q = exch_slots[idx].rx_q;

    // Put the packets in the ring
    if (rte_ring_enqueue_bulk(rx_q, (void **)exch_slots[idx].rx_buffer, exch_slots[idx].rx_count, NULL) == 0) {
        for (j = 0; j < exch_slots[idx].rx_count; j++)
            rte_pktmbuf_free(exch_slots[idx].rx_buffer[j]);
    }
    exch_slots[idx].rx_count = 0;
}

static inline void enqueue_rx_packet(uint8_t exc_buf_idx, struct rte_mbuf *buf)
{
    // Enqueue the packet for the appropriate exc buffer, and increment the counter
    exch_slots[exc_buf_idx].rx_buffer[exch_slots[exc_buf_idx].rx_count++] = buf;
}

static inline uint16_t is_udp_pkt(struct rte_ipv4_hdr *ip_hdr)
{
    return (ip_hdr->next_proto_id == IPPROTO_UDP);
}

static inline uint16_t get_udp_dst_port(struct rte_udp_hdr *udp_hdr)
{
    return rte_be_to_cpu_16(udp_hdr->dst_port);
}

static inline uint32_t get_ip_dst(struct rte_ipv4_hdr *ip_hdr)
{
    return rte_be_to_cpu_32(ip_hdr->dst_addr);
}

static inline void reassemble(struct rte_mbuf *m, uint16_t portid, uint32_t queue,
                              struct lcore_queue_conf *qconf, uint64_t tms)
{
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_ip_frag_tbl *tbl;
    struct rte_ip_frag_death_row *dr;
    struct rx_queue *rxq;
    uint16_t udp_dst_port;
    uint32_t ip_dst;
    int sock_id;

    rxq = &qconf->rx_queue;

    eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

    if (RTE_ETH_IS_IPV4_HDR(m->packet_type)) {

        ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);

        if (rte_ipv4_frag_pkt_is_fragmented(ip_hdr)) {
            struct rte_mbuf *mo;

            tbl = rxq->frag_tbl;
            dr = &qconf->death_row;

            // prepare mbuf (setup l2_len/l3_len)
            m->l2_len = sizeof(*eth_hdr);
            m->l3_len = sizeof(*ip_hdr);

            // Handle this fragment (returns # of fragments if all already received, NULL otherwise)
            mo = rte_ipv4_frag_reassemble_packet(tbl, dr, m, tms, ip_hdr);
            if (mo == NULL)
                // More fragments needed...
                return;

            // Reassembled packet (update pointers to headers if needed)
            if (mo != m) {
                m = mo;
                eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
                ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
            }
        }
    } else {
        RTE_LOG(WARNING, POLLBODY, "Received non-IPv4 packet.\n");
        return;
    }

    if (!is_udp_pkt(ip_hdr)) {
        RTE_LOG(WARNING, POLLBODY, "Received non-UDP packet.\n");
        return;
    }
    udp_dst_port = get_udp_dst_port(
            (struct rte_udp_hdr *)((unsigned char *)ip_hdr + sizeof(struct rte_ipv4_hdr)));
    ip_dst = get_ip_dst(ip_hdr);

    char ip_str[16];                                    // TODO DEBUG
    ipv4_int_to_str(ip_dst, ip_str);                    // TODO DEBUG
    printf("[DBG] UDP dest port: %d\n", udp_dst_port);  // TODO DEBUG
    printf("[DBG] IP dest addr: %s\n", ip_str); // TODO DEBUG

    // Find the sock_id corresponding to the UDP dst port (L4 switching) and enqueue the packet to its queue
    sock_id = htable_lookup(udp_port_table, udp_dst_port);
    if (sock_id < 0 || sock_id >= NUM_SOCKETS_MAX) {
        errno = EINVAL;
        RTE_LOG(ERR, POLLBODY, "Invalid L4 port mapping: port %d maps to sock_id %d\n", udp_dst_port, sock_id);
        return;
    }
    enqueue_rx_packet(sock_id, m);
}

/* Packet polling routine */
void poller_body(void)
{
    unsigned lcore_id;
    uint64_t cur_tsc;
    struct lcore_queue_conf *qconf;

    lcore_id = rte_lcore_id();
    qconf = &lcore_queue_conf[lcore_id];

    while (poller_alive) {
        struct rte_mbuf *rxbuf[PKT_READ_SIZE];
        struct rte_mbuf *txbuf[PKT_WRITE_SIZE];
        uint16_t rx_count, tx_sendable, tx_count;
        int i, j;

        // Get current timestamp (needed for reassembly)
        cur_tsc = rte_rdtsc();

        // Transmit packets to DPDK port 0 (queue 0)
        for (i = 0; i < NUM_SOCKETS_MAX; i++) {
            if (exch_zone_desc->slots[i].used) {
                tx_sendable = rte_ring_dequeue_burst(exch_slots[i].tx_q, (void **)txbuf, PKT_WRITE_SIZE, NULL);
                if (likely(tx_sendable > 0)) {
                    tx_count = rte_eth_tx_burst(PORT_TX, QUEUE_TX, txbuf, tx_sendable);  // TODO should call a send function that accoubts for fragmentation
                    if (unlikely(tx_count < tx_sendable)) {
                        do {
                            rte_pktmbuf_free(txbuf[tx_count]);
                        } while (++tx_count < tx_sendable);
                    }
                }
            }
        }

        // Receive packets from DPDK port 0 (queue 0)   TODO use more queues
        rx_count = rte_eth_rx_burst(PORT_RX, QUEUE_RX, rxbuf, PKT_READ_SIZE);

        if (likely(rx_count > 0)) {
            // Prefetch some packets (to reduce cache misses later)
            for (j = 0; j < PREFETCH_OFFSET && j < rx_count; j++) {
                rte_prefetch0(rte_pktmbuf_mtod(rxbuf[j], void *));
            }

            // Prefetch the remaining packets, and reassemble the first ones
            for (j = 0; j < (rx_count - PREFETCH_OFFSET); j++) {
                rte_prefetch0(rte_pktmbuf_mtod(rxbuf[j + PREFETCH_OFFSET], void *));
                reassemble(rxbuf[j], PORT_RX, QUEUE_RX, qconf, cur_tsc);
            }

            // Reassemble the second batch of fragments
            for (; j < rx_count; j++) {
                reassemble(rxbuf[j], PORT_RX, QUEUE_RX, qconf, cur_tsc);
            }

            // Effectively flush the packets to exchange buffers
            for (i = 0; i < NUM_SOCKETS_MAX; i++) {
                if (exch_zone_desc->slots[i].used) {
                    flush_rx_queue(i);
                }
            }

            // Free death row
            rte_ip_frag_free_death_row(&qconf->death_row, PREFETCH_OFFSET);
        }
    }
    // Exit directly to avoid returning in the application main (as we forked)
    exit(0);
}