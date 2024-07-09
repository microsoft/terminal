#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "config.h"
#include "cmark-gfm.h"
#include "node.h"
#include "buffer.h"
#include "utf8.h"
#include "scanners.h"
#include "render.h"
#include "syntax_extension.h"

#define OUT(s, wrap, escaping) renderer->out(renderer, node, s, wrap, escaping)
#define LIT(s) renderer->out(renderer, node, s, false, LITERAL)
#define CR() renderer->cr(renderer)
#define BLANKLINE() renderer->blankline(renderer)
#define ENCODED_SIZE 20
#define LISTMARKER_SIZE 20

// Functions to convert cmark_nodes to commonmark strings.

static CMARK_INLINE void outc(cmark_renderer *renderer, cmark_node *node, 
                              cmark_escaping escape,
                              int32_t c, unsigned char nextc) {
  bool needs_escaping = false;
  bool follows_digit =
      renderer->buffer->size > 0 &&
      cmark_isdigit(renderer->buffer->ptr[renderer->buffer->size - 1]);
  char encoded[ENCODED_SIZE];

  needs_escaping =
      c < 0x80 && escape != LITERAL &&
      ((escape == NORMAL &&
        (c < 0x20 ||
	 c == '*' || c == '_' || c == '[' || c == ']' || c == '#' || c == '<' ||
         c == '>' || c == '\\' || c == '`' || c == '~' || c == '!' ||
         (c == '&' && cmark_isalpha(nextc)) || (c == '!' && nextc == '[') ||
         (renderer->begin_content && (c == '-' || c == '+' || c == '=') &&
          // begin_content doesn't get set to false til we've passed digits
          // at the beginning of line, so...
          !follows_digit) ||
         (renderer->begin_content && (c == '.' || c == ')') && follows_digit &&
          (nextc == 0 || cmark_isspace(nextc))))) ||
       (escape == URL &&
        (c == '`' || c == '<' || c == '>' || cmark_isspace((char)c) || c == '\\' ||
         c == ')' || c == '(')) ||
       (escape == TITLE &&
        (c == '`' || c == '<' || c == '>' || c == '"' || c == '\\')));

  if (needs_escaping) {
    if (escape == URL && cmark_isspace((char)c)) {
      // use percent encoding for spaces
      snprintf(encoded, ENCODED_SIZE, "%%%2X", c);
      cmark_strbuf_puts(renderer->buffer, encoded);
      renderer->column += 3;
    } else if (cmark_ispunct((char)c)) {
      cmark_render_ascii(renderer, "\\");
      cmark_render_code_point(renderer, c);
    } else { // render as entity
      snprintf(encoded, ENCODED_SIZE, "&#%d;", c);
      cmark_strbuf_puts(renderer->buffer, encoded);
      renderer->column += (int)strlen(encoded);
    }
  } else {
    cmark_render_code_point(renderer, c);
  }
}

static int longest_backtick_sequence(const char *code) {
  int longest = 0;
  int current = 0;
  size_t i = 0;
  size_t code_len = strlen(code);
  while (i <= code_len) {
    if (code[i] == '`') {
      current++;
    } else {
      if (current > longest) {
        longest = current;
      }
      current = 0;
    }
    i++;
  }
  return longest;
}

static int shortest_unused_backtick_sequence(const char *code) {
  // note: if the shortest sequence is >= 32, this returns 32
  // so as not to overflow the bit array.
  uint32_t used = 1;
  int current = 0;
  size_t i = 0;
  size_t code_len = strlen(code);
  while (i <= code_len) {
    if (code[i] == '`') {
      current++;
    } else {
      if (current > 0 && current < 32) {
        used |= (1U << current);
      }
      current = 0;
    }
    i++;
  }
  // return number of first bit that is 0:
  i = 0;
  while (i < 32 && used & 1) {
    used = used >> 1;
    i++;
  }
  return (int)i;
}

static bool is_autolink(cmark_node *node) {
  cmark_chunk *title;
  cmark_chunk *url;
  cmark_node *link_text;
  char *realurl;
  int realurllen;

  if (node->type != CMARK_NODE_LINK) {
    return false;
  }

  url = &node->as.link.url;
  if (url->len == 0 || scan_scheme(url, 0) == 0) {
    return false;
  }

  title = &node->as.link.title;
  // if it has a title, we can't treat it as an autolink:
  if (title->len > 0) {
    return false;
  }

  link_text = node->first_child;
  if (link_text == NULL) {
    return false;
  }
  cmark_consolidate_text_nodes(link_text);
  realurl = (char *)url->data;
  realurllen = url->len;
  if (strncmp(realurl, "mailto:", 7) == 0) {
    realurl += 7;
    realurllen -= 7;
  }
  return (realurllen == link_text->as.literal.len &&
          strncmp(realurl, (char *)link_text->as.literal.data,
                  link_text->as.literal.len) == 0);
}

static int S_render_node(cmark_renderer *renderer, cmark_node *node,
                         cmark_event_type ev_type, int options) {
  int list_number;
  cmark_delim_type list_delim;
  int numticks;
  bool extra_spaces;
  int i;
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  const char *info, *code, *title;
  char fencechar[2] = {'\0', '\0'};
  size_t info_len, code_len;
  char listmarker[LISTMARKER_SIZE];
  const char *emph_delim;
  bool first_in_list_item;
  bufsize_t marker_width;
  bool allow_wrap = renderer->width > 0 && !(CMARK_OPT_NOBREAKS & options) &&
                    !(CMARK_OPT_HARDBREAKS & options);

  // Don't adjust tight list status til we've started the list.
  // Otherwise we loose the blank line between a paragraph and
  // a following list.
  if (entering) {
    if (node->parent && node->parent->type == CMARK_NODE_ITEM) {
      renderer->in_tight_list_item = node->parent->parent->as.list.tight;
    }
  } else {
    if (node->type == CMARK_NODE_LIST) {
      renderer->in_tight_list_item =
        node->parent &&
        node->parent->type == CMARK_NODE_ITEM &&
        node->parent->parent->as.list.tight;
    }
  }

  if (node->extension && node->extension->commonmark_render_func) {
    node->extension->commonmark_render_func(node->extension, renderer, node, ev_type, options);
    return 1;
  }

  switch (node->type) {
  case CMARK_NODE_DOCUMENT:
    break;

  case CMARK_NODE_BLOCK_QUOTE:
    if (entering) {
      LIT("> ");
      renderer->begin_content = true;
      cmark_strbuf_puts(renderer->prefix, "> ");
    } else {
      cmark_strbuf_truncate(renderer->prefix, renderer->prefix->size - 2);
      BLANKLINE();
    }
    break;

  case CMARK_NODE_LIST:
    if (!entering && node->next && (node->next->type == CMARK_NODE_CODE_BLOCK ||
                                    node->next->type == CMARK_NODE_LIST)) {
      // this ensures that a following indented code block or list will be
      // inteprereted correctly.
      CR();
      LIT("<!-- end list -->");
      BLANKLINE();
    }
    break;

  case CMARK_NODE_ITEM:
    if (cmark_node_get_list_type(node->parent) == CMARK_BULLET_LIST) {
      marker_width = 4;
    } else {
      list_number = cmark_node_get_item_index(node);
      list_delim = cmark_node_get_list_delim(node->parent);
      // we ensure a width of at least 4 so
      // we get nice transition from single digits
      // to double
      snprintf(listmarker, LISTMARKER_SIZE, "%d%s%s", list_number,
               list_delim == CMARK_PAREN_DELIM ? ")" : ".",
               list_number < 10 ? "  " : " ");
      marker_width = (bufsize_t)strlen(listmarker);
    }
    if (entering) {
      if (cmark_node_get_list_type(node->parent) == CMARK_BULLET_LIST) {
        LIT("  - ");
        renderer->begin_content = true;
      } else {
        LIT(listmarker);
        renderer->begin_content = true;
      }
      for (i = marker_width; i--;) {
        cmark_strbuf_putc(renderer->prefix, ' ');
      }
    } else {
      cmark_strbuf_truncate(renderer->prefix,
                            renderer->prefix->size - marker_width);
      CR();
    }
    break;

  case CMARK_NODE_HEADING:
    if (entering) {
      for (i = cmark_node_get_heading_level(node); i > 0; i--) {
        LIT("#");
      }
      LIT(" ");
      renderer->begin_content = true;
      renderer->no_linebreaks = true;
    } else {
      renderer->no_linebreaks = false;
      BLANKLINE();
    }
    break;

  case CMARK_NODE_CODE_BLOCK:
    first_in_list_item = node->prev == NULL && node->parent &&
                         node->parent->type == CMARK_NODE_ITEM;

    if (!first_in_list_item) {
      BLANKLINE();
    }
    info = cmark_node_get_fence_info(node);
    info_len = strlen(info);
    fencechar[0] = strchr(info, '`') == NULL ? '`' : '~';
    code = cmark_node_get_literal(node);
    code_len = strlen(code);
    // use indented form if no info, and code doesn't
    // begin or end with a blank line, and code isn't
    // first thing in a list item
    if (info_len == 0 && (code_len > 2 && !cmark_isspace(code[0]) &&
                          !(cmark_isspace(code[code_len - 1]) &&
                            cmark_isspace(code[code_len - 2]))) &&
        !first_in_list_item) {
      LIT("    ");
      cmark_strbuf_puts(renderer->prefix, "    ");
      OUT(cmark_node_get_literal(node), false, LITERAL);
      cmark_strbuf_truncate(renderer->prefix, renderer->prefix->size - 4);
    } else {
      numticks = longest_backtick_sequence(code) + 1;
      if (numticks < 3) {
        numticks = 3;
      }
      for (i = 0; i < numticks; i++) {
        LIT(fencechar);
      }
      LIT(" ");
      OUT(info, false, LITERAL);
      CR();
      OUT(cmark_node_get_literal(node), false, LITERAL);
      CR();
      for (i = 0; i < numticks; i++) {
        LIT(fencechar);
      }
    }
    BLANKLINE();
    break;

  case CMARK_NODE_HTML_BLOCK:
    BLANKLINE();
    OUT(cmark_node_get_literal(node), false, LITERAL);
    BLANKLINE();
    break;

  case CMARK_NODE_CUSTOM_BLOCK:
    BLANKLINE();
    OUT(entering ? cmark_node_get_on_enter(node) : cmark_node_get_on_exit(node),
        false, LITERAL);
    BLANKLINE();
    break;

  case CMARK_NODE_THEMATIC_BREAK:
    BLANKLINE();
    LIT("-----");
    BLANKLINE();
    break;

  case CMARK_NODE_PARAGRAPH:
    if (!entering) {
      BLANKLINE();
    }
    break;

  case CMARK_NODE_TEXT:
    OUT(cmark_node_get_literal(node), allow_wrap, NORMAL);
    break;

  case CMARK_NODE_LINEBREAK:
    if (!(CMARK_OPT_HARDBREAKS & options)) {
      LIT("  ");
    }
    CR();
    break;

  case CMARK_NODE_SOFTBREAK:
    if (CMARK_OPT_HARDBREAKS & options) {
      LIT("  ");
      CR();
    } else if (!renderer->no_linebreaks && renderer->width == 0 &&
               !(CMARK_OPT_HARDBREAKS & options) &&
               !(CMARK_OPT_NOBREAKS & options)) {
      CR();
    } else {
      OUT(" ", allow_wrap, LITERAL);
    }
    break;

  case CMARK_NODE_CODE:
    code = cmark_node_get_literal(node);
    code_len = strlen(code);
    numticks = shortest_unused_backtick_sequence(code);
    extra_spaces = code_len == 0 ||
	    code[0] == '`' || code[code_len - 1] == '`' ||
	    code[0] == ' ' || code[code_len - 1] == ' ';
    for (i = 0; i < numticks; i++) {
      LIT("`");
    }
    if (extra_spaces) {
      LIT(" ");
    }
    OUT(cmark_node_get_literal(node), allow_wrap, LITERAL);
    if (extra_spaces) {
      LIT(" ");
    }
    for (i = 0; i < numticks; i++) {
      LIT("`");
    }
    break;

  case CMARK_NODE_HTML_INLINE:
    OUT(cmark_node_get_literal(node), false, LITERAL);
    break;

  case CMARK_NODE_CUSTOM_INLINE:
    OUT(entering ? cmark_node_get_on_enter(node) : cmark_node_get_on_exit(node),
        false, LITERAL);
    break;

  case CMARK_NODE_STRONG:
    if (node->parent == NULL || node->parent->type != CMARK_NODE_STRONG) {
      if (entering) {
        LIT("**");
      } else {
        LIT("**");
      }
    }
    break;

  case CMARK_NODE_EMPH:
    // If we have EMPH(EMPH(x)), we need to use *_x_*
    // because **x** is STRONG(x):
    if (node->parent && node->parent->type == CMARK_NODE_EMPH &&
        node->next == NULL && node->prev == NULL) {
      emph_delim = "_";
    } else {
      emph_delim = "*";
    }
    if (entering) {
      LIT(emph_delim);
    } else {
      LIT(emph_delim);
    }
    break;

  case CMARK_NODE_LINK:
    if (is_autolink(node)) {
      if (entering) {
        LIT("<");
        if (strncmp(cmark_node_get_url(node), "mailto:", 7) == 0) {
          LIT((const char *)cmark_node_get_url(node) + 7);
        } else {
          LIT((const char *)cmark_node_get_url(node));
        }
        LIT(">");
        // return signal to skip contents of node...
        return 0;
      }
    } else {
      if (entering) {
        LIT("[");
      } else {
        LIT("](");
        OUT(cmark_node_get_url(node), false, URL);
        title = cmark_node_get_title(node);
        if (strlen(title) > 0) {
          LIT(" \"");
          OUT(title, false, TITLE);
          LIT("\"");
        }
        LIT(")");
      }
    }
    break;

  case CMARK_NODE_IMAGE:
    if (entering) {
      LIT("![");
    } else {
      LIT("](");
      OUT(cmark_node_get_url(node), false, URL);
      title = cmark_node_get_title(node);
      if (strlen(title) > 0) {
        OUT(" \"", allow_wrap, LITERAL);
        OUT(title, false, TITLE);
        LIT("\"");
      }
      LIT(")");
    }
    break;

  case CMARK_NODE_FOOTNOTE_REFERENCE:
    if (entering) {
      LIT("[^");

      char *footnote_label = renderer->mem->calloc(node->parent_footnote_def->as.literal.len + 1, sizeof(char));
      memmove(footnote_label, node->parent_footnote_def->as.literal.data, node->parent_footnote_def->as.literal.len);

      OUT(footnote_label, false, LITERAL);
      renderer->mem->free(footnote_label);

      LIT("]");
    }
    break;

  case CMARK_NODE_FOOTNOTE_DEFINITION:
    if (entering) {
      renderer->footnote_ix += 1;
      LIT("[^");

      char *footnote_label = renderer->mem->calloc(node->as.literal.len + 1, sizeof(char));
      memmove(footnote_label, node->as.literal.data, node->as.literal.len);

      OUT(footnote_label, false, LITERAL);
      renderer->mem->free(footnote_label);

      LIT("]:\n");

      cmark_strbuf_puts(renderer->prefix, "    ");
    } else {
      cmark_strbuf_truncate(renderer->prefix, renderer->prefix->size - 4);
    }
    break;

  default:
    assert(false);
    break;
  }

  return 1;
}

char *cmark_render_commonmark(cmark_node *root, int options, int width) {
  return cmark_render_commonmark_with_mem(root, options, width, cmark_node_mem(root));
}

char *cmark_render_commonmark_with_mem(cmark_node *root, int options, int width, cmark_mem *mem) {
  if (options & CMARK_OPT_HARDBREAKS) {
    // disable breaking on width, since it has
    // a different meaning with OPT_HARDBREAKS
    width = 0;
  }
  return cmark_render(mem, root, options, width, outc, S_render_node);
}
