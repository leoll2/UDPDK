//
// Created by leoll2 on 10/4/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
// Options:
//  -f <func>  : function ('send' or 'recv')
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

#define PORT_SEND   10000
#define PORT_RECV   10001
#define IP_RECV     "172.31.100.1"

#define ETH_HDR_LEN 14
#define IP_HDR_LEN  20
#define UDP_HDR_LEN 8

const char mydata[2048] = {
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam varius semper faucibus. Vivamus consectetur pharetra massa. Nam at tellus semper, eleifend tellus elementum, pulvinar odio. Ut accumsan ligula ac ex rhoncus, nec pretium urna pharetra. Suspendisse potenti. Duis vel enim vel tellus dictum vehicula eu vitae elit. Etiam at lacus varius diam rutrum accumsan quis sed velit. Nulla tortor mi, congue sit amet malesuada sit amet, pellentesque ac mi. Nunc feugiat mi vitae turpis elementum maximus. Sed malesuada mi ac rhoncus condimentum. Nunc venenatis, libero a pharetra molestie, risus orci tempus arcu, sit amet molestie ipsum purus ac urna. Nunc nec ligula massa. Aenean ut libero ut erat tincidunt aliquet."
        "Praesent consequat, dui nec pellentesque dapibus, lorem orci placerat sem, consectetur egestas metus lacus eu purus. Sed scelerisque, lorem nec euismod laoreet, sem tellus tincidunt lorem, eu faucibus tortor massa at sapien. Maecenas commodo consectetur leo, non rhoncus sem ullamcorper ut. Proin in purus quis ante posuere rutrum sed ut nisl. Etiam pretium egestas purus sed egestas. Vivamus ut suscipit neque. Donec eleifend magna ut mauris pellentesque, ut lobortis neque pharetra. Phasellus vel erat condimentum diam vulputate rhoncus nec eu lacus. Suspendisse condimentum, magna eu pulvinar porta, nisi ligula accumsan urna, a suscipit turpis ex a elit. Proin ipsum dolor, ultrices at luctus eu, sagittis ac ligula."
        "Etiam faucibus mauris et efficitur maximus. Proin fringilla fringilla volutpat. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi velit elit, convallis et maximus a, placerat non nisl. Duis porttitor convallis odio. Integer aliquam aliquam tortor non blandit. Duis nec tortor suscipit, viverra felis nec, laoreet ex. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Fusce viverra nulla ut interdum dapibus. Cras vulputate luctus tellus, ut elementum quam gravida ut. Donec pulvinar nunc molestie turpis ullamcorper, at interdum magna molestie. Aenean pellentesque felis quis blandit in."
};

typedef enum {SEND, RECV} app_mode;

static app_mode mode = SEND;
static int pktlen = 64;
static volatile int app_alive = 1;
static const char *progname;

static void signal_handler(int signum)
{
    printf("Caught signal %d in sendrecv main process\n", signum);
    udpdk_interrupt(signum);
    app_alive = 0;
}

static void send_body(void)
{
    struct sockaddr_in servaddr, destaddr;
    int n;

    printf("SEND mode\n");

    // Create a socket
    int sock;
    if ((sock = udpdk_socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Send: socket creation failed");
        return;
    }
    // Bind it
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT_SEND);
    if (udpdk_bind(sock, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "Send: bind failed");
        return;
    }

    printf(("Entering send loop\n"));
    while (app_alive) {

        // Send packet
        destaddr.sin_family = AF_INET;
        destaddr.sin_addr.s_addr = inet_addr(IP_RECV);
        destaddr.sin_port = htons(PORT_RECV);
        udpdk_sendto(sock, (void *)mydata, pktlen - ETH_HDR_LEN - IP_HDR_LEN - UDP_HDR_LEN, 0,
                (const struct sockaddr *) &destaddr, sizeof(destaddr));

        // TODO rate
        sleep(1);
    }
}

static void recv_body(void)
{
    int sock, n;
    struct sockaddr_in servaddr, cliaddr;
    char buf[2048];

    printf("RECV mode\n");

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
    servaddr.sin_port = htons(PORT_RECV);
    if (udpdk_bind(sock, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "Pong: bind failed");
        return;
    }

    printf(("Entering recv loop\n"));
    while (app_alive) {
        // Bounce incoming packets
        int len = sizeof(cliaddr);
        n = udpdk_recvfrom(sock, (void *)buf, 2048, 0, ( struct sockaddr *) &cliaddr, &len);
        if (n > 0) {
            buf[n] = '\0';
            printf("Received payload of %d bytes: %s\n", n, buf);
        }
    }
}

static void usage(void)
{
    printf("%s -c CONFIG -f FUNCTION \n"
            " -c CONFIG: .ini configuration file"
            " -f FUNCTION: 'send' or 'recv'\n"
            " -l LEN: payload length (not including ETH, IPv4 and UDP headers) \n"
            , progname);
}


static int parse_app_args(int argc, char *argv[])
{
    int c;

    progname = argv[0];

    while ((c = getopt(argc, argv, "c:f:l:")) != -1) {
        switch (c) {
            case 'c':
                // this is for the .ini cfg file needed by DPDK, not by the app
                break;
            case 'f':
                if (strcmp(optarg, "send") == 0) {
                    mode = SEND;
                } else if (strcmp(optarg, "recv") == 0) {
                    mode = RECV;
                } else {
                    fprintf(stderr, "Unsupported function %s (must be 'send' or 'recv')\n", optarg);
                    return -1;
                }
                break;
            case 'l':
                pktlen = atoi(optarg);
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
        goto pktgen_end;
        return -1;
    }
    sleep(2);
    printf("App: UDPDK Intialized\n");

    // Parse app-specific arguments
    printf("Parsing app arguments...\n");
    retval = parse_app_args(argc, argv);
    if (retval != 0) {
        goto pktgen_end;
        return -1;
    }


    if (mode == SEND) {
        send_body();
    } else {
        recv_body();
    }

pktgen_end:
    udpdk_interrupt(0);
    udpdk_cleanup();
    return 0;
}
