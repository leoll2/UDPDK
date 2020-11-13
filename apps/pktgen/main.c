//
// Created by leoll2 on 10/4/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
// Options:
//  -f <func>  : function ('send' or 'recv')
//

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <udpdk_api.h>

#define BUSYWAIT

#define PORT_SEND   10000
#define PORT_RECV   10001
#define IP_RECV     "172.31.100.1"

#define ETH_HDR_LEN 14
#define IP_HDR_LEN  20
#define UDP_HDR_LEN 8

typedef struct stats_t {
    uint64_t bytes_sent;
    uint64_t bytes_recv;
    uint64_t bytes_sent_prev;
    uint64_t bytes_recv_prev;
} stats_t;

const char mydata[2048] = {
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam varius semper faucibus. Vivamus consectetur pharetra massa. Nam at tellus semper, eleifend tellus elementum, pulvinar odio. Ut accumsan ligula ac ex rhoncus, nec pretium urna pharetra. Suspendisse potenti. Duis vel enim vel tellus dictum vehicula eu vitae elit. Etiam at lacus varius diam rutrum accumsan quis sed velit. Nulla tortor mi, congue sit amet malesuada sit amet, pellentesque ac mi. Nunc feugiat mi vitae turpis elementum maximus. Sed malesuada mi ac rhoncus condimentum. Nunc venenatis, libero a pharetra molestie, risus orci tempus arcu, sit amet molestie ipsum purus ac urna. Nunc nec ligula massa. Aenean ut libero ut erat tincidunt aliquet."
        "Praesent consequat, dui nec pellentesque dapibus, lorem orci placerat sem, consectetur egestas metus lacus eu purus. Sed scelerisque, lorem nec euismod laoreet, sem tellus tincidunt lorem, eu faucibus tortor massa at sapien. Maecenas commodo consectetur leo, non rhoncus sem ullamcorper ut. Proin in purus quis ante posuere rutrum sed ut nisl. Etiam pretium egestas purus sed egestas. Vivamus ut suscipit neque. Donec eleifend magna ut mauris pellentesque, ut lobortis neque pharetra. Phasellus vel erat condimentum diam vulputate rhoncus nec eu lacus. Suspendisse condimentum, magna eu pulvinar porta, nisi ligula accumsan urna, a suscipit turpis ex a elit. Proin ipsum dolor, ultrices at luctus eu, sagittis ac ligula."
        "Etiam faucibus mauris et efficitur maximus. Proin fringilla fringilla volutpat. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Morbi velit elit, convallis et maximus a, placerat non nisl. Duis porttitor convallis odio. Integer aliquam aliquam tortor non blandit. Duis nec tortor suscipit, viverra felis nec, laoreet ex. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Fusce viverra nulla ut interdum dapibus. Cras vulputate luctus tellus, ut elementum quam gravida ut. Donec pulvinar nunc molestie turpis ullamcorper, at interdum magna molestie. Aenean pellentesque felis quis blandit in."
};

typedef enum {SEND, RECV} app_mode;

static app_mode mode = SEND;
static int pktlen = 64;
static bool hdr_stats = false;
static bool dump = false;
static uint64_t tx_rate = 0;
static struct timespec tx_period;
static volatile bool app_alive = true;
static const char *progname;

static void signal_handler(int signum)
{
    printf("Caught signal %d in sendrecv main process\n", signum);
    udpdk_interrupt(signum);
    app_alive = false;
}

static inline struct timespec timespec_add(struct timespec a, struct timespec b)
{
    struct timespec ret = { a.tv_sec + b.tv_sec, a.tv_nsec + b.tv_nsec };
    if (ret.tv_nsec >= 1000000000) {
        ret.tv_sec++;
        ret.tv_nsec -= 1000000000;
    }
    return ret;
}

static inline struct timespec timespec_sub(struct timespec a, struct timespec b)
{
	struct timespec ret = { a.tv_sec - b.tv_sec, a.tv_nsec - b.tv_nsec };
	if (ret.tv_nsec < 0) {
		ret.tv_sec--;
		ret.tv_nsec += 1000000000;
	}
	return ret;
}

static struct timespec wait_time(struct timespec ts)
{
#ifdef BUSYWAIT
    for (;;) {
        struct timespec w, cur;
        clock_gettime(CLOCK_REALTIME, &cur);
        w = timespec_sub(ts, cur);
        if (w.tv_sec < 0)
            return cur;
    }
#else
    while (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL) != 0);
    return ts;
#endif
}

static void send_body(stats_t *stats)
{
    struct sockaddr_in servaddr, destaddr;
    struct timespec t_next;
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

    tx_period.tv_sec = tx_period.tv_nsec = 0;
    if (tx_rate > 0) {
        uint64_t x = (uint64_t)1000000000 / (uint64_t)tx_rate;
        tx_period.tv_nsec = x;
        tx_period.tv_sec = tx_period.tv_nsec / 1000000000;
        tx_period.tv_nsec = tx_period.tv_nsec % 1000000000;

        clock_gettime(CLOCK_REALTIME, &t_next);
    }

    printf("Entering send loop\n");
    printf("Sending a packet every %ld.%09ld s\n", tx_period.tv_sec, tx_period.tv_nsec);

    while (app_alive) {
        int ret;

        // Wait for the right moment to send the packet
        if (tx_rate > 0) {
            t_next = timespec_add(t_next, tx_period);
            wait_time(t_next);
        }
        // Send packet
        destaddr.sin_family = AF_INET;
        destaddr.sin_addr.s_addr = inet_addr(IP_RECV);
        destaddr.sin_port = htons(PORT_RECV);
        ret = udpdk_sendto(sock, (void *)mydata, pktlen, 0,
                (const struct sockaddr *) &destaddr, sizeof(destaddr));
        if (ret > 0) {
            stats->bytes_sent += ret;
            if (hdr_stats) {
                stats->bytes_sent += ETH_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN;
            }
        }
    }
}

static void recv_body(stats_t *stats)
{
    int sock, n;
    struct sockaddr_in servaddr, cliaddr;
    char buf[2048];
    char clientname[100];

    printf("RECV mode\n");

    // Create a socket
    if ((sock = udpdk_socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Recv: socket creation failed");
        return;
    }
    // Bind it
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT_RECV);
    if (udpdk_bind(sock, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "Recv: bind failed");
        return;
    }

    printf(("Entering recv loop\n"));
    while (app_alive) {
        // Bounce incoming packets
        int len = sizeof(cliaddr);
        n = udpdk_recvfrom(sock, (void *)buf, 2048, 0, ( struct sockaddr *) &cliaddr, &len);
        if (n > 0) {
            stats->bytes_recv += n;
            if (hdr_stats) {
                stats->bytes_recv += ETH_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN;
            }
            if (dump) {
                buf[n] = '\0';
                printf("Received payload of %d bytes from %s port %d:\n%s\n", n,
                    inet_ntop(AF_INET,&cliaddr.sin_addr, clientname, sizeof(clientname)),
                    ntohs(cliaddr.sin_port), buf);
            }
        }
    }
}

static void usage(void)
{
    printf("%s -c CONFIG -f FUNCTION [-r RATE] [-l LEN] [-h] [-d] \n"
            " -c CONFIG: .ini configuration file"
            " -f FUNCTION: 'send' or 'recv'\n"
            " -r RATE: desired transmission rate in bytes"
            " -l LEN: payload length\n"
            " -h consider also the MAC, IPv4 and UDP headers bytes for tx_rate and stats\n"
            " -d dump the payload (ASCII)\n"
            , progname);
}


static int parse_app_args(int argc, char *argv[])
{
    int c;

    progname = argv[0];

    while ((c = getopt(argc, argv, "c:f:r:l:hd")) != -1) {
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
            case 'r':
                tx_rate = atoi(optarg);
                break;
            case 'l':
                pktlen = atoi(optarg);
                break;
            case 'h':
                hdr_stats = true;
                break;
            case 'd':
                dump = true;
                break;
            default:
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                usage();
                return -1;
        }
    }
    return 0;
}

static void reset_stats(stats_t *stats)
{
    stats->bytes_sent = 0;
    stats->bytes_recv = 0;
    stats->bytes_sent_prev = 0;
    stats->bytes_recv_prev = 0;
}

static void *stats_routine(void *arg)
{
    stats_t *stats = (stats_t *)arg;
    uint64_t tx_bps = 0, rx_bps = 0;

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    while (1) {
        tx_bps = stats->bytes_sent - stats->bytes_sent_prev;
        rx_bps = stats->bytes_recv - stats->bytes_recv_prev;
        printf("Sent: %ld bytes  %ld bps  |  Recv: %ld bytes  %ld bps\n",
                stats->bytes_sent, tx_bps, stats->bytes_recv, rx_bps);
        stats->bytes_sent_prev = stats->bytes_sent;
        stats->bytes_recv_prev = stats->bytes_recv;
        sleep(1);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t stats_thr;
    stats_t stats;
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

    // Start the thread to visualize statistics in real time
    reset_stats(&stats);
    if (pthread_create(&stats_thr, NULL, stats_routine, &stats)) {
        fprintf(stderr, "Error creating thread\n");
        return -1;
    }

    if (mode == SEND) {
        send_body(&stats);
    } else {
        recv_body(&stats);
    }

pktgen_end:
    // Halt the stats thread
    pthread_cancel(stats_thr);
    pthread_join(stats_thr, NULL);
    // Cleanup
    udpdk_interrupt(0);
    udpdk_cleanup();
    return 0;
}
