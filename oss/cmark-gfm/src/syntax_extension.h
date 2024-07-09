#ifndef CMARK_SYNTAX_EXTENSION_H
#define CMARK_SYNTAX_EXTENSION_H

#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "config.h"

struct cmark_syntax_extension {
  cmark_match_block_func          last_block_matches;
  cmark_open_block_func           try_opening_block;
  cmark_match_inline_func         match_inline;
  cmark_inline_from_delim_func    insert_inline_from_delim;
  cmark_llist                   * special_inline_chars;
  char                          * name;
  void                          * priv;
  bool                            emphasis;
  cmark_free_func                 free_function;
  cmark_get_type_string_func      get_type_string_func;
  cmark_can_contain_func          can_contain_func;
  cmark_contains_inlines_func     contains_inlines_func;
  cmark_common_render_func        commonmark_render_func;
  cmark_common_render_func        plaintext_render_func;
  cmark_common_render_func        latex_render_func;
  cmark_xml_attr_func             xml_attr_func;
  cmark_common_render_func        man_render_func;
  cmark_html_render_func          html_render_func;
  cmark_html_filter_func          html_filter_func;
  cmark_postprocess_func          postprocess_func;
  cmark_opaque_alloc_func         opaque_alloc_func;
  cmark_opaque_free_func          opaque_free_func;
  cmark_commonmark_escape_func    commonmark_escape_func;
};

#endif
