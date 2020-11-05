//
// Created by leoll2 on 11/05/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//

#include <stdlib.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_log.h>

#include "udpdk_monitor.h"

#define RTE_LOGTYPE_MONITOR RTE_LOGTYPE_USER1


/* Show information about the current state of a DPDK port */
void check_port_link_status(uint16_t portid) {
#define CHECK_INTERVAL 100  // 100ms
#define MAX_CHECK_TIME 90   // how many times
    uint8_t count, all_ports_up, print_flag = 0;
    struct rte_eth_link link;
    int ret;

    RTE_LOG(INFO, MONITOR, "Checking link status of port %d.\n", portid);
    for (count = 0; count <= MAX_CHECK_TIME; count++) {
        all_ports_up = 1;
        memset(&link, 0, sizeof(link));
        ret = rte_eth_link_get_nowait(portid, &link);
        if (ret < 0) {
            all_ports_up = 0;
            if (print_flag == 1)
                RTE_LOG(WARNING, MONITOR, "Port %u link get failed: %s\n", portid, rte_strerror(-ret));
            continue;
        }
        if (print_flag == 1) {
            if (link.link_status) {
                RTE_LOG(INFO, MONITOR, "Port %d Link Up - speed %u Mbps - %s\n", portid, (unsigned) link.link_speed,
                        (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? ("full-duplex") : ("half-duplex\n"));
                break;
            } else {
                RTE_LOG(INFO, MONITOR, "Port %d Link Down\n", (uint8_t) portid);
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
