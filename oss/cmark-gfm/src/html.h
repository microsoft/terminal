#ifndef CMARK_HTML_H
#define CMARK_HTML_H

#include "buffer.h"
#include "node.h"

CMARK_INLINE
static void cmark_html_render_cr(cmark_strbuf *html) {
  if (html->size && html->ptr[html->size - 1] != '\n')
    cmark_strbuf_putc(html, '\n');
}

#define BUFFER_SIZE 100

CMARK_INLINE 
static void cmark_html_render_sourcepos(cmark_node *node, cmark_strbuf *html, int options) {
  char buffer[BUFFER_SIZE];
  if (CMARK_OPT_SOURCEPOS & options) {
    snprintf(buffer, BUFFER_SIZE, " data-sourcepos=\"%d:%d-%d:%d\"",
             cmark_node_get_start_line(node), cmark_node_get_start_column(node),
             cmark_node_get_end_line(node), cmark_node_get_end_column(node));
    cmark_strbuf_puts(html, buffer);
  }
}


#endif
