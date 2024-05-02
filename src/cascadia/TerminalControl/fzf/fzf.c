#include "fzf.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <icu.h>

#pragma warning(push)
#pragma warning(disable : 4100)

// TODO(conni2461): UNICODE HEADER
#define UNICODE_MAXASCII 0x7f

#define SFREE(x)                                                               \
  if (x) {                                                                     \
    free(x);                                                                   \
  }

/* Helpers */
#define free_alloc(obj)                                                        \
  if ((obj).allocated) {                                                       \
    free((obj).data);                                                          \
  }

#define gen_simple_slice(name, type)                                           \
  typedef struct {                                                             \
    type *data;                                                                \
    size_t size;                                                               \
  } name##_slice_t;                                                            \
  static name##_slice_t slice_##name(type *input, size_t from, size_t to) {    \
    return (name##_slice_t){.data = input + from, .size = to - from};          \
  }

#define gen_slice(name, type)                                                  \
  gen_simple_slice(name, type);                                                \
  static name##_slice_t slice_##name##_right(type *input, size_t to) {         \
    return slice_##name(input, 0, to);                                         \
  }

gen_slice(i16, int16_t);
gen_simple_slice(i32, int32_t);
gen_slice(str, const char);
gen_slice(ustr, const UChar);
#undef gen_slice
#undef gen_simple_slice

/* TODO(conni2461): additional types (utf8) */
typedef int32_t char_class;
//typedef char byte;

typedef enum {
  ScoreMatch = 16,
  ScoreGapStart = -3,
  ScoreGapExtention = -1,
  BonusBoundary = ScoreMatch / 2,
  BonusNonWord = ScoreMatch / 2,
  BonusCamel123 = BonusBoundary + ScoreGapExtention,
  BonusConsecutive = -(ScoreGapStart + ScoreGapExtention),
  BonusFirstCharMultiplier = 2,
} score_t;

typedef enum {
  UCharNonWord = 0,
  UCharLower,
  UCharUpper,
  UCharLetter,
  UCharNumber
} char_types;


typedef struct
{
  UText* utext;
  int32_t start;
  int32_t end; // Exclusive
  size_t size;
} UTextSlice;

void u_text_slice_init(UTextSlice* dest, UText* uText, int32_t start, int32_t end)
{
  dest->utext = uText;
  dest->start = start;
  dest->end = end;
  dest->size = end - start;
}

void u_text_slice(UTextSlice* dest, UTextSlice* slice, int32_t start, int32_t end)
{
  dest->utext = slice->utext;
  dest->start = slice->start + start;
  dest->end = slice->start + end;
  dest->size = dest->end - dest->start;
}

void u_text_slice_right(UTextSlice* dest, UTextSlice* slice, int32_t end)
{
  u_text_slice(dest, slice, 0, end);
}

UChar32 utextSlice_charAt(const UTextSlice* slice, int64_t index)
{
  if (index < 0 || index >= (slice->end - slice->start))
  {
      return U_SENTINEL;
  }
  UChar32 result = utext_char32At(slice->utext, slice->start + index);
  return result;
}

static int32_t uindex_byte(UTextSlice *string, UChar32 b) {
    for (size_t i = 0; i < string->size; i++) {
        if (utextSlice_charAt(string, i) == b) {
            return (int32_t)i;
        }
    }
    return -1;
}

static size_t uleading_whitespaces(UText* utext)
{
  size_t whitespaces = 0;
  //UErrorCode status = U_ZERO_ERROR;

  // Ensure the UText is properly initialized and ready for use
  // This includes opening it on the specific text if not already done

  int64_t length = utext_nativeLength(utext);

  for (int64_t i = 0; i < length; ++i)
  {
    UChar32 ch = utext_char32At(utext, i);
    if (!u_isUWhiteSpace(ch))
    {
      break;
    }
    whitespaces++;
    // Advance i to skip over code points that are represented by more than one unit in UTF-16
    if (U_IS_SUPPLEMENTARY(ch))
    {
      i++;
    }
  }

  return whitespaces;
}

static size_t utrailing_whitespaces(UText* utext)
{
  size_t whitespaces = 0;

  int64_t length = utext_nativeLength(utext);

  for (int64_t i = length - 1; i >= 0; --i)
  {
    UChar32 ch = utext_char32At(utext, i);
    if (!u_isUWhiteSpace(ch))
    {
      break;
    }
    whitespaces++;
    // When a supplementary character is encountered, adjust the index accordingly
    if (U_IS_SUPPLEMENTARY(ch))
    {
      // Prevents skipping an extra code unit since the loop will decrement i again
      if (i > 0)
          i--;
    }
  }

  return whitespaces;
}

static void copy_into_i16(i16_slice_t *src, fzf_i16_t *dest) {
  for (size_t i = 0; i < src->size; i++) {
    dest->data[i] = src->data[i];
  }
}

static UChar *utrim_whitespace_left(UChar *str, size_t *len) {
    for (size_t i = 0; i < *len; i++) {
        if (str[0] == 0x0020) { // Check for space character in UTF-16
            (*len)--;
            str++;
        } else {
            break;
        }
    }
    return str;
}

static bool uhas_prefix(const UChar* str, const UChar *prefix, int32_t prefix_len)
{
  return u_strncmp(prefix, str, prefix_len) == 0;
}

static bool uhas_suffix(const UChar *str, size_t len, const UChar *suffix, size_t suffix_len) {
    if (len < suffix_len) {
        return false;
    }

    const UChar *str_suffix_start = str + (len - suffix_len);
    return u_memcmp(str_suffix_start, suffix, (int32_t)suffix_len) == 0;
}

static UChar* ustr_replace_char(UChar* str, UChar find, UChar replace)
{
    UChar* current_pos = str;
    while (*current_pos != 0)
    {
        if (*current_pos == find)
        {
            *current_pos = replace;
        }
        ++current_pos;
    }
    return str;
}

static UChar *ustr_replace(const UChar *orig, const UChar *rep, const UChar *with) {
    if (!orig || !rep || !with) {
        return NULL;
    }

    int32_t len_rep = u_strlen(rep);
    int32_t len_with = u_strlen(with);
    int32_t len_orig = u_strlen(orig);

    // Temporary variables for the replacement process
    const UChar *ins = orig; // Current position in the original string
    const UChar *tmp;
    int32_t count = 0; // Number of replacements

    // First pass: count the number of replacements needed
    while ((tmp = u_strstr(ins, rep)) != NULL) {
        count++;
        ins = tmp + len_rep;
    }

    // Allocate memory for the new string
    int32_t new_len = len_orig + (len_with - len_rep) * count;
    UChar* result = (UChar*)malloc((new_len + 1) * sizeof(UChar));
    if (!result) {
        return NULL;
    }

    UChar *current_pos = result;
    while (count--) {
        tmp = u_strstr(orig, rep); // Find next occurrence of rep
        int32_t len_front = (int32_t)(tmp - orig); // Length before the rep
        u_strncpy(current_pos, orig, len_front); // Copy the part before rep
        current_pos += len_front;
        u_strcpy(current_pos, with); // Replace rep with with
        current_pos += len_with;
        orig += len_front + len_rep; // Move past the replaced part
    }

    // Copy the remaining part of the original string
    u_strcpy(current_pos, orig);

    return result;
}

static int16_t max16(int16_t a, int16_t b) {
  return (a > b) ? a : b;
}

static size_t min64u(size_t a, size_t b) {
  return (a < b) ? a : b;
}

fzf_position_t *fzf_pos_array(size_t len) {
  fzf_position_t *pos = (fzf_position_t *)malloc(sizeof(fzf_position_t));
  pos->size = 0;
  pos->cap = len;
  if (len > 0) {
    pos->data = (uint32_t *)malloc(len * sizeof(uint32_t));
  } else {
    pos->data = NULL;
  }
  return pos;
}

static void resize_pos(fzf_position_t *pos, size_t add_len, size_t comp) {
  if (!pos) {
    return;
  }
  if (pos->size + comp > pos->cap) {
    pos->cap += add_len > 0 ? add_len : 1;
    pos->data = (uint32_t *)realloc(pos->data, sizeof(uint32_t) * pos->cap);
  }
}

static void unsafe_append_pos(fzf_position_t *pos, size_t value) {
  resize_pos(pos, pos->cap, 1);
  pos->data[pos->size] = (int32_t)value;
  pos->size++;
}

static void append_pos(fzf_position_t *pos, size_t value) {
  if (pos) {
    unsafe_append_pos(pos, value);
  }
}

static void insert_range(fzf_position_t *pos, size_t start, size_t end) {
  if (!pos) {
    return;
  }

  int32_t diff = ((int32_t)end - (int32_t)start);
  if (diff <= 0) {
    return;
  }

  resize_pos(pos, end - start, end - start);
  for (size_t k = start; k < end; k++) {
    pos->data[pos->size] = (uint32_t)k;
    pos->size++;
  }
}

static fzf_i16_t alloc16(size_t *offset, fzf_slab_t *slab, size_t size) {
  if (slab != NULL && slab->I16.cap > *offset + size) {
    i16_slice_t slice = slice_i16(slab->I16.data, *offset, (*offset) + size);
    *offset = *offset + size;
    return (fzf_i16_t){.data = slice.data,
                       .size = slice.size,
                       .cap = slice.size,
                       .allocated = false};
  }
  int16_t *data = (int16_t *)malloc(size * sizeof(int16_t));
  memset(data, 0, size * sizeof(int16_t));
  return (fzf_i16_t){
      .data = data, .size = size, .cap = size, .allocated = true};
}

static fzf_i32_t alloc32(size_t *offset, fzf_slab_t *slab, size_t size) {
  if (slab != NULL && slab->I32.cap > *offset + size) {
    i32_slice_t slice = slice_i32(slab->I32.data, *offset, (*offset) + size);
    *offset = *offset + size;
    return (fzf_i32_t){.data = slice.data,
                       .size = slice.size,
                       .cap = slice.size,
                       .allocated = false};
  }
  int32_t *data = (int32_t *)malloc(size * sizeof(int32_t));
  memset(data, 0, size * sizeof(int32_t));
  return (fzf_i32_t){
      .data = data, .size = size, .cap = size, .allocated = true};
}

static char_class UText_class_of_ascii(UChar32 ch) {
    if (u_islower(ch)) {
        return UCharLower;
    }
    if (u_isupper(ch)) {
        return UCharUpper;
    }
    if (u_isdigit(ch)) {
        return UCharNumber;
    }
    return UCharNonWord;
}

// static char_class char_class_of_non_ascii(char ch) {
//   return 0;
// }

static char_class UText_class_of(UChar32 ch) {
    return UText_class_of_ascii(ch);
    // if (ch <= 0x7f) {
    //   return char_class_of_ascii(ch);
    // }
    // return char_class_of_non_ascii(ch);
}

static int16_t bonus_for(char_class prev_class, char_class class) {
  if (prev_class == UCharNonWord && class != UCharNonWord) {
    return BonusBoundary;
  }
  if ((prev_class == UCharLower && class == UCharUpper) ||
      (prev_class != UCharNumber && class == UCharNumber)) {
    return BonusCamel123;
  }
  if (class == UCharNonWord) {
    return BonusNonWord;
  }
  return 0;
}

static int16_t ubonus_at(ufzf_string_t *input, size_t idx) {
    if (idx == 0) {
        return BonusBoundary;
    }
    return bonus_for(UText_class_of(utext_char32At(input->data, idx - 1)),
                     UText_class_of(utext_char32At(input->data, idx)));
}

/* TODO(conni2461): maybe just not do this */
static UChar32 unormalize_rune(UChar32 r) {
    // TODO(conni2461)
    /* if (r < 0x00C0 || r > 0x2184) { */
    /*   return r; */
    /* } */
    /* rune n = normalized[r]; */
    /* if n > 0 { */
    /*   return n; */
    /* } */
    return r;
}

static int32_t utry_skip(ufzf_string_t *input, bool case_sensitive, UChar32 b,
                        int32_t from) {
    UTextSlice slice;
    u_text_slice_init(&slice, input->data, from, (int32_t)input->size);
    int32_t idx = uindex_byte(&slice, b);
    if (idx == 0) {
        return from;
    }

    if (!case_sensitive) {
        if (idx > 0) {
            UTextSlice tmp;
            u_text_slice_right(&tmp, &slice, idx);
            slice.start = tmp.start;
            slice.end = tmp.end;
            slice.size = tmp.size;
        }
        UChar32 upperCaseB = u_toupper(b);
        int32_t uidx = uindex_byte(&slice, upperCaseB);
        if (uidx >= 0) {
            idx = uidx;
        }
    }
    if (idx < 0) {
        return -1;
    }

    return from + idx;
}

static bool uis_ascii(const UText *runes, size_t size) {
    // TODO(conni2461): future use
    /* for (size_t i = 0; i < size; i++) { */
    /*   if (runes[i] >= 256) { */
    /*     return false; */
    /*   } */
    /* } */
    return true;
}

static int32_t uascii_fuzzy_index(ufzf_string_t *input, UText *pattern,
                                 size_t size, bool case_sensitive) {
    if (!uis_ascii(pattern, size)) {
        return -1;
    }

    int32_t first_idx = 0;
    int32_t idx = 0;
    for (size_t pidx = 0; pidx < size; pidx++) {
        idx = utry_skip(input, case_sensitive, utext_char32At(pattern, pidx), idx);
        if (idx < 0) {
            return -1;
        }
        if (pidx == 0 && idx > 0) {
            first_idx = idx - 1;
        }
        idx++;
    }

    return first_idx;
}

static int32_t ucalculate_score(bool case_sensitive, bool normalize,
                               ufzf_string_t *text, ufzf_string_t *pattern,
                               size_t sidx, size_t eidx, fzf_position_t *pos) {
    const size_t M = pattern->size;

    size_t pidx = 0;
    int32_t score = 0;
    int32_t consecutive = 0;
    bool in_gap = false;
    int16_t first_bonus = 0;

    resize_pos(pos, M, M);
    int32_t prev_class = UCharNonWord;
    if (sidx > 0) {
        prev_class = UText_class_of(utext_char32At(text->data, sidx - 1));
    }
    for (size_t idx = sidx; idx < eidx; idx++) {
        UChar32 c = utext_char32At(text->data, idx);
        int32_t class = UText_class_of(c);
        if (!case_sensitive) {
            /* TODO(conni2461): He does some unicode stuff here, investigate */
            c = u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }
        if (c == utext_char32At(pattern->data, pidx)) {
            append_pos(pos, idx);
            score += ScoreMatch;
            int16_t bonus = bonus_for(prev_class, class);
            if (consecutive == 0) {
                first_bonus = bonus;
            } else {
                if (bonus == BonusBoundary) {
                    first_bonus = bonus;
                }
                bonus = max16(max16(bonus, first_bonus), BonusConsecutive);
            }
            if (pidx == 0) {
                score += (int32_t)(bonus * BonusFirstCharMultiplier);
            } else {
                score += (int32_t)bonus;
            }
            in_gap = false;
            consecutive++;
            pidx++;
        } else {
            if (in_gap) {
                score += ScoreGapExtention;
            } else {
                score += ScoreGapStart;
            }
            in_gap = true;
            consecutive = 0;
            first_bonus = 0;
        }
        prev_class = class;
    }
    return score;
}

fzf_result_t ufzf_fuzzy_match_v1(bool case_sensitive, bool normalize, ufzf_string_t* text, ufzf_string_t* pattern, fzf_position_t* pos, fzf_slab_t* slab)
{
  const size_t M = pattern->size;
  const size_t N = text->size;
  if (M == 0)
  {
    return (fzf_result_t){ 0, 0, 0 };
  }
  if (uascii_fuzzy_index(text, pattern->data, M, case_sensitive) < 0)
  {
    return (fzf_result_t){ -1, -1, 0 };
  }

  int32_t pidx = 0;
  int32_t sidx = -1;
  int32_t eidx = -1;
  for (size_t idx = 0; idx < N; idx++)
  {
    UChar32 c = utext_char32At(text->data, idx);
    /* TODO(conni2461): Common pattern maybe a macro would be good here */
    if (!case_sensitive)
    {
      /* TODO(conni2461): He does some unicode stuff here, investigate */
      c = u_tolower(c);
    }
    if (normalize)
    {
      c = unormalize_rune(c);
    }
    if (c == utext_char32At(pattern->data, pidx))
    {
      if (sidx < 0)
      {
        sidx = (int32_t)idx;
      }
      pidx++;
      if (pidx == M)
      {
        eidx = (int32_t)idx + 1;
        break;
      }
    }
  }
  if (sidx >= 0 && eidx >= 0)
  {
    size_t start = (size_t)sidx;
    size_t end = (size_t)eidx;
    pidx--;
    for (size_t idx = end - 1; idx >= start; idx--)
    {
      UChar32 c = utext_char32At(text->data, idx);
      if (!case_sensitive)
      {
        /* TODO(conni2461): He does some unicode stuff here, investigate */
        c = u_tolower(c);
      }
      if (c == utext_char32At(pattern->data, pidx))
      {
        pidx--;
        if (pidx < 0)
        {
          start = idx;
          break;
        }
      }
    }

    int32_t score = ucalculate_score(case_sensitive, normalize, text, pattern, start, end, pos);
    return (fzf_result_t){ (int32_t)start, (int32_t)end, score };
  }
  return (fzf_result_t){ -1, -1, 0 };
}

fzf_result_t ufzf_fuzzy_match_v2(bool case_sensitive, bool normalize,
                                ufzf_string_t *text, ufzf_string_t *pattern,
                                fzf_position_t *pos, fzf_slab_t *slab) {
    const size_t M = pattern->size;
    const size_t N = text->size;
    if (M == 0) {
        return (fzf_result_t){0, 0, 0};
    }
    if (slab != NULL && N * M > slab->I16.cap) {
        return ufzf_fuzzy_match_v1(case_sensitive, normalize, text, pattern, pos,
                                  slab);
    }

    size_t idx;
    {
        int32_t tmp_idx = uascii_fuzzy_index(text, pattern->data, M, case_sensitive);
        if (tmp_idx < 0) {
            return (fzf_result_t){-1, -1, 0};
        }
        idx = (size_t)tmp_idx;
    }

    size_t offset16 = 0;
    size_t offset32 = 0;

    fzf_i16_t h0 = alloc16(&offset16, slab, N);
    fzf_i16_t c0 = alloc16(&offset16, slab, N);
    // Bonus point for each positions
    fzf_i16_t bo = alloc16(&offset16, slab, N);
    // The first occurrence of each character in the pattern
    fzf_i32_t f = alloc32(&offset32, slab, M);

    // Phase 2. Calculate bonus for each point
    int16_t max_score = 0;
    size_t max_score_pos = 0;

    size_t pidx = 0;
    size_t last_idx = 0;

    UChar32 pchar0 = utext_char32At(pattern->data, 0);
    UChar32 pchar = utext_char32At(pattern->data, 0);
    int16_t prev_h0 = 0;
    int32_t prev_class = UCharNonWord;
    bool in_gap = false;

    UTextSlice t_sub;
    u_text_slice_init(&t_sub, text->data, (int32_t)idx, (int32_t)text->size);
    i16_slice_t h0_sub =
            slice_i16_right(slice_i16(h0.data, idx, h0.size).data, t_sub.size);
    i16_slice_t c0_sub =
            slice_i16_right(slice_i16(c0.data, idx, c0.size).data, t_sub.size);
    i16_slice_t b_sub =
            slice_i16_right(slice_i16(bo.data, idx, bo.size).data, t_sub.size);

    for (size_t off = 0; off < t_sub.size; off++) {
        char_class class;
        UChar32 c = utextSlice_charAt(&t_sub, off);
        class = UText_class_of_ascii(c);
        if (!case_sensitive && class == UCharUpper) {
            /* TODO(conni2461): unicode support */
            c = u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }

        int16_t bonus = bonus_for(prev_class, class);
        b_sub.data[off] = bonus;
        prev_class = class;
        if (c == pchar) {
            if (pidx < M) {
                f.data[pidx] = (int32_t)(idx + off);
                pidx++;
                pchar = utext_char32At(pattern->data, min64u(pidx, M - 1));
            }
            last_idx = idx + off;
        }

        if (c == pchar0) {
            int16_t score = ScoreMatch + bonus * BonusFirstCharMultiplier;
            h0_sub.data[off] = score;
            c0_sub.data[off] = 1;
            if (M == 1 && (score > max_score)) {
                max_score = score;
                max_score_pos = idx + off;
                if (bonus == BonusBoundary) {
                    break;
                }
            }
            in_gap = false;
        } else {
            if (in_gap) {
                h0_sub.data[off] = max16(prev_h0 + ScoreGapExtention, 0);
            } else {
                h0_sub.data[off] = max16(prev_h0 + ScoreGapStart, 0);
            }
            c0_sub.data[off] = 0;
            in_gap = true;
        }
        prev_h0 = h0_sub.data[off];
    }
    if (pidx != M) {
        free_alloc(f);
        free_alloc(bo);
        free_alloc(c0);
        free_alloc(h0);
        return (fzf_result_t){-1, -1, 0};
    }
    if (M == 1) {
        free_alloc(f);
        free_alloc(bo);
        free_alloc(c0);
        free_alloc(h0);
        fzf_result_t res = {(int32_t)max_score_pos, (int32_t)max_score_pos + 1,
                            max_score};
        append_pos(pos, max_score_pos);
        return res;
    }

    size_t f0 = (size_t)f.data[0];
    size_t width = last_idx - f0 + 1;
    fzf_i16_t h = alloc16(&offset16, slab, width * M);
    {
        i16_slice_t h0_tmp_slice = slice_i16(h0.data, f0, last_idx + 1);
        copy_into_i16(&h0_tmp_slice, &h);
    }

    fzf_i16_t c = alloc16(&offset16, slab, width * M);
    {
        i16_slice_t c0_tmp_slice = slice_i16(c0.data, f0, last_idx + 1);
        copy_into_i16(&c0_tmp_slice, &c);
    }

    i32_slice_t f_sub = slice_i32(f.data, 1, f.size);
    UTextSlice p_subTmp;
    UTextSlice p_sub;
    u_text_slice_init(&p_subTmp, pattern->data, 1, (int32_t)M);
    u_text_slice_right(&p_sub, &p_subTmp, (int32_t)f_sub.size);
    for (size_t off = 0; off < f_sub.size; off++) {
        size_t f = (size_t)f_sub.data[off];
        pchar = utextSlice_charAt(&p_sub, off);
        pidx = off + 1;
        size_t row = pidx * width;
        in_gap = false;
        UTextSlice t_sub2;
        u_text_slice_init(&t_sub2, text->data, (int32_t)f, (int32_t)last_idx + 1);
        t_sub = t_sub2;
        b_sub = slice_i16_right(slice_i16(bo.data, (int32_t)f, bo.size).data, (int32_t)t_sub.size);
        i16_slice_t c_sub = slice_i16_right(
                slice_i16(c.data, row + f - f0, c.size).data, t_sub.size);
        i16_slice_t c_diag = slice_i16_right(
                slice_i16(c.data, row + f - f0 - 1 - width, c.size).data, t_sub.size);
        i16_slice_t h_sub = slice_i16_right(
                slice_i16(h.data, row + f - f0, h.size).data, t_sub.size);
        i16_slice_t h_diag = slice_i16_right(
                slice_i16(h.data, row + f - f0 - 1 - width, h.size).data, t_sub.size);
        i16_slice_t h_left = slice_i16_right(
                slice_i16(h.data, row + f - f0 - 1, h.size).data, t_sub.size);
        h_left.data[0] = 0;
        for (size_t j = 0; j < t_sub.size; j++) {
            UChar32 ch = utextSlice_charAt(&t_sub, j);

            char_class class = UText_class_of_ascii(ch);
            if (!case_sensitive && class == UCharUpper)
            {
                /* TODO(conni2461): unicode support */
                ch = u_tolower(ch);
            }

            size_t col = j + f;
            int16_t s1 = 0;
            int16_t s2 = 0;
            int16_t consecutive = 0;

            if (in_gap) {
                s2 = h_left.data[j] + ScoreGapExtention;
            } else {
                s2 = h_left.data[j] + ScoreGapStart;
            }

            if (pchar == ch) {
                s1 = h_diag.data[j] + ScoreMatch;
                int16_t b = b_sub.data[j];
                consecutive = c_diag.data[j] + 1;
                if (b == BonusBoundary) {
                    consecutive = 1;
                } else if (consecutive > 1) {
                    b = max16(b, max16(BonusConsecutive,
                                       bo.data[col - ((size_t)consecutive) + 1]));
                }
                if (s1 + b < s2) {
                    s1 += b_sub.data[j];
                    consecutive = 0;
                } else {
                    s1 += b;
                }
            }
            c_sub.data[j] = consecutive;
            in_gap = s1 < s2;
            int16_t score = max16(max16(s1, s2), 0);
            if (pidx == M - 1 && (score > max_score)) {
                max_score = score;
                max_score_pos = col;
            }
            h_sub.data[j] = score;
        }
    }

    resize_pos(pos, M, M);
    size_t j = max_score_pos;
    if (pos) {
        size_t i = M - 1;
        bool prefer_match = true;
        for (;;) {
            size_t ii = i * width;
            size_t j0 = j - f0;
            int16_t s = h.data[ii + j0];

            int16_t s1 = 0;
            int16_t s2 = 0;
            if (i > 0 && j >= f.data[i]) {
                s1 = h.data[ii - width + j0 - 1];
            }
            if (j > f.data[i]) {
                s2 = h.data[ii + j0 - 1];
            }

            if (s > s1 && (s > s2 || (s == s2 && prefer_match))) {
                unsafe_append_pos(pos, j);
                if (i == 0) {
                    break;
                }
                i--;
            }
            prefer_match = c.data[ii + j0] > 1 || (ii + width + j0 + 1 < c.size &&
                                                   c.data[ii + width + j0 + 1] > 0);
            j--;
        }
    }

    free_alloc(h);
    free_alloc(c);
    free_alloc(f);
    free_alloc(bo);
    free_alloc(c0);
    free_alloc(h0);
    return (fzf_result_t){(int32_t)j, (int32_t)max_score_pos + 1,
                          (int32_t)max_score};
}

fzf_result_t ufzf_exact_match_naive(bool case_sensitive, bool normalize,
                                   ufzf_string_t *text, ufzf_string_t *pattern,
                                   fzf_position_t *pos, fzf_slab_t *slab) {
    const size_t M = pattern->size;
    const size_t N = text->size;

    if (M == 0) {
        return (fzf_result_t){0, 0, 0};
    }
    if (N < M) {
        return (fzf_result_t){-1, -1, 0};
    }
    if (uascii_fuzzy_index(text, pattern->data, M, case_sensitive) < 0) {
        return (fzf_result_t){-1, -1, 0};
    }

    size_t pidx = 0;
    int32_t best_pos = -1;
    int16_t bonus = 0;
    int16_t best_bonus = -1;
    for (size_t idx = 0; idx < N; idx++) {
        UChar32 c = utext_char32At(text->data, idx);
        if (!case_sensitive) {
            /* TODO(conni2461): He does some unicode stuff here, investigate */
            c = u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }
        if (c == utext_char32At(pattern->data, pidx))
        {
            if (pidx == 0) {
                bonus = ubonus_at(text, idx);
            }
            pidx++;
            if (pidx == M) {
                if (bonus > best_bonus) {
                    best_pos = (int32_t)idx;
                    best_bonus = bonus;
                }
                if (bonus == BonusBoundary) {
                    break;
                }
                idx -= pidx - 1;
                pidx = 0;
                bonus = 0;
            }
        } else {
            idx -= pidx;
            pidx = 0;
            bonus = 0;
        }
    }
    if (best_pos >= 0) {
        size_t bp = (size_t)best_pos;
        size_t sidx = bp - M + 1;
        size_t eidx = bp + 1;
        int32_t score = ucalculate_score(case_sensitive, normalize, text, pattern,
                                        sidx, eidx, NULL);
        insert_range(pos, sidx, eidx);
        return (fzf_result_t){(int32_t)sidx, (int32_t)eidx, score};
    }
    return (fzf_result_t){-1, -1, 0};
}

fzf_result_t ufzf_prefix_match(bool case_sensitive, bool normalize,
                              ufzf_string_t *text, ufzf_string_t *pattern,
                              fzf_position_t *pos, fzf_slab_t *slab) {
    const size_t M = pattern->size;
    if (M == 0) {
        return (fzf_result_t){0, 0, 0};
    }
    size_t trimmed_len = 0;
    /* TODO(conni2461): i feel this is wrong */
    if (!u_isWhitespace(utext_char32At(pattern->data, 0))) {
        trimmed_len = uleading_whitespaces(text->data);
    }
    if (text->size - trimmed_len < M) {
        return (fzf_result_t){-1, -1, 0};
    }
    for (size_t i = 0; i < M; i++) {
        UChar32 c = utext_char32At(text->data, trimmed_len + i);
        if (!case_sensitive) {
            c = u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }
        if (c != utext_char32At(pattern->data, i)) {
            return (fzf_result_t){-1, -1, 0};
        }
    }
    size_t start = trimmed_len;
    size_t end = trimmed_len + M;
    int32_t score = ucalculate_score(case_sensitive, normalize, text, pattern,
                                    start, end, NULL);
    insert_range(pos, start, end);
    return (fzf_result_t){(int32_t)start, (int32_t)end, score};
}

fzf_result_t ufzf_suffix_match(bool case_sensitive, bool normalize,
                              ufzf_string_t *text, ufzf_string_t *pattern,
                              fzf_position_t *pos, fzf_slab_t *slab) {
    size_t trimmed_len = text->size;
    const size_t M = pattern->size;
    /* TODO(conni2461): i think this is wrong */
    if (M == 0 || !u_isWhitespace(utext_char32At(pattern->data, M - 1))) {
        trimmed_len -= utrailing_whitespaces(text->data);
    }
    if (M == 0) {
        return (fzf_result_t){(int32_t)trimmed_len, (int32_t)trimmed_len, 0};
    }
    size_t diff = trimmed_len - M;
    if (diff < 0) {
        return (fzf_result_t){-1, -1, 0};
    }

    for (size_t idx = 0; idx < M; idx++) {
        UChar32 c = utext_char32At(text->data, idx + diff);
        if (!case_sensitive) {
            c = u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }
        if (c != utext_char32At(pattern->data, idx)) {
            return (fzf_result_t){-1, -1, 0};
        }
    }
    size_t start = trimmed_len - M;
    size_t end = trimmed_len;
    int32_t score = ucalculate_score(case_sensitive, normalize, text, pattern,
                                    start, end, NULL);
    insert_range(pos, start, end);
    return (fzf_result_t){(int32_t)start, (int32_t)end, score};
}

fzf_result_t ufzf_equal_match(bool case_sensitive, bool normalize,
                             ufzf_string_t *text, ufzf_string_t *pattern,
                             fzf_position_t *pos, fzf_slab_t *slab) {
    const size_t M = pattern->size;
    if (M == 0) {
        return (fzf_result_t){-1, -1, 0};
    }

    size_t trimmed_len = uleading_whitespaces(text->data);
    size_t trimmed_end_len = utrailing_whitespaces(text->data);

    if ((text->size - trimmed_len - trimmed_end_len) != M) {
        return (fzf_result_t){-1, -1, 0};
    }

    bool match = true;
    if (normalize) {
        // TODO(conni2461): to rune
        for (size_t idx = 0; idx < M; idx++) {
            UChar32 pchar = utext_char32At(pattern->data, idx);
            UChar32 c = utext_char32At(text->data, trimmed_len + idx);
            if (!case_sensitive) {
                c = u_tolower(c);
            }
            if (unormalize_rune(c) != unormalize_rune(pchar)) {
                match = false;
                break;
            }
        }
    } else {
        // TODO(conni2461): to rune
        for (size_t idx = 0; idx < M; idx++) {
            UChar32 pchar = utext_char32At(pattern->data, idx);
            UChar32 c = utext_char32At(text->data, trimmed_len + idx);
            if (!case_sensitive) {
                c = u_tolower(c);
            }
            if (c != pchar) {
                match = false;
                break;
            }
        }
    }
    if (match) {
        insert_range(pos, trimmed_len, trimmed_len + M);
        return (fzf_result_t){(int32_t)trimmed_len,
                              ((int32_t)trimmed_len + (int32_t)M),
                              (ScoreMatch + BonusBoundary) * (int32_t)M +
                              (BonusFirstCharMultiplier - 1) * BonusBoundary};
    }
    return (fzf_result_t){-1, -1, 0};
}

static void uappend_set(ufzf_term_set_t *set, ufzf_term_t value) {
    if (set->cap == 0) {
        set->cap = 1;
        set->ptr = (ufzf_term_t *)malloc(sizeof(ufzf_term_t));
    } else if (set->size + 1 > set->cap) {
        set->cap *= 2;
        set->ptr = realloc(set->ptr, sizeof(ufzf_term_t) * set->cap);
    }
    set->ptr[set->size] = value;
    set->size++;
}

static void uappend_pattern(ufzf_pattern_t *pattern, ufzf_term_set_t *value) {
    if (pattern->cap == 0) {
        pattern->cap = 1;
        pattern->ptr = (ufzf_term_set_t **)malloc(sizeof(ufzf_term_set_t *));
    } else if (pattern->size + 1 > pattern->cap) {
        pattern->cap *= 2;
        pattern->ptr =
                realloc(pattern->ptr, sizeof(ufzf_term_set_t *) * pattern->cap);
    }
    pattern->ptr[pattern->size] = value;
    pattern->size++;
}

#define UCALL_ALG(term, normalize, input, pos, slab)                           \
  term->fn((term)->case_sensitive, normalize, &(input),                        \
           (ufzf_string_t *)(term)->text, pos, slab)

UChar* ustrtok_r(UChar* src, const UChar delim, UChar** context, size_t length)
{
    if (src == NULL && (src = *context) == NULL)
    {
        return NULL; // No more tokens
    }

    UChar* end = src;
    UChar* src_end = src + length; // Calculate the end of the src based on the length

    while (end < src_end && *end && *end != delim)
    {
        ++end;
    }

    if (end >= src_end || *end == 0)
    { // End of string or reaching the length limit
        *context = NULL;
    }
    else
    {
        *end = 0; // Replace delim with null terminator to end token
        *context = end + 1;
    }

    return src;
}

UChar* u_strdup(const UChar* src) {
    if (src == NULL) {
        return NULL;
    }

    int32_t len = u_strlen(src);
    UChar* dup = malloc((len + 1) * sizeof(UChar));
    if (dup == NULL) {
        return NULL;
    }

    u_memcpy(dup, src, len + 1);

    return dup;
}

ufzf_pattern_t *ufzf_parse_pattern(fzf_case_types case_mode, bool normalize,
                                 const UChar *pattern, bool fuzzy) {
    ufzf_pattern_t *pat_obj = (ufzf_pattern_t *)malloc(sizeof(ufzf_pattern_t));
    memset(pat_obj, 0, sizeof(*pat_obj));

    size_t pat_len = u_strlen(pattern);
    if (pat_len == 0) {
        return pat_obj;
    }

    UChar* patternDup = u_strdup(pattern);

    patternDup = utrim_whitespace_left(patternDup, &pat_len);

    UChar spaceChar = 0x0020;
    UChar space_suffix[] = { 0x0020, 0x0000 };
    UChar escaped_space_suffix[] = { 0x005C, 0x0020, 0 };
    UChar tabChar = 0x0009;
    UChar tab[] = { tabChar, 0x0000 };
    while (uhas_suffix(patternDup, pat_len, space_suffix, 1) &&
           !uhas_suffix(patternDup, pat_len, escaped_space_suffix, 2)) {
        patternDup[pat_len - 1] = 0;
        pat_len--;
    }

    UChar* pattern_copy = ustr_replace(patternDup, escaped_space_suffix, tab);
    UChar* context = NULL;
    UChar udelim = 0x0020;
    UChar* ptr = ustrtok_r(pattern_copy, udelim, &context, pat_len);

    ufzf_term_set_t *set = (ufzf_term_set_t *)malloc(sizeof(ufzf_term_set_t));
    memset(set, 0, sizeof(*set));

    bool switch_set = false;
    bool after_bar = false;
    while (ptr != NULL) {
        ufzf_algo_t fn = ufzf_fuzzy_match_v2;
        bool inv = false;

        size_t len = u_strlen(ptr);
        ustr_replace_char(ptr, tabChar, spaceChar);
        UChar* text = u_strdup(ptr);

        UChar* og_str = text;
        int32_t bufferLength = (int32_t)len + 1;
        UChar* lower_text = malloc(bufferLength * sizeof(UChar));
        UErrorCode status = U_ZERO_ERROR;
        u_strToLower(lower_text, bufferLength, text, (int32_t)len, "", &status);
        bool case_sensitive =
                case_mode == CaseRespect ||
                (case_mode == CaseSmart && u_strcmp(text, lower_text) != 0);
        if (!case_sensitive) {
            SFREE(text);
            text = lower_text;
            og_str = lower_text;
        } else {
            SFREE(lower_text);
        }
        if (!fuzzy) {
            fn = ufzf_exact_match_naive;
        }
        UChar pipeChar[] = { 0x007C, 0x0000 };
        if (set->size > 0 && !after_bar && u_strcmp(text, pipeChar) == 0) {
            switch_set = false;
            after_bar = true;
            ptr = ustrtok_r(NULL, udelim, &context, pat_len);
            SFREE(og_str);
            continue;
        }
        after_bar = false;
        UChar exclamationPrefix[] = { 0x0021, 0 };
        if (uhas_prefix(text, exclamationPrefix, 1)) {
            inv = true;
            fn = ufzf_exact_match_naive;
            text++;
            len--;
        }

        UChar dollarSignChar[] = { 0x0024, 0 };
        if (u_strcmp(text, dollarSignChar) != 0 && uhas_suffix(text, len, dollarSignChar, 1)) {
            fn = ufzf_suffix_match;
            text[len - 1] = 0;
            len--;
        }

        UChar singleQuoteChar[] = { 0x0027, 0 };
        UChar caretChar[] = { 0x005E, 0 };
        if (uhas_prefix(text, singleQuoteChar, 1)) {
            if (fuzzy && !inv) {
                fn = ufzf_exact_match_naive;
                text++;
                len--;
            } else {
                fn = ufzf_fuzzy_match_v2;
                text++;
                len--;
            }
        } else if (uhas_prefix(text, caretChar, 1)) {
            if (fn == ufzf_suffix_match) {
                fn = ufzf_equal_match;
            } else {
                fn = ufzf_prefix_match;
            }
            text++;
            len--;
        }

        if (len > 0) {
            if (switch_set) {
                uappend_pattern(pat_obj, set);
                set = (ufzf_term_set_t *)malloc(sizeof(ufzf_term_set_t));
                set->cap = 0;
                set->size = 0;
            }
            UErrorCode status = U_ZERO_ERROR;
            UText* utextPattern = utext_openUChars(NULL, og_str, u_strlen(og_str), &status);
            UText* utextText = utext_openUChars(NULL, text, u_strlen(text), &status);
            ufzf_string_t* text_ptr = (ufzf_string_t*)malloc(sizeof(ufzf_string_t));
            text_ptr->data = utextText;
            text_ptr->size = len;
            uappend_set(set, (ufzf_term_t){.fn = fn,
                    .inv = inv,
                    .ptr = utextPattern,
                    .text = text_ptr,
                    .case_sensitive = case_sensitive});
            switch_set = true;
        } else {
            SFREE(og_str);
        }

        ptr = ustrtok_r(NULL, udelim, &context, pat_len);
    }
    if (set->size > 0) {
        uappend_pattern(pat_obj, set);
    } else {
        SFREE(set->ptr);
        SFREE(set);
    }
    bool only = true;
    for (size_t i = 0; i < pat_obj->size; i++) {
        ufzf_term_set_t *term_set = pat_obj->ptr[i];
        if (term_set->size > 1) {
            only = false;
            break;
        }
        if (term_set->ptr[0].inv == false) {
            only = false;
            break;
        }
    }
    pat_obj->only_inv = only;
    SFREE(pattern_copy);
    SFREE(patternDup);
    return pat_obj;
}

void ufzf_free_pattern(ufzf_pattern_t* pattern)
{
  if (pattern->ptr)
  {
    for (size_t i = 0; i < pattern->size; i++)
    {
      ufzf_term_set_t* term_set = pattern->ptr[i];
      for (size_t j = 0; j < term_set->size; j++)
      {
        ufzf_term_t* term = &term_set->ptr[j];
        utext_close(term->ptr);
        utext_close(term->text);
      }
      free(term_set->ptr);
      free(term_set);
    }
    free(pattern->ptr);
  }
  SFREE(pattern);
}

int32_t ufzf_get_score(UText *text, ufzf_pattern_t *pattern,
                      fzf_slab_t *slab) {
    // If the pattern is an empty string then pattern->ptr will be NULL and we
    // basically don't want to filter. Return 1 for telescope
    if (pattern->ptr == NULL) {
        return 1;
    }

    ufzf_string_t input = {.data = text, .size = utext_nativeLength(text)};
    if (pattern->only_inv) {
        int final = 0;
        for (size_t i = 0; i < pattern->size; i++) {
            ufzf_term_set_t *term_set = pattern->ptr[i];
            ufzf_term_t *term = &term_set->ptr[0];

            final += UCALL_ALG(term, false, input, NULL, slab).score;
        }
        //I changed this to 100 to work better with a min score
        return (final > 0) ? 0 : 1;
    }

    int32_t total_score = 0;
    for (size_t i = 0; i < pattern->size; i++) {
        ufzf_term_set_t *term_set = pattern->ptr[i];
        int32_t current_score = 0;
        bool matched = false;
        for (size_t j = 0; j < term_set->size; j++) {
            ufzf_term_t *term = &term_set->ptr[j];
            fzf_result_t res = UCALL_ALG(term, false, input, NULL, slab);
            if (res.start >= 0) {
                if (term->inv) {
                    continue;
                }
                current_score = res.score;
                matched = true;
                break;
            }

            if (term->inv) {
                current_score = 0;
                matched = true;
            }
        }
        if (matched) {
            total_score += current_score;
        } else {
            total_score = 0;
            break;
        }
    }

    return total_score;
}

fzf_position_t *ufzf_get_positions(UText *text, ufzf_pattern_t *pattern,
                                  fzf_slab_t *slab) {
    // If the pattern is an empty string then pattern->ptr will be NULL and we
    // basically don't want to filter. Return 1 for telescope
    if (pattern->ptr == NULL) {
        return NULL;
    }

    ufzf_string_t input = {.data = text, .size = utext_nativeLength(text)};
    fzf_position_t *all_pos = fzf_pos_array(0);
    for (size_t i = 0; i < pattern->size; i++) {
        ufzf_term_set_t *term_set = pattern->ptr[i];
        bool matched = false;
        for (size_t j = 0; j < term_set->size; j++) {
            ufzf_term_t *term = &term_set->ptr[j];
            if (term->inv) {
                // If we have an inverse term we need to check if we have a match, but
                // we are not interested in the positions (for highlights) so to speed
                // this up we can pass in NULL here and don't calculate the positions
                fzf_result_t res = UCALL_ALG(term, false, input, NULL, slab);
                if (res.start < 0) {
                    matched = true;
                }
                continue;
            }
            fzf_result_t res = UCALL_ALG(term, false, input, all_pos, slab);
            if (res.start >= 0) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            fzf_free_positions(all_pos);
            return NULL;
        }
    }
    return all_pos;
}

void fzf_free_positions(fzf_position_t *pos) {
  if (pos) {
    SFREE(pos->data);
    free(pos);
  }
}

fzf_slab_t *fzf_make_slab(fzf_slab_config_t config) {
  fzf_slab_t *slab = (fzf_slab_t *)malloc(sizeof(fzf_slab_t));
  memset(slab, 0, sizeof(*slab));

  slab->I16.data = (int16_t *)malloc(config.size_16 * sizeof(int16_t));
  memset(slab->I16.data, 0, config.size_16 * sizeof(*slab->I16.data));
  slab->I16.cap = config.size_16;
  slab->I16.size = 0;
  slab->I16.allocated = true;

  slab->I32.data = (int32_t *)malloc(config.size_32 * sizeof(int32_t));
  memset(slab->I32.data, 0, config.size_32 * sizeof(*slab->I32.data));
  slab->I32.cap = config.size_32;
  slab->I32.size = 0;
  slab->I32.allocated = true;

  return slab;
}

fzf_slab_t *fzf_make_default_slab(void) {
  return fzf_make_slab((fzf_slab_config_t){(size_t)100 * 1024, 2048});
}

void fzf_free_slab(fzf_slab_t *slab) {
  if (slab) {
    free(slab->I16.data);
    free(slab->I32.data);
    free(slab);
  }
}
#pragma warning(pop)
