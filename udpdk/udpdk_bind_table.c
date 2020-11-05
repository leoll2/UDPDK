//
// Created by leoll2 on 9/27/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
// Data structure to hold (ip, port) pairs of bound sockets
// It is an array of size MAX_PORTS of lists; each list contains all
// the IPs bound to that port (typically one, but can be many)
//

#include <arpa/inet.h>      // inet_ntop
#include <netinet/in.h>     // INADDR_ANY
#include "udpdk_bind_table.h"
#include "udpdk_shmalloc.h"

#define RTE_LOGTYPE_BTABLE RTE_LOGTYPE_USER1

const void *bind_info_alloc = NULL;
list_t **sock_bind_table;

/* Initialize the bindings table */
void btable_init(void)
{
    // Create the allocator for bind_info elements
    bind_info_alloc = udpdk_init_allocator("bind_info_alloc", NUM_SOCKETS_MAX, sizeof(struct bind_info));
    
    // All ports are initially free
    for (unsigned i = 0; i < UDP_MAX_PORT; i++) {
        sock_bind_table[i] = NULL;
    }
}

/* Get the index of a free port (-1 if none available) */
int btable_get_free_port(void)
{
    for (unsigned i = 0; i < UDP_MAX_PORT; i++) {
        if (sock_bind_table[i] == NULL) {
            return i;
        }
    }
    RTE_LOG(WARNING, BTABLE, "Failed to find a free port\n");
    return -1;
}

/* Verify if binding the pair (ip, port) is possible, provided the
 * options and the previous bindings.
 */
static inline bool btable_can_bind(struct in_addr ip, int port, int opts)
{
    bool reuse_addr = opts & SO_REUSEADDR;
    bool reuse_port = opts & SO_REUSEPORT;
    bool can_bind = true;
    list_iterator_t *it;
    list_node_t *node;
    unsigned long ip_oth, ip_new;
    // bool oth_reuseaddr;
    bool oth_reuseport;

    if (sock_bind_table[port] == NULL) {
        return true;
    }

    ip_new = ip.s_addr;

    it = list_iterator_new(sock_bind_table[port], LIST_HEAD);
    while ((node = list_iterator_next(it))) {
        ip_oth = ((struct bind_info *)(node->val))->ip_addr.s_addr;
        // oth_reuseaddr = ((struct bind_info *)(node->val))->reuse_addr;
        oth_reuseport = ((struct bind_info *)(node->val))->reuse_port;
        // If different, and none is INADDR_ANY, continue
        if ((ip_oth != ip_new) && (ip_oth != INADDR_ANY) && (ip_new != INADDR_ANY)) {
            continue;
        }
        // If different, one is INADDR_ANY, and the new has SO_REUSEADDR or SO_REUSEPORT, continue
        if ((ip_oth != ip_new) && ((ip_oth == INADDR_ANY) || (ip_new != INADDR_ANY))
                && ((opts & SO_REUSEADDR) || (opts & SO_REUSEPORT))) {
            continue;
        }
        // If same, not INADDR_ANY and both have SO_REUSEPORT, continue
        if ((ip_oth == ip_new) && (ip_new != INADDR_ANY) 
                && (opts & SO_REUSEPORT) && oth_reuseport) {
            continue;
        }
        can_bind = false;
        break;
    }

    list_iterator_destroy(it);
    return can_bind;
}

/* Bind a socket to a (IP, port) pair */
int btable_add_binding(int s, struct in_addr ip, int port, int opts)
{
    struct bind_info *b;
    list_node_t *ln;

    // Check if binding this pair is allowed
    if (!btable_can_bind(ip, port, opts)) {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip, buf, sizeof(buf));
        RTE_LOG(WARNING, BTABLE, "Cannot bind socket %d to %s:%d\n", s, buf, ntohs(port));
        return -1;
    }

    // Allocate the list if missing
    if (sock_bind_table[port] == NULL) {
        sock_bind_table[port] = list_new();
    }

    // Allocate and setup a new bind_info element
    b = (struct bind_info *)udpdk_shmalloc(bind_info_alloc);
    b->sockfd = s;
    b->ip_addr = ip;
    b->reuse_addr = opts & SO_REUSEADDR;
    b->reuse_port = opts & SO_REUSEPORT;
    b->closed = false;

    // Insert the bind_info in the list
    ln = list_node_new(b);
    if (ip.s_addr == INADDR_ANY) {
        list_lpush(sock_bind_table[port], ln);
    } else {
        list_rpush(sock_bind_table[port], ln);
    }
    return 0;
}

/* Remove a binding from the port */
void btable_del_binding(int s, int port) {
    list_node_t *node;
    list_iterator_t *it;

    // Remove the binding from the list
    it = list_iterator_new(sock_bind_table[port], LIST_HEAD);
    while ((node = list_iterator_next(it))) {
        if (((struct bind_info *)(node->val))->sockfd == s) {
            udpdk_shfree(bind_info_alloc, node->val);
            list_remove(sock_bind_table[port], node);
            break;
        }
    }
    list_iterator_destroy(it);

    // If no more bindings left, free the port
    if (sock_bind_table[port]->len == 0) {
        list_destroy(sock_bind_table[port]);
        sock_bind_table[port] = NULL;
    }
}

/* Get all the bind_info descriptors of the sockets bound to the given port */
list_t *btable_get_bindings(int port) {
    return sock_bind_table[port];
}

/* Destroy the bindings table */
void btable_destroy(void)
{
    udpdk_destroy_allocator(bind_info_alloc);
}
