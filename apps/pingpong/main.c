//
// Created by leoll2 on 10/4/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
// Options:
//  -f <func>  : function ('ping' or 'pong')
//

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <udpdk_api.h>

#define PORT_PING   10000
#define PORT_PONG   10001
#define IP_PONG     "172.31.100.1"

typedef enum {PING, PONG} app_mode;

static app_mode mode = PING;
static volatile int app_alive = 1;
static const char *progname;

static void signal_handler(int signum)
{
    printf("Caught signal %d in pingpong main process\n", signum);
    udpdk_interrupt(signum);
    app_alive = 0;
}

static void ping_body(void)
{
    struct sockaddr_in servaddr, destaddr;
    struct timespec ts, ts_msg, ts_now;
    int n;

    printf("PING mode\n");

    // Create a socket
    int sock;
    if ((sock = udpdk_socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Ping: socket creation failed");
        return;
    }
    // Bind it
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT_PING);
    if (udpdk_bind(sock, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "bind failed");
        return;
    }

    while (app_alive) {
        printf(("Application loop\n"));

        // Send ping
        printf("Sending ping\n");
        destaddr.sin_family = AF_INET;
        destaddr.sin_addr.s_addr = inet_addr(IP_PONG);
        destaddr.sin_port = htons(PORT_PONG);
        clock_gettime(CLOCK_REALTIME, &ts);
        udpdk_sendto(sock, (void *)&ts, sizeof(struct timespec), 0,
                (const struct sockaddr *) &destaddr, sizeof(destaddr));

        // Get pong response
        n = udpdk_recvfrom(sock, (void *)&ts_msg, sizeof(struct timespec), 0, NULL, NULL);
        if (n > 0) {
            clock_gettime(CLOCK_REALTIME, &ts_now);
            ts.tv_sec = ts_now.tv_sec - ts_msg.tv_sec;
            ts.tv_nsec = ts_now.tv_nsec - ts_msg.tv_nsec;
            if (ts.tv_nsec < 0) {
                ts.tv_nsec += 1000000000;
                ts.tv_sec--;
            }
            printf("Received pong; delta = %d.%09d seconds\n", (int)ts.tv_sec, (int)ts.tv_nsec);
        }

        sleep(1);
    }
}

static void pong_body(void)
{
    int sock, n;
    struct sockaddr_in servaddr, cliaddr;
    struct timespec ts_msg;

    printf("PONG mode\n");

    // Create a socket
    if ((sock = udpdk_socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Pong: socket creation failed");
        return;
    }
    // Bind it
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT_PONG);
    if (udpdk_bind(sock, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "Pong: bind failed");
        return;
    }

    while (app_alive) {
        // Bounce incoming packets
        int len = sizeof(cliaddr);
        n = udpdk_recvfrom(sock, (void *)&ts_msg, sizeof(struct timespec), 0, ( struct sockaddr *) &cliaddr, &len);
        if (n > 0) {
            udpdk_sendto(sock, (void *)&ts_msg, sizeof(struct timespec), 0, (const struct sockaddr *) &cliaddr, len);
        }
    }
}

static void usage(void)
{
    printf("%s -c CONFIG -f FUNCTION \n"
            " -c CONFIG: .ini configuration file"
            " -f FUNCTION: 'ping' or 'pong'\n"
            , progname);
}


static int parse_app_args(int argc, char *argv[])
{
    int c;

    progname = argv[0];

    while ((c = getopt(argc, argv, "c:f:")) != -1) {
        switch (c) {
            case 'c':
                // this is for the .ini cfg file needed by DPDK, not by the app
                break;
            case 'f':
                if (strcmp(optarg, "ping") == 0) {
                    mode = PING;
                } else if (strcmp(optarg, "pong") == 0) {
                    mode = PONG;
                } else {
                    fprintf(stderr, "Unsupported function %s (must be 'ping' or 'pong')\n", optarg);
                    return -1;
                }
                break;
            default:
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                usage();
                return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int retval;

    // Register signals for shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize UDPDK
    retval = udpdk_init(argc, argv);
    if (retval < 0) {
        goto pingpong_end;
        return -1;
    }
    sleep(2);
    printf("App: UDPDK Intialized\n");

    // Parse app-specific arguments
    printf("Parsing app arguments...\n");
    retval = parse_app_args(argc, argv);
    if (retval != 0) {
        goto pingpong_end;
        return -1;
    }


    if (mode == PING) {
        ping_body();
    } else {
        pong_body();
    }

pingpong_end:
    udpdk_interrupt(0);
    udpdk_cleanup();
    return 0;
}
