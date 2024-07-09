#include "tagfilter.h"
#include "parser.h"
#include <ctype.h>

static const char *blacklist[] = {
    "title",   "textarea", "style",  "xmp",       "iframe",
    "noembed", "noframes", "script", "plaintext", NULL,
};

static int is_tag(const unsigned char *tag_data, size_t tag_size,
                  const char *tagname) {
  size_t i;

  if (tag_size < 3 || tag_data[0] != '<')
    return 0;

  i = 1;

  if (tag_data[i] == '/') {
    i++;
  }

  for (; i < tag_size; ++i, ++tagname) {
    if (*tagname == 0)
      break;

    if (tolower(tag_data[i]) != *tagname)
      return 0;
  }

  if (i == tag_size)
    return 0;

  if (cmark_isspace(tag_data[i]) || tag_data[i] == '>')
    return 1;

  if (tag_data[i] == '/' && tag_size >= i + 2 && tag_data[i + 1] == '>')
    return 1;

  return 0;
}

static int filter(cmark_syntax_extension *ext, const unsigned char *tag,
                  size_t tag_len) {
  const char **it;

  for (it = blacklist; *it; ++it) {
    if (is_tag(tag, tag_len, *it)) {
      return 0;
    }
  }

  return 1;
}

cmark_syntax_extension *create_tagfilter_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("tagfilter");
  cmark_syntax_extension_set_html_filter_func(ext, filter);
  return ext;
}
