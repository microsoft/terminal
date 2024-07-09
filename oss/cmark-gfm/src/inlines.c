#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cmark_ctype.h"
#include "config.h"
#include "node.h"
#include "parser.h"
#include "references.h"
#include "cmark-gfm.h"
#include "houdini.h"
#include "utf8.h"
#include "scanners.h"
#include "inlines.h"
#include "syntax_extension.h"

static const char *EMDASH = "\xE2\x80\x94";
static const char *ENDASH = "\xE2\x80\x93";
static const char *ELLIPSES = "\xE2\x80\xA6";
static const char *LEFTDOUBLEQUOTE = "\xE2\x80\x9C";
static const char *RIGHTDOUBLEQUOTE = "\xE2\x80\x9D";
static const char *LEFTSINGLEQUOTE = "\xE2\x80\x98";
static const char *RIGHTSINGLEQUOTE = "\xE2\x80\x99";

// Macros for creating various kinds of simple.
#define make_str(subj, sc, ec, s) make_literal(subj, CMARK_NODE_TEXT, sc, ec, s)
#define make_code(subj, sc, ec, s) make_literal(subj, CMARK_NODE_CODE, sc, ec, s)
#define make_raw_html(subj, sc, ec, s) make_literal(subj, CMARK_NODE_HTML_INLINE, sc, ec, s)
#define make_linebreak(mem) make_simple(mem, CMARK_NODE_LINEBREAK)
#define make_softbreak(mem) make_simple(mem, CMARK_NODE_SOFTBREAK)
#define make_emph(mem) make_simple(mem, CMARK_NODE_EMPH)
#define make_strong(mem) make_simple(mem, CMARK_NODE_STRONG)

#define MAXBACKTICKS 80

typedef struct bracket {
  struct bracket *previous;
  cmark_node *inl_text;
  bufsize_t position;
  bool image;
  bool active;
  bool bracket_after;
  bool in_bracket_image0;
  bool in_bracket_image1;
} bracket;

#define FLAG_SKIP_HTML_CDATA        (1u << 0)
#define FLAG_SKIP_HTML_DECLARATION  (1u << 1)
#define FLAG_SKIP_HTML_PI           (1u << 2)
#define FLAG_SKIP_HTML_COMMENT      (1u << 3)

typedef struct subject{
  cmark_mem *mem;
  cmark_chunk input;
  unsigned flags;
  int line;
  bufsize_t pos;
  int block_offset;
  int column_offset;
  cmark_map *refmap;
  delimiter *last_delim;
  bracket *last_bracket;
  bufsize_t backticks[MAXBACKTICKS + 1];
  bool scanned_for_backticks;
  bool no_link_openers;
} subject;

// Extensions may populate this.
static int8_t SKIP_CHARS[256];

static CMARK_INLINE bool S_is_line_end_char(char c) {
  return (c == '\n' || c == '\r');
}

static delimiter *S_insert_emph(subject *subj, delimiter *opener,
                                delimiter *closer);

static int parse_inline(cmark_parser *parser, subject *subj, cmark_node *parent, int options);

static void subject_from_buf(cmark_mem *mem, int line_number, int block_offset, subject *e,
                             cmark_chunk *buffer, cmark_map *refmap);
static bufsize_t subject_find_special_char(subject *subj, int options);

// Create an inline with a literal string value.
static CMARK_INLINE cmark_node *make_literal(subject *subj, cmark_node_type t,
                                             int start_column, int end_column,
                                             cmark_chunk s) {
  cmark_node *e = (cmark_node *)subj->mem->calloc(1, sizeof(*e));
  cmark_strbuf_init(subj->mem, &e->content, 0);
  e->type = (uint16_t)t;
  e->as.literal = s;
  e->start_line = e->end_line = subj->line;
  // columns are 1 based.
  e->start_column = start_column + 1 + subj->column_offset + subj->block_offset;
  e->end_column = end_column + 1 + subj->column_offset + subj->block_offset;
  return e;
}

// Create an inline with no value.
static CMARK_INLINE cmark_node *make_simple(cmark_mem *mem, cmark_node_type t) {
  cmark_node *e = (cmark_node *)mem->calloc(1, sizeof(*e));
  cmark_strbuf_init(mem, &e->content, 0);
  e->type = (uint16_t)t;
  return e;
}

// Like make_str, but parses entities.
static cmark_node *make_str_with_entities(subject *subj,
                                          int start_column, int end_column,
                                          cmark_chunk *content) {
  cmark_strbuf unescaped = CMARK_BUF_INIT(subj->mem);

  if (houdini_unescape_html(&unescaped, content->data, content->len)) {
    return make_str(subj, start_column, end_column, cmark_chunk_buf_detach(&unescaped));
  } else {
    return make_str(subj, start_column, end_column, *content);
  }
}

// Like cmark_node_append_child but without costly sanity checks.
// Assumes that child was newly created.
static void append_child(cmark_node *node, cmark_node *child) {
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
}

// Duplicate a chunk by creating a copy of the buffer not by reusing the
// buffer like cmark_chunk_dup does.
static cmark_chunk chunk_clone(cmark_mem *mem, cmark_chunk *src) {
  cmark_chunk c;
  bufsize_t len = src->len;

  c.len = len;
  c.data = (unsigned char *)mem->calloc(len + 1, 1);
  c.alloc = 1;
  if (len)
    memcpy(c.data, src->data, len);
  c.data[len] = '\0';

  return c;
}

static cmark_chunk cmark_clean_autolink(cmark_mem *mem, cmark_chunk *url,
                                        int is_email) {
  cmark_strbuf buf = CMARK_BUF_INIT(mem);

  cmark_chunk_trim(url);

  if (url->len == 0) {
    cmark_chunk result = CMARK_CHUNK_EMPTY;
    return result;
  }

  if (is_email)
    cmark_strbuf_puts(&buf, "mailto:");

  houdini_unescape_html_f(&buf, url->data, url->len);
  return cmark_chunk_buf_detach(&buf);
}

static CMARK_INLINE cmark_node *make_autolink(subject *subj,
                                              int start_column, int end_column,
                                              cmark_chunk url, int is_email) {
  cmark_node *link = make_simple(subj->mem, CMARK_NODE_LINK);
  link->as.link.url = cmark_clean_autolink(subj->mem, &url, is_email);
  link->as.link.title = cmark_chunk_literal("");
  link->start_line = link->end_line = subj->line;
  link->start_column = start_column + 1;
  link->end_column = end_column + 1;
  append_child(link, make_str_with_entities(subj, start_column + 1, end_column - 1, &url));
  return link;
}

static void subject_from_buf(cmark_mem *mem, int line_number, int block_offset, subject *e,
                             cmark_chunk *chunk, cmark_map *refmap) {
  int i;
  e->mem = mem;
  e->input = *chunk;
  e->flags = 0;
  e->line = line_number;
  e->pos = 0;
  e->block_offset = block_offset;
  e->column_offset = 0;
  e->refmap = refmap;
  e->last_delim = NULL;
  e->last_bracket = NULL;
  for (i = 0; i <= MAXBACKTICKS; i++) {
    e->backticks[i] = 0;
  }
  e->scanned_for_backticks = false;
  e->no_link_openers = true;
}

static CMARK_INLINE int isbacktick(int c) { return (c == '`'); }

static CMARK_INLINE unsigned char peek_char_n(subject *subj, bufsize_t n) {
  // NULL bytes should have been stripped out by now.  If they're
  // present, it's a programming error:
  assert(!(subj->pos + n < subj->input.len && subj->input.data[subj->pos + n] == 0));
  return (subj->pos + n < subj->input.len) ? subj->input.data[subj->pos + n] : 0;
}

static CMARK_INLINE unsigned char peek_char(subject *subj) {
  return peek_char_n(subj, 0);
}

static CMARK_INLINE unsigned char peek_at(subject *subj, bufsize_t pos) {
  return subj->input.data[pos];
}

// Return true if there are more characters in the subject.
static CMARK_INLINE int is_eof(subject *subj) {
  return (subj->pos >= subj->input.len);
}

// Advance the subject.  Doesn't check for eof.
#define advance(subj) (subj)->pos += 1

static CMARK_INLINE bool skip_spaces(subject *subj) {
  bool skipped = false;
  while (peek_char(subj) == ' ' || peek_char(subj) == '\t') {
    advance(subj);
    skipped = true;
  }
  return skipped;
}

static CMARK_INLINE bool skip_line_end(subject *subj) {
  bool seen_line_end_char = false;
  if (peek_char(subj) == '\r') {
    advance(subj);
    seen_line_end_char = true;
  }
  if (peek_char(subj) == '\n') {
    advance(subj);
    seen_line_end_char = true;
  }
  return seen_line_end_char || is_eof(subj);
}

// Take characters while a predicate holds, and return a string.
static CMARK_INLINE cmark_chunk take_while(subject *subj, int (*f)(int)) {
  unsigned char c;
  bufsize_t startpos = subj->pos;
  bufsize_t len = 0;

  while ((c = peek_char(subj)) && (*f)(c)) {
    advance(subj);
    len++;
  }

  return cmark_chunk_dup(&subj->input, startpos, len);
}

// Return the number of newlines in a given span of text in a subject.  If
// the number is greater than zero, also return the number of characters
// between the last newline and the end of the span in `since_newline`.
static int count_newlines(subject *subj, bufsize_t from, bufsize_t len, int *since_newline) {
  int nls = 0;
  int since_nl = 0;

  while (len--) {
    if (subj->input.data[from++] == '\n') {
      ++nls;
      since_nl = 0;
    } else {
      ++since_nl;
    }
  }

  if (!nls)
    return 0;

  *since_newline = since_nl;
  return nls;
}

// Adjust `node`'s `end_line`, `end_column`, and `subj`'s `line` and
// `column_offset` according to the number of newlines in a just-matched span
// of text in `subj`.
static void adjust_subj_node_newlines(subject *subj, cmark_node *node, int matchlen, int extra, int options) {
  if (!(options & CMARK_OPT_SOURCEPOS)) {
    return;
  }

  int since_newline;
  int newlines = count_newlines(subj, subj->pos - matchlen - extra, matchlen, &since_newline);
  if (newlines) {
    subj->line += newlines;
    node->end_line += newlines;
    node->end_column = since_newline;
    subj->column_offset = -subj->pos + since_newline + extra;
  }
}

// Try to process a backtick code span that began with a
// span of ticks of length openticklength length (already
// parsed).  Return 0 if you don't find matching closing
// backticks, otherwise return the position in the subject
// after the closing backticks.
static bufsize_t scan_to_closing_backticks(subject *subj,
                                           bufsize_t openticklength) {

  bool found = false;
  if (openticklength > MAXBACKTICKS) {
    // we limit backtick string length because of the array subj->backticks:
    return 0;
  }
  if (subj->scanned_for_backticks &&
      subj->backticks[openticklength] <= subj->pos) {
    // return if we already know there's no closer
    return 0;
  }
  while (!found) {
    // read non backticks
    unsigned char c;
    while ((c = peek_char(subj)) && c != '`') {
      advance(subj);
    }
    if (is_eof(subj)) {
      break;
    }
    bufsize_t numticks = 0;
    while (peek_char(subj) == '`') {
      advance(subj);
      numticks++;
    }
    // store position of ender
    if (numticks <= MAXBACKTICKS) {
      subj->backticks[numticks] = subj->pos - numticks;
    }
    if (numticks == openticklength) {
      return (subj->pos);
    }
  }
  // got through whole input without finding closer
  subj->scanned_for_backticks = true;
  return 0;
}

// Destructively modify string, converting newlines to
// spaces, then removing a single leading + trailing space,
// unless the code span consists entirely of space characters.
static void S_normalize_code(cmark_strbuf *s) {
  bufsize_t r, w;
  bool contains_nonspace = false;

  for (r = 0, w = 0; r < s->size; ++r) {
    switch (s->ptr[r]) {
    case '\r':
      if (s->ptr[r + 1] != '\n') {
	s->ptr[w++] = ' ';
      }
      break;
    case '\n':
      s->ptr[w++] = ' ';
      break;
    default:
      s->ptr[w++] = s->ptr[r];
    }
    if (s->ptr[r] != ' ') {
      contains_nonspace = true;
    }
  }

  // begins and ends with space?
  if (contains_nonspace &&
      s->ptr[0] == ' ' && s->ptr[w - 1] == ' ') {
    cmark_strbuf_drop(s, 1);
    cmark_strbuf_truncate(s, w - 2);
  } else {
    cmark_strbuf_truncate(s, w);
  }

}


// Parse backtick code section or raw backticks, return an inline.
// Assumes that the subject has a backtick at the current position.
static cmark_node *handle_backticks(subject *subj, int options) {
  cmark_chunk openticks = take_while(subj, isbacktick);
  bufsize_t startpos = subj->pos;
  bufsize_t endpos = scan_to_closing_backticks(subj, openticks.len);

  if (endpos == 0) {      // not found
    subj->pos = startpos; // rewind
    return make_str(subj, subj->pos, subj->pos, openticks);
  } else {
    cmark_strbuf buf = CMARK_BUF_INIT(subj->mem);

    cmark_strbuf_set(&buf, subj->input.data + startpos,
                     endpos - startpos - openticks.len);
    S_normalize_code(&buf);

    cmark_node *node = make_code(subj, startpos, endpos - openticks.len - 1, cmark_chunk_buf_detach(&buf));
    adjust_subj_node_newlines(subj, node, endpos - startpos, openticks.len, options);
    return node;
  }
}


// Scan ***, **, or * and return number scanned, or 0.
// Advances position.
static int scan_delims(subject *subj, unsigned char c, bool *can_open,
                       bool *can_close) {
  int numdelims = 0;
  bufsize_t before_char_pos, after_char_pos;
  int32_t after_char = 0;
  int32_t before_char = 0;
  int len;
  bool left_flanking, right_flanking;

  if (subj->pos == 0) {
    before_char = 10;
  } else {
    before_char_pos = subj->pos - 1;
    // walk back to the beginning of the UTF_8 sequence:
    while ((peek_at(subj, before_char_pos) >> 6 == 2 || SKIP_CHARS[peek_at(subj, before_char_pos)]) && before_char_pos > 0) {
      before_char_pos -= 1;
    }
    len = cmark_utf8proc_iterate(subj->input.data + before_char_pos,
                                 subj->pos - before_char_pos, &before_char);
    if (len == -1 || (before_char < 256 && SKIP_CHARS[(unsigned char) before_char])) {
      before_char = 10;
    }
  }

  if (c == '\'' || c == '"') {
    numdelims++;
    advance(subj); // limit to 1 delim for quotes
  } else {
    while (peek_char(subj) == c) {
      numdelims++;
      advance(subj);
    }
  }

  if (subj->pos == subj->input.len) {
    after_char = 10;
  } else {
    after_char_pos = subj->pos;
    while (SKIP_CHARS[peek_at(subj, after_char_pos)] && after_char_pos < subj->input.len) {
      after_char_pos += 1;
    }
    len = cmark_utf8proc_iterate(subj->input.data + after_char_pos,
                                 subj->input.len - after_char_pos, &after_char);
    if (len == -1 || (after_char < 256 && SKIP_CHARS[(unsigned char) after_char])) {
    after_char = 10;
  }
  }

  left_flanking = numdelims > 0 && !cmark_utf8proc_is_space(after_char) &&
                  (!cmark_utf8proc_is_punctuation(after_char) ||
                   cmark_utf8proc_is_space(before_char) ||
                   cmark_utf8proc_is_punctuation(before_char));
  right_flanking = numdelims > 0 && !cmark_utf8proc_is_space(before_char) &&
                   (!cmark_utf8proc_is_punctuation(before_char) ||
                    cmark_utf8proc_is_space(after_char) ||
                    cmark_utf8proc_is_punctuation(after_char));
  if (c == '_') {
    *can_open = left_flanking &&
                (!right_flanking || cmark_utf8proc_is_punctuation(before_char));
    *can_close = right_flanking &&
                 (!left_flanking || cmark_utf8proc_is_punctuation(after_char));
  } else if (c == '\'' || c == '"') {
    *can_open = left_flanking && !right_flanking &&
	         before_char != ']' && before_char != ')';
    *can_close = right_flanking;
  } else {
    *can_open = left_flanking;
    *can_close = right_flanking;
  }
  return numdelims;
}

/*
static void print_delimiters(subject *subj)
{
        delimiter *delim;
        delim = subj->last_delim;
        while (delim != NULL) {
                printf("Item at stack pos %p: %d %d %d next(%p) prev(%p)\n",
                       (void*)delim, delim->delim_char,
                       delim->can_open, delim->can_close,
                       (void*)delim->next, (void*)delim->previous);
                delim = delim->previous;
        }
}
*/

static void remove_delimiter(subject *subj, delimiter *delim) {
  if (delim == NULL)
    return;
  if (delim->next == NULL) {
    // end of list:
    assert(delim == subj->last_delim);
    subj->last_delim = delim->previous;
  } else {
    delim->next->previous = delim->previous;
  }
  if (delim->previous != NULL) {
    delim->previous->next = delim->next;
  }
  subj->mem->free(delim);
}

static void pop_bracket(subject *subj) {
  bracket *b;
  if (subj->last_bracket == NULL)
    return;
  b = subj->last_bracket;
  subj->last_bracket = subj->last_bracket->previous;
  subj->mem->free(b);
}

static void push_delimiter(subject *subj, unsigned char c, bool can_open,
                           bool can_close, cmark_node *inl_text) {
  delimiter *delim = (delimiter *)subj->mem->calloc(1, sizeof(delimiter));
  delim->delim_char = c;
  delim->can_open = can_open;
  delim->can_close = can_close;
  delim->inl_text = inl_text;
  delim->position = subj->pos;
  delim->length = inl_text->as.literal.len;
  delim->previous = subj->last_delim;
  delim->next = NULL;
  if (delim->previous != NULL) {
    delim->previous->next = delim;
  }
  subj->last_delim = delim;
}

static void push_bracket(subject *subj, bool image, cmark_node *inl_text) {
  bracket *b = (bracket *)subj->mem->calloc(1, sizeof(bracket));
  if (subj->last_bracket != NULL) {
    subj->last_bracket->bracket_after = true;
    b->in_bracket_image0 = subj->last_bracket->in_bracket_image0;
    b->in_bracket_image1 = subj->last_bracket->in_bracket_image1;
  }
  b->image = image;
  b->active = true;
  b->inl_text = inl_text;
  b->previous = subj->last_bracket;
  b->position = subj->pos;
  b->bracket_after = false;
  if (image) {
    b->in_bracket_image1 = true;
  } else {
    b->in_bracket_image0 = true;
  }
  subj->last_bracket = b;
  if (!image) {
    subj->no_link_openers = false;
  }
}

// Assumes the subject has a c at the current position.
static cmark_node *handle_delim(subject *subj, unsigned char c, bool smart) {
  bufsize_t numdelims;
  cmark_node *inl_text;
  bool can_open, can_close;
  cmark_chunk contents;

  numdelims = scan_delims(subj, c, &can_open, &can_close);

  if (c == '\'' && smart) {
    contents = cmark_chunk_literal(RIGHTSINGLEQUOTE);
  } else if (c == '"' && smart) {
    contents =
        cmark_chunk_literal(can_close ? RIGHTDOUBLEQUOTE : LEFTDOUBLEQUOTE);
  } else {
    contents = cmark_chunk_dup(&subj->input, subj->pos - numdelims, numdelims);
  }

  inl_text = make_str(subj, subj->pos - numdelims, subj->pos - 1, contents);

  if ((can_open || can_close) && (!(c == '\'' || c == '"') || smart)) {
    push_delimiter(subj, c, can_open, can_close, inl_text);
  }

  return inl_text;
}

// Assumes we have a hyphen at the current position.
static cmark_node *handle_hyphen(subject *subj, bool smart) {
  int startpos = subj->pos;

  advance(subj);

  if (!smart || peek_char(subj) != '-') {
    return make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("-"));
  }

  while (smart && peek_char(subj) == '-') {
    advance(subj);
  }

  int numhyphens = subj->pos - startpos;
  int en_count = 0;
  int em_count = 0;
  int i;
  cmark_strbuf buf = CMARK_BUF_INIT(subj->mem);

  if (numhyphens % 3 == 0) { // if divisible by 3, use all em dashes
    em_count = numhyphens / 3;
  } else if (numhyphens % 2 == 0) { // if divisible by 2, use all en dashes
    en_count = numhyphens / 2;
  } else if (numhyphens % 3 == 2) { // use one en dash at end
    en_count = 1;
    em_count = (numhyphens - 2) / 3;
  } else { // use two en dashes at the end
    en_count = 2;
    em_count = (numhyphens - 4) / 3;
  }

  for (i = em_count; i > 0; i--) {
    cmark_strbuf_puts(&buf, EMDASH);
  }

  for (i = en_count; i > 0; i--) {
    cmark_strbuf_puts(&buf, ENDASH);
  }

  return make_str(subj, startpos, subj->pos - 1, cmark_chunk_buf_detach(&buf));
}

// Assumes we have a period at the current position.
static cmark_node *handle_period(subject *subj, bool smart) {
  advance(subj);
  if (smart && peek_char(subj) == '.') {
    advance(subj);
    if (peek_char(subj) == '.') {
      advance(subj);
      return make_str(subj, subj->pos - 3, subj->pos - 1, cmark_chunk_literal(ELLIPSES));
    } else {
      return make_str(subj, subj->pos - 2, subj->pos - 1, cmark_chunk_literal(".."));
    }
  } else {
    return make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("."));
  }
}

static cmark_syntax_extension *get_extension_for_special_char(cmark_parser *parser, unsigned char c) {
  cmark_llist *tmp_ext;

  for (tmp_ext = parser->inline_syntax_extensions; tmp_ext; tmp_ext=tmp_ext->next) {
    cmark_syntax_extension *ext = (cmark_syntax_extension *) tmp_ext->data;
    cmark_llist *tmp_char;
    for (tmp_char = ext->special_inline_chars; tmp_char; tmp_char=tmp_char->next) {
      unsigned char tmp_c = (unsigned char)(size_t)tmp_char->data;

      if (tmp_c == c) {
        return ext;
      }
    }
  }

  return NULL;
}

static void process_emphasis(cmark_parser *parser, subject *subj, bufsize_t stack_bottom) {
  delimiter *candidate;
  delimiter *closer = NULL;
  delimiter *opener;
  delimiter *old_closer;
  bool opener_found;
  bufsize_t openers_bottom[3][128];
  int i;

  // initialize openers_bottom:
  memset(&openers_bottom, 0, sizeof(openers_bottom));
  for (i=0; i < 3; i++) {
    openers_bottom[i]['*'] = stack_bottom;
    openers_bottom[i]['_'] = stack_bottom;
    openers_bottom[i]['\''] = stack_bottom;
    openers_bottom[i]['"'] = stack_bottom;
  }

  // move back to first relevant delim.
  candidate = subj->last_delim;
  while (candidate != NULL && candidate->position >= stack_bottom) {
    closer = candidate;
    candidate = candidate->previous;
  }

  // now move forward, looking for closers, and handling each
  while (closer != NULL) {
    cmark_syntax_extension *extension = get_extension_for_special_char(parser, closer->delim_char);
    if (closer->can_close) {
      // Now look backwards for first matching opener:
      opener = closer->previous;
      opener_found = false;
      while (opener != NULL && opener->position >= stack_bottom &&
             opener->position >= openers_bottom[closer->length % 3][closer->delim_char]) {
        if (opener->can_open && opener->delim_char == closer->delim_char) {
          // interior closer of size 2 can't match opener of size 1
          // or of size 1 can't match 2
          if (!(closer->can_open || opener->can_close) ||
	      closer->length % 3 == 0 ||
              (opener->length + closer->length) % 3 != 0) {
            opener_found = true;
            break;
          }
        }
        opener = opener->previous;
      }
      old_closer = closer;

      if (extension) {
        if (opener_found)
          closer = extension->insert_inline_from_delim(extension, parser, subj, opener, closer);
        else
          closer = closer->next;
      } else if (closer->delim_char == '*' || closer->delim_char == '_') {
        if (opener_found) {
          closer = S_insert_emph(subj, opener, closer);
        } else {
          closer = closer->next;
        }
      } else if (closer->delim_char == '\'' || closer->delim_char == '"') {
        cmark_chunk_free(subj->mem, &closer->inl_text->as.literal);
        if (closer->delim_char == '\'') {
          closer->inl_text->as.literal = cmark_chunk_literal(RIGHTSINGLEQUOTE);
        } else {
          closer->inl_text->as.literal = cmark_chunk_literal(RIGHTDOUBLEQUOTE);
        }
        closer = closer->next;
        if (opener_found) {
          cmark_chunk_free(subj->mem, &opener->inl_text->as.literal);
          if (old_closer->delim_char == '\'') {
            opener->inl_text->as.literal = cmark_chunk_literal(LEFTSINGLEQUOTE);
          } else {
            opener->inl_text->as.literal = cmark_chunk_literal(LEFTDOUBLEQUOTE);
          }
          remove_delimiter(subj, opener);
          remove_delimiter(subj, old_closer);
        }
      }
      if (!opener_found) {
        // set lower bound for future searches for openers
        openers_bottom[old_closer->length % 3][old_closer->delim_char] =
		old_closer->position;
        if (!old_closer->can_open) {
          // we can remove a closer that can't be an
          // opener, once we've seen there's no
          // matching opener:
          remove_delimiter(subj, old_closer);
        }
      }
    } else {
      closer = closer->next;
    }
  }
  // free all delimiters in list until stack_bottom:
  while (subj->last_delim != NULL &&
         subj->last_delim->position >= stack_bottom) {
    remove_delimiter(subj, subj->last_delim);
  }
}

static delimiter *S_insert_emph(subject *subj, delimiter *opener,
                                delimiter *closer) {
  delimiter *delim, *tmp_delim;
  bufsize_t use_delims;
  cmark_node *opener_inl = opener->inl_text;
  cmark_node *closer_inl = closer->inl_text;
  bufsize_t opener_num_chars = opener_inl->as.literal.len;
  bufsize_t closer_num_chars = closer_inl->as.literal.len;
  cmark_node *tmp, *tmpnext, *emph;

  // calculate the actual number of characters used from this closer
  use_delims = (closer_num_chars >= 2 && opener_num_chars >= 2) ? 2 : 1;

  // remove used characters from associated inlines.
  opener_num_chars -= use_delims;
  closer_num_chars -= use_delims;
  opener_inl->as.literal.len = opener_num_chars;
  closer_inl->as.literal.len = closer_num_chars;

  // free delimiters between opener and closer
  delim = closer->previous;
  while (delim != NULL && delim != opener) {
    tmp_delim = delim->previous;
    remove_delimiter(subj, delim);
    delim = tmp_delim;
  }

  // create new emph or strong, and splice it in to our inlines
  // between the opener and closer
  emph = use_delims == 1 ? make_emph(subj->mem) : make_strong(subj->mem);

  tmp = opener_inl->next;
  while (tmp && tmp != closer_inl) {
    tmpnext = tmp->next;
    cmark_node_unlink(tmp);
    append_child(emph, tmp);
    tmp = tmpnext;
  }
  cmark_node_insert_after(opener_inl, emph);

  emph->start_line = opener_inl->start_line;
  emph->end_line = closer_inl->end_line;
  emph->start_column = opener_inl->start_column;
  emph->end_column = closer_inl->end_column;

  // if opener has 0 characters, remove it and its associated inline
  if (opener_num_chars == 0) {
    cmark_node_free(opener_inl);
    remove_delimiter(subj, opener);
  }

  // if closer has 0 characters, remove it and its associated inline
  if (closer_num_chars == 0) {
    // remove empty closer inline
    cmark_node_free(closer_inl);
    // remove closer from list
    tmp_delim = closer->next;
    remove_delimiter(subj, closer);
    closer = tmp_delim;
  }

  return closer;
}

// Parse backslash-escape or just a backslash, returning an inline.
static cmark_node *handle_backslash(cmark_parser *parser, subject *subj) {
  advance(subj);
  unsigned char nextchar = peek_char(subj);
  if ((parser->backslash_ispunct ? parser->backslash_ispunct : cmark_ispunct)(nextchar)) {
    // only ascii symbols and newline can be escaped
    advance(subj);
    return make_str(subj, subj->pos - 2, subj->pos - 1, cmark_chunk_dup(&subj->input, subj->pos - 1, 1));
  } else if (!is_eof(subj) && skip_line_end(subj)) {
    return make_linebreak(subj->mem);
  } else {
    return make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("\\"));
  }
}

// Parse an entity or a regular "&" string.
// Assumes the subject has an '&' character at the current position.
static cmark_node *handle_entity(subject *subj) {
  cmark_strbuf ent = CMARK_BUF_INIT(subj->mem);
  bufsize_t len;

  advance(subj);

  len = houdini_unescape_ent(&ent, subj->input.data + subj->pos,
                             subj->input.len - subj->pos);

  if (len == 0)
    return make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("&"));

  subj->pos += len;
  return make_str(subj, subj->pos - 1 - len, subj->pos - 1, cmark_chunk_buf_detach(&ent));
}

// Clean a URL: remove surrounding whitespace, and remove \ that escape
// punctuation.
cmark_chunk cmark_clean_url(cmark_mem *mem, cmark_chunk *url) {
  cmark_strbuf buf = CMARK_BUF_INIT(mem);

  cmark_chunk_trim(url);

  if (url->len == 0) {
    cmark_chunk result = CMARK_CHUNK_EMPTY;
    return result;
  }

  houdini_unescape_html_f(&buf, url->data, url->len);

  cmark_strbuf_unescape(&buf);
  return cmark_chunk_buf_detach(&buf);
}

cmark_chunk cmark_clean_title(cmark_mem *mem, cmark_chunk *title) {
  cmark_strbuf buf = CMARK_BUF_INIT(mem);
  unsigned char first, last;

  if (title->len == 0) {
    cmark_chunk result = CMARK_CHUNK_EMPTY;
    return result;
  }

  first = title->data[0];
  last = title->data[title->len - 1];

  // remove surrounding quotes if any:
  if ((first == '\'' && last == '\'') || (first == '(' && last == ')') ||
      (first == '"' && last == '"')) {
    houdini_unescape_html_f(&buf, title->data + 1, title->len - 2);
  } else {
    houdini_unescape_html_f(&buf, title->data, title->len);
  }

  cmark_strbuf_unescape(&buf);
  return cmark_chunk_buf_detach(&buf);
}

// Parse an autolink or HTML tag.
// Assumes the subject has a '<' character at the current position.
static cmark_node *handle_pointy_brace(subject *subj, int options) {
  bufsize_t matchlen = 0;
  cmark_chunk contents;

  advance(subj); // advance past first <

  // first try to match a URL autolink
  matchlen = scan_autolink_uri(&subj->input, subj->pos);
  if (matchlen > 0) {
    contents = cmark_chunk_dup(&subj->input, subj->pos, matchlen - 1);
    subj->pos += matchlen;

    return make_autolink(subj, subj->pos - 1 - matchlen, subj->pos - 1, contents, 0);
  }

  // next try to match an email autolink
  matchlen = scan_autolink_email(&subj->input, subj->pos);
  if (matchlen > 0) {
    contents = cmark_chunk_dup(&subj->input, subj->pos, matchlen - 1);
    subj->pos += matchlen;

    return make_autolink(subj, subj->pos - 1 - matchlen, subj->pos - 1, contents, 1);
  }

  // finally, try to match an html tag
  if (subj->pos + 2 <= subj->input.len) {
    int c = subj->input.data[subj->pos];
    if (c == '!' && (subj->flags & FLAG_SKIP_HTML_COMMENT) == 0) {
      c = subj->input.data[subj->pos+1];
      if (c == '-' && subj->input.data[subj->pos+2] == '-') {
        if (subj->input.data[subj->pos+3] == '>') {
          matchlen = 4;
        } else if (subj->input.data[subj->pos+3] == '-' &&
                   subj->input.data[subj->pos+4] == '>') {
          matchlen = 5;
        } else {
          matchlen = scan_html_comment(&subj->input, subj->pos + 1);
          if (matchlen > 0) {
            matchlen += 1; // prefix "<"
          } else { // no match through end of input: set a flag so
                   // we don't reparse looking for -->:
            subj->flags |= FLAG_SKIP_HTML_COMMENT;
          }
        }
      } else if (c == '[') {
        if ((subj->flags & FLAG_SKIP_HTML_CDATA) == 0) {
          matchlen = scan_html_cdata(&subj->input, subj->pos + 2);
          if (matchlen > 0) {
            // The regex doesn't require the final "]]>". But if we're not at
            // the end of input, it must come after the match. Otherwise,
            // disable subsequent scans to avoid quadratic behavior.
            matchlen += 5; // prefix "![", suffix "]]>"
            if (subj->pos + matchlen > subj->input.len) {
              subj->flags |= FLAG_SKIP_HTML_CDATA;
              matchlen = 0;
            }
          }
        }
      } else if ((subj->flags & FLAG_SKIP_HTML_DECLARATION) == 0) {
        matchlen = scan_html_declaration(&subj->input, subj->pos + 1);
        if (matchlen > 0) {
          matchlen += 2; // prefix "!", suffix ">"
          if (subj->pos + matchlen > subj->input.len) {
            subj->flags |= FLAG_SKIP_HTML_DECLARATION;
            matchlen = 0;
          }
        }
      }
    } else if (c == '?') {
      if ((subj->flags & FLAG_SKIP_HTML_PI) == 0) {
        // Note that we allow an empty match.
        matchlen = scan_html_pi(&subj->input, subj->pos + 1);
        matchlen += 3; // prefix "?", suffix "?>"
        if (subj->pos + matchlen > subj->input.len) {
          subj->flags |= FLAG_SKIP_HTML_PI;
          matchlen = 0;
        }
      }
    } else {
      matchlen = scan_html_tag(&subj->input, subj->pos);
    }
  }
  if (matchlen > 0) {
    contents = cmark_chunk_dup(&subj->input, subj->pos - 1, matchlen + 1);
    subj->pos += matchlen;
    cmark_node *node = make_raw_html(subj, subj->pos - matchlen - 1, subj->pos - 1, contents);
    adjust_subj_node_newlines(subj, node, matchlen, 1, options);
    return node;
  }

  if (options & CMARK_OPT_LIBERAL_HTML_TAG) {
    matchlen = scan_liberal_html_tag(&subj->input, subj->pos);
    if (matchlen > 0) {
      contents = cmark_chunk_dup(&subj->input, subj->pos - 1, matchlen + 1);
      subj->pos += matchlen;
      cmark_node *node = make_raw_html(subj, subj->pos - matchlen - 1, subj->pos - 1, contents);
      adjust_subj_node_newlines(subj, node, matchlen, 1, options);
      return node;
    }
  }

  // if nothing matches, just return the opening <:
  return make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("<"));
}

// Parse a link label.  Returns 1 if successful.
// Note:  unescaped brackets are not allowed in labels.
// The label begins with `[` and ends with the first `]` character
// encountered.  Backticks in labels do not start code spans.
static int link_label(subject *subj, cmark_chunk *raw_label) {
  bufsize_t startpos = subj->pos;
  int length = 0;
  unsigned char c;

  // advance past [
  if (peek_char(subj) == '[') {
    advance(subj);
  } else {
    return 0;
  }

  while ((c = peek_char(subj)) && c != '[' && c != ']') {
    if (c == '\\') {
      advance(subj);
      length++;
      if (cmark_ispunct(peek_char(subj))) {
        advance(subj);
        length++;
      }
    } else {
      advance(subj);
      length++;
    }
    if (length > MAX_LINK_LABEL_LENGTH) {
      goto noMatch;
    }
  }

  if (c == ']') { // match found
    *raw_label =
        cmark_chunk_dup(&subj->input, startpos + 1, subj->pos - (startpos + 1));
    cmark_chunk_trim(raw_label);
    advance(subj); // advance past ]
    return 1;
  }

noMatch:
  subj->pos = startpos; // rewind
  return 0;
}

static bufsize_t manual_scan_link_url_2(cmark_chunk *input, bufsize_t offset,
                                        cmark_chunk *output) {
  bufsize_t i = offset;
  size_t nb_p = 0;

  while (i < input->len) {
    if (input->data[i] == '\\' &&
        i + 1 < input-> len &&
        cmark_ispunct(input->data[i+1]))
      i += 2;
    else if (input->data[i] == '(') {
      ++nb_p;
      ++i;
      if (nb_p > 32)
        return -1;
    } else if (input->data[i] == ')') {
      if (nb_p == 0)
        break;
      --nb_p;
      ++i;
    } else if (cmark_isspace(input->data[i])) {
      if (i == offset) {
        return -1;
      }
      break;
    } else {
      ++i;
    }
  }

  if (i >= input->len)
    return -1;

  {
    cmark_chunk result = {input->data + offset, i - offset, 0};
    *output = result;
  }
  return i - offset;
}

static bufsize_t manual_scan_link_url(cmark_chunk *input, bufsize_t offset,
                                      cmark_chunk *output) {
  bufsize_t i = offset;

  if (i < input->len && input->data[i] == '<') {
    ++i;
    while (i < input->len) {
      if (input->data[i] == '>') {
        ++i;
        break;
      } else if (input->data[i] == '\\')
        i += 2;
      else if (input->data[i] == '\n' || input->data[i] == '<')
        return -1;
      else
        ++i;
    }
  } else {
    return manual_scan_link_url_2(input, offset, output);
  }

  if (i >= input->len)
    return -1;

  {
    cmark_chunk result = {input->data + offset + 1, i - 2 - offset, 0};
    *output = result;
  }
  return i - offset;
}

// Return a link, an image, or a literal close bracket.
static cmark_node *handle_close_bracket(cmark_parser *parser, subject *subj) {
  bufsize_t initial_pos, after_link_text_pos;
  bufsize_t endurl, starttitle, endtitle, endall;
  bufsize_t sps, n;
  cmark_reference *ref = NULL;
  cmark_chunk url_chunk, title_chunk;
  cmark_chunk url, title;
  bracket *opener;
  cmark_node *inl;
  cmark_chunk raw_label;
  int found_label;
  cmark_node *tmp, *tmpnext;
  bool is_image;

  advance(subj); // advance past ]
  initial_pos = subj->pos;

  // get last [ or ![
  opener = subj->last_bracket;

  if (opener == NULL) {
    return make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("]"));
  }

  // If we got here, we matched a potential link/image text.
  // Now we check to see if it's a link/image.
  is_image = opener->image;

  if (!is_image && subj->no_link_openers) {
    // take delimiter off stack
    pop_bracket(subj);
    return make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("]"));
  }

  after_link_text_pos = subj->pos;

  // First, look for an inline link.
  if (peek_char(subj) == '(' &&
      ((sps = scan_spacechars(&subj->input, subj->pos + 1)) > -1) &&
      ((n = manual_scan_link_url(&subj->input, subj->pos + 1 + sps,
                                 &url_chunk)) > -1)) {

    // try to parse an explicit link:
    endurl = subj->pos + 1 + sps + n;
    starttitle = endurl + scan_spacechars(&subj->input, endurl);

    // ensure there are spaces btw url and title
    endtitle = (starttitle == endurl)
                   ? starttitle
                   : starttitle + scan_link_title(&subj->input, starttitle);

    endall = endtitle + scan_spacechars(&subj->input, endtitle);

    if (peek_at(subj, endall) == ')') {
      subj->pos = endall + 1;

      title_chunk =
          cmark_chunk_dup(&subj->input, starttitle, endtitle - starttitle);
      url = cmark_clean_url(subj->mem, &url_chunk);
      title = cmark_clean_title(subj->mem, &title_chunk);
      cmark_chunk_free(subj->mem, &url_chunk);
      cmark_chunk_free(subj->mem, &title_chunk);
      goto match;

    } else {
      // it could still be a shortcut reference link
      subj->pos = after_link_text_pos;
    }
  }

  // Next, look for a following [link label] that matches in refmap.
  // skip spaces
  raw_label = cmark_chunk_literal("");
  found_label = link_label(subj, &raw_label);
  if (!found_label) {
    // If we have a shortcut reference link, back up
    // to before the spacse we skipped.
    subj->pos = initial_pos;
  }

  if ((!found_label || raw_label.len == 0) && !opener->bracket_after) {
    cmark_chunk_free(subj->mem, &raw_label);
    raw_label = cmark_chunk_dup(&subj->input, opener->position,
                                initial_pos - opener->position - 1);
    found_label = true;
  }

  if (found_label) {
    ref = (cmark_reference *)cmark_map_lookup(subj->refmap, &raw_label);
    cmark_chunk_free(subj->mem, &raw_label);
  }

  if (ref != NULL) { // found
    url = chunk_clone(subj->mem, &ref->url);
    title = chunk_clone(subj->mem, &ref->title);
    goto match;
  } else {
    goto noMatch;
  }

noMatch:
  // If we fall through to here, it means we didn't match a link.
  // What if we're a footnote link?
  if (parser->options & CMARK_OPT_FOOTNOTES &&
      opener->inl_text->next &&
      opener->inl_text->next->type == CMARK_NODE_TEXT) {

    cmark_chunk *literal = &opener->inl_text->next->as.literal;

    // look back to the opening '[', and skip ahead to the next character
    // if we're looking at a '[^' sequence, and there is other text or nodes
    // after the ^, let's call it a footnote reference.
    if ((literal->len > 0 && literal->data[0] == '^') && (literal->len > 1 || opener->inl_text->next->next)) {

      // Before we got this far, the `handle_close_bracket` function may have
      // advanced the current state beyond our footnote's actual closing
      // bracket, ie if it went looking for a `link_label`.
      // Let's just rewind the subject's position:
      subj->pos = initial_pos;

      cmark_node *fnref = make_simple(subj->mem, CMARK_NODE_FOOTNOTE_REFERENCE);

      // the start and end of the footnote ref is the opening and closing brace
      // i.e. the subject's current position, and the opener's start_column
      int fnref_end_column = subj->pos + subj->column_offset + subj->block_offset;
      int fnref_start_column = opener->inl_text->start_column;

      // any given node delineates a substring of the line being processed,
      // with the remainder of the line being pointed to thru its 'literal'
      // struct member.
      // here, we copy the literal's pointer, moving it past the '^' character
      // for a length equal to the size of footnote reference text.
      // i.e. end_col minus start_col, minus the [ and the ^ characters
      //
      // this copies the footnote reference string, even if between the
      // `opener` and the subject's current position there are other nodes
      //
      // (first, check for underflows)
      if ((fnref_start_column + 2) <= fnref_end_column) {
        fnref->as.literal = cmark_chunk_dup(literal, 1, (fnref_end_column - fnref_start_column) - 2);
      } else {
        fnref->as.literal = cmark_chunk_dup(literal, 1, 0);
      }

      fnref->start_line = fnref->end_line = subj->line;
      fnref->start_column = fnref_start_column;
      fnref->end_column = fnref_end_column;

      // we then replace the opener with this new fnref node, the net effect
      // being replacing the opening '[' text node with a `^footnote-ref]` node.
      cmark_node_insert_before(opener->inl_text, fnref);

      process_emphasis(parser, subj, opener->position);
      // sometimes, the footnote reference text gets parsed into multiple nodes
      // i.e. '[^example]' parsed into '[', '^exam', 'ple]'.
      // this happens for ex with the autolink extension. when the autolinker
      // finds the 'w' character, it will split the text into multiple nodes
      // in hopes of being able to match a 'www.' substring.
      //
      // because this function is called one character at a time via the
      // `parse_inlines` function, and the current subj->pos is pointing at the
      // closing ] brace, and because we copy all the text between the [ ]
      // braces, we should be able to safely ignore and delete any nodes after
      // the opener->inl_text->next.
      //
      // therefore, here we walk thru the list and free them all up
      cmark_node *next_node;
      cmark_node *current_node = opener->inl_text->next;
      while(current_node) {
        next_node = current_node->next;
        cmark_node_free(current_node);
        current_node = next_node;
      }

      cmark_node_free(opener->inl_text);

      pop_bracket(subj);
      return NULL;
    }
  }

  pop_bracket(subj); // remove this opener from delimiter list
  subj->pos = initial_pos;
  return make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("]"));

match:
  inl = make_simple(subj->mem, is_image ? CMARK_NODE_IMAGE : CMARK_NODE_LINK);
  inl->as.link.url = url;
  inl->as.link.title = title;
  inl->start_line = inl->end_line = subj->line;
  inl->start_column = opener->inl_text->start_column;
  inl->end_column = subj->pos + subj->column_offset + subj->block_offset;
  cmark_node_insert_before(opener->inl_text, inl);
  // Add link text:
  tmp = opener->inl_text->next;
  while (tmp) {
    tmpnext = tmp->next;
    cmark_node_unlink(tmp);
    append_child(inl, tmp);
    tmp = tmpnext;
  }

  // Free the bracket [:
  cmark_node_free(opener->inl_text);

  process_emphasis(parser, subj, opener->position);
  pop_bracket(subj);

  // Now, if we have a link, we also want to deactivate links until
  // we get a new opener. (This code can be removed if we decide to allow links
  // inside links.)
  if (!is_image) {
    subj->no_link_openers = true;
  }

  return NULL;
}

// Parse a hard or soft linebreak, returning an inline.
// Assumes the subject has a cr or newline at the current position.
static cmark_node *handle_newline(subject *subj) {
  bufsize_t nlpos = subj->pos;
  // skip over cr, crlf, or lf:
  if (peek_at(subj, subj->pos) == '\r') {
    advance(subj);
  }
  if (peek_at(subj, subj->pos) == '\n') {
    advance(subj);
  }
  ++subj->line;
  subj->column_offset = -subj->pos;
  // skip spaces at beginning of line
  skip_spaces(subj);
  if (nlpos > 1 && peek_at(subj, nlpos - 1) == ' ' &&
      peek_at(subj, nlpos - 2) == ' ') {
    return make_linebreak(subj->mem);
  } else {
    return make_softbreak(subj->mem);
  }
}

// "\r\n\\`&_*[]<!"
static int8_t SPECIAL_CHARS[256] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// " ' . -
static char SMART_PUNCT_CHARS[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static bufsize_t subject_find_special_char(subject *subj, int options) {
  bufsize_t n = subj->pos + 1;

  while (n < subj->input.len) {
    if (SPECIAL_CHARS[subj->input.data[n]])
      return n;
    if (options & CMARK_OPT_SMART && SMART_PUNCT_CHARS[subj->input.data[n]])
      return n;
    n++;
  }

  return subj->input.len;
}

void cmark_inlines_add_special_character(unsigned char c, bool emphasis) {
  SPECIAL_CHARS[c] = 1;
  if (emphasis)
    SKIP_CHARS[c] = 1;
}

void cmark_inlines_remove_special_character(unsigned char c, bool emphasis) {
  SPECIAL_CHARS[c] = 0;
  if (emphasis)
    SKIP_CHARS[c] = 0;
}

static cmark_node *try_extensions(cmark_parser *parser,
                                  cmark_node *parent,
                                  unsigned char c,
                                  subject *subj) {
  cmark_node *res = NULL;
  cmark_llist *tmp;

  for (tmp = parser->inline_syntax_extensions; tmp; tmp = tmp->next) {
    cmark_syntax_extension *ext = (cmark_syntax_extension *) tmp->data;
    res = ext->match_inline(ext, parser, parent, c, subj);

    if (res)
      break;
  }

  return res;
}

// Parse an inline, advancing subject, and add it as a child of parent.
// Return 0 if no inline can be parsed, 1 otherwise.
static int parse_inline(cmark_parser *parser, subject *subj, cmark_node *parent, int options) {
  cmark_node *new_inl = NULL;
  cmark_chunk contents;
  unsigned char c;
  bufsize_t startpos, endpos;
  c = peek_char(subj);
  if (c == 0) {
    return 0;
  }
  switch (c) {
  case '\r':
  case '\n':
    new_inl = handle_newline(subj);
    break;
  case '`':
    new_inl = handle_backticks(subj, options);
    break;
  case '\\':
    new_inl = handle_backslash(parser, subj);
    break;
  case '&':
    new_inl = handle_entity(subj);
    break;
  case '<':
    new_inl = handle_pointy_brace(subj, options);
    break;
  case '*':
  case '_':
  case '\'':
  case '"':
    new_inl = handle_delim(subj, c, (options & CMARK_OPT_SMART) != 0);
    break;
  case '-':
    new_inl = handle_hyphen(subj, (options & CMARK_OPT_SMART) != 0);
    break;
  case '.':
    new_inl = handle_period(subj, (options & CMARK_OPT_SMART) != 0);
    break;
  case '[':
    advance(subj);
    new_inl = make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("["));
    push_bracket(subj, false, new_inl);
    break;
  case ']':
    new_inl = handle_close_bracket(parser, subj);
    break;
  case '!':
    advance(subj);
    if (peek_char(subj) == '[' && peek_char_n(subj, 1) != '^') {
      advance(subj);
      new_inl = make_str(subj, subj->pos - 2, subj->pos - 1, cmark_chunk_literal("!["));
      push_bracket(subj, true, new_inl);
    } else {
      new_inl = make_str(subj, subj->pos - 1, subj->pos - 1, cmark_chunk_literal("!"));
    }
    break;
  default:
    new_inl = try_extensions(parser, parent, c, subj);
    if (new_inl != NULL)
      break;

    endpos = subject_find_special_char(subj, options);
    contents = cmark_chunk_dup(&subj->input, subj->pos, endpos - subj->pos);
    startpos = subj->pos;
    subj->pos = endpos;

    // if we're at a newline, strip trailing spaces.
    if (S_is_line_end_char(peek_char(subj))) {
      cmark_chunk_rtrim(&contents);
    }

    new_inl = make_str(subj, startpos, endpos - 1, contents);
  }
  if (new_inl != NULL) {
    append_child(parent, new_inl);
  }

  return 1;
}

// Parse inlines from parent's string_content, adding as children of parent.
void cmark_parse_inlines(cmark_parser *parser,
                         cmark_node *parent,
                         cmark_map *refmap,
                         int options) {
  subject subj;
  cmark_chunk content = {parent->content.ptr, parent->content.size, 0};
  subject_from_buf(parser->mem, parent->start_line, parent->start_column - 1 + parent->internal_offset, &subj, &content, refmap);
  cmark_chunk_rtrim(&subj.input);

  while (!is_eof(&subj) && parse_inline(parser, &subj, parent, options))
    ;

  process_emphasis(parser, &subj, 0);
  // free bracket and delim stack
  while (subj.last_delim) {
    remove_delimiter(&subj, subj.last_delim);
  }
  while (subj.last_bracket) {
    pop_bracket(&subj);
  }
}

// Parse zero or more space characters, including at most one newline.
static void spnl(subject *subj) {
  skip_spaces(subj);
  if (skip_line_end(subj)) {
    skip_spaces(subj);
  }
}

// Parse reference.  Assumes string begins with '[' character.
// Modify refmap if a reference is encountered.
// Return 0 if no reference found, otherwise position of subject
// after reference is parsed.
bufsize_t cmark_parse_reference_inline(cmark_mem *mem, cmark_chunk *input,
                                       cmark_map *refmap) {
  subject subj;

  cmark_chunk lab;
  cmark_chunk url;
  cmark_chunk title;

  bufsize_t matchlen = 0;
  bufsize_t beforetitle;

  subject_from_buf(mem, -1, 0, &subj, input, NULL);

  // parse label:
  if (!link_label(&subj, &lab) || lab.len == 0)
    return 0;

  // colon:
  if (peek_char(&subj) == ':') {
    advance(&subj);
  } else {
    return 0;
  }

  // parse link url:
  spnl(&subj);
  if ((matchlen = manual_scan_link_url(&subj.input, subj.pos, &url)) > -1) {
    subj.pos += matchlen;
  } else {
    return 0;
  }

  // parse optional link_title
  beforetitle = subj.pos;
  spnl(&subj);
  matchlen = subj.pos == beforetitle ? 0 : scan_link_title(&subj.input, subj.pos);
  if (matchlen) {
    title = cmark_chunk_dup(&subj.input, subj.pos, matchlen);
    subj.pos += matchlen;
  } else {
    subj.pos = beforetitle;
    title = cmark_chunk_literal("");
  }

  // parse final spaces and newline:
  skip_spaces(&subj);
  if (!skip_line_end(&subj)) {
    if (matchlen) { // try rewinding before title
      subj.pos = beforetitle;
      skip_spaces(&subj);
      if (!skip_line_end(&subj)) {
        return 0;
      }
    } else {
      return 0;
    }
  }
  // insert reference into refmap
  cmark_reference_create(refmap, &lab, &url, &title);
  return subj.pos;
}

unsigned char cmark_inline_parser_peek_char(cmark_inline_parser *parser) {
  return peek_char(parser);
}

unsigned char cmark_inline_parser_peek_at(cmark_inline_parser *parser, bufsize_t pos) {
  return peek_at(parser, pos);
}

int cmark_inline_parser_is_eof(cmark_inline_parser *parser) {
  return is_eof(parser);
}

static char *
my_strndup (const char *s, size_t n)
{
  char *result;
  size_t len = strlen (s);

  if (n < len)
    len = n;

  result = (char *) malloc (len + 1);
  if (!result)
    return 0;

  result[len] = '\0';
  return (char *) memcpy (result, s, len);
}

char *cmark_inline_parser_take_while(cmark_inline_parser *parser, cmark_inline_predicate pred) {
  unsigned char c;
  bufsize_t startpos = parser->pos;
  bufsize_t len = 0;

  while ((c = peek_char(parser)) && (*pred)(c)) {
    advance(parser);
    len++;
  }

  return my_strndup((const char *) parser->input.data + startpos, len);
}

void cmark_inline_parser_push_delimiter(cmark_inline_parser *parser,
                                  unsigned char c,
                                  int can_open,
                                  int can_close,
                                  cmark_node *inl_text) {
  push_delimiter(parser, c, can_open != 0, can_close != 0, inl_text);
}

void cmark_inline_parser_remove_delimiter(cmark_inline_parser *parser, delimiter *delim) {
  remove_delimiter(parser, delim);
}

int cmark_inline_parser_scan_delimiters(cmark_inline_parser *parser,
                                  int max_delims,
                                  unsigned char c,
                                  int *left_flanking,
                                  int *right_flanking,
                                  int *punct_before,
                                  int *punct_after) {
  int numdelims = 0;
  bufsize_t before_char_pos;
  int32_t after_char = 0;
  int32_t before_char = 0;
  int len;
  bool space_before, space_after;

  if (parser->pos == 0) {
    before_char = 10;
  } else {
    before_char_pos = parser->pos - 1;
    // walk back to the beginning of the UTF_8 sequence:
    while (peek_at(parser, before_char_pos) >> 6 == 2 && before_char_pos > 0) {
      before_char_pos -= 1;
    }
    len = cmark_utf8proc_iterate(parser->input.data + before_char_pos,
                                 parser->pos - before_char_pos, &before_char);
    if (len == -1) {
      before_char = 10;
    }
  }

  while (peek_char(parser) == c && numdelims < max_delims) {
    numdelims++;
    advance(parser);
  }

  len = cmark_utf8proc_iterate(parser->input.data + parser->pos,
                               parser->input.len - parser->pos, &after_char);
  if (len == -1) {
    after_char = 10;
  }

  *punct_before = cmark_utf8proc_is_punctuation(before_char);
  *punct_after = cmark_utf8proc_is_punctuation(after_char);
  space_before = cmark_utf8proc_is_space(before_char) != 0;
  space_after = cmark_utf8proc_is_space(after_char) != 0;

  *left_flanking = numdelims > 0 && !cmark_utf8proc_is_space(after_char) &&
                  !(*punct_after && !space_before && !*punct_before);
  *right_flanking = numdelims > 0 && !cmark_utf8proc_is_space(before_char) &&
                  !(*punct_before && !space_after && !*punct_after);

  return numdelims;
}

void cmark_inline_parser_advance_offset(cmark_inline_parser *parser) {
  advance(parser);
}

int cmark_inline_parser_get_offset(cmark_inline_parser *parser) {
  return parser->pos;
}

void cmark_inline_parser_set_offset(cmark_inline_parser *parser, int offset) {
  parser->pos = offset;
}

int cmark_inline_parser_get_column(cmark_inline_parser *parser) {
  return parser->pos + 1 + parser->column_offset + parser->block_offset;
}

cmark_chunk *cmark_inline_parser_get_chunk(cmark_inline_parser *parser) {
  return &parser->input;
}

int cmark_inline_parser_in_bracket(cmark_inline_parser *parser, int image) {
  bracket *b = parser->last_bracket;
  if (!b) {
    return 0;
  }
  if (image != 0) {
    return b->in_bracket_image1;
  } else {
    return b->in_bracket_image0;
  }
}

void cmark_node_unput(cmark_node *node, int n) {
	node = node->last_child;
	while (n > 0 && node && node->type == CMARK_NODE_TEXT) {
		if (node->as.literal.len < n) {
			n -= node->as.literal.len;
			node->as.literal.len = 0;
		} else {
			node->as.literal.len -= n;
			n = 0;
		}
		node = node->prev;
	}
}

delimiter *cmark_inline_parser_get_last_delimiter(cmark_inline_parser *parser) {
  return parser->last_delim;
}

int cmark_inline_parser_get_line(cmark_inline_parser *parser) {
  return parser->line;
}
