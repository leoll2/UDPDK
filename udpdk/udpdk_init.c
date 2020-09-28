//
// Created by leoll2 on 9/25/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memzone.h>

#include "udpdk_api.h"
#include "udpdk_constants.h"
#include "udpdk_lookup_table.h"
#include "udpdk_poller.h"
#include "udpdk_types.h"

#define RTE_LOGTYPE_INIT RTE_LOGTYPE_USER1

extern struct exch_zone_info *exch_zone_desc;
struct exch_slot *exch_slots = NULL;
extern htable_item *udp_port_table;
static struct rte_mempool *pktmbuf_pool;
static pid_t poller_pid;

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

/* Initialize a pool of mbuf for reception and transmission */
static int init_mbuf_pool(void)
{
    const unsigned int num_mbufs_rx = NUM_RX_DESC_DEFAULT;
    const unsigned int num_mbufs_tx = NUM_TX_DESC_DEFAULT;
    const unsigned int num_mbufs_cache = 2 * MBUF_CACHE_SIZE;
    const unsigned int num_mbufs = num_mbufs_rx + num_mbufs_tx + num_mbufs_cache;

    pktmbuf_pool = rte_pktmbuf_pool_create(PKTMBUF_POOL_NAME, num_mbufs, MBUF_CACHE_SIZE, 0,
            RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    return pktmbuf_pool == NULL;   // 0  on success
}

/* Initialize a DPDK port */
static int init_port(uint16_t port_num)
{
    // TODO add support for multiple rings
    const struct rte_eth_conf port_conf = {
        .rxmode = {
            .mq_mode = ETH_MQ_RX_RSS
        }
    };
    const uint16_t rx_rings = 1;
    const uint16_t tx_rings = 1;
    uint16_t rx_ring_size = NUM_RX_DESC_DEFAULT;
    uint16_t tx_ring_size = NUM_TX_DESC_DEFAULT;
    uint16_t q;
    int retval;

    // Configure mode and number of rings
    retval = rte_eth_dev_configure(port_num, rx_rings, tx_rings, &port_conf);
    if (retval != 0) {
        RTE_LOG(ERR, INIT, "Could not configure port %d\n", port_num);
        return retval;
    }

    // Adjust the number of descriptors
    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_num, &rx_ring_size, &tx_ring_size);
    if (retval != 0) {
        RTE_LOG(ERR, INIT, "Could not adjust rx/tx descriptors on port %d\n", port_num);
        return retval;
    }

    // Setup the RX queues
    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(port_num, q, rx_ring_size,
                rte_eth_dev_socket_id(port_num), NULL, pktmbuf_pool);
        if (retval < 0) {
            RTE_LOG(ERR, INIT, "Could not setup RX queue %d on port %d\n", q, port_num);
            return retval;
        }
    }

    // Setup the TX queues
    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port_num, q, tx_ring_size,
                rte_eth_dev_socket_id(port_num), NULL); // no particular configuration needed
        if (retval < 0) {
            RTE_LOG(ERR, INIT, "Could not setup TX queue %d on port %d\n", q, port_num);
            return retval;
        }
    }

    // Enable promiscuous mode
    retval = rte_eth_promiscuous_enable(port_num);
    if (retval < 0) {
        RTE_LOG(ERR, INIT, "Could not set port %d to promiscous mode\n", port_num);
        return retval;
    }

    // Start the DPDK port
    retval = rte_eth_dev_start(port_num);
    if (retval < 0) {
        RTE_LOG(ERR, INIT, "Could not start port %d\n", port_num);
        return retval;
    }

    RTE_LOG(INFO, INIT, "Initialized port %d.\n", port_num);
    return 0;
}

static void check_port_link_status(uint16_t portid) {
#define CHECK_INTERVAL 100  // 100ms
#define MAX_CHECK_TIME 90   // how many times
    uint8_t count, all_ports_up, print_flag = 0;
    struct rte_eth_link link;
    int ret;

    RTE_LOG(INFO, INIT, "Checking link status of port %d.\n", portid);
    for (count = 0; count <= MAX_CHECK_TIME; count++) {
        all_ports_up = 1;
        memset(&link, 0, sizeof(link));
        ret = rte_eth_link_get_nowait(portid, &link);
        if (ret < 0) {
            all_ports_up = 0;
            if (print_flag == 1)
                RTE_LOG(WARNING, INIT, "Port %u link get failed: %s\n", portid, rte_strerror(-ret));
            continue;
        }
        if (print_flag == 1) {
            if (link.link_status) {
                RTE_LOG(INFO, INIT, "Port %d Link Up - speed %u Mbps - %s\n", portid, (unsigned) link.link_speed,
                        (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? ("full-duplex") : ("half-duplex\n"));
                break;
            } else {
                RTE_LOG(INFO, INIT, "Port %d Link Down\n", (uint8_t) portid);
            }
            continue;
        }
        if (link.link_status == ETH_LINK_DOWN) {
            all_ports_up = 0;
            break;
        }
        if (print_flag == 1)
            return;

        if (all_ports_up == 0) {
            printf(".");
            fflush(stdout);
            rte_delay_ms(CHECK_INTERVAL);
        }

        if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
            print_flag = 1;
        }
    }
}

/* Initialize a shared memory region to contain descriptors for the exchange slots */
static int init_shared_memzone(void)
{
    const struct rte_memzone *mz;

    mz = rte_memzone_reserve(EXCH_MEMZONE_NAME, sizeof(*exch_zone_desc), rte_socket_id(), 0);
    if (mz == NULL) {
        RTE_LOG(ERR, INIT, "Cannot allocate shared memory for exchange slot descriptors\n");
        return -1;
    }
    memset(mz->addr, 0, sizeof(*exch_zone_desc));
    exch_zone_desc = mz->addr;

    // TODO any field to set to a specific value?
    return 0;
}

/* Initialize table in shared memory for UDP port switching */
static int init_udp_table(void)
{
    const struct rte_memzone *mz;

    mz = rte_memzone_reserve(UDP_PORT_TABLE_NAME, NUM_SOCKETS_MAX * sizeof(htable_item), rte_socket_id(), 0);
    if (mz == NULL) {
        RTE_LOG(ERR, INIT, "Cannot allocate shared memory for UDP port switching table\n");
        return -1;
    }
    udp_port_table = mz->addr;
    htable_init(udp_port_table);
    return 0;
}

/* Initialize slots to exchange packets between the application and the poller */
static int init_exchange_slots(void)
{
    // TODO allocate slots dynamically in udpdk_socket() instead of pre-allocating all them statically
    unsigned i;
    unsigned socket_id;
    const char *q_name;

    socket_id = rte_socket_id();

    // Allocate enough memory to store the exchange slots
    exch_slots = rte_malloc(EXCH_SLOTS_NAME, sizeof(*exch_slots) * NUM_SOCKETS_MAX, 0);
    if (exch_slots == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate memory for exchange slots\n");

    // Create a rte_ring for each RX and TX slot
    for (i = 0; i < NUM_SOCKETS_MAX; i++) {
        q_name = get_exch_ring_name(i, EXCH_RING_RX);
        exch_slots[i].rx_q = rte_ring_create(q_name, EXCH_RING_SIZE, socket_id, RING_F_SP_ENQ | RING_F_SC_DEQ);
        q_name = get_exch_ring_name(i, EXCH_RING_TX);
        exch_slots[i].tx_q = rte_ring_create(q_name, EXCH_RING_SIZE, socket_id, RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (exch_slots[i].rx_q == NULL || exch_slots[i].tx_q == NULL) {
            RTE_LOG(ERR, INIT, "Cannot create exchange RX/TX exchange rings (index %d)\n", i);
            return -1;
        }
    }
    return 0;
}

/* Initialize UDPDK */
int udpdk_init(int argc, char *argv[])
{
    int retval;

    // Start the secondary process
    poller_pid = fork();
    if (poller_pid != 0) {
        // application

        // Initialize EAL (returns how many arguments it consumed)
        retval = rte_eal_init(argc, argv);
        if (retval < 0) {
            RTE_LOG(ERR, INIT, "Cannot initialize EAL\n");
            return -1;
        }
        argc -= retval;
        argv += retval;

        // Initialize pool of mbuf
        retval = init_mbuf_pool();
        if (retval < 0) {
            RTE_LOG(ERR, INIT, "Cannot initialize pool of mbufs\n");
            return -1;
        }

        // Initialize DPDK ports
        retval = init_port(PORT_RX);
        if (retval < 0) {
            RTE_LOG(ERR, INIT, "Cannot initialize RX port %d\n", PORT_RX);
            return -1;
        }
        check_port_link_status(PORT_RX);

        if (PORT_TX != PORT_RX) {
            retval = init_port(PORT_TX);
            if (retval < 0) {
                RTE_LOG(ERR, INIT, "Cannot initialize TX port %d\n", PORT_TX);
                return -1;
            }
            check_port_link_status(PORT_TX);
        } else {
            RTE_LOG(INFO, INIT, "Using the same port for RX and TX\n");
        }

        // Initialize memzone for exchange
        retval = init_shared_memzone();
        if (retval < 0) {
            RTE_LOG(ERR, INIT, "Cannot initialize memzone for exchange zone descriptors\n");
            return -1;
        }

        retval = init_udp_table();
        if (retval < 0) {
            RTE_LOG(ERR, INIT, "Cannot create table for UDP port switching\n");
            return -1;
        }

        retval = init_exchange_slots();
        if (retval < 0) {
            RTE_LOG(ERR, INIT, "Cannot initialize exchange slots\n");
            return -1;
        }
        // TODO initialize shared structures
    } else {
        // child -> packet poller
        // TODO the arguments should come from a config rather than being hardcoded
        int poller_argc = 6;
        char *poller_argv[6] = {
                "./testapp",
                "-l",
                "3-4",
                "-n",
                "2",
                "--proc-type=secondary"
        };
        sleep(1);   // TODO use some synchronization mechanism between primary and secondary
        if (poller_init(poller_argc, poller_argv) < 0) {
            return -1;
        }
        poller_body();
    }
    // The parent process (application) returns immediately from init; instead, poller doesn't till it dies (or error)
    return 0;
}

void udpdk_cleanup(void)
{
    uint16_t port_id;
    pid_t pid;

    // Kill the poller process
    RTE_LOG(INFO, INIT, "Killing the poller process (%d)...\n", poller_pid);
    kill(poller_pid, SIGTERM);
    pid = waitpid(poller_pid, NULL, 0);
    if (pid < 0) {
        RTE_LOG(WARNING, INIT, "Failed killing the poller process\n");
    } else {
        RTE_LOG(INFO, INIT, "...killed!\n");
    }

    // Stop and close DPDK ports
    RTE_ETH_FOREACH_DEV(port_id) {
        rte_eth_dev_stop(port_id);
        rte_eth_dev_close(port_id);
    }
}