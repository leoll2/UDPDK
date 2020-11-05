
//
// list.h
//
// Copyright (c) 2010 TJ Holowaychuk <tj@vision-media.ca>
//

#ifndef __CLIBS_LIST_H__
#define __CLIBS_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "udpdk_list_init.h"

// Library version

#define LIST_VERSION "0.0.5"

// Memory management macros
#ifdef LIST_CONFIG_H
#define _STR(x) #x
#define STR(x) _STR(x)
#include STR(LIST_CONFIG_H)
#undef _STR
#undef STR
#endif

/*
 * udpdk_list_t iterator direction.
 */

typedef enum {
    LIST_HEAD
  , LIST_TAIL
} udpdk_list_direction_t;

/*
 * udpdk_list_t node struct.
 */

typedef struct list_node {
  struct list_node *prev;
  struct list_node *next;
  void *val;
} udpdk_list_node_t;

/*
 * udpdk_list_t struct.
 */

typedef struct {
  udpdk_list_node_t *head;
  udpdk_list_node_t *tail;
  unsigned int len;
  void (*free)(void *val);
  int (*match)(void *a, void *b);
} udpdk_list_t;

/*
 * udpdk_list_t iterator struct.
 */

typedef struct {
  udpdk_list_node_t *next;
  udpdk_list_direction_t direction;
} udpdk_list_iterator_t;

// Node prototypes.

udpdk_list_node_t *
list_node_new(void *val);

// udpdk_list_t prototypes.

udpdk_list_t *
list_new(void);

udpdk_list_node_t *
list_rpush(udpdk_list_t *self, udpdk_list_node_t *node);

udpdk_list_node_t *
list_lpush(udpdk_list_t *self, udpdk_list_node_t *node);

udpdk_list_node_t *
list_find(udpdk_list_t *self, void *val);

udpdk_list_node_t *
list_at(udpdk_list_t *self, int index);

udpdk_list_node_t *
list_rpop(udpdk_list_t *self);

udpdk_list_node_t *
list_lpop(udpdk_list_t *self);

void
list_remove(udpdk_list_t *self, udpdk_list_node_t *node);

void
list_destroy(udpdk_list_t *self);

// udpdk_list_t iterator prototypes.

udpdk_list_iterator_t *
list_iterator_new(udpdk_list_t *list, udpdk_list_direction_t direction);

udpdk_list_iterator_t *
list_iterator_new_from_node(udpdk_list_node_t *node, udpdk_list_direction_t direction);

udpdk_list_node_t *
list_iterator_next(udpdk_list_iterator_t *self);

void
list_iterator_destroy(udpdk_list_iterator_t *self);

#ifdef __cplusplus
}
#endif

#endif /* __CLIBS_LIST_H__ */
