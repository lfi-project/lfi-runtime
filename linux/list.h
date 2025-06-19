#pragma once

#include <stddef.h>

#define LIST_CONTAINER(type, field, ptr) \
    ((type *) (((char *) (ptr)) - offsetof(type, field)))

struct List {
    struct List *next;
    struct List *prev;
};

static inline void
list_init(struct List *l)
{
    l->next = l;
    l->prev = l;
}

static inline struct List *
list_first(struct List *list) {
  struct List *first = 0;
  if (list)
      first = list->next;
  return first;
}

static inline struct List *
list_next(struct List *list, struct List *e) {
  struct List *next = 0;
  if (e != list)
      next = e->next;
  return next;
}

void
list_make_first(struct List **list, struct List *e);

void
list_remove(struct List **list, struct List *e);
