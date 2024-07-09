#include "autolink.h"
#include "parser.h"
#include <string.h>
#include "utf8.h"
#include "chunk.h"
#include <stddef.h>

#if defined(_WIN32)
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

static int is_valid_hostchar(const uint8_t *link, size_t link_len) {
  int32_t ch;
  int r = cmark_utf8proc_iterate(link, (bufsize_t)link_len, &ch);
  if (r < 0)
    return 0;
  return !cmark_utf8proc_is_space(ch) && !cmark_utf8proc_is_punctuation(ch);
}

static int sd_autolink_issafe(const uint8_t *link, size_t link_len) {
  static const size_t valid_uris_count = 3;
  static const char *valid_uris[] = {"http://", "https://", "ftp://"};

  size_t i;

  for (i = 0; i < valid_uris_count; ++i) {
    size_t len = strlen(valid_uris[i]);

    if (link_len > len && strncasecmp((char *)link, valid_uris[i], len) == 0 &&
        is_valid_hostchar(link + len, link_len - len))
      return 1;
  }

  return 0;
}

static size_t autolink_delim(uint8_t *data, size_t link_end) {
  size_t i;
  size_t closing = 0;
  size_t opening = 0;

  for (i = 0; i < link_end; ++i) {
    const uint8_t c = data[i];
    if (c == '<') {
      link_end = i;
      break;
    } else if (c == '(') {
      opening++;
    } else if (c == ')') {
      closing++;
    }
  }

  while (link_end > 0) {
    switch (data[link_end - 1]) {
    case ')':
      /* Allow any number of matching brackets (as recognised in copen/cclose)
       * at the end of the URL.  If there is a greater number of closing
       * brackets than opening ones, we remove one character from the end of
       * the link.
       *
       * Examples (input text => output linked portion):
       *
       *        http://www.pokemon.com/Pikachu_(Electric)
       *                => http://www.pokemon.com/Pikachu_(Electric)
       *
       *        http://www.pokemon.com/Pikachu_((Electric)
       *                => http://www.pokemon.com/Pikachu_((Electric)
       *
       *        http://www.pokemon.com/Pikachu_(Electric))
       *                => http://www.pokemon.com/Pikachu_(Electric)
       *
       *        http://www.pokemon.com/Pikachu_((Electric))
       *                => http://www.pokemon.com/Pikachu_((Electric))
       */
      if (closing <= opening) {
        return link_end;
      }
      closing--;
      link_end--;
      break;
    case '?':
    case '!':
    case '.':
    case ',':
    case ':':
    case '*':
    case '_':
    case '~':
    case '\'':
    case '"':
      link_end--;
      break;
    case ';': {
      size_t new_end = link_end - 2;

      while (new_end > 0 && cmark_isalpha(data[new_end]))
        new_end--;

      if (new_end < link_end - 2 && data[new_end] == '&')
        link_end = new_end;
      else
        link_end--;
      break;
    }

    default:
      return link_end;
    }
  }

  return link_end;
}

static size_t check_domain(uint8_t *data, size_t size, int allow_short) {
  size_t i, np = 0, uscore1 = 0, uscore2 = 0;

  /* The purpose of this code is to reject urls that contain an underscore
   * in one of the last two segments. Examples:
   *
   *   www.xxx.yyy.zzz     autolinked
   *   www.xxx.yyy._zzz    not autolinked
   *   www.xxx._yyy.zzz    not autolinked
   *   www._xxx.yyy.zzz    autolinked
   *
   * The reason is that domain names are allowed to include underscores,
   * but host names are not. See: https://stackoverflow.com/a/2183140
   */
  for (i = 1; i < size - 1; i++) {
    if (data[i] == '\\' && i < size - 2)
      i++;
    if (data[i] == '_')
      uscore2++;
    else if (data[i] == '.') {
      uscore1 = uscore2;
      uscore2 = 0;
      np++;
    } else if (!is_valid_hostchar(data + i, size - i) && data[i] != '-')
      break;
  }

  if (uscore1 > 0 || uscore2 > 0) {
    /* If the url is very long then accept it despite the underscores,
     * to avoid quadratic behavior causing a denial of service. See:
     * https://github.com/github/cmark-gfm/security/advisories/GHSA-29g3-96g3-jg6c
     * Reasonable urls are unlikely to have more than 10 segments, so
     * this extra condition shouldn't have any impact on normal usage.
     */
    if (np <= 10) {
      return 0;
    }
  }

  if (allow_short) {
    /* We don't need a valid domain in the strict sense (with
     * least one dot; so just make sure it's composed of valid
     * domain characters and return the length of the the valid
     * sequence. */
    return i;
  } else {
    /* a valid domain needs to have at least a dot.
     * that's as far as we get */
    return np ? i : 0;
  }
}

static cmark_node *www_match(cmark_parser *parser, cmark_node *parent,
                             cmark_inline_parser *inline_parser) {
  cmark_chunk *chunk = cmark_inline_parser_get_chunk(inline_parser);
  size_t max_rewind = cmark_inline_parser_get_offset(inline_parser);
  uint8_t *data = chunk->data + max_rewind;
  size_t size = chunk->len - max_rewind;
  int start = cmark_inline_parser_get_column(inline_parser);

  size_t link_end;

  if (max_rewind > 0 && strchr("*_~(", data[-1]) == NULL &&
      !cmark_isspace(data[-1]))
    return 0;

  if (size < 4 || memcmp(data, "www.", strlen("www.")) != 0)
    return 0;

  link_end = check_domain(data, size, 0);

  if (link_end == 0)
    return NULL;

  while (link_end < size && !cmark_isspace(data[link_end]) && data[link_end] != '<')
    link_end++;

  link_end = autolink_delim(data, link_end);

  if (link_end == 0)
    return NULL;

  cmark_inline_parser_set_offset(inline_parser, (int)(max_rewind + link_end));

  cmark_node *node = cmark_node_new_with_mem(CMARK_NODE_LINK, parser->mem);

  cmark_strbuf buf;
  cmark_strbuf_init(parser->mem, &buf, 10);
  cmark_strbuf_puts(&buf, "http://");
  cmark_strbuf_put(&buf, data, (bufsize_t)link_end);
  node->as.link.url = cmark_chunk_buf_detach(&buf);

  cmark_node *text = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
  text->as.literal =
      cmark_chunk_dup(chunk, (bufsize_t)max_rewind, (bufsize_t)link_end);
  cmark_node_append_child(node, text);

  node->start_line = text->start_line =
    node->end_line = text->end_line =
    cmark_inline_parser_get_line(inline_parser);

  node->start_column = text->start_column = start - 1;
  node->end_column = text->end_column = cmark_inline_parser_get_column(inline_parser) - 1;

  return node;
}

static cmark_node *url_match(cmark_parser *parser, cmark_node *parent,
                             cmark_inline_parser *inline_parser) {
  size_t link_end, domain_len;
  int rewind = 0;

  cmark_chunk *chunk = cmark_inline_parser_get_chunk(inline_parser);
  int max_rewind = cmark_inline_parser_get_offset(inline_parser);
  uint8_t *data = chunk->data + max_rewind;
  size_t size = chunk->len - max_rewind;

  if (size < 4 || data[1] != '/' || data[2] != '/')
    return 0;

  while (rewind < max_rewind && cmark_isalpha(data[-rewind - 1]))
    rewind++;

  if (!sd_autolink_issafe(data - rewind, size + rewind))
    return 0;

  link_end = strlen("://");

  domain_len = check_domain(data + link_end, size - link_end, 1);

  if (domain_len == 0)
    return 0;

  link_end += domain_len;
  while (link_end < size && !cmark_isspace(data[link_end]) && data[link_end] != '<')
    link_end++;

  link_end = autolink_delim(data, link_end);

  if (link_end == 0)
    return NULL;

  cmark_inline_parser_set_offset(inline_parser, (int)(max_rewind + link_end));
  cmark_node_unput(parent, rewind);

  cmark_node *node = cmark_node_new_with_mem(CMARK_NODE_LINK, parser->mem);

  cmark_chunk url = cmark_chunk_dup(chunk, max_rewind - rewind,
                                    (bufsize_t)(link_end + rewind));
  node->as.link.url = url;

  cmark_node *text = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
  text->as.literal = url;
  cmark_node_append_child(node, text);
  
  node->start_line = text->start_line = node->end_line = text->end_line = cmark_inline_parser_get_line(inline_parser);

  node->start_column = text->start_column = max_rewind - rewind;
  node->end_column = text->end_column = cmark_inline_parser_get_column(inline_parser) - 1;

  return node;
}

static cmark_node *match(cmark_syntax_extension *ext, cmark_parser *parser,
                         cmark_node *parent, unsigned char c,
                         cmark_inline_parser *inline_parser) {
  if (cmark_inline_parser_in_bracket(inline_parser, false) ||
      cmark_inline_parser_in_bracket(inline_parser, true))
    return NULL;

  if (c == ':')
    return url_match(parser, parent, inline_parser);

  if (c == 'w')
    return www_match(parser, parent, inline_parser);

  return NULL;

  // note that we could end up re-consuming something already a
  // part of an inline, because we don't track when the last
  // inline was finished in inlines.c.
}

static bool validate_protocol(const char protocol[], uint8_t *data, size_t rewind, size_t max_rewind) {
  size_t len = strlen(protocol);

  if (len > (max_rewind - rewind)) {
    return false;
  }

  // Check that the protocol matches
  if (memcmp(data - rewind - len, protocol, len) != 0) {
    return false;
  }

  if (len == (max_rewind - rewind)) {
    return true;
  }

  char prev_char = data[-((ptrdiff_t)rewind) - len - 1];

  // Make sure the character before the protocol is non-alphanumeric
  return !cmark_isalnum(prev_char);
}

static void postprocess_text(cmark_parser *parser, cmark_node *text) {
  size_t start = 0;
  size_t offset = 0;
  // `text` is going to be split into a list of nodes containing shorter segments
  // of text, so we detach the memory buffer from text and use `cmark_chunk_dup` to
  // create references to it. Later, `cmark_chunk_to_cstr` is used to convert
  // the references into allocated buffers. The detached buffer is freed before we
  // return.
  cmark_chunk detached_chunk = text->as.literal;
  text->as.literal = cmark_chunk_dup(&detached_chunk, 0, detached_chunk.len);

  uint8_t *data = text->as.literal.data;
  size_t remaining = text->as.literal.len;

  while (true) {
    size_t link_end;
    uint8_t *at;
    bool auto_mailto = true;
    bool is_xmpp = false;
    size_t rewind;
    size_t max_rewind;
    size_t np = 0;

    if (offset >= remaining)
      break;

    at = (uint8_t *)memchr(data + start + offset, '@', remaining - offset);
    if (!at)
      break;

    max_rewind = at - (data + start + offset);

found_at:
    for (rewind = 0; rewind < max_rewind; ++rewind) {
      uint8_t c = data[start + offset + max_rewind - rewind - 1];

      if (cmark_isalnum(c))
        continue;

      if (strchr(".+-_", c) != NULL)
        continue;

      if (strchr(":", c) != NULL) {
        if (validate_protocol("mailto:", data + start + offset + max_rewind, rewind, max_rewind)) {
          auto_mailto = false;
          continue;
        }

        if (validate_protocol("xmpp:", data + start + offset + max_rewind, rewind, max_rewind)) {
          auto_mailto = false;
          is_xmpp = true;
          continue;
        }
      }

      break;
    }

    if (rewind == 0) {
      offset += max_rewind + 1;
      continue;
    }

    assert(data[start + offset + max_rewind] == '@');
    for (link_end = 1; link_end < remaining - offset - max_rewind; ++link_end) {
      uint8_t c = data[start + offset + max_rewind + link_end];

      if (cmark_isalnum(c))
        continue;

      if (c == '@') {
        // Found another '@', so go back and try again with an updated offset and max_rewind.
        offset += max_rewind + 1;
        max_rewind = link_end - 1;
        goto found_at;
      } else if (c == '.' && link_end < remaining - offset - max_rewind - 1 &&
               cmark_isalnum(data[start + offset + max_rewind + link_end + 1]))
        np++;
      else if (c == '/' && is_xmpp)
        continue;
      else if (c != '-' && c != '_')
        break;
    }

    if (link_end < 2 || np == 0 ||
        (!cmark_isalpha(data[start + offset + max_rewind + link_end - 1]) &&
         data[start + offset + max_rewind + link_end - 1] != '.')) {
      offset += max_rewind + link_end;
      continue;
    }

    link_end = autolink_delim(data + start + offset + max_rewind, link_end);

    if (link_end == 0) {
      offset += max_rewind + 1;
      continue;
    }

    cmark_node *link_node = cmark_node_new_with_mem(CMARK_NODE_LINK, parser->mem);
    cmark_strbuf buf;
    cmark_strbuf_init(parser->mem, &buf, 10);
    if (auto_mailto)
      cmark_strbuf_puts(&buf, "mailto:");
    cmark_strbuf_put(&buf, data + start + offset + max_rewind - rewind, (bufsize_t)(link_end + rewind));
    link_node->as.link.url = cmark_chunk_buf_detach(&buf);

    cmark_node *link_text = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
    cmark_chunk email = cmark_chunk_dup(
      &detached_chunk,
      (bufsize_t)(start + offset + max_rewind - rewind),
      (bufsize_t)(link_end + rewind));
    cmark_chunk_to_cstr(parser->mem, &email);
    link_text->as.literal = email;
    cmark_node_append_child(link_node, link_text);

    cmark_node_insert_after(text, link_node);

    cmark_node *post = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
    post->as.literal = cmark_chunk_dup(&detached_chunk,
                                       (bufsize_t)(start + offset + max_rewind + link_end),
                                       (bufsize_t)(remaining - offset - max_rewind - link_end));

    cmark_node_insert_after(link_node, post);

    text->as.literal = cmark_chunk_dup(&detached_chunk, (bufsize_t)start, (bufsize_t)(offset + max_rewind - rewind));
    cmark_chunk_to_cstr(parser->mem, &text->as.literal);

    text = post;
    start += offset + max_rewind + link_end;
    remaining -= offset + max_rewind + link_end;
    offset = 0;
  }

  // Convert the reference to allocated memory.
  assert(!text->as.literal.alloc);
  cmark_chunk_to_cstr(parser->mem, &text->as.literal);

  // Free the detached buffer.
  cmark_chunk_free(parser->mem, &detached_chunk);
}

static cmark_node *postprocess(cmark_syntax_extension *ext, cmark_parser *parser, cmark_node *root) {
  cmark_iter *iter;
  cmark_event_type ev;
  cmark_node *node;
  bool in_link = false;

  cmark_consolidate_text_nodes(root);
  iter = cmark_iter_new(root);

  while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    node = cmark_iter_get_node(iter);
    if (in_link) {
      if (ev == CMARK_EVENT_EXIT && node->type == CMARK_NODE_LINK) {
        in_link = false;
      }
      continue;
    }

    if (ev == CMARK_EVENT_ENTER && node->type == CMARK_NODE_LINK) {
      in_link = true;
      continue;
    }

    if (ev == CMARK_EVENT_ENTER && node->type == CMARK_NODE_TEXT) {
      postprocess_text(parser, node);
    }
  }

  cmark_iter_free(iter);

  return root;
}

cmark_syntax_extension *create_autolink_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("autolink");
  cmark_llist *special_chars = NULL;

  cmark_syntax_extension_set_match_inline_func(ext, match);
  cmark_syntax_extension_set_postprocess_func(ext, postprocess);

  cmark_mem *mem = cmark_get_default_mem_allocator();
  special_chars = cmark_llist_append(mem, special_chars, (void *)':');
  special_chars = cmark_llist_append(mem, special_chars, (void *)'w');
  cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

  return ext;
}
