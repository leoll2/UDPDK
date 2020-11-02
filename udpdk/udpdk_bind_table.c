//
// Created by leoll2 on 9/27/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
// Data structure to hold (ip, port) pairs of bound sockets
// It is an array of size MAX_PORTS of lists; each list contains all
// the IPs bound to that port (typically one, but can be many)
//

#include "udpdk_bind_table.h"

void *bind_info_alloc = NULL;
list_t *sock_bind_table[UDP_MAX_PORT];

/* Initialize the bindings table */
inline void btable_init(void)
{
    // Create the allocator for bind_info elements
    bind_info_alloc = udpdk_init_allocator("list_t_alloc", NUM_SOCKETS_MAX, sizeof(struct bind_info));
    
    // All ports are initially free
    for (unsigned i = 0; i < UDP_MAX_PORT; i++) {
        sock_bind_table[i] = NULL;
    }
}

/* Get the index of a free port (-1 if none available) */
inline int btable_get_free_port(void)
{
    for (unsigned i = 0; i < UDP_MAX_PORT; i++) {
        if (sock_bind_table[i] == NULL) {
            return i;
        }
    }
    return -1;
}

/* Verify if binding the pair (ip, port) is possible, provided the
 * options and the previous bindings.
 */
static inline int btable_can_bind(struct in_addr ip, int port, int opts)
{
    bool reuse_addr = opts & SO_REUSEADDR;
    bool reuse_port = opts & SO_REUSEPORT;
    bool can_bind = true;
    list_iterator_t *it;
    list_node_t *node;
    unsigned long ip_oth, ip_new;
    int opts_oth;

    if (sock_bind_table[port] == NULL) {
        return 0;
    }

    ip_new = ip.s_addr;

    it = list_iterator_new(sock_bind_table[port], LIST_HEAD);
    while ((node = list_iterator_next(it))) {
        ip_oth = node->val.ip_addr.s_addr;
        opts_oth = node->val.opts;
        // If different, and none is INADDR_ANY, continue
        if ((ip_oth != ip_new) && (ip_oth != INADDR_ANY) && (ip_new != INADDRY_ANY)) {
            continue;
        }
        // If different, one is INADDR_ANY, and the new has SO_REUSEADDR or SO_REUSEPORT, continue
        if ((ip_oth != ip_new) && ((ip_oth == INADDR_ANY) || (ip_new != INADDRY_ANY)) 
                && ((opts & SO_REUSEADDR) || (opts & SO_REUSEPORT))) {
            continue;
        }
        // If same, not INADDR_ANY and both have SO_REUSEPORT, continue
        if ((ip_oth == ip_new) && (ip_new != INADDR_ANY) 
                && (opts & SO_REUSEPORT) && (ops_oth & SO_REUSEPORT)) {
            continue;
        }
        can_bind = false;
        break;
    }

    list_iterator_destroy(it);
    if (can_bind) {
        return 0;
    } else {
        return -1;
    }
}

inline int btable_add_binding(int s, struct in_addr ip, int port, int opts)
{
    struct bind_info *b;
    list_node_t *ln;

    // Check if binding this pair is allowed
    if (!btable_can_bind(ip, port, opts)) {
        return -1;
    }

    // Allocate the list if missing
    if (sock_bind_table[port] == NULL) {
        sock_bind_table[port] = list_new();
    }

    // Allocate and setup a new bind_info element
    b = (struct bind_info)udpdk_shmalloc(bind_info_alloc);
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
}

/* Remove a binding from the port */
void btable_del_binding(int s, int port) {
    // TODO rimuovi dalla lista
    // TODO free bind_info memory
    // TODO se la lista è vuota, dealloca la lista -> porta torna libera
}

/* Get all the bind_info descriptors of the sockets bound to the given port */
list_t *btable_get_bindings(int port) {
    return sock_bind_table[port];
}
