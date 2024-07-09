#include <stdlib.h>

#include "cmark-gfm.h"

cmark_llist *cmark_llist_append(cmark_mem *mem, cmark_llist *head, void *data) {
  cmark_llist *tmp;
  cmark_llist *new_node = (cmark_llist *) mem->calloc(1, sizeof(cmark_llist));

  new_node->data = data;
  new_node->next = NULL;

  if (!head)
    return new_node;

  for (tmp = head; tmp->next; tmp=tmp->next);

  tmp->next = new_node;

  return head;
}

void cmark_llist_free_full(cmark_mem *mem, cmark_llist *head, cmark_free_func free_func) {
  cmark_llist *tmp, *prev;

  for (tmp = head; tmp;) {
    if (free_func)
      free_func(mem, tmp->data);

    prev = tmp;
    tmp = tmp->next;
    mem->free(prev);
  }
}

void cmark_llist_free(cmark_mem *mem, cmark_llist *head) {
  cmark_llist_free_full(mem, head, NULL);
}
