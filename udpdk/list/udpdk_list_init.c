#include "udpdk_list.h"
#include "udpdk_shmalloc.h"

extern const void *list_t_alloc;
extern const void *list_node_t_alloc;
extern const void *list_iterator_t_alloc;
extern const void *bind_info_alloc;

void udpdk_list_init(void)
{
    list_t_alloc = udpdk_init_allocator("list_t_alloc", UDP_MAX_PORT, sizeof(list_t));
    list_node_t_alloc = udpdk_init_allocator("list_node_t_alloc", NUM_SOCKETS_MAX, sizeof(list_node_t));
    list_iterator_t_alloc = udpdk_init_allocator("list_iterator_t_alloc", 10, sizeof(list_iterator_t));
}

int udpdk_list_reinit(void)
{
    list_t_alloc = udpdk_retrieve_allocator("list_t_alloc");
    if (list_t_alloc == NULL) {
        return -1;
    }
    list_node_t_alloc = udpdk_retrieve_allocator("list_node_t_alloc");
    if (list_node_t_alloc == NULL) {
        return -1;
    }
    list_iterator_t_alloc = udpdk_retrieve_allocator("list_iterator_t_alloc");
    if (list_iterator_t_alloc == NULL) {
        return -1;
    }
    bind_info_alloc = udpdk_retrieve_allocator("bind_info_alloc");
    if (bind_info_alloc == NULL) {
        return -1;
    }
    return 0;
}

void udpdk_list_deinit(void)
{
    udpdk_destroy_allocator(list_t_alloc);
    udpdk_destroy_allocator(list_node_t_alloc);
    udpdk_destroy_allocator(list_iterator_t_alloc);
}
