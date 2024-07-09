#ifndef CMARK_HOUDINI_H
#define CMARK_HOUDINI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "config.h"
#include "buffer.h"

#ifdef HAVE___BUILTIN_EXPECT
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#ifdef HOUDINI_USE_LOCALE
#define _isxdigit(c) isxdigit(c)
#define _isdigit(c) isdigit(c)
#else
/*
 * Helper _isdigit methods -- do not trust the current locale
 * */
#define _isxdigit(c) (strchr("0123456789ABCDEFabcdef", (c)) != NULL)
#define _isdigit(c) ((c) >= '0' && (c) <= '9')
#endif

#define HOUDINI_ESCAPED_SIZE(x) (((x)*12) / 10)
#define HOUDINI_UNESCAPED_SIZE(x) (x)


bufsize_t houdini_unescape_ent(cmark_strbuf *ob, const uint8_t *src,
                                      bufsize_t size);

int houdini_escape_html(cmark_strbuf *ob, const uint8_t *src,
                               bufsize_t size);

int houdini_escape_html0(cmark_strbuf *ob, const uint8_t *src,
                                bufsize_t size, int secure);

int houdini_unescape_html(cmark_strbuf *ob, const uint8_t *src,
                                 bufsize_t size);

void houdini_unescape_html_f(cmark_strbuf *ob, const uint8_t *src,
                                    bufsize_t size);

int houdini_escape_href(cmark_strbuf *ob, const uint8_t *src,
                               bufsize_t size);

#ifdef __cplusplus
}
#endif

#endif
