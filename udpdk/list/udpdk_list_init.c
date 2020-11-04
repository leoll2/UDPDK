#include "udpdk_list.h"
#include "udpdk_shmalloc.h"

extern const void *list_t_alloc;
extern const void *list_node_t_alloc;
extern const void *list_iterator_t_alloc;

void udpdk_list_init(void)
{
    list_t_alloc = udpdk_init_allocator("list_t_alloc", UDP_MAX_PORT, sizeof(list_t));
    list_node_t_alloc = udpdk_init_allocator("list_node_t_alloc", NUM_SOCKETS_MAX, sizeof(list_node_t));
    list_iterator_t_alloc = udpdk_init_allocator("list_iterator_t_alloc", 10, sizeof(list_iterator_t));
}

void udpdk_list_deinit(void)
{
    udpdk_destroy_allocator(list_t_alloc);
    udpdk_destroy_allocator(list_node_t_alloc);
    udpdk_destroy_allocator(list_iterator_t_alloc);
}
