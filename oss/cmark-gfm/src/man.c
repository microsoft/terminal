#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "cmark-gfm.h"
#include "node.h"
#include "buffer.h"
#include "utf8.h"
#include "render.h"
#include "syntax_extension.h"

#define OUT(s, wrap, escaping) renderer->out(renderer, node, s, wrap, escaping)
#define LIT(s) renderer->out(renderer, node, s, false, LITERAL)
#define CR() renderer->cr(renderer)
#define BLANKLINE() renderer->blankline(renderer)
#define LIST_NUMBER_SIZE 20

// Functions to convert cmark_nodes to groff man strings.
static void S_outc(cmark_renderer *renderer, cmark_node *node, 
                   cmark_escaping escape, int32_t c,
                   unsigned char nextc) {
  (void)(nextc);

  if (escape == LITERAL) {
    cmark_render_code_point(renderer, c);
    return;
  }

  switch (c) {
  case 46:
    if (renderer->begin_line) {
      cmark_render_ascii(renderer, "\\&.");
    } else {
      cmark_render_code_point(renderer, c);
    }
    break;
  case 39:
    if (renderer->begin_line) {
      cmark_render_ascii(renderer, "\\&'");
    } else {
      cmark_render_code_point(renderer, c);
    }
    break;
  case 45:
    cmark_render_ascii(renderer, "\\-");
    break;
  case 92:
    cmark_render_ascii(renderer, "\\e");
    break;
  case 8216: // left single quote
    cmark_render_ascii(renderer, "\\[oq]");
    break;
  case 8217: // right single quote
    cmark_render_ascii(renderer, "\\[cq]");
    break;
  case 8220: // left double quote
    cmark_render_ascii(renderer, "\\[lq]");
    break;
  case 8221: // right double quote
    cmark_render_ascii(renderer, "\\[rq]");
    break;
  case 8212: // em dash
    cmark_render_ascii(renderer, "\\[em]");
    break;
  case 8211: // en dash
    cmark_render_ascii(renderer, "\\[en]");
    break;
  default:
    cmark_render_code_point(renderer, c);
  }
}

static int S_render_node(cmark_renderer *renderer, cmark_node *node,
                         cmark_event_type ev_type, int options) {
  int list_number;
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  bool allow_wrap = renderer->width > 0 && !(CMARK_OPT_NOBREAKS & options);

  if (node->extension && node->extension->man_render_func) {
    node->extension->man_render_func(node->extension, renderer, node, ev_type, options);
    return 1;
  }

  switch (node->type) {
  case CMARK_NODE_DOCUMENT:
    if (entering) {
      /* Define a strikethrough macro */
      /* Commenting out because this makes tests fail
      LIT(".de ST");
      CR();
      LIT(".nr ww \\w'\\\\$1'");
      CR();
      LIT("\\Z@\\v'-.25m'\\l'\\\\n[ww]u'@\\\\$1");
      CR();
      LIT("..");
      CR();
      */
    }
    break;

  case CMARK_NODE_BLOCK_QUOTE:
    if (entering) {
      CR();
      LIT(".RS");
      CR();
    } else {
      CR();
      LIT(".RE");
      CR();
    }
    break;

  case CMARK_NODE_LIST:
    break;

  case CMARK_NODE_ITEM:
    if (entering) {
      CR();
      LIT(".IP ");
      if (cmark_node_get_list_type(node->parent) == CMARK_BULLET_LIST) {
        LIT("\\[bu] 2");
      } else {
        list_number = cmark_node_get_item_index(node);
        char list_number_s[LIST_NUMBER_SIZE];
        snprintf(list_number_s, LIST_NUMBER_SIZE, "\"%d.\" 4", list_number);
        LIT(list_number_s);
      }
      CR();
    } else {
      CR();
    }
    break;

  case CMARK_NODE_HEADING:
    if (entering) {
      CR();
      LIT(cmark_node_get_heading_level(node) == 1 ? ".SH" : ".SS");
      CR();
    } else {
      CR();
    }
    break;

  case CMARK_NODE_CODE_BLOCK:
    CR();
    LIT(".IP\n.nf\n\\f[C]\n");
    OUT(cmark_node_get_literal(node), false, NORMAL);
    CR();
    LIT("\\f[]\n.fi");
    CR();
    break;

  case CMARK_NODE_HTML_BLOCK:
    break;

  case CMARK_NODE_CUSTOM_BLOCK:
    CR();
    OUT(entering ? cmark_node_get_on_enter(node) : cmark_node_get_on_exit(node),
        false, LITERAL);
    CR();
    break;

  case CMARK_NODE_THEMATIC_BREAK:
    CR();
    LIT(".PP\n  *  *  *  *  *");
    CR();
    break;

  case CMARK_NODE_PARAGRAPH:
    if (entering) {
      // no blank line if first paragraph in list:
      if (node->parent && node->parent->type == CMARK_NODE_ITEM &&
          node->prev == NULL) {
        // no blank line or .PP
      } else {
        CR();
        LIT(".PP");
        CR();
      }
    } else {
      CR();
    }
    break;

  case CMARK_NODE_TEXT:
    OUT(cmark_node_get_literal(node), allow_wrap, NORMAL);
    break;

  case CMARK_NODE_LINEBREAK:
    LIT(".PD 0\n.P\n.PD");
    CR();
    break;

  case CMARK_NODE_SOFTBREAK:
    if (options & CMARK_OPT_HARDBREAKS) {
      LIT(".PD 0\n.P\n.PD");
      CR();
    } else if (renderer->width == 0 && !(CMARK_OPT_NOBREAKS & options)) {
      CR();
    } else {
      OUT(" ", allow_wrap, LITERAL);
    }
    break;

  case CMARK_NODE_CODE:
    LIT("\\f[C]");
    OUT(cmark_node_get_literal(node), allow_wrap, NORMAL);
    LIT("\\f[]");
    break;

  case CMARK_NODE_HTML_INLINE:
    break;

  case CMARK_NODE_CUSTOM_INLINE:
    OUT(entering ? cmark_node_get_on_enter(node) : cmark_node_get_on_exit(node),
        false, LITERAL);
    break;

  case CMARK_NODE_STRONG:
    if (node->parent == NULL || node->parent->type != CMARK_NODE_STRONG) {
      if (entering) {
        LIT("\\f[B]");
      } else {
        LIT("\\f[]");
      }
    }
    break;

  case CMARK_NODE_EMPH:
    if (entering) {
      LIT("\\f[I]");
    } else {
      LIT("\\f[]");
    }
    break;

  case CMARK_NODE_LINK:
    if (!entering) {
      LIT(" (");
      OUT(cmark_node_get_url(node), allow_wrap, URL);
      LIT(")");
    }
    break;

  case CMARK_NODE_IMAGE:
    if (entering) {
      LIT("[IMAGE: ");
    } else {
      LIT("]");
    }
    break;

  case CMARK_NODE_FOOTNOTE_DEFINITION:
  case CMARK_NODE_FOOTNOTE_REFERENCE:
    // TODO
    break;

  default:
    assert(false);
    break;
  }

  return 1;
}

char *cmark_render_man(cmark_node *root, int options, int width) {
  return cmark_render_man_with_mem(root, options, width, cmark_node_mem(root));
}

char *cmark_render_man_with_mem(cmark_node *root, int options, int width, cmark_mem *mem) {
  return cmark_render(mem, root, options, width, S_outc, S_render_node);
}
