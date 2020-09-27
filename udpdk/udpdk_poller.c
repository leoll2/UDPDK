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
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

static volatile int poller_alive = 1;

/* Poller signal handler */
static void poller_sighandler(int sig)
{
    printf("Poller: received request to stop\n");
    poller_alive = 0;
}

/* Initialize UDPDK packet poller (runs in a separate process) */
int poller_init(int argc, char *argv[])
{
    int retval;

    // Initialize EAL
    if ((retval = rte_eal_init(argc, argv)) < 0) {
        return -1;
    }

    // Setup signals for termination
    signal(SIGINT, poller_sighandler);
    signal(SIGTERM, poller_sighandler);

    return 0;
}

/* Packet polling routine */
void poller_body(void)
{
    while (poller_alive) {
        printf("Poller: main body\n");
        sleep(1);
    }
    // Exit directly to avoid returning in the application main (as we forked)
    exit(0);
}