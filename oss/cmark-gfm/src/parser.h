#ifndef CMARK_PARSER_H
#define CMARK_PARSER_H

#include <stdio.h>
#include "references.h"
#include "node.h"
#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LINK_LABEL_LENGTH 1000

struct cmark_parser {
  struct cmark_mem *mem;
  /* A hashtable of urls in the current document for cross-references */
  struct cmark_map *refmap;
  /* The root node of the parser, always a CMARK_NODE_DOCUMENT */
  struct cmark_node *root;
  /* The last open block after a line is fully processed */
  struct cmark_node *current;
  /* See the documentation for cmark_parser_get_line_number() in cmark.h */
  int line_number;
  /* See the documentation for cmark_parser_get_offset() in cmark.h */
  bufsize_t offset;
  /* See the documentation for cmark_parser_get_column() in cmark.h */
  bufsize_t column;
  /* See the documentation for cmark_parser_get_first_nonspace() in cmark.h */
  bufsize_t first_nonspace;
  /* See the documentation for cmark_parser_get_first_nonspace_column() in cmark.h */
  bufsize_t first_nonspace_column;
  bufsize_t thematic_break_kill_pos;
  /* See the documentation for cmark_parser_get_indent() in cmark.h */
  int indent;
  /* See the documentation for cmark_parser_is_blank() in cmark.h */
  bool blank;
  /* See the documentation for cmark_parser_has_partially_consumed_tab() in cmark.h */
  bool partially_consumed_tab;
  /* Contains the currently processed line */
  cmark_strbuf curline;
  /* See the documentation for cmark_parser_get_last_line_length() in cmark.h */
  bufsize_t last_line_length;
  /* FIXME: not sure about the difference with curline */
  cmark_strbuf linebuf;
  /* Options set by the user, see the Options section in cmark.h */
  int options;
  bool last_buffer_ended_with_cr;
  size_t total_size;
  cmark_llist *syntax_extensions;
  cmark_llist *inline_syntax_extensions;
  cmark_ispunct_func backslash_ispunct;
};

#ifdef __cplusplus
}
#endif

#endif
