#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "node.h"
#include "syntax_extension.h"

/**
 * Expensive safety checks are off by default, but can be enabled
 * by calling cmark_enable_safety_checks().
 */
static bool enable_safety_checks = false;

void cmark_enable_safety_checks(bool enable) {
  enable_safety_checks = enable;
}

static void S_node_unlink(cmark_node *node);

#define NODE_MEM(node) cmark_node_mem(node)

void cmark_register_node_flag(cmark_node_internal_flags *flags) {
  static cmark_node_internal_flags nextflag = CMARK_NODE__REGISTER_FIRST;

  // flags should be a pointer to a global variable and this function
  // should only be called once to initialize its value.
  if (*flags) {
    fprintf(stderr, "flag initialization error in cmark_register_node_flag\n");
    abort();
  }

  // Check that we haven't run out of bits.
  if (nextflag == 0) {
    fprintf(stderr, "too many flags in cmark_register_node_flag\n");
    abort();
  }

  *flags = nextflag;
  nextflag <<= 1;
}

void cmark_init_standard_node_flags(void) {}

bool cmark_node_can_contain_type(cmark_node *node, cmark_node_type child_type) {
  if (child_type == CMARK_NODE_DOCUMENT) {
      return false;
    }

  if (node->extension && node->extension->can_contain_func) {
    return node->extension->can_contain_func(node->extension, node, child_type) != 0;
  }

  switch (node->type) {
  case CMARK_NODE_DOCUMENT:
  case CMARK_NODE_BLOCK_QUOTE:
  case CMARK_NODE_FOOTNOTE_DEFINITION:
  case CMARK_NODE_ITEM:
    return CMARK_NODE_TYPE_BLOCK_P(child_type) && child_type != CMARK_NODE_ITEM;

  case CMARK_NODE_LIST:
    return child_type == CMARK_NODE_ITEM;

  case CMARK_NODE_CUSTOM_BLOCK:
    return true;

  case CMARK_NODE_PARAGRAPH:
  case CMARK_NODE_HEADING:
  case CMARK_NODE_EMPH:
  case CMARK_NODE_STRONG:
  case CMARK_NODE_LINK:
  case CMARK_NODE_IMAGE:
  case CMARK_NODE_CUSTOM_INLINE:
    return CMARK_NODE_TYPE_INLINE_P(child_type);

  default:
    break;
  }

  return false;
}

static bool S_can_contain(cmark_node *node, cmark_node *child) {
  if (node == NULL || child == NULL) {
    return false;
  }
  if (NODE_MEM(node) != NODE_MEM(child)) {
    return 0;
  }

  if (enable_safety_checks) {
    // Verify that child is not an ancestor of node or equal to node.
    cmark_node *cur = node;
    do {
      if (cur == child) {
        return false;
      }
      cur = cur->parent;
    } while (cur != NULL);
  }

  return cmark_node_can_contain_type(node, (cmark_node_type) child->type);
}

cmark_node *cmark_node_new_with_mem_and_ext(cmark_node_type type, cmark_mem *mem, cmark_syntax_extension *extension) {
  cmark_node *node = (cmark_node *)mem->calloc(1, sizeof(*node));
  cmark_strbuf_init(mem, &node->content, 0);
  node->type = (uint16_t)type;
  node->extension = extension;

  switch (node->type) {
  case CMARK_NODE_HEADING:
    node->as.heading.level = 1;
    break;

  case CMARK_NODE_LIST: {
    cmark_list *list = &node->as.list;
    list->list_type = CMARK_BULLET_LIST;
    list->start = 0;
    list->tight = false;
    break;
  }

  default:
    break;
  }

  if (node->extension && node->extension->opaque_alloc_func) {
    node->extension->opaque_alloc_func(node->extension, mem, node);
  }

  return node;
}

cmark_node *cmark_node_new_with_ext(cmark_node_type type, cmark_syntax_extension *extension) {
  extern cmark_mem CMARK_DEFAULT_MEM_ALLOCATOR;
  return cmark_node_new_with_mem_and_ext(type, &CMARK_DEFAULT_MEM_ALLOCATOR, extension);
}

cmark_node *cmark_node_new_with_mem(cmark_node_type type, cmark_mem *mem)
{
  return cmark_node_new_with_mem_and_ext(type, mem, NULL);
}

cmark_node *cmark_node_new(cmark_node_type type) {
  return cmark_node_new_with_ext(type, NULL);
}

static void free_node_as(cmark_node *node) {
  switch (node->type) {
    case CMARK_NODE_CODE_BLOCK:
    cmark_chunk_free(NODE_MEM(node), &node->as.code.info);
    cmark_chunk_free(NODE_MEM(node), &node->as.code.literal);
      break;
    case CMARK_NODE_TEXT:
    case CMARK_NODE_HTML_INLINE:
    case CMARK_NODE_CODE:
    case CMARK_NODE_HTML_BLOCK:
    case CMARK_NODE_FOOTNOTE_REFERENCE:
    case CMARK_NODE_FOOTNOTE_DEFINITION:
    cmark_chunk_free(NODE_MEM(node), &node->as.literal);
      break;
    case CMARK_NODE_LINK:
    case CMARK_NODE_IMAGE:
    cmark_chunk_free(NODE_MEM(node), &node->as.link.url);
    cmark_chunk_free(NODE_MEM(node), &node->as.link.title);
      break;
    case CMARK_NODE_CUSTOM_BLOCK:
    case CMARK_NODE_CUSTOM_INLINE:
    cmark_chunk_free(NODE_MEM(node), &node->as.custom.on_enter);
    cmark_chunk_free(NODE_MEM(node), &node->as.custom.on_exit);
      break;
    default:
      break;
    }
}

// Free a cmark_node list and any children.
static void S_free_nodes(cmark_node *e) {
  cmark_node *next;
  while (e != NULL) {
    cmark_strbuf_free(&e->content);

    if (e->user_data && e->user_data_free_func)
      e->user_data_free_func(NODE_MEM(e), e->user_data);

    if (e->as.opaque && e->extension && e->extension->opaque_free_func)
      e->extension->opaque_free_func(e->extension, NODE_MEM(e), e);

    free_node_as(e);

    if (e->last_child) {
      // Splice children into list
      e->last_child->next = e->next;
      e->next = e->first_child;
    }
    next = e->next;
    NODE_MEM(e)->free(e);
    e = next;
  }
}

void cmark_node_free(cmark_node *node) {
  S_node_unlink(node);
  node->next = NULL;
  S_free_nodes(node);
}

cmark_node_type cmark_node_get_type(cmark_node *node) {
  if (node == NULL) {
    return CMARK_NODE_NONE;
  } else {
    return (cmark_node_type)node->type;
  }
}

int cmark_node_set_type(cmark_node * node, cmark_node_type type) {
  cmark_node_type initial_type;

  if (type == node->type)
    return 1;

  initial_type = (cmark_node_type) node->type;
  node->type = (uint16_t)type;

  if (!S_can_contain(node->parent, node)) {
    node->type = (uint16_t)initial_type;
    return 0;
  }

  /* We rollback the type to free the union members appropriately */
  node->type = (uint16_t)initial_type;
  free_node_as(node);

  node->type = (uint16_t)type;

  return 1;
}

const char *cmark_node_get_type_string(cmark_node *node) {
  if (node == NULL) {
    return "NONE";
  }

  if (node->extension && node->extension->get_type_string_func) {
    return node->extension->get_type_string_func(node->extension, node);
  }

  switch (node->type) {
  case CMARK_NODE_NONE:
    return "none";
  case CMARK_NODE_DOCUMENT:
    return "document";
  case CMARK_NODE_BLOCK_QUOTE:
    return "block_quote";
  case CMARK_NODE_LIST:
    return "list";
  case CMARK_NODE_ITEM:
    return "item";
  case CMARK_NODE_CODE_BLOCK:
    return "code_block";
  case CMARK_NODE_HTML_BLOCK:
    return "html_block";
  case CMARK_NODE_CUSTOM_BLOCK:
    return "custom_block";
  case CMARK_NODE_PARAGRAPH:
    return "paragraph";
  case CMARK_NODE_HEADING:
    return "heading";
  case CMARK_NODE_THEMATIC_BREAK:
    return "thematic_break";
  case CMARK_NODE_TEXT:
    return "text";
  case CMARK_NODE_SOFTBREAK:
    return "softbreak";
  case CMARK_NODE_LINEBREAK:
    return "linebreak";
  case CMARK_NODE_CODE:
    return "code";
  case CMARK_NODE_HTML_INLINE:
    return "html_inline";
  case CMARK_NODE_CUSTOM_INLINE:
    return "custom_inline";
  case CMARK_NODE_EMPH:
    return "emph";
  case CMARK_NODE_STRONG:
    return "strong";
  case CMARK_NODE_LINK:
    return "link";
  case CMARK_NODE_IMAGE:
    return "image";
  }

  return "<unknown>";
}

cmark_node *cmark_node_next(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->next;
  }
}

cmark_node *cmark_node_previous(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->prev;
  }
}

cmark_node *cmark_node_parent(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->parent;
  }
}

cmark_node *cmark_node_first_child(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->first_child;
  }
}

cmark_node *cmark_node_last_child(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->last_child;
  }
}

cmark_node *cmark_node_parent_footnote_def(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->parent_footnote_def;
  }
}

void *cmark_node_get_user_data(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->user_data;
  }
}

int cmark_node_set_user_data(cmark_node *node, void *user_data) {
  if (node == NULL) {
    return 0;
  }
  node->user_data = user_data;
  return 1;
}

int cmark_node_set_user_data_free_func(cmark_node *node,
                                        cmark_free_func free_func) {
  if (node == NULL) {
    return 0;
  }
  node->user_data_free_func = free_func;
  return 1;
}

const char *cmark_node_get_literal(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case CMARK_NODE_HTML_BLOCK:
  case CMARK_NODE_TEXT:
  case CMARK_NODE_HTML_INLINE:
  case CMARK_NODE_CODE:
  case CMARK_NODE_FOOTNOTE_REFERENCE:
  case CMARK_NODE_FOOTNOTE_DEFINITION:
    return cmark_chunk_to_cstr(NODE_MEM(node), &node->as.literal);

  case CMARK_NODE_CODE_BLOCK:
    return cmark_chunk_to_cstr(NODE_MEM(node), &node->as.code.literal);

  default:
    break;
  }

  return NULL;
}

int cmark_node_set_literal(cmark_node *node, const char *content) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case CMARK_NODE_HTML_BLOCK:
  case CMARK_NODE_TEXT:
  case CMARK_NODE_HTML_INLINE:
  case CMARK_NODE_CODE:
  case CMARK_NODE_FOOTNOTE_REFERENCE:
    cmark_chunk_set_cstr(NODE_MEM(node), &node->as.literal, content);
    return 1;

  case CMARK_NODE_CODE_BLOCK:
    cmark_chunk_set_cstr(NODE_MEM(node), &node->as.code.literal, content);
    return 1;

  default:
    break;
  }

  return 0;
}

const char *cmark_node_get_string_content(cmark_node *node) {
  return (char *) node->content.ptr;
}

int cmark_node_set_string_content(cmark_node *node, const char *content) {
  cmark_strbuf_sets(&node->content, content);
  return true;
}

int cmark_node_get_heading_level(cmark_node *node) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case CMARK_NODE_HEADING:
    return node->as.heading.level;

  default:
    break;
  }

  return 0;
}

int cmark_node_set_heading_level(cmark_node *node, int level) {
  if (node == NULL || level < 1 || level > 6) {
    return 0;
  }

  switch (node->type) {
  case CMARK_NODE_HEADING:
    node->as.heading.level = level;
    return 1;

  default:
    break;
  }

  return 0;
}

cmark_list_type cmark_node_get_list_type(cmark_node *node) {
  if (node == NULL) {
    return CMARK_NO_LIST;
  }

  if (node->type == CMARK_NODE_LIST) {
    return node->as.list.list_type;
  } else {
    return CMARK_NO_LIST;
  }
}

int cmark_node_set_list_type(cmark_node *node, cmark_list_type type) {
  if (!(type == CMARK_BULLET_LIST || type == CMARK_ORDERED_LIST)) {
    return 0;
  }

  if (node == NULL) {
    return 0;
  }

  if (node->type == CMARK_NODE_LIST) {
    node->as.list.list_type = type;
    return 1;
  } else {
    return 0;
  }
}

cmark_delim_type cmark_node_get_list_delim(cmark_node *node) {
  if (node == NULL) {
    return CMARK_NO_DELIM;
  }

  if (node->type == CMARK_NODE_LIST) {
    return node->as.list.delimiter;
  } else {
    return CMARK_NO_DELIM;
  }
}

int cmark_node_set_list_delim(cmark_node *node, cmark_delim_type delim) {
  if (!(delim == CMARK_PERIOD_DELIM || delim == CMARK_PAREN_DELIM)) {
    return 0;
  }

  if (node == NULL) {
    return 0;
  }

  if (node->type == CMARK_NODE_LIST) {
    node->as.list.delimiter = delim;
    return 1;
  } else {
    return 0;
  }
}

int cmark_node_get_list_start(cmark_node *node) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == CMARK_NODE_LIST) {
    return node->as.list.start;
  } else {
    return 0;
  }
}

int cmark_node_set_list_start(cmark_node *node, int start) {
  if (node == NULL || start < 0) {
    return 0;
  }

  if (node->type == CMARK_NODE_LIST) {
    node->as.list.start = start;
    return 1;
  } else {
    return 0;
  }
}

int cmark_node_get_list_tight(cmark_node *node) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == CMARK_NODE_LIST) {
    return node->as.list.tight;
  } else {
    return 0;
  }
}

int cmark_node_set_list_tight(cmark_node *node, int tight) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == CMARK_NODE_LIST) {
    node->as.list.tight = tight == 1;
    return 1;
  } else {
    return 0;
  }
}

int cmark_node_get_item_index(cmark_node *node) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == CMARK_NODE_ITEM) {
    return node->as.list.start;
  } else {
    return 0;
  }
}

int cmark_node_set_item_index(cmark_node *node, int idx) {
  if (node == NULL || idx < 0) {
    return 0;
  }

  if (node->type == CMARK_NODE_ITEM) {
    node->as.list.start = idx;
    return 1;
  } else {
    return 0;
  }
}

const char *cmark_node_get_fence_info(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  if (node->type == CMARK_NODE_CODE_BLOCK) {
    return cmark_chunk_to_cstr(NODE_MEM(node), &node->as.code.info);
  } else {
    return NULL;
  }
}

int cmark_node_set_fence_info(cmark_node *node, const char *info) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == CMARK_NODE_CODE_BLOCK) {
    cmark_chunk_set_cstr(NODE_MEM(node), &node->as.code.info, info);
    return 1;
  } else {
    return 0;
  }
}

int cmark_node_get_fenced(cmark_node *node, int *length, int *offset, char *character) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == CMARK_NODE_CODE_BLOCK) {
    *length = node->as.code.fence_length;
    *offset = node->as.code.fence_offset;
    *character = node->as.code.fence_char;
    return node->as.code.fenced;
  } else {
    return 0;
  }
}

int cmark_node_set_fenced(cmark_node * node, int fenced,
    int length, int offset, char character) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == CMARK_NODE_CODE_BLOCK) {
    node->as.code.fenced = (int8_t)fenced;
    node->as.code.fence_length = (uint8_t)length;
    node->as.code.fence_offset = (uint8_t)offset;
    node->as.code.fence_char = character;
    return 1;
  } else {
    return 0;
  }
}

const char *cmark_node_get_url(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case CMARK_NODE_LINK:
  case CMARK_NODE_IMAGE:
    return cmark_chunk_to_cstr(NODE_MEM(node), &node->as.link.url);
  default:
    break;
  }

  return NULL;
}

int cmark_node_set_url(cmark_node *node, const char *url) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case CMARK_NODE_LINK:
  case CMARK_NODE_IMAGE:
    cmark_chunk_set_cstr(NODE_MEM(node), &node->as.link.url, url);
    return 1;
  default:
    break;
  }

  return 0;
}

const char *cmark_node_get_title(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case CMARK_NODE_LINK:
  case CMARK_NODE_IMAGE:
    return cmark_chunk_to_cstr(NODE_MEM(node), &node->as.link.title);
  default:
    break;
  }

  return NULL;
}

int cmark_node_set_title(cmark_node *node, const char *title) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case CMARK_NODE_LINK:
  case CMARK_NODE_IMAGE:
    cmark_chunk_set_cstr(NODE_MEM(node), &node->as.link.title, title);
    return 1;
  default:
    break;
  }

  return 0;
}

const char *cmark_node_get_on_enter(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case CMARK_NODE_CUSTOM_INLINE:
  case CMARK_NODE_CUSTOM_BLOCK:
    return cmark_chunk_to_cstr(NODE_MEM(node), &node->as.custom.on_enter);
  default:
    break;
  }

  return NULL;
}

int cmark_node_set_on_enter(cmark_node *node, const char *on_enter) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case CMARK_NODE_CUSTOM_INLINE:
  case CMARK_NODE_CUSTOM_BLOCK:
    cmark_chunk_set_cstr(NODE_MEM(node), &node->as.custom.on_enter, on_enter);
    return 1;
  default:
    break;
  }

  return 0;
}

const char *cmark_node_get_on_exit(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case CMARK_NODE_CUSTOM_INLINE:
  case CMARK_NODE_CUSTOM_BLOCK:
    return cmark_chunk_to_cstr(NODE_MEM(node), &node->as.custom.on_exit);
  default:
    break;
  }

  return NULL;
}

int cmark_node_set_on_exit(cmark_node *node, const char *on_exit) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case CMARK_NODE_CUSTOM_INLINE:
  case CMARK_NODE_CUSTOM_BLOCK:
    cmark_chunk_set_cstr(NODE_MEM(node), &node->as.custom.on_exit, on_exit);
    return 1;
  default:
    break;
  }

  return 0;
}

cmark_syntax_extension *cmark_node_get_syntax_extension(cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  return node->extension;
}

int cmark_node_set_syntax_extension(cmark_node *node, cmark_syntax_extension *extension) {
  if (node == NULL) {
    return 0;
  }

  node->extension = extension;
  return 1;
}

int cmark_node_get_start_line(cmark_node *node) {
  if (node == NULL) {
    return 0;
  }
  return node->start_line;
}

int cmark_node_get_start_column(cmark_node *node) {
  if (node == NULL) {
    return 0;
  }
  return node->start_column;
}

int cmark_node_get_end_line(cmark_node *node) {
  if (node == NULL) {
    return 0;
  }
  return node->end_line;
}

int cmark_node_get_end_column(cmark_node *node) {
  if (node == NULL) {
    return 0;
  }
  return node->end_column;
}

// Unlink a node without adjusting its next, prev, and parent pointers.
static void S_node_unlink(cmark_node *node) {
  if (node == NULL) {
    return;
  }

  if (node->prev) {
    node->prev->next = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  }

  // Adjust first_child and last_child of parent.
  cmark_node *parent = node->parent;
  if (parent) {
    if (parent->first_child == node) {
      parent->first_child = node->next;
    }
    if (parent->last_child == node) {
      parent->last_child = node->prev;
    }
  }
}

void cmark_node_unlink(cmark_node *node) {
  S_node_unlink(node);

  node->next = NULL;
  node->prev = NULL;
  node->parent = NULL;
}

int cmark_node_insert_before(cmark_node *node, cmark_node *sibling) {
  if (node == NULL || sibling == NULL) {
    return 0;
  }

  if (!node->parent || !S_can_contain(node->parent, sibling)) {
    return 0;
  }

  S_node_unlink(sibling);

  cmark_node *old_prev = node->prev;

  // Insert 'sibling' between 'old_prev' and 'node'.
  if (old_prev) {
    old_prev->next = sibling;
  }
  sibling->prev = old_prev;
  sibling->next = node;
  node->prev = sibling;

  // Set new parent.
  cmark_node *parent = node->parent;
  sibling->parent = parent;

  // Adjust first_child of parent if inserted as first child.
  if (parent && !old_prev) {
    parent->first_child = sibling;
  }

  return 1;
}

int cmark_node_insert_after(cmark_node *node, cmark_node *sibling) {
  if (node == NULL || sibling == NULL) {
    return 0;
  }

  if (!node->parent || !S_can_contain(node->parent, sibling)) {
    return 0;
  }

  S_node_unlink(sibling);

  cmark_node *old_next = node->next;

  // Insert 'sibling' between 'node' and 'old_next'.
  if (old_next) {
    old_next->prev = sibling;
  }
  sibling->next = old_next;
  sibling->prev = node;
  node->next = sibling;

  // Set new parent.
  cmark_node *parent = node->parent;
  sibling->parent = parent;

  // Adjust last_child of parent if inserted as last child.
  if (parent && !old_next) {
    parent->last_child = sibling;
  }

  return 1;
}

int cmark_node_replace(cmark_node *oldnode, cmark_node *newnode) {
  if (!cmark_node_insert_before(oldnode, newnode)) {
    return 0;
  }
  cmark_node_unlink(oldnode);
  return 1;
}

int cmark_node_prepend_child(cmark_node *node, cmark_node *child) {
  if (!S_can_contain(node, child)) {
    return 0;
  }

  S_node_unlink(child);

  cmark_node *old_first_child = node->first_child;

  child->next = old_first_child;
  child->prev = NULL;
  child->parent = node;
  node->first_child = child;

  if (old_first_child) {
    old_first_child->prev = child;
  } else {
    // Also set last_child if node previously had no children.
    node->last_child = child;
  }

  return 1;
}

int cmark_node_append_child(cmark_node *node, cmark_node *child) {
  if (!S_can_contain(node, child)) {
    return 0;
  }

  S_node_unlink(child);

  cmark_node *old_last_child = node->last_child;

  child->next = NULL;
  child->prev = old_last_child;
  child->parent = node;
  node->last_child = child;

  if (old_last_child) {
    old_last_child->next = child;
  } else {
    // Also set first_child if node previously had no children.
    node->first_child = child;
  }

  return 1;
}

static void S_print_error(FILE *out, cmark_node *node, const char *elem) {
  if (out == NULL) {
    return;
  }
  fprintf(out, "Invalid '%s' in node type %s at %d:%d\n", elem,
          cmark_node_get_type_string(node), node->start_line,
          node->start_column);
}

int cmark_node_check(cmark_node *node, FILE *out) {
  cmark_node *cur;
  int errors = 0;

  if (!node) {
    return 0;
  }

  cur = node;
  for (;;) {
    if (cur->first_child) {
      if (cur->first_child->prev != NULL) {
        S_print_error(out, cur->first_child, "prev");
        cur->first_child->prev = NULL;
        ++errors;
      }
      if (cur->first_child->parent != cur) {
        S_print_error(out, cur->first_child, "parent");
        cur->first_child->parent = cur;
        ++errors;
      }
      cur = cur->first_child;
      continue;
    }

  next_sibling:
    if (cur == node) {
      break;
    }
    if (cur->next) {
      if (cur->next->prev != cur) {
        S_print_error(out, cur->next, "prev");
        cur->next->prev = cur;
        ++errors;
      }
      if (cur->next->parent != cur->parent) {
        S_print_error(out, cur->next, "parent");
        cur->next->parent = cur->parent;
        ++errors;
      }
      cur = cur->next;
      continue;
    }

    if (cur->parent->last_child != cur) {
      S_print_error(out, cur->parent, "last_child");
      cur->parent->last_child = cur;
      ++errors;
    }
    cur = cur->parent;
    goto next_sibling;
  }

  return errors;
}
