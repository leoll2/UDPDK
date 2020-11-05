
//
// node.c
//
// Copyright (c) 2010 TJ Holowaychuk <tj@vision-media.ca>
//

#include "udpdk_list.h"
#include "udpdk_shmalloc.h"

extern void *udpdk_list_node_t_alloc;

/*
 * Allocates a new udpdk_list_node_t. NULL on failure.
 */

udpdk_list_node_t *
list_node_new(void *val) {
  udpdk_list_node_t *self;
  if (!(self = udpdk_shmalloc(udpdk_list_node_t_alloc)))
    return NULL;
  self->prev = NULL;
  self->next = NULL;
  self->val = val;
  return self;
}
