#ifndef CMARK_RENDER_H
#define CMARK_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "buffer.h"
#include "chunk.h"

typedef enum { LITERAL, NORMAL, TITLE, URL } cmark_escaping;

struct cmark_renderer {
  cmark_mem *mem;
  cmark_strbuf *buffer;
  cmark_strbuf *prefix;
  int column;
  int width;
  int need_cr;
  bufsize_t last_breakable;
  bool begin_line;
  bool begin_content;
  bool no_linebreaks;
  bool in_tight_list_item;
  void (*outc)(struct cmark_renderer *, cmark_node *, cmark_escaping, int32_t, unsigned char);
  void (*cr)(struct cmark_renderer *);
  void (*blankline)(struct cmark_renderer *);
  void (*out)(struct cmark_renderer *, cmark_node *, const char *, bool, cmark_escaping);
  unsigned int footnote_ix;
};

typedef struct cmark_renderer cmark_renderer;

struct cmark_html_renderer {
  cmark_strbuf *html;
  cmark_node *plain;
  cmark_llist *filter_extensions;
  unsigned int footnote_ix;
  unsigned int written_footnote_ix;
  void *opaque;
};

typedef struct cmark_html_renderer cmark_html_renderer;

void cmark_render_ascii(cmark_renderer *renderer, const char *s);

void cmark_render_code_point(cmark_renderer *renderer, uint32_t c);

char *cmark_render(cmark_mem *mem, cmark_node *root, int options, int width,
                   void (*outc)(cmark_renderer *, cmark_node *,
                                cmark_escaping, int32_t,
                                unsigned char),
                   int (*render_node)(cmark_renderer *renderer,
                                      cmark_node *node,
                                      cmark_event_type ev_type, int options));

#ifdef __cplusplus
}
#endif

#endif
