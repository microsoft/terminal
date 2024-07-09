#ifndef CMARK_UTF8_H
#define CMARK_UTF8_H

#include <stdint.h>
#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif


void cmark_utf8proc_case_fold(cmark_strbuf *dest, const uint8_t *str,
                              bufsize_t len);


void cmark_utf8proc_encode_char(int32_t uc, cmark_strbuf *buf);


int cmark_utf8proc_iterate(const uint8_t *str, bufsize_t str_len, int32_t *dst);


void cmark_utf8proc_check(cmark_strbuf *dest, const uint8_t *line,
                          bufsize_t size);


int cmark_utf8proc_is_space(int32_t uc);


int cmark_utf8proc_is_punctuation(int32_t uc);

#ifdef __cplusplus
}
#endif

#endif
