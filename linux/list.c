// This list implementation is from jart/blink.
//
// ISC License
//
// Copyright 2022 Justine Alexandra Roberts Tunney
//
// Permission to use, copy, modify, and/or distribute this software for
// any purpose with or without fee is hereby granted, provided that the
// above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
// WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
// AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
// DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
// PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include "list.h"

static void
splice_after(struct List *elem, struct List *succ)
{
    struct List *tmp1, *tmp2;
    tmp1 = elem->next;
    tmp2 = succ->prev;
    elem->next = succ;
    succ->prev = elem;
    tmp2->next = tmp1;
    tmp1->prev = tmp2;
}

void
list_remove(struct List **list, struct List *elem)
{
    if (*list == elem) {
        if ((*list)->prev == *list) {
            *list = 0;
        } else {
            *list = (*list)->prev;
        }
    }
    elem->next->prev = elem->prev;
    elem->prev->next = elem->next;
    elem->next = elem;
    elem->prev = elem;
}

void
list_make_first(struct List **list, struct List *elem)
{
    if (elem) {
        if (!*list) {
            *list = elem->prev;
        } else {
            splice_after(*list, elem);
        }
    }
}
