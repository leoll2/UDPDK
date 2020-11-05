
//
// iterator.c
//
// Copyright (c) 2010 TJ Holowaychuk <tj@vision-media.ca>
//

#include "udpdk_list.h"
#include "udpdk_shmalloc.h"

extern void *udpdk_list_iterator_t_alloc;

/*
 * Allocate a new udpdk_list_iterator_t. NULL on failure.
 * Accepts a direction, which may be LIST_HEAD or LIST_TAIL.
 */

udpdk_list_iterator_t *
list_iterator_new(udpdk_list_t *list, udpdk_list_direction_t direction) {
  udpdk_list_node_t *node = direction == LIST_HEAD
    ? list->head
    : list->tail;
  return list_iterator_new_from_node(node, direction);
}

/*
 * Allocate a new udpdk_list_iterator_t with the given start
 * node. NULL on failure.
 */

udpdk_list_iterator_t *
list_iterator_new_from_node(udpdk_list_node_t *node, udpdk_list_direction_t direction) {
  udpdk_list_iterator_t *self;
  if (!(self = udpdk_shmalloc(udpdk_list_iterator_t_alloc)))
    return NULL;
  self->next = node;
  self->direction = direction;
  return self;
}

/*
 * Return the next udpdk_list_node_t or NULL when no more
 * nodes remain in the list.
 */

udpdk_list_node_t *
list_iterator_next(udpdk_list_iterator_t *self) {
  udpdk_list_node_t *curr = self->next;
  if (curr) {
    self->next = self->direction == LIST_HEAD
      ? curr->next
      : curr->prev;
  }
  return curr;
}

/*
 * Free the list iterator.
 */

void
list_iterator_destroy(udpdk_list_iterator_t *self) {
  udpdk_shfree(udpdk_list_iterator_t_alloc, self);
  self = NULL;
}
