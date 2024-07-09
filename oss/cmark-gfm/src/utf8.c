#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "cmark_ctype.h"
#include "utf8.h"

static const int8_t utf8proc_utf8class[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0};

static void encode_unknown(cmark_strbuf *buf) {
  static const uint8_t repl[] = {239, 191, 189};
  cmark_strbuf_put(buf, repl, 3);
}

static int utf8proc_charlen(const uint8_t *str, bufsize_t str_len) {
  int length, i;

  if (!str_len)
    return 0;

  length = utf8proc_utf8class[str[0]];

  if (!length)
    return -1;

  if (str_len >= 0 && (bufsize_t)length > str_len)
    return -str_len;

  for (i = 1; i < length; i++) {
    if ((str[i] & 0xC0) != 0x80)
      return -i;
  }

  return length;
}

// Validate a single UTF-8 character according to RFC 3629.
static int utf8proc_valid(const uint8_t *str, bufsize_t str_len) {
  int length = utf8proc_utf8class[str[0]];

  if (!length)
    return -1;

  if ((bufsize_t)length > str_len)
    return -str_len;

  switch (length) {
  case 2:
    if ((str[1] & 0xC0) != 0x80)
      return -1;
    if (str[0] < 0xC2) {
      // Overlong
      return -length;
    }
    break;

  case 3:
    if ((str[1] & 0xC0) != 0x80)
      return -1;
    if ((str[2] & 0xC0) != 0x80)
      return -2;
    if (str[0] == 0xE0) {
      if (str[1] < 0xA0) {
        // Overlong
        return -length;
      }
    } else if (str[0] == 0xED) {
      if (str[1] >= 0xA0) {
        // Surrogate
        return -length;
      }
    }
    break;

  case 4:
    if ((str[1] & 0xC0) != 0x80)
      return -1;
    if ((str[2] & 0xC0) != 0x80)
      return -2;
    if ((str[3] & 0xC0) != 0x80)
      return -3;
    if (str[0] == 0xF0) {
      if (str[1] < 0x90) {
        // Overlong
        return -length;
      }
    } else if (str[0] >= 0xF4) {
      if (str[0] > 0xF4 || str[1] >= 0x90) {
        // Above 0x10FFFF
        return -length;
      }
    }
    break;
  }

  return length;
}

void cmark_utf8proc_check(cmark_strbuf *ob, const uint8_t *line,
                          bufsize_t size) {
  bufsize_t i = 0;

  while (i < size) {
    bufsize_t org = i;
    int charlen = 0;

    while (i < size) {
      if (line[i] < 0x80 && line[i] != 0) {
        i++;
      } else if (line[i] >= 0x80) {
        charlen = utf8proc_valid(line + i, size - i);
        if (charlen < 0) {
          charlen = -charlen;
          break;
        }
        i += charlen;
      } else if (line[i] == 0) {
        // ASCII NUL is technically valid but rejected
        // for security reasons.
        charlen = 1;
        break;
      }
    }

    if (i > org) {
      cmark_strbuf_put(ob, line + org, i - org);
    }

    if (i >= size) {
      break;
    } else {
      // Invalid UTF-8
      encode_unknown(ob);
      i += charlen;
    }
  }
}

int cmark_utf8proc_iterate(const uint8_t *str, bufsize_t str_len,
                           int32_t *dst) {
  int length;
  int32_t uc = -1;

  *dst = -1;
  length = utf8proc_charlen(str, str_len);
  if (length < 0)
    return -1;

  switch (length) {
  case 1:
    uc = str[0];
    break;
  case 2:
    uc = ((str[0] & 0x1F) << 6) + (str[1] & 0x3F);
    if (uc < 0x80)
      uc = -1;
    break;
  case 3:
    uc = ((str[0] & 0x0F) << 12) + ((str[1] & 0x3F) << 6) + (str[2] & 0x3F);
    if (uc < 0x800 || (uc >= 0xD800 && uc < 0xE000))
      uc = -1;
    break;
  case 4:
    uc = ((str[0] & 0x07) << 18) + ((str[1] & 0x3F) << 12) +
         ((str[2] & 0x3F) << 6) + (str[3] & 0x3F);
    if (uc < 0x10000 || uc >= 0x110000)
      uc = -1;
    break;
  }

  if (uc < 0)
    return -1;

  *dst = uc;
  return length;
}

void cmark_utf8proc_encode_char(int32_t uc, cmark_strbuf *buf) {
  uint8_t dst[4];
  bufsize_t len = 0;

  assert(uc >= 0);

  if (uc < 0x80) {
    dst[0] = (uint8_t)(uc);
    len = 1;
  } else if (uc < 0x800) {
    dst[0] = (uint8_t)(0xC0 + (uc >> 6));
    dst[1] = 0x80 + (uc & 0x3F);
    len = 2;
  } else if (uc == 0xFFFF) {
    dst[0] = 0xFF;
    len = 1;
  } else if (uc == 0xFFFE) {
    dst[0] = 0xFE;
    len = 1;
  } else if (uc < 0x10000) {
    dst[0] = (uint8_t)(0xE0 + (uc >> 12));
    dst[1] = 0x80 + ((uc >> 6) & 0x3F);
    dst[2] = 0x80 + (uc & 0x3F);
    len = 3;
  } else if (uc < 0x110000) {
    dst[0] = (uint8_t)(0xF0 + (uc >> 18));
    dst[1] = 0x80 + ((uc >> 12) & 0x3F);
    dst[2] = 0x80 + ((uc >> 6) & 0x3F);
    dst[3] = 0x80 + (uc & 0x3F);
    len = 4;
  } else {
    encode_unknown(buf);
    return;
  }

  cmark_strbuf_put(buf, dst, len);
}

void cmark_utf8proc_case_fold(cmark_strbuf *dest, const uint8_t *str,
                              bufsize_t len) {
  int32_t c;

#define bufpush(x) cmark_utf8proc_encode_char(x, dest)

  while (len > 0) {
    bufsize_t char_len = cmark_utf8proc_iterate(str, len, &c);

    if (char_len >= 0) {
#include "case_fold_switch.inc"
    } else {
      encode_unknown(dest);
      char_len = -char_len;
    }

    str += char_len;
    len -= char_len;
  }
}

// matches anything in the Zs class, plus LF, CR, TAB, FF.
int cmark_utf8proc_is_space(int32_t uc) {
  return (uc == 9 || uc == 10 || uc == 12 || uc == 13 || uc == 32 ||
          uc == 160 || uc == 5760 || (uc >= 8192 && uc <= 8202) || uc == 8239 ||
          uc == 8287 || uc == 12288);
}

// matches anything in the P[cdefios] classes.
int cmark_utf8proc_is_punctuation(int32_t uc) {
  return (
      (uc < 128 && cmark_ispunct((char)uc)) || uc == 161 || uc == 167 ||
      uc == 171 || uc == 182 || uc == 183 || uc == 187 || uc == 191 ||
      uc == 894 || uc == 903 || (uc >= 1370 && uc <= 1375) || uc == 1417 ||
      uc == 1418 || uc == 1470 || uc == 1472 || uc == 1475 || uc == 1478 ||
      uc == 1523 || uc == 1524 || uc == 1545 || uc == 1546 || uc == 1548 ||
      uc == 1549 || uc == 1563 || uc == 1566 || uc == 1567 ||
      (uc >= 1642 && uc <= 1645) || uc == 1748 || (uc >= 1792 && uc <= 1805) ||
      (uc >= 2039 && uc <= 2041) || (uc >= 2096 && uc <= 2110) || uc == 2142 ||
      uc == 2404 || uc == 2405 || uc == 2416 || uc == 2800 || uc == 3572 ||
      uc == 3663 || uc == 3674 || uc == 3675 || (uc >= 3844 && uc <= 3858) ||
      uc == 3860 || (uc >= 3898 && uc <= 3901) || uc == 3973 ||
      (uc >= 4048 && uc <= 4052) || uc == 4057 || uc == 4058 ||
      (uc >= 4170 && uc <= 4175) || uc == 4347 || (uc >= 4960 && uc <= 4968) ||
      uc == 5120 || uc == 5741 || uc == 5742 || uc == 5787 || uc == 5788 ||
      (uc >= 5867 && uc <= 5869) || uc == 5941 || uc == 5942 ||
      (uc >= 6100 && uc <= 6102) || (uc >= 6104 && uc <= 6106) ||
      (uc >= 6144 && uc <= 6154) || uc == 6468 || uc == 6469 || uc == 6686 ||
      uc == 6687 || (uc >= 6816 && uc <= 6822) || (uc >= 6824 && uc <= 6829) ||
      (uc >= 7002 && uc <= 7008) || (uc >= 7164 && uc <= 7167) ||
      (uc >= 7227 && uc <= 7231) || uc == 7294 || uc == 7295 ||
      (uc >= 7360 && uc <= 7367) || uc == 7379 || (uc >= 8208 && uc <= 8231) ||
      (uc >= 8240 && uc <= 8259) || (uc >= 8261 && uc <= 8273) ||
      (uc >= 8275 && uc <= 8286) || uc == 8317 || uc == 8318 || uc == 8333 ||
      uc == 8334 || (uc >= 8968 && uc <= 8971) || uc == 9001 || uc == 9002 ||
      (uc >= 10088 && uc <= 10101) || uc == 10181 || uc == 10182 ||
      (uc >= 10214 && uc <= 10223) || (uc >= 10627 && uc <= 10648) ||
      (uc >= 10712 && uc <= 10715) || uc == 10748 || uc == 10749 ||
      (uc >= 11513 && uc <= 11516) || uc == 11518 || uc == 11519 ||
      uc == 11632 || (uc >= 11776 && uc <= 11822) ||
      (uc >= 11824 && uc <= 11842) || (uc >= 12289 && uc <= 12291) ||
      (uc >= 12296 && uc <= 12305) || (uc >= 12308 && uc <= 12319) ||
      uc == 12336 || uc == 12349 || uc == 12448 || uc == 12539 || uc == 42238 ||
      uc == 42239 || (uc >= 42509 && uc <= 42511) || uc == 42611 ||
      uc == 42622 || (uc >= 42738 && uc <= 42743) ||
      (uc >= 43124 && uc <= 43127) || uc == 43214 || uc == 43215 ||
      (uc >= 43256 && uc <= 43258) || uc == 43310 || uc == 43311 ||
      uc == 43359 || (uc >= 43457 && uc <= 43469) || uc == 43486 ||
      uc == 43487 || (uc >= 43612 && uc <= 43615) || uc == 43742 ||
      uc == 43743 || uc == 43760 || uc == 43761 || uc == 44011 || uc == 64830 ||
      uc == 64831 || (uc >= 65040 && uc <= 65049) ||
      (uc >= 65072 && uc <= 65106) || (uc >= 65108 && uc <= 65121) ||
      uc == 65123 || uc == 65128 || uc == 65130 || uc == 65131 ||
      (uc >= 65281 && uc <= 65283) || (uc >= 65285 && uc <= 65290) ||
      (uc >= 65292 && uc <= 65295) || uc == 65306 || uc == 65307 ||
      uc == 65311 || uc == 65312 || (uc >= 65339 && uc <= 65341) ||
      uc == 65343 || uc == 65371 || uc == 65373 ||
      (uc >= 65375 && uc <= 65381) || (uc >= 65792 && uc <= 65794) ||
      uc == 66463 || uc == 66512 || uc == 66927 || uc == 67671 || uc == 67871 ||
      uc == 67903 || (uc >= 68176 && uc <= 68184) || uc == 68223 ||
      (uc >= 68336 && uc <= 68342) || (uc >= 68409 && uc <= 68415) ||
      (uc >= 68505 && uc <= 68508) || (uc >= 69703 && uc <= 69709) ||
      uc == 69819 || uc == 69820 || (uc >= 69822 && uc <= 69825) ||
      (uc >= 69952 && uc <= 69955) || uc == 70004 || uc == 70005 ||
      (uc >= 70085 && uc <= 70088) || uc == 70093 ||
      (uc >= 70200 && uc <= 70205) || uc == 70854 ||
      (uc >= 71105 && uc <= 71113) || (uc >= 71233 && uc <= 71235) ||
      (uc >= 74864 && uc <= 74868) || uc == 92782 || uc == 92783 ||
      uc == 92917 || (uc >= 92983 && uc <= 92987) || uc == 92996 ||
      uc == 113823);
}
