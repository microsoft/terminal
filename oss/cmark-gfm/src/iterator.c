#include <assert.h>
#include <stdlib.h>

#include "config.h"
#include "node.h"
#include "cmark-gfm.h"
#include "iterator.h"

cmark_iter *cmark_iter_new(cmark_node *root) {
  if (root == NULL) {
    return NULL;
  }
  cmark_mem *mem = root->content.mem;
  cmark_iter *iter = (cmark_iter *)mem->calloc(1, sizeof(cmark_iter));
  iter->mem = mem;
  iter->root = root;
  iter->cur.ev_type = CMARK_EVENT_NONE;
  iter->cur.node = NULL;
  iter->next.ev_type = CMARK_EVENT_ENTER;
  iter->next.node = root;
  return iter;
}

void cmark_iter_free(cmark_iter *iter) { iter->mem->free(iter); }

static bool S_is_leaf(cmark_node *node) {
  switch (node->type) {
  case CMARK_NODE_HTML_BLOCK:
  case CMARK_NODE_THEMATIC_BREAK:
  case CMARK_NODE_CODE_BLOCK:
  case CMARK_NODE_TEXT:
  case CMARK_NODE_SOFTBREAK:
  case CMARK_NODE_LINEBREAK:
  case CMARK_NODE_CODE:
  case CMARK_NODE_HTML_INLINE:
    return 1;
  }
  return 0;
}

cmark_event_type cmark_iter_next(cmark_iter *iter) {
  cmark_event_type ev_type = iter->next.ev_type;
  cmark_node *node = iter->next.node;

  iter->cur.ev_type = ev_type;
  iter->cur.node = node;

  if (ev_type == CMARK_EVENT_DONE) {
    return ev_type;
  }

  /* roll forward to next item, setting both fields */
  if (ev_type == CMARK_EVENT_ENTER && !S_is_leaf(node)) {
    if (node->first_child == NULL) {
      /* stay on this node but exit */
      iter->next.ev_type = CMARK_EVENT_EXIT;
    } else {
      iter->next.ev_type = CMARK_EVENT_ENTER;
      iter->next.node = node->first_child;
    }
  } else if (node == iter->root) {
    /* don't move past root */
    iter->next.ev_type = CMARK_EVENT_DONE;
    iter->next.node = NULL;
  } else if (node->next) {
    iter->next.ev_type = CMARK_EVENT_ENTER;
    iter->next.node = node->next;
  } else if (node->parent) {
    iter->next.ev_type = CMARK_EVENT_EXIT;
    iter->next.node = node->parent;
  } else {
    assert(false);
    iter->next.ev_type = CMARK_EVENT_DONE;
    iter->next.node = NULL;
  }

  return ev_type;
}

void cmark_iter_reset(cmark_iter *iter, cmark_node *current,
                      cmark_event_type event_type) {
  iter->next.ev_type = event_type;
  iter->next.node = current;
  cmark_iter_next(iter);
}

cmark_node *cmark_iter_get_node(cmark_iter *iter) { return iter->cur.node; }

cmark_event_type cmark_iter_get_event_type(cmark_iter *iter) {
  return iter->cur.ev_type;
}

cmark_node *cmark_iter_get_root(cmark_iter *iter) { return iter->root; }

void cmark_consolidate_text_nodes(cmark_node *root) {
  if (root == NULL) {
    return;
  }
  cmark_iter *iter = cmark_iter_new(root);
  cmark_strbuf buf = CMARK_BUF_INIT(iter->mem);
  cmark_event_type ev_type;
  cmark_node *cur, *tmp, *next;

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    if (ev_type == CMARK_EVENT_ENTER && cur->type == CMARK_NODE_TEXT &&
        cur->next && cur->next->type == CMARK_NODE_TEXT) {
      cmark_strbuf_clear(&buf);
      cmark_strbuf_put(&buf, cur->as.literal.data, cur->as.literal.len);
      tmp = cur->next;
      while (tmp && tmp->type == CMARK_NODE_TEXT) {
        cmark_iter_next(iter); // advance pointer
        cmark_strbuf_put(&buf, tmp->as.literal.data, tmp->as.literal.len);
        cur->end_column = tmp->end_column;
        next = tmp->next;
        cmark_node_free(tmp);
        tmp = next;
      }
      cmark_chunk_free(iter->mem, &cur->as.literal);
      cur->as.literal = cmark_chunk_buf_detach(&buf);
    }
  }

  cmark_strbuf_free(&buf);
  cmark_iter_free(iter);
}

void cmark_node_own(cmark_node *root) {
  if (root == NULL) {
    return;
  }
  cmark_iter *iter = cmark_iter_new(root);
  cmark_event_type ev_type;
  cmark_node *cur;

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    if (ev_type == CMARK_EVENT_ENTER) {
      switch (cur->type) {
      case CMARK_NODE_TEXT:
      case CMARK_NODE_HTML_INLINE:
      case CMARK_NODE_CODE:
      case CMARK_NODE_HTML_BLOCK:
        cmark_chunk_to_cstr(iter->mem, &cur->as.literal);
        break;
      case CMARK_NODE_LINK:
        cmark_chunk_to_cstr(iter->mem, &cur->as.link.url);
        cmark_chunk_to_cstr(iter->mem, &cur->as.link.title);
        break;
      case CMARK_NODE_CUSTOM_INLINE:
        cmark_chunk_to_cstr(iter->mem, &cur->as.custom.on_enter);
        cmark_chunk_to_cstr(iter->mem, &cur->as.custom.on_exit);
        break;
      }
    }
  }

  cmark_iter_free(iter);
}
