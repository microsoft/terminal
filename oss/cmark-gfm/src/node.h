#ifndef CMARK_NODE_H
#define CMARK_NODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "buffer.h"
#include "chunk.h"

typedef struct {
  cmark_list_type list_type;
  int marker_offset;
  int padding;
  int start;
  cmark_delim_type delimiter;
  unsigned char bullet_char;
  bool tight;
  bool checked; // For task list extension
} cmark_list;

typedef struct {
  cmark_chunk info;
  cmark_chunk literal;
  uint8_t fence_length;
  uint8_t fence_offset;
  unsigned char fence_char;
  int8_t fenced;
} cmark_code;

typedef struct {
  int level;
  bool setext;
} cmark_heading;

typedef struct {
  cmark_chunk url;
  cmark_chunk title;
} cmark_link;

typedef struct {
  cmark_chunk on_enter;
  cmark_chunk on_exit;
} cmark_custom;

enum cmark_node__internal_flags {
  CMARK_NODE__OPEN = (1 << 0),
  CMARK_NODE__LAST_LINE_BLANK = (1 << 1),
  CMARK_NODE__LAST_LINE_CHECKED = (1 << 2),

  // Extensions can register custom flags by calling `cmark_register_node_flag`.
  // This is the starting value for the custom flags.
  CMARK_NODE__REGISTER_FIRST = (1 << 3),
};

typedef uint16_t cmark_node_internal_flags;

struct cmark_node {
  cmark_strbuf content;

  struct cmark_node *next;
  struct cmark_node *prev;
  struct cmark_node *parent;
  struct cmark_node *first_child;
  struct cmark_node *last_child;

  void *user_data;
  cmark_free_func user_data_free_func;

  int start_line;
  int start_column;
  int end_line;
  int end_column;
  int internal_offset;
  uint16_t type;
  cmark_node_internal_flags flags;

  cmark_syntax_extension *extension;

  /**
   * Used during cmark_render() to cache the most recent non-NULL
   * extension, if you go up the parent chain like this:
   *
   * node->parent->...parent->extension
   */
  cmark_syntax_extension *ancestor_extension;

  union {
    int ref_ix;
    int def_count;
  } footnote;

  cmark_node *parent_footnote_def;

  union {
    cmark_chunk literal;
    cmark_list list;
    cmark_code code;
    cmark_heading heading;
    cmark_link link;
    cmark_custom custom;
    int html_block_type;
    int cell_index; // For keeping track of TABLE_CELL table alignments
    void *opaque;
  } as;
};

/**
 * Syntax extensions can use this function to register a custom node
 * flag. The flags are stored in the `flags` field of the `cmark_node`
 * struct. The `flags` parameter should be the address of a global variable
 * which will store the flag value.
 */

void cmark_register_node_flag(cmark_node_internal_flags *flags);

/**
 * DEPRECATED.
 *
 * This function was added in cmark-gfm version 0.29.0.gfm.7, and was
 * required to be called at program start time, which caused
 * backwards-compatibility issues in applications that use cmark-gfm as a
 * library. It is now a no-op.
 */

void cmark_init_standard_node_flags(void);

static CMARK_INLINE cmark_mem *cmark_node_mem(cmark_node *node) {
  return node->content.mem;
}
 int cmark_node_check(cmark_node *node, FILE *out);

static CMARK_INLINE bool CMARK_NODE_TYPE_BLOCK_P(cmark_node_type node_type) {
	return (node_type & CMARK_NODE_TYPE_MASK) == CMARK_NODE_TYPE_BLOCK;
}

static CMARK_INLINE bool CMARK_NODE_BLOCK_P(cmark_node *node) {
	return node != NULL && CMARK_NODE_TYPE_BLOCK_P((cmark_node_type) node->type);
}

static CMARK_INLINE bool CMARK_NODE_TYPE_INLINE_P(cmark_node_type node_type) {
	return (node_type & CMARK_NODE_TYPE_MASK) == CMARK_NODE_TYPE_INLINE;
}

static CMARK_INLINE bool CMARK_NODE_INLINE_P(cmark_node *node) {
	return node != NULL && CMARK_NODE_TYPE_INLINE_P((cmark_node_type) node->type);
}

 bool cmark_node_can_contain_type(cmark_node *node, cmark_node_type child_type);

/**
 * Enable (or disable) extra safety checks. These extra checks cause
 * extra performance overhead (in some cases quadratic), so they are only
 * intended to be used during testing.
 */
 void cmark_enable_safety_checks(bool enable);

#ifdef __cplusplus
}
#endif

#endif
