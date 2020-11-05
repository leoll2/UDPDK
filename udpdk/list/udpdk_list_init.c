#include "udpdk_list.h"
#include "udpdk_shmalloc.h"

extern const void *udpdk_list_t_alloc;
extern const void *udpdk_list_node_t_alloc;
extern const void *udpdk_list_iterator_t_alloc;

void udpdk_list_init(void)
{
    udpdk_list_t_alloc = udpdk_init_allocator("udpdk_list_t_alloc", UDP_MAX_PORT, sizeof(udpdk_list_t));
    udpdk_list_node_t_alloc = udpdk_init_allocator("udpdk_list_node_t_alloc", NUM_SOCKETS_MAX, sizeof(udpdk_list_node_t));
    udpdk_list_iterator_t_alloc = udpdk_init_allocator("udpdk_list_iterator_t_alloc", 10, sizeof(udpdk_list_iterator_t));
}

int udpdk_list_reinit(void)
{
    udpdk_list_t_alloc = udpdk_retrieve_allocator("udpdk_list_t_alloc");
    if (udpdk_list_t_alloc == NULL) {
        return -1;
    }
    udpdk_list_node_t_alloc = udpdk_retrieve_allocator("udpdk_list_node_t_alloc");
    if (udpdk_list_node_t_alloc == NULL) {
        return -1;
    }
    udpdk_list_iterator_t_alloc = udpdk_retrieve_allocator("udpdk_list_iterator_t_alloc");
    if (udpdk_list_iterator_t_alloc == NULL) {
        return -1;
    }
    return 0;
}

void udpdk_list_deinit(void)
{
    udpdk_destroy_allocator(udpdk_list_t_alloc);
    udpdk_destroy_allocator(udpdk_list_node_t_alloc);
    udpdk_destroy_allocator(udpdk_list_iterator_t_alloc);
}
