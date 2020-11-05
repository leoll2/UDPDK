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
#include "udpdk_bind_table.h"
#include "udpdk_shmalloc.h"
#include "udpdk_types.h"

#define RTE_LOGTYPE_POLLBODY RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_POLLINIT RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_POLLINTR RTE_LOGTYPE_USER1

static volatile int poller_alive = 1;

extern struct exch_zone_info *exch_zone_desc;
extern struct exch_slot *exch_slots;
extern list_t **sock_bind_table;
extern const void *bind_info_alloc;

/* Descriptor of a RX queue */
struct rx_queue {
    struct rte_mbuf *rx_mbuf_table[RX_MBUF_TABLE_SIZE];
    struct rte_ip_frag_tbl *frag_tbl;       // table to store incoming packet fragments
    struct rte_mempool *pool;               // pool of mbufs
    uint16_t portid;
};

struct tx_queue {
    struct rte_mbuf *tx_mbuf_table[TX_MBUF_TABLE_SIZE];
    struct rte_mempool *direct_pool;
    struct rte_mempool *indirect_pool;
};

/* Descriptor of each lcore (queue configuration) */
struct lcore_queue_conf {
    struct rx_queue rx_queue;
    struct tx_queue tx_queue;
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

/* Initialize the allocators */
static int setup_allocators(void)
{
    bind_info_alloc = udpdk_retrieve_allocator("bind_info_alloc");
    if (bind_info_alloc == NULL) {
        RTE_LOG(ERR, POLLINIT, "Cannot retrieve bind_info shmem allocator\n");
        return -1;
    }

    if (udpdk_list_reinit() < 0) {
        RTE_LOG(ERR, POLLINIT, "Cannot retrieve list shmem allocators\n");
        return -1;
    }
    return 0;
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

    // Pool of mbufs for RX
    // NOTE actually unused because pool is needed only to initialize a queue, which is done in 'application' anyway
    qconf->rx_queue.pool = rte_mempool_lookup(PKTMBUF_POOL_RX_NAME);
    if (qconf->rx_queue.pool == NULL) {
        RTE_LOG(ERR, POLLINIT, "Cannot retrieve pool of mbufs for RX\n");
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

    // Pool of direct mbufs for TX
    qconf->tx_queue.direct_pool = rte_mempool_lookup(PKTMBUF_POOL_DIRECT_TX_NAME);
    if (qconf->tx_queue.direct_pool == NULL) {
        RTE_LOG(ERR, POLLINIT, "Cannot retrieve pool of direct mbufs for TX\n");
        return -1;
    }

    // Pool of indirect mbufs for TX
    qconf->tx_queue.indirect_pool = rte_mempool_lookup(PKTMBUF_POOL_INDIRECT_TX_NAME);
    if (qconf->tx_queue.indirect_pool == NULL) {
        RTE_LOG(ERR, POLLINIT, "Cannot retrieve pool of indirect mbufs for TX\n");
        return -1;
    }

    return 0;
}

/* Setup the data structures needed to exchange packets with the app */
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

/* Retrieve the L4 switching table (initialized by the primary) */
static int setup_udp_table(void)
{
    const struct rte_memzone *sock_bind_table_mz;

    sock_bind_table_mz = rte_memzone_lookup(UDP_BIND_TABLE_NAME);
    if (sock_bind_table_mz == NULL) {
        RTE_LOG(ERR, POLLINIT, "Cannot retrieve L4 switching table memory\n");
        return -1;
    }
    sock_bind_table = sock_bind_table_mz->addr;

    return 0;
}

/* Initialize UDPDK packet poller (which runs in a separate process w.r.t. app) */
int poller_init(int argc, char *argv[])
{
    int retval;

    // Initialize EAL
    retval = rte_eal_init(argc, argv);
    if (retval < 0) {
        RTE_LOG(ERR, POLLINIT, "Cannot initialize EAL for poller\n");
        return -1;
    }

    // Setup memory allocators
    retval = setup_allocators();
    if (retval < 0) {
        RTE_LOG(ERR, POLLINIT, "Cannot setup allocators for poller\n");
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
    return udp_hdr->dst_port;
}

static inline unsigned long get_ipv4_dst_addr(struct rte_ipv4_hdr *ip_hdr)
{
    return ip_hdr->dst_addr;
}

// TODO reassemble() is given too much responsibility: decompose into multiple functions
static inline void reassemble(struct rte_mbuf *m, uint16_t portid, uint32_t queue,
                              struct lcore_queue_conf *qconf, uint64_t tms)
{
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_ip_frag_tbl *tbl;
    struct rte_ip_frag_death_row *dr;
    struct rx_queue *rxq;
    uint16_t udp_dst_port;
    unsigned long ip_dst_addr;
    int sock_id;
    bool delivered_once = false;
    bool delivered_last = false;

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
                // TODO must fix the IP header checksum as done in ip-sec example
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
    udp_dst_port = get_udp_dst_port((struct rte_udp_hdr *)(ip_hdr + 1));
    ip_dst_addr = get_ipv4_dst_addr(ip_hdr);

    // Find the sock_ids corresponding to the UDP dst port (L4 switching) and enqueue the packet to its queue
    list_t *binds = btable_get_bindings(udp_dst_port);
    if (binds == NULL) {
        RTE_LOG(WARNING, POLLBODY, "Dropping packet for port %d: no socket bound\n", ntohs(udp_dst_port));
        return;
    }
    list_iterator_t *it = list_iterator_new(binds, LIST_HEAD);
    list_node_t *node;
    while ((node = list_iterator_next(it))) {
        unsigned long ip_oth = ((struct bind_info *)(node->val))->ip_addr.s_addr;
        bool oth_reuseaddr = ((struct bind_info *)(node->val))->reuse_addr;
        bool oth_reuseport = ((struct bind_info *)(node->val))->reuse_port;
        // TODO the semantic should be more complex actually:
        //   if dest unicast and SO_REUSEPORT, should load balance
        //   if dest broadcast and SO_REUSEADDR or SO_REUSEPORT, should deliver to all
        // If matching
        if (likely((ip_dst_addr == ip_oth) || (ip_oth == INADDR_ANY))) {
            // Deliver to this socket
            enqueue_rx_packet(((struct bind_info *)(node->val))->sockfd, m);
            delivered_once = true;
            // If other socket may exist on the same port, keep scanning
            if (oth_reuseaddr || oth_reuseport) {
                m = rte_pktmbuf_clone(m, rxq->pool);
                delivered_last = false;
                continue;
            } else {
                delivered_last = true;
                break;
            }
        }
    }
    if (!delivered_last) {
        rte_pktmbuf_free(m);
    }
    if (!delivered_once) {
        RTE_LOG(WARNING, POLLBODY, "Dropped packet to port %d: no socket matching\n", ntohs(udp_dst_port));
    }
    list_iterator_destroy(it);
}

static inline void flush_tx_table(struct rte_mbuf **tx_mbuf_table, uint16_t tx_count)
{
    int tx_sent;
    tx_sent = rte_eth_tx_burst(PORT_TX, QUEUE_TX, tx_mbuf_table, tx_count);
    if (unlikely(tx_sent < tx_count)) {
        // Free unsent mbufs
        do {
            rte_pktmbuf_free(tx_mbuf_table[tx_sent]);
        } while (++tx_sent < tx_count);
    }
}

/* Packet polling routine */
void poller_body(void)
{
    unsigned lcore_id;
    uint64_t cur_tsc;
    struct lcore_queue_conf *qconf;
    struct rte_mbuf **rx_mbuf_table;
    struct rte_mbuf **tx_mbuf_table;
    struct rte_mbuf *pkt = NULL;
    const struct rte_ether_hdr *old_eth_hdr;
    struct rte_ether_hdr *new_eth_hdr;
    uint16_t rx_count = 0, tx_count = 0;
    uint64_t ol_flags;
    int n_fragments;
    int i, j;

    lcore_id = rte_lcore_id();
    qconf = &lcore_queue_conf[lcore_id];
    rx_mbuf_table = qconf->rx_queue.rx_mbuf_table;
    tx_mbuf_table = qconf->tx_queue.tx_mbuf_table;

    while (poller_alive) {
        // Get current timestamp (needed for reassembly)
        cur_tsc = rte_rdtsc();

        // Transmit packets to DPDK port 0 (queue 0)
        for (i = 0; i < NUM_SOCKETS_MAX; i++) {
            if (exch_zone_desc->slots[i].used) {
                while (tx_count < BURST_SIZE) {
                    // Try to dequeue one packet (and move to next slot if this was empty)
                    if (rte_ring_dequeue(exch_slots[i].tx_q, (void **)&pkt) < 0) {
                        break;
                    }
                    // Fragment the packet if needed
                    if (likely(pkt->pkt_len <= IPV4_MTU_DEFAULT)) {   // fragmentation not needed
                        tx_mbuf_table[tx_count] = pkt;
                        tx_count++;
                    } else {    // fragmentation needed
                        // Save the Ethernet header and strip it (because fragmentation applies from IPv4 header)
                        old_eth_hdr = rte_pktmbuf_mtod(pkt, const struct rte_ether_hdr *);
                        rte_pktmbuf_adj(pkt, (uint16_t)sizeof(struct rte_ether_hdr));
                        // Put the fragments in the TX table, one after the other starting from the pos of the last mbuf
                        n_fragments = rte_ipv4_fragment_packet(pkt, &tx_mbuf_table[tx_count],
                                (uint16_t)(TX_MBUF_TABLE_SIZE - tx_count), IPV4_MTU_DEFAULT,
                                qconf->tx_queue.direct_pool, qconf->tx_queue.indirect_pool);
                        // Free the original mbuf
                        rte_pktmbuf_free(pkt);
                        // Checksum must be recomputed
                        ol_flags = (PKT_TX_IPV4 | PKT_TX_IP_CKSUM);
                        if (unlikely(n_fragments < 0)) {
                            RTE_LOG(ERR, POLLBODY, "Failed to fragment a packet\n");
                            break;
                        }
                        // Re-attach (and adjust) the Ethernet header to each fragment
                        for (j = tx_count; j < tx_count + n_fragments; j++) {
                            pkt = tx_mbuf_table[j];
                            new_eth_hdr = (struct rte_ether_hdr *)rte_pktmbuf_prepend(pkt, sizeof(struct rte_ether_hdr));
                            if (unlikely(new_eth_hdr == NULL)) {
                                RTE_LOG(ERR, POLLBODY, "mbuf has no room to rebuild the Ethernet header\n");
                                for (int k = tx_count; k < tx_count + n_fragments; k++) {
                                    rte_pktmbuf_free(tx_mbuf_table[k]);
                                }
                                tx_count -= n_fragments;
                                break;
                            }
                            new_eth_hdr->ether_type = old_eth_hdr->ether_type;
                            rte_ether_addr_copy(&old_eth_hdr->s_addr, &new_eth_hdr->s_addr);
                            rte_ether_addr_copy(&old_eth_hdr->d_addr, &new_eth_hdr->d_addr);
                            pkt->packet_type = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP;
                            pkt->ol_flags |= ol_flags;
                            pkt->l2_len = sizeof(struct rte_ether_hdr);
                            pkt->l3_len = sizeof(struct rte_ipv4_hdr);
                        }
                        tx_count += n_fragments;
                    }
                }
                // If a batch of packets is ready, send it
                if (tx_count >= BURST_SIZE) {
                    flush_tx_table(tx_mbuf_table, tx_count);
                    tx_count = 0;
                }
            }
        }
        // Flush remaining packets (otherwise we'd need a timeout to ensure progress for sporadic traffic)
        if (tx_count > 0) {
            flush_tx_table(tx_mbuf_table, tx_count);
            tx_count = 0;
        }

        // Receive packets from DPDK port 0 (queue 0)   TODO use more queues (RSS)
        rx_count = rte_eth_rx_burst(PORT_RX, QUEUE_RX, rx_mbuf_table, RX_MBUF_TABLE_SIZE);

        if (likely(rx_count > 0)) {
            // Prefetch some packets (to reduce cache misses later)
            for (j = 0; j < PREFETCH_OFFSET && j < rx_count; j++) {
                rte_prefetch0(rte_pktmbuf_mtod(rx_mbuf_table[j], void *));
            }

            // Prefetch the remaining packets, and reassemble the first ones
            for (j = 0; j < (rx_count - PREFETCH_OFFSET); j++) {
                rte_prefetch0(rte_pktmbuf_mtod(rx_mbuf_table[j + PREFETCH_OFFSET], void *));
                reassemble(rx_mbuf_table[j], PORT_RX, QUEUE_RX, qconf, cur_tsc);
            }

            // Reassemble the second batch of fragments
            for (; j < rx_count; j++) {
                reassemble(rx_mbuf_table[j], PORT_RX, QUEUE_RX, qconf, cur_tsc);
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
    RTE_LOG(INFO, POLLBODY, "Polling process exiting.\n");
    exit(0);
}
