/*
 * MD4C: Markdown parser for C
 * (http://github.com/mity/md4c)
 *
 * Copyright (c) 2016-2024 Martin Mitáš
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "md4c.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*****************************
 ***  Miscellaneous Stuff  ***
 *****************************/

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199409L
    /* C89/90 or old compilers in general may not understand "inline". */
    #if defined __GNUC__
        #define inline __inline__
    #elif defined _MSC_VER
        #define inline __inline
    #else
        #define inline
    #endif
#endif

/* Make the UTF-8 support the default. */
#if !defined MD4C_USE_ASCII && !defined MD4C_USE_UTF8 && !defined MD4C_USE_UTF16
    #define MD4C_USE_UTF8
#endif

/* Magic for making wide literals with MD4C_USE_UTF16. */
#ifdef _T
    #undef _T
#endif
#if defined MD4C_USE_UTF16
    #define _T(x)           L##x
#else
    #define _T(x)           x
#endif

/* Misc. macros. */
#define SIZEOF_ARRAY(a)     (sizeof(a) / sizeof(a[0]))

#define STRINGIZE_(x)       #x
#define STRINGIZE(x)        STRINGIZE_(x)

#define MAX(a,b)            ((a) > (b) ? (a) : (b))
#define MIN(a,b)            ((a) < (b) ? (a) : (b))

#ifndef TRUE
    #define TRUE            1
    #define FALSE           0
#endif

#define MD_LOG(msg)                                                     \
    do {                                                                \
        if(ctx->parser.debug_log != NULL)                               \
            ctx->parser.debug_log((msg), ctx->userdata);                \
    } while(0)

#ifdef DEBUG
    #define MD_ASSERT(cond)                                             \
            do {                                                        \
                if(!(cond)) {                                           \
                    MD_LOG(__FILE__ ":" STRINGIZE(__LINE__) ": "        \
                           "Assertion '" STRINGIZE(cond) "' failed.");  \
                    exit(1);                                            \
                }                                                       \
            } while(0)

    #define MD_UNREACHABLE()        MD_ASSERT(1 == 0)
#else
    #ifdef __GNUC__
        #define MD_ASSERT(cond)     do { if(!(cond)) __builtin_unreachable(); } while(0)
        #define MD_UNREACHABLE()    do { __builtin_unreachable(); } while(0)
    #elif defined _MSC_VER  &&  _MSC_VER > 120
        #define MD_ASSERT(cond)     do { __assume(cond); } while(0)
        #define MD_UNREACHABLE()    do { __assume(0); } while(0)
    #else
        #define MD_ASSERT(cond)     do {} while(0)
        #define MD_UNREACHABLE()    do {} while(0)
    #endif
#endif

/* For falling through case labels in switch statements. */
#if defined __clang__ && __clang_major__ >= 12
    #define MD_FALLTHROUGH()        __attribute__((fallthrough))
#elif defined __GNUC__ && __GNUC__ >= 7
    #define MD_FALLTHROUGH()        __attribute__((fallthrough))
#else
    #define MD_FALLTHROUGH()        ((void)0)
#endif

/* Suppress "unused parameter" warnings. */
#define MD_UNUSED(x)                ((void)x)


/******************************
 ***  Some internal limits  ***
 ******************************/

/* We limit code span marks to lower than 32 backticks. This solves the
 * pathologic case of too many openers, each of different length: Their
 * resolving would be then O(n^2). */
#define CODESPAN_MARK_MAXLEN    32

/* We limit column count of tables to prevent quadratic explosion of output
 * from pathological input of a table thousands of columns and thousands
 * of rows where rows are requested with as little as single character
 * per-line, relying on us to "helpfully" fill all the missing "<td></td>". */
#define TABLE_MAXCOLCOUNT       128


/************************
 ***  Internal Types  ***
 ************************/

/* These are omnipresent so lets save some typing. */
#define CHAR    MD_CHAR
#define SZ      MD_SIZE
#define OFF     MD_OFFSET

#define SZ_MAX      (sizeof(SZ) == 8 ? UINT64_MAX : UINT32_MAX)
#define OFF_MAX     (sizeof(OFF) == 8 ? UINT64_MAX : UINT32_MAX)

typedef struct MD_MARK_tag MD_MARK;
typedef struct MD_BLOCK_tag MD_BLOCK;
typedef struct MD_CONTAINER_tag MD_CONTAINER;
typedef struct MD_REF_DEF_tag MD_REF_DEF;


/* During analyzes of inline marks, we need to manage stacks of unresolved
 * openers of the given type.
 * The stack connects the marks via MD_MARK::next;
 */
typedef struct MD_MARKSTACK_tag MD_MARKSTACK;
struct MD_MARKSTACK_tag {
    int top;        /* -1 if empty. */
};

/* Context propagated through all the parsing. */
typedef struct MD_CTX_tag MD_CTX;
struct MD_CTX_tag {
    /* Immutable stuff (parameters of md_parse()). */
    const CHAR* text;
    SZ size;
    MD_PARSER parser;
    void* userdata;

    /* When this is true, it allows some optimizations. */
    int doc_ends_with_newline;

    /* Helper temporary growing buffer. */
    CHAR* buffer;
    unsigned alloc_buffer;

    /* Reference definitions. */
    MD_REF_DEF* ref_defs;
    int n_ref_defs;
    int alloc_ref_defs;
    void** ref_def_hashtable;
    int ref_def_hashtable_size;
    SZ max_ref_def_output;

    /* Stack of inline/span markers.
     * This is only used for parsing a single block contents but by storing it
     * here we may reuse the stack for subsequent blocks; i.e. we have fewer
     * (re)allocations. */
    MD_MARK* marks;
    int n_marks;
    int alloc_marks;

#if defined MD4C_USE_UTF16
    char mark_char_map[128];
#else
    char mark_char_map[256];
#endif

    /* For resolving of inline spans. */
    MD_MARKSTACK opener_stacks[16];
#define ASTERISK_OPENERS_oo_mod3_0      (ctx->opener_stacks[0])     /* Opener-only */
#define ASTERISK_OPENERS_oo_mod3_1      (ctx->opener_stacks[1])
#define ASTERISK_OPENERS_oo_mod3_2      (ctx->opener_stacks[2])
#define ASTERISK_OPENERS_oc_mod3_0      (ctx->opener_stacks[3])     /* Both opener and closer candidate */
#define ASTERISK_OPENERS_oc_mod3_1      (ctx->opener_stacks[4])
#define ASTERISK_OPENERS_oc_mod3_2      (ctx->opener_stacks[5])
#define UNDERSCORE_OPENERS_oo_mod3_0    (ctx->opener_stacks[6])     /* Opener-only */
#define UNDERSCORE_OPENERS_oo_mod3_1    (ctx->opener_stacks[7])
#define UNDERSCORE_OPENERS_oo_mod3_2    (ctx->opener_stacks[8])
#define UNDERSCORE_OPENERS_oc_mod3_0    (ctx->opener_stacks[9])     /* Both opener and closer candidate */
#define UNDERSCORE_OPENERS_oc_mod3_1    (ctx->opener_stacks[10])
#define UNDERSCORE_OPENERS_oc_mod3_2    (ctx->opener_stacks[11])
#define TILDE_OPENERS_1                 (ctx->opener_stacks[12])
#define TILDE_OPENERS_2                 (ctx->opener_stacks[13])
#define BRACKET_OPENERS                 (ctx->opener_stacks[14])
#define DOLLAR_OPENERS                  (ctx->opener_stacks[15])

    /* Stack of dummies which need to call free() for pointers stored in them.
     * These are constructed during inline parsing and freed after all the block
     * is processed (i.e. all callbacks referring those strings are called). */
    MD_MARKSTACK ptr_stack;

    /* For resolving table rows. */
    int n_table_cell_boundaries;
    int table_cell_boundaries_head;
    int table_cell_boundaries_tail;

    /* For resolving links. */
    int unresolved_link_head;
    int unresolved_link_tail;

    /* For resolving raw HTML. */
    OFF html_comment_horizon;
    OFF html_proc_instr_horizon;
    OFF html_decl_horizon;
    OFF html_cdata_horizon;

    /* For block analysis.
     * Notes:
     *   -- It holds MD_BLOCK as well as MD_LINE structures. After each
     *      MD_BLOCK, its (multiple) MD_LINE(s) follow.
     *   -- For MD_BLOCK_HTML and MD_BLOCK_CODE, MD_VERBATIMLINE(s) are used
     *      instead of MD_LINE(s).
     */
    void* block_bytes;
    MD_BLOCK* current_block;
    int n_block_bytes;
    int alloc_block_bytes;

    /* For container block analysis. */
    MD_CONTAINER* containers;
    int n_containers;
    int alloc_containers;

    /* Minimal indentation to call the block "indented code block". */
    unsigned code_indent_offset;

    /* Contextual info for line analysis. */
    SZ code_fence_length;   /* For checking closing fence length. */
    int html_block_type;    /* For checking closing raw HTML condition. */
    int last_line_has_list_loosening_effect;
    int last_list_item_starts_with_two_blank_lines;
};

enum MD_LINETYPE_tag {
    MD_LINE_BLANK,
    MD_LINE_HR,
    MD_LINE_ATXHEADER,
    MD_LINE_SETEXTHEADER,
    MD_LINE_SETEXTUNDERLINE,
    MD_LINE_INDENTEDCODE,
    MD_LINE_FENCEDCODE,
    MD_LINE_HTML,
    MD_LINE_TEXT,
    MD_LINE_TABLE,
    MD_LINE_TABLEUNDERLINE
};
typedef enum MD_LINETYPE_tag MD_LINETYPE;

typedef struct MD_LINE_ANALYSIS_tag MD_LINE_ANALYSIS;
struct MD_LINE_ANALYSIS_tag {
    MD_LINETYPE type;
    unsigned data;
    int enforce_new_block;
    OFF beg;
    OFF end;
    unsigned indent;        /* Indentation level. */
};

typedef struct MD_LINE_tag MD_LINE;
struct MD_LINE_tag {
    OFF beg;
    OFF end;
};

typedef struct MD_VERBATIMLINE_tag MD_VERBATIMLINE;
struct MD_VERBATIMLINE_tag {
    OFF beg;
    OFF end;
    OFF indent;
};


/*****************
 ***  Helpers  ***
 *****************/

/* Character accessors. */
#define CH(off)                 (ctx->text[(off)])
#define STR(off)                (ctx->text + (off))

/* Character classification.
 * Note we assume ASCII compatibility of code points < 128 here. */
#define ISIN_(ch, ch_min, ch_max)       ((ch_min) <= (unsigned)(ch) && (unsigned)(ch) <= (ch_max))
#define ISANYOF_(ch, palette)           ((ch) != _T('\0')  &&  md_strchr((palette), (ch)) != NULL)
#define ISANYOF2_(ch, ch1, ch2)         ((ch) == (ch1) || (ch) == (ch2))
#define ISANYOF3_(ch, ch1, ch2, ch3)    ((ch) == (ch1) || (ch) == (ch2) || (ch) == (ch3))
#define ISASCII_(ch)                    ((unsigned)(ch) <= 127)
#define ISBLANK_(ch)                    (ISANYOF2_((ch), _T(' '), _T('\t')))
#define ISNEWLINE_(ch)                  (ISANYOF2_((ch), _T('\r'), _T('\n')))
#define ISWHITESPACE_(ch)               (ISBLANK_(ch) || ISANYOF2_((ch), _T('\v'), _T('\f')))
#define ISCNTRL_(ch)                    ((unsigned)(ch) <= 31 || (unsigned)(ch) == 127)
#define ISPUNCT_(ch)                    (ISIN_(ch, 33, 47) || ISIN_(ch, 58, 64) || ISIN_(ch, 91, 96) || ISIN_(ch, 123, 126))
#define ISUPPER_(ch)                    (ISIN_(ch, _T('A'), _T('Z')))
#define ISLOWER_(ch)                    (ISIN_(ch, _T('a'), _T('z')))
#define ISALPHA_(ch)                    (ISUPPER_(ch) || ISLOWER_(ch))
#define ISDIGIT_(ch)                    (ISIN_(ch, _T('0'), _T('9')))
#define ISXDIGIT_(ch)                   (ISDIGIT_(ch) || ISIN_(ch, _T('A'), _T('F')) || ISIN_(ch, _T('a'), _T('f')))
#define ISALNUM_(ch)                    (ISALPHA_(ch) || ISDIGIT_(ch))

#define ISANYOF(off, palette)           ISANYOF_(CH(off), (palette))
#define ISANYOF2(off, ch1, ch2)         ISANYOF2_(CH(off), (ch1), (ch2))
#define ISANYOF3(off, ch1, ch2, ch3)    ISANYOF3_(CH(off), (ch1), (ch2), (ch3))
#define ISASCII(off)                    ISASCII_(CH(off))
#define ISBLANK(off)                    ISBLANK_(CH(off))
#define ISNEWLINE(off)                  ISNEWLINE_(CH(off))
#define ISWHITESPACE(off)               ISWHITESPACE_(CH(off))
#define ISCNTRL(off)                    ISCNTRL_(CH(off))
#define ISPUNCT(off)                    ISPUNCT_(CH(off))
#define ISUPPER(off)                    ISUPPER_(CH(off))
#define ISLOWER(off)                    ISLOWER_(CH(off))
#define ISALPHA(off)                    ISALPHA_(CH(off))
#define ISDIGIT(off)                    ISDIGIT_(CH(off))
#define ISXDIGIT(off)                   ISXDIGIT_(CH(off))
#define ISALNUM(off)                    ISALNUM_(CH(off))


#if defined MD4C_USE_UTF16
    #define md_strchr wcschr
#else
    #define md_strchr strchr
#endif


/* Case insensitive check of string equality. */
static inline int
md_ascii_case_eq(const CHAR* s1, const CHAR* s2, SZ n)
{
    OFF i;
    for(i = 0; i < n; i++) {
        CHAR ch1 = s1[i];
        CHAR ch2 = s2[i];

        if(ISLOWER_(ch1))
            ch1 += ('A'-'a');
        if(ISLOWER_(ch2))
            ch2 += ('A'-'a');
        if(ch1 != ch2)
            return FALSE;
    }
    return TRUE;
}

static inline int
md_ascii_eq(const CHAR* s1, const CHAR* s2, SZ n)
{
    return memcmp(s1, s2, n * sizeof(CHAR)) == 0;
}

static int
md_text_with_null_replacement(MD_CTX* ctx, MD_TEXTTYPE type, const CHAR* str, SZ size)
{
    OFF off = 0;
    int ret = 0;

    while(1) {
        while(off < size  &&  str[off] != _T('\0'))
            off++;

        if(off > 0) {
            ret = ctx->parser.text(type, str, off, ctx->userdata);
            if(ret != 0)
                return ret;

            str += off;
            size -= off;
            off = 0;
        }

        if(off >= size)
            return 0;

        ret = ctx->parser.text(MD_TEXT_NULLCHAR, _T(""), 1, ctx->userdata);
        if(ret != 0)
            return ret;
        off++;
    }
}


#define MD_CHECK(func)                                                      \
    do {                                                                    \
        ret = (func);                                                       \
        if(ret < 0)                                                         \
            goto abort;                                                     \
    } while(0)


#define MD_TEMP_BUFFER(sz)                                                  \
    do {                                                                    \
        if(sz > ctx->alloc_buffer) {                                        \
            CHAR* new_buffer;                                               \
            SZ new_size = ((sz) + (sz) / 2 + 128) & ~127;                   \
                                                                            \
            new_buffer = realloc(ctx->buffer, new_size);                    \
            if(new_buffer == NULL) {                                        \
                MD_LOG("realloc() failed.");                                \
                ret = -1;                                                   \
                goto abort;                                                 \
            }                                                               \
                                                                            \
            ctx->buffer = new_buffer;                                       \
            ctx->alloc_buffer = new_size;                                   \
        }                                                                   \
    } while(0)


#define MD_ENTER_BLOCK(type, arg)                                           \
    do {                                                                    \
        ret = ctx->parser.enter_block((type), (arg), ctx->userdata);        \
        if(ret != 0) {                                                      \
            MD_LOG("Aborted from enter_block() callback.");                 \
            goto abort;                                                     \
        }                                                                   \
    } while(0)

#define MD_LEAVE_BLOCK(type, arg)                                           \
    do {                                                                    \
        ret = ctx->parser.leave_block((type), (arg), ctx->userdata);        \
        if(ret != 0) {                                                      \
            MD_LOG("Aborted from leave_block() callback.");                 \
            goto abort;                                                     \
        }                                                                   \
    } while(0)

#define MD_ENTER_SPAN(type, arg)                                            \
    do {                                                                    \
        ret = ctx->parser.enter_span((type), (arg), ctx->userdata);         \
        if(ret != 0) {                                                      \
            MD_LOG("Aborted from enter_span() callback.");                  \
            goto abort;                                                     \
        }                                                                   \
    } while(0)

#define MD_LEAVE_SPAN(type, arg)                                            \
    do {                                                                    \
        ret = ctx->parser.leave_span((type), (arg), ctx->userdata);         \
        if(ret != 0) {                                                      \
            MD_LOG("Aborted from leave_span() callback.");                  \
            goto abort;                                                     \
        }                                                                   \
    } while(0)

#define MD_TEXT(type, str, size)                                            \
    do {                                                                    \
        if(size > 0) {                                                      \
            ret = ctx->parser.text((type), (str), (size), ctx->userdata);   \
            if(ret != 0) {                                                  \
                MD_LOG("Aborted from text() callback.");                    \
                goto abort;                                                 \
            }                                                               \
        }                                                                   \
    } while(0)

#define MD_TEXT_INSECURE(type, str, size)                                   \
    do {                                                                    \
        if(size > 0) {                                                      \
            ret = md_text_with_null_replacement(ctx, type, str, size);      \
            if(ret != 0) {                                                  \
                MD_LOG("Aborted from text() callback.");                    \
                goto abort;                                                 \
            }                                                               \
        }                                                                   \
    } while(0)


/* If the offset falls into a gap between line, we return the following
 * line. */
static const MD_LINE*
md_lookup_line(OFF off, const MD_LINE* lines, MD_SIZE n_lines, MD_SIZE* p_line_index)
{
    MD_SIZE lo, hi;
    MD_SIZE pivot;
    const MD_LINE* line;

    lo = 0;
    hi = n_lines - 1;
    while(lo <= hi) {
        pivot = (lo + hi) / 2;
        line = &lines[pivot];

        if(off < line->beg) {
            if(hi == 0  ||  lines[hi-1].end < off) {
                if(p_line_index != NULL)
                    *p_line_index = pivot;
                return line;
            }
            hi = pivot - 1;
        } else if(off > line->end) {
            lo = pivot + 1;
        } else {
            if(p_line_index != NULL)
                *p_line_index = pivot;
            return line;
        }
    }

    return NULL;
}


/*************************
 ***  Unicode Support  ***
 *************************/

typedef struct MD_UNICODE_FOLD_INFO_tag MD_UNICODE_FOLD_INFO;
struct MD_UNICODE_FOLD_INFO_tag {
    unsigned codepoints[3];
    unsigned n_codepoints;
};


#if defined MD4C_USE_UTF16 || defined MD4C_USE_UTF8
    /* Binary search over sorted "map" of codepoints. Consecutive sequences
     * of codepoints may be encoded in the map by just using the
     * (MIN_CODEPOINT | 0x40000000) and (MAX_CODEPOINT | 0x80000000).
     *
     * Returns index of the found record in the map (in the case of ranges,
     * the minimal value is used); or -1 on failure. */
    static int
    md_unicode_bsearch__(unsigned codepoint, const unsigned* map, size_t map_size)
    {
        int beg, end;
        int pivot_beg, pivot_end;

        beg = 0;
        end = (int) map_size-1;
        while(beg <= end) {
            /* Pivot may be a range, not just a single value. */
            pivot_beg = pivot_end = (beg + end) / 2;
            if(map[pivot_end] & 0x40000000)
                pivot_end++;
            if(map[pivot_beg] & 0x80000000)
                pivot_beg--;

            if(codepoint < (map[pivot_beg] & 0x00ffffff))
                end = pivot_beg - 1;
            else if(codepoint > (map[pivot_end] & 0x00ffffff))
                beg = pivot_end + 1;
            else
                return pivot_beg;
        }

        return -1;
    }

    static int
    md_is_unicode_whitespace__(unsigned codepoint)
    {
#define R(cp_min, cp_max)   ((cp_min) | 0x40000000), ((cp_max) | 0x80000000)
#define S(cp)               (cp)
        /* Unicode "Zs" category.
         * (generated by scripts/build_whitespace_map.py) */
        static const unsigned WHITESPACE_MAP[] = {
            S(0x0020), S(0x00a0), S(0x1680), R(0x2000,0x200a), S(0x202f), S(0x205f), S(0x3000)
        };
#undef R
#undef S

        /* The ASCII ones are the most frequently used ones, also CommonMark
         * specification requests few more in this range. */
        if(codepoint <= 0x7f)
            return ISWHITESPACE_(codepoint);

        return (md_unicode_bsearch__(codepoint, WHITESPACE_MAP, SIZEOF_ARRAY(WHITESPACE_MAP)) >= 0);
    }

    static int
    md_is_unicode_punct__(unsigned codepoint)
    {
#define R(cp_min, cp_max)   ((cp_min) | 0x40000000), ((cp_max) | 0x80000000)
#define S(cp)               (cp)
        /* Unicode general "P" and "S" categories.
         * (generated by scripts/build_punct_map.py) */
        static const unsigned PUNCT_MAP[] = {
            R(0x0021,0x002f), R(0x003a,0x0040), R(0x005b,0x0060), R(0x007b,0x007e), R(0x00a1,0x00a9),
            R(0x00ab,0x00ac), R(0x00ae,0x00b1), S(0x00b4), R(0x00b6,0x00b8), S(0x00bb), S(0x00bf), S(0x00d7),
            S(0x00f7), R(0x02c2,0x02c5), R(0x02d2,0x02df), R(0x02e5,0x02eb), S(0x02ed), R(0x02ef,0x02ff), S(0x0375),
            S(0x037e), R(0x0384,0x0385), S(0x0387), S(0x03f6), S(0x0482), R(0x055a,0x055f), R(0x0589,0x058a),
            R(0x058d,0x058f), S(0x05be), S(0x05c0), S(0x05c3), S(0x05c6), R(0x05f3,0x05f4), R(0x0606,0x060f),
            S(0x061b), R(0x061d,0x061f), R(0x066a,0x066d), S(0x06d4), S(0x06de), S(0x06e9), R(0x06fd,0x06fe),
            R(0x0700,0x070d), R(0x07f6,0x07f9), R(0x07fe,0x07ff), R(0x0830,0x083e), S(0x085e), S(0x0888),
            R(0x0964,0x0965), S(0x0970), R(0x09f2,0x09f3), R(0x09fa,0x09fb), S(0x09fd), S(0x0a76), R(0x0af0,0x0af1),
            S(0x0b70), R(0x0bf3,0x0bfa), S(0x0c77), S(0x0c7f), S(0x0c84), S(0x0d4f), S(0x0d79), S(0x0df4), S(0x0e3f),
            S(0x0e4f), R(0x0e5a,0x0e5b), R(0x0f01,0x0f17), R(0x0f1a,0x0f1f), S(0x0f34), S(0x0f36), S(0x0f38),
            R(0x0f3a,0x0f3d), S(0x0f85), R(0x0fbe,0x0fc5), R(0x0fc7,0x0fcc), R(0x0fce,0x0fda), R(0x104a,0x104f),
            R(0x109e,0x109f), S(0x10fb), R(0x1360,0x1368), R(0x1390,0x1399), S(0x1400), R(0x166d,0x166e),
            R(0x169b,0x169c), R(0x16eb,0x16ed), R(0x1735,0x1736), R(0x17d4,0x17d6), R(0x17d8,0x17db),
            R(0x1800,0x180a), S(0x1940), R(0x1944,0x1945), R(0x19de,0x19ff), R(0x1a1e,0x1a1f), R(0x1aa0,0x1aa6),
            R(0x1aa8,0x1aad), R(0x1b5a,0x1b6a), R(0x1b74,0x1b7e), R(0x1bfc,0x1bff), R(0x1c3b,0x1c3f),
            R(0x1c7e,0x1c7f), R(0x1cc0,0x1cc7), S(0x1cd3), S(0x1fbd), R(0x1fbf,0x1fc1), R(0x1fcd,0x1fcf),
            R(0x1fdd,0x1fdf), R(0x1fed,0x1fef), R(0x1ffd,0x1ffe), R(0x2010,0x2027), R(0x2030,0x205e),
            R(0x207a,0x207e), R(0x208a,0x208e), R(0x20a0,0x20c0), R(0x2100,0x2101), R(0x2103,0x2106),
            R(0x2108,0x2109), S(0x2114), R(0x2116,0x2118), R(0x211e,0x2123), S(0x2125), S(0x2127), S(0x2129),
            S(0x212e), R(0x213a,0x213b), R(0x2140,0x2144), R(0x214a,0x214d), S(0x214f), R(0x218a,0x218b),
            R(0x2190,0x2426), R(0x2440,0x244a), R(0x249c,0x24e9), R(0x2500,0x2775), R(0x2794,0x2b73),
            R(0x2b76,0x2b95), R(0x2b97,0x2bff), R(0x2ce5,0x2cea), R(0x2cf9,0x2cfc), R(0x2cfe,0x2cff), S(0x2d70),
            R(0x2e00,0x2e2e), R(0x2e30,0x2e5d), R(0x2e80,0x2e99), R(0x2e9b,0x2ef3), R(0x2f00,0x2fd5),
            R(0x2ff0,0x2fff), R(0x3001,0x3004), R(0x3008,0x3020), S(0x3030), R(0x3036,0x3037), R(0x303d,0x303f),
            R(0x309b,0x309c), S(0x30a0), S(0x30fb), R(0x3190,0x3191), R(0x3196,0x319f), R(0x31c0,0x31e3), S(0x31ef),
            R(0x3200,0x321e), R(0x322a,0x3247), S(0x3250), R(0x3260,0x327f), R(0x328a,0x32b0), R(0x32c0,0x33ff),
            R(0x4dc0,0x4dff), R(0xa490,0xa4c6), R(0xa4fe,0xa4ff), R(0xa60d,0xa60f), S(0xa673), S(0xa67e),
            R(0xa6f2,0xa6f7), R(0xa700,0xa716), R(0xa720,0xa721), R(0xa789,0xa78a), R(0xa828,0xa82b),
            R(0xa836,0xa839), R(0xa874,0xa877), R(0xa8ce,0xa8cf), R(0xa8f8,0xa8fa), S(0xa8fc), R(0xa92e,0xa92f),
            S(0xa95f), R(0xa9c1,0xa9cd), R(0xa9de,0xa9df), R(0xaa5c,0xaa5f), R(0xaa77,0xaa79), R(0xaade,0xaadf),
            R(0xaaf0,0xaaf1), S(0xab5b), R(0xab6a,0xab6b), S(0xabeb), S(0xfb29), R(0xfbb2,0xfbc2), R(0xfd3e,0xfd4f),
            S(0xfdcf), R(0xfdfc,0xfdff), R(0xfe10,0xfe19), R(0xfe30,0xfe52), R(0xfe54,0xfe66), R(0xfe68,0xfe6b),
            R(0xff01,0xff0f), R(0xff1a,0xff20), R(0xff3b,0xff40), R(0xff5b,0xff65), R(0xffe0,0xffe6),
            R(0xffe8,0xffee), R(0xfffc,0xfffd), R(0x10100,0x10102), R(0x10137,0x1013f), R(0x10179,0x10189),
            R(0x1018c,0x1018e), R(0x10190,0x1019c), S(0x101a0), R(0x101d0,0x101fc), S(0x1039f), S(0x103d0),
            S(0x1056f), S(0x10857), R(0x10877,0x10878), S(0x1091f), S(0x1093f), R(0x10a50,0x10a58), S(0x10a7f),
            S(0x10ac8), R(0x10af0,0x10af6), R(0x10b39,0x10b3f), R(0x10b99,0x10b9c), S(0x10ead), R(0x10f55,0x10f59),
            R(0x10f86,0x10f89), R(0x11047,0x1104d), R(0x110bb,0x110bc), R(0x110be,0x110c1), R(0x11140,0x11143),
            R(0x11174,0x11175), R(0x111c5,0x111c8), S(0x111cd), S(0x111db), R(0x111dd,0x111df), R(0x11238,0x1123d),
            S(0x112a9), R(0x1144b,0x1144f), R(0x1145a,0x1145b), S(0x1145d), S(0x114c6), R(0x115c1,0x115d7),
            R(0x11641,0x11643), R(0x11660,0x1166c), S(0x116b9), R(0x1173c,0x1173f), S(0x1183b), R(0x11944,0x11946),
            S(0x119e2), R(0x11a3f,0x11a46), R(0x11a9a,0x11a9c), R(0x11a9e,0x11aa2), R(0x11b00,0x11b09),
            R(0x11c41,0x11c45), R(0x11c70,0x11c71), R(0x11ef7,0x11ef8), R(0x11f43,0x11f4f), R(0x11fd5,0x11ff1),
            S(0x11fff), R(0x12470,0x12474), R(0x12ff1,0x12ff2), R(0x16a6e,0x16a6f), S(0x16af5), R(0x16b37,0x16b3f),
            R(0x16b44,0x16b45), R(0x16e97,0x16e9a), S(0x16fe2), S(0x1bc9c), S(0x1bc9f), R(0x1cf50,0x1cfc3),
            R(0x1d000,0x1d0f5), R(0x1d100,0x1d126), R(0x1d129,0x1d164), R(0x1d16a,0x1d16c), R(0x1d183,0x1d184),
            R(0x1d18c,0x1d1a9), R(0x1d1ae,0x1d1ea), R(0x1d200,0x1d241), S(0x1d245), R(0x1d300,0x1d356), S(0x1d6c1),
            S(0x1d6db), S(0x1d6fb), S(0x1d715), S(0x1d735), S(0x1d74f), S(0x1d76f), S(0x1d789), S(0x1d7a9),
            S(0x1d7c3), R(0x1d800,0x1d9ff), R(0x1da37,0x1da3a), R(0x1da6d,0x1da74), R(0x1da76,0x1da83),
            R(0x1da85,0x1da8b), S(0x1e14f), S(0x1e2ff), R(0x1e95e,0x1e95f), S(0x1ecac), S(0x1ecb0), S(0x1ed2e),
            R(0x1eef0,0x1eef1), R(0x1f000,0x1f02b), R(0x1f030,0x1f093), R(0x1f0a0,0x1f0ae), R(0x1f0b1,0x1f0bf),
            R(0x1f0c1,0x1f0cf), R(0x1f0d1,0x1f0f5), R(0x1f10d,0x1f1ad), R(0x1f1e6,0x1f202), R(0x1f210,0x1f23b),
            R(0x1f240,0x1f248), R(0x1f250,0x1f251), R(0x1f260,0x1f265), R(0x1f300,0x1f6d7), R(0x1f6dc,0x1f6ec),
            R(0x1f6f0,0x1f6fc), R(0x1f700,0x1f776), R(0x1f77b,0x1f7d9), R(0x1f7e0,0x1f7eb), S(0x1f7f0),
            R(0x1f800,0x1f80b), R(0x1f810,0x1f847), R(0x1f850,0x1f859), R(0x1f860,0x1f887), R(0x1f890,0x1f8ad),
            R(0x1f8b0,0x1f8b1), R(0x1f900,0x1fa53), R(0x1fa60,0x1fa6d), R(0x1fa70,0x1fa7c), R(0x1fa80,0x1fa88),
            R(0x1fa90,0x1fabd), R(0x1fabf,0x1fac5), R(0x1face,0x1fadb), R(0x1fae0,0x1fae8), R(0x1faf0,0x1faf8),
            R(0x1fb00,0x1fb92), R(0x1fb94,0x1fbca)
        };
#undef R
#undef S

        /* The ASCII ones are the most frequently used ones, also CommonMark
         * specification requests few more in this range. */
        if(codepoint <= 0x7f)
            return ISPUNCT_(codepoint);

        return (md_unicode_bsearch__(codepoint, PUNCT_MAP, SIZEOF_ARRAY(PUNCT_MAP)) >= 0);
    }

    static void
    md_get_unicode_fold_info(unsigned codepoint, MD_UNICODE_FOLD_INFO* info)
    {
#define R(cp_min, cp_max)   ((cp_min) | 0x40000000), ((cp_max) | 0x80000000)
#define S(cp)               (cp)
        /* Unicode "Pc", "Pd", "Pe", "Pf", "Pi", "Po", "Ps" categories.
         * (generated by scripts/build_folding_map.py) */
        static const unsigned FOLD_MAP_1[] = {
            R(0x0041,0x005a), S(0x00b5), R(0x00c0,0x00d6), R(0x00d8,0x00de), R(0x0100,0x012e), R(0x0132,0x0136),
            R(0x0139,0x0147), R(0x014a,0x0176), S(0x0178), R(0x0179,0x017d), S(0x017f), S(0x0181), S(0x0182),
            S(0x0184), S(0x0186), S(0x0187), S(0x0189), S(0x018a), S(0x018b), S(0x018e), S(0x018f), S(0x0190),
            S(0x0191), S(0x0193), S(0x0194), S(0x0196), S(0x0197), S(0x0198), S(0x019c), S(0x019d), S(0x019f),
            R(0x01a0,0x01a4), S(0x01a6), S(0x01a7), S(0x01a9), S(0x01ac), S(0x01ae), S(0x01af), S(0x01b1), S(0x01b2),
            S(0x01b3), S(0x01b5), S(0x01b7), S(0x01b8), S(0x01bc), S(0x01c4), S(0x01c5), S(0x01c7), S(0x01c8),
            S(0x01ca), R(0x01cb,0x01db), R(0x01de,0x01ee), S(0x01f1), S(0x01f2), S(0x01f4), S(0x01f6), S(0x01f7),
            R(0x01f8,0x021e), S(0x0220), R(0x0222,0x0232), S(0x023a), S(0x023b), S(0x023d), S(0x023e), S(0x0241),
            S(0x0243), S(0x0244), S(0x0245), R(0x0246,0x024e), S(0x0345), S(0x0370), S(0x0372), S(0x0376), S(0x037f),
            S(0x0386), R(0x0388,0x038a), S(0x038c), S(0x038e), S(0x038f), R(0x0391,0x03a1), R(0x03a3,0x03ab),
            S(0x03c2), S(0x03cf), S(0x03d0), S(0x03d1), S(0x03d5), S(0x03d6), R(0x03d8,0x03ee), S(0x03f0), S(0x03f1),
            S(0x03f4), S(0x03f5), S(0x03f7), S(0x03f9), S(0x03fa), R(0x03fd,0x03ff), R(0x0400,0x040f),
            R(0x0410,0x042f), R(0x0460,0x0480), R(0x048a,0x04be), S(0x04c0), R(0x04c1,0x04cd), R(0x04d0,0x052e),
            R(0x0531,0x0556), R(0x10a0,0x10c5), S(0x10c7), S(0x10cd), R(0x13f8,0x13fd), S(0x1c80), S(0x1c81),
            S(0x1c82), S(0x1c83), S(0x1c84), S(0x1c85), S(0x1c86), S(0x1c87), S(0x1c88), R(0x1c90,0x1cba),
            R(0x1cbd,0x1cbf), R(0x1e00,0x1e94), S(0x1e9b), R(0x1ea0,0x1efe), R(0x1f08,0x1f0f), R(0x1f18,0x1f1d),
            R(0x1f28,0x1f2f), R(0x1f38,0x1f3f), R(0x1f48,0x1f4d), S(0x1f59), S(0x1f5b), S(0x1f5d), S(0x1f5f),
            R(0x1f68,0x1f6f), S(0x1fb8), S(0x1fb9), S(0x1fba), S(0x1fbb), S(0x1fbe), R(0x1fc8,0x1fcb), S(0x1fd8),
            S(0x1fd9), S(0x1fda), S(0x1fdb), S(0x1fe8), S(0x1fe9), S(0x1fea), S(0x1feb), S(0x1fec), S(0x1ff8),
            S(0x1ff9), S(0x1ffa), S(0x1ffb), S(0x2126), S(0x212a), S(0x212b), S(0x2132), R(0x2160,0x216f), S(0x2183),
            R(0x24b6,0x24cf), R(0x2c00,0x2c2f), S(0x2c60), S(0x2c62), S(0x2c63), S(0x2c64), R(0x2c67,0x2c6b),
            S(0x2c6d), S(0x2c6e), S(0x2c6f), S(0x2c70), S(0x2c72), S(0x2c75), S(0x2c7e), S(0x2c7f), R(0x2c80,0x2ce2),
            S(0x2ceb), S(0x2ced), S(0x2cf2), R(0xa640,0xa66c), R(0xa680,0xa69a), R(0xa722,0xa72e), R(0xa732,0xa76e),
            S(0xa779), S(0xa77b), S(0xa77d), R(0xa77e,0xa786), S(0xa78b), S(0xa78d), S(0xa790), S(0xa792),
            R(0xa796,0xa7a8), S(0xa7aa), S(0xa7ab), S(0xa7ac), S(0xa7ad), S(0xa7ae), S(0xa7b0), S(0xa7b1), S(0xa7b2),
            S(0xa7b3), R(0xa7b4,0xa7c2), S(0xa7c4), S(0xa7c5), S(0xa7c6), S(0xa7c7), S(0xa7c9), S(0xa7d0), S(0xa7d6),
            S(0xa7d8), S(0xa7f5), R(0xab70,0xabbf), R(0xff21,0xff3a), R(0x10400,0x10427), R(0x104b0,0x104d3),
            R(0x10570,0x1057a), R(0x1057c,0x1058a), R(0x1058c,0x10592), S(0x10594), S(0x10595), R(0x10c80,0x10cb2),
            R(0x118a0,0x118bf), R(0x16e40,0x16e5f), R(0x1e900,0x1e921)
        };
        static const unsigned FOLD_MAP_1_DATA[] = {
            0x0061, 0x007a, 0x03bc, 0x00e0, 0x00f6, 0x00f8, 0x00fe, 0x0101, 0x012f, 0x0133, 0x0137, 0x013a, 0x0148,
            0x014b, 0x0177, 0x00ff, 0x017a, 0x017e, 0x0073, 0x0253, 0x0183, 0x0185, 0x0254, 0x0188, 0x0256, 0x0257,
            0x018c, 0x01dd, 0x0259, 0x025b, 0x0192, 0x0260, 0x0263, 0x0269, 0x0268, 0x0199, 0x026f, 0x0272, 0x0275,
            0x01a1, 0x01a5, 0x0280, 0x01a8, 0x0283, 0x01ad, 0x0288, 0x01b0, 0x028a, 0x028b, 0x01b4, 0x01b6, 0x0292,
            0x01b9, 0x01bd, 0x01c6, 0x01c6, 0x01c9, 0x01c9, 0x01cc, 0x01cc, 0x01dc, 0x01df, 0x01ef, 0x01f3, 0x01f3,
            0x01f5, 0x0195, 0x01bf, 0x01f9, 0x021f, 0x019e, 0x0223, 0x0233, 0x2c65, 0x023c, 0x019a, 0x2c66, 0x0242,
            0x0180, 0x0289, 0x028c, 0x0247, 0x024f, 0x03b9, 0x0371, 0x0373, 0x0377, 0x03f3, 0x03ac, 0x03ad, 0x03af,
            0x03cc, 0x03cd, 0x03ce, 0x03b1, 0x03c1, 0x03c3, 0x03cb, 0x03c3, 0x03d7, 0x03b2, 0x03b8, 0x03c6, 0x03c0,
            0x03d9, 0x03ef, 0x03ba, 0x03c1, 0x03b8, 0x03b5, 0x03f8, 0x03f2, 0x03fb, 0x037b, 0x037d, 0x0450, 0x045f,
            0x0430, 0x044f, 0x0461, 0x0481, 0x048b, 0x04bf, 0x04cf, 0x04c2, 0x04ce, 0x04d1, 0x052f, 0x0561, 0x0586,
            0x2d00, 0x2d25, 0x2d27, 0x2d2d, 0x13f0, 0x13f5, 0x0432, 0x0434, 0x043e, 0x0441, 0x0442, 0x0442, 0x044a,
            0x0463, 0xa64b, 0x10d0, 0x10fa, 0x10fd, 0x10ff, 0x1e01, 0x1e95, 0x1e61, 0x1ea1, 0x1eff, 0x1f00, 0x1f07,
            0x1f10, 0x1f15, 0x1f20, 0x1f27, 0x1f30, 0x1f37, 0x1f40, 0x1f45, 0x1f51, 0x1f53, 0x1f55, 0x1f57, 0x1f60,
            0x1f67, 0x1fb0, 0x1fb1, 0x1f70, 0x1f71, 0x03b9, 0x1f72, 0x1f75, 0x1fd0, 0x1fd1, 0x1f76, 0x1f77, 0x1fe0,
            0x1fe1, 0x1f7a, 0x1f7b, 0x1fe5, 0x1f78, 0x1f79, 0x1f7c, 0x1f7d, 0x03c9, 0x006b, 0x00e5, 0x214e, 0x2170,
            0x217f, 0x2184, 0x24d0, 0x24e9, 0x2c30, 0x2c5f, 0x2c61, 0x026b, 0x1d7d, 0x027d, 0x2c68, 0x2c6c, 0x0251,
            0x0271, 0x0250, 0x0252, 0x2c73, 0x2c76, 0x023f, 0x0240, 0x2c81, 0x2ce3, 0x2cec, 0x2cee, 0x2cf3, 0xa641,
            0xa66d, 0xa681, 0xa69b, 0xa723, 0xa72f, 0xa733, 0xa76f, 0xa77a, 0xa77c, 0x1d79, 0xa77f, 0xa787, 0xa78c,
            0x0265, 0xa791, 0xa793, 0xa797, 0xa7a9, 0x0266, 0x025c, 0x0261, 0x026c, 0x026a, 0x029e, 0x0287, 0x029d,
            0xab53, 0xa7b5, 0xa7c3, 0xa794, 0x0282, 0x1d8e, 0xa7c8, 0xa7ca, 0xa7d1, 0xa7d7, 0xa7d9, 0xa7f6, 0x13a0,
            0x13ef, 0xff41, 0xff5a, 0x10428, 0x1044f, 0x104d8, 0x104fb, 0x10597, 0x105a1, 0x105a3, 0x105b1, 0x105b3,
            0x105b9, 0x105bb, 0x105bc, 0x10cc0, 0x10cf2, 0x118c0, 0x118df, 0x16e60, 0x16e7f, 0x1e922, 0x1e943
        };
        static const unsigned FOLD_MAP_2[] = {
            S(0x00df), S(0x0130), S(0x0149), S(0x01f0), S(0x0587), S(0x1e96), S(0x1e97), S(0x1e98), S(0x1e99),
            S(0x1e9a), S(0x1e9e), S(0x1f50), R(0x1f80,0x1f87), R(0x1f88,0x1f8f), R(0x1f90,0x1f97), R(0x1f98,0x1f9f),
            R(0x1fa0,0x1fa7), R(0x1fa8,0x1faf), S(0x1fb2), S(0x1fb3), S(0x1fb4), S(0x1fb6), S(0x1fbc), S(0x1fc2),
            S(0x1fc3), S(0x1fc4), S(0x1fc6), S(0x1fcc), S(0x1fd6), S(0x1fe4), S(0x1fe6), S(0x1ff2), S(0x1ff3),
            S(0x1ff4), S(0x1ff6), S(0x1ffc), S(0xfb00), S(0xfb01), S(0xfb02), S(0xfb05), S(0xfb06), S(0xfb13),
            S(0xfb14), S(0xfb15), S(0xfb16), S(0xfb17)
        };
        static const unsigned FOLD_MAP_2_DATA[] = {
            0x0073,0x0073, 0x0069,0x0307, 0x02bc,0x006e, 0x006a,0x030c, 0x0565,0x0582, 0x0068,0x0331, 0x0074,0x0308,
            0x0077,0x030a, 0x0079,0x030a, 0x0061,0x02be, 0x0073,0x0073, 0x03c5,0x0313, 0x1f00,0x03b9, 0x1f07,0x03b9,
            0x1f00,0x03b9, 0x1f07,0x03b9, 0x1f20,0x03b9, 0x1f27,0x03b9, 0x1f20,0x03b9, 0x1f27,0x03b9, 0x1f60,0x03b9,
            0x1f67,0x03b9, 0x1f60,0x03b9, 0x1f67,0x03b9, 0x1f70,0x03b9, 0x03b1,0x03b9, 0x03ac,0x03b9, 0x03b1,0x0342,
            0x03b1,0x03b9, 0x1f74,0x03b9, 0x03b7,0x03b9, 0x03ae,0x03b9, 0x03b7,0x0342, 0x03b7,0x03b9, 0x03b9,0x0342,
            0x03c1,0x0313, 0x03c5,0x0342, 0x1f7c,0x03b9, 0x03c9,0x03b9, 0x03ce,0x03b9, 0x03c9,0x0342, 0x03c9,0x03b9,
            0x0066,0x0066, 0x0066,0x0069, 0x0066,0x006c, 0x0073,0x0074, 0x0073,0x0074, 0x0574,0x0576, 0x0574,0x0565,
            0x0574,0x056b, 0x057e,0x0576, 0x0574,0x056d
        };
        static const unsigned FOLD_MAP_3[] = {
            S(0x0390), S(0x03b0), S(0x1f52), S(0x1f54), S(0x1f56), S(0x1fb7), S(0x1fc7), S(0x1fd2), S(0x1fd3),
            S(0x1fd7), S(0x1fe2), S(0x1fe3), S(0x1fe7), S(0x1ff7), S(0xfb03), S(0xfb04)
        };
        static const unsigned FOLD_MAP_3_DATA[] = {
            0x03b9,0x0308,0x0301, 0x03c5,0x0308,0x0301, 0x03c5,0x0313,0x0300, 0x03c5,0x0313,0x0301,
            0x03c5,0x0313,0x0342, 0x03b1,0x0342,0x03b9, 0x03b7,0x0342,0x03b9, 0x03b9,0x0308,0x0300,
            0x03b9,0x0308,0x0301, 0x03b9,0x0308,0x0342, 0x03c5,0x0308,0x0300, 0x03c5,0x0308,0x0301,
            0x03c5,0x0308,0x0342, 0x03c9,0x0342,0x03b9, 0x0066,0x0066,0x0069, 0x0066,0x0066,0x006c
        };
#undef R
#undef S
        static const struct {
            const unsigned* map;
            const unsigned* data;
            size_t map_size;
            unsigned n_codepoints;
        } FOLD_MAP_LIST[] = {
            { FOLD_MAP_1, FOLD_MAP_1_DATA, SIZEOF_ARRAY(FOLD_MAP_1), 1 },
            { FOLD_MAP_2, FOLD_MAP_2_DATA, SIZEOF_ARRAY(FOLD_MAP_2), 2 },
            { FOLD_MAP_3, FOLD_MAP_3_DATA, SIZEOF_ARRAY(FOLD_MAP_3), 3 }
        };

        int i;

        /* Fast path for ASCII characters. */
        if(codepoint <= 0x7f) {
            info->codepoints[0] = codepoint;
            if(ISUPPER_(codepoint))
                info->codepoints[0] += 'a' - 'A';
            info->n_codepoints = 1;
            return;
        }

        /* Try to locate the codepoint in any of the maps. */
        for(i = 0; i < (int) SIZEOF_ARRAY(FOLD_MAP_LIST); i++) {
            int index;

            index = md_unicode_bsearch__(codepoint, FOLD_MAP_LIST[i].map, FOLD_MAP_LIST[i].map_size);
            if(index >= 0) {
                /* Found the mapping. */
                unsigned n_codepoints = FOLD_MAP_LIST[i].n_codepoints;
                const unsigned* map = FOLD_MAP_LIST[i].map;
                const unsigned* codepoints = FOLD_MAP_LIST[i].data + (index * n_codepoints);

                memcpy(info->codepoints, codepoints, sizeof(unsigned) * n_codepoints);
                info->n_codepoints = n_codepoints;

                if(FOLD_MAP_LIST[i].map[index] != codepoint) {
                    /* The found mapping maps whole range of codepoints,
                     * i.e. we have to offset info->codepoints[0] accordingly. */
                    if((map[index] & 0x00ffffff)+1 == codepoints[0]) {
                        /* Alternating type of the range. */
                        info->codepoints[0] = codepoint + ((codepoint & 0x1) == (map[index] & 0x1) ? 1 : 0);
                    } else {
                        /* Range to range kind of mapping. */
                        info->codepoints[0] += (codepoint - (map[index] & 0x00ffffff));
                    }
                }

                return;
            }
        }

        /* No mapping found. Map the codepoint to itself. */
        info->codepoints[0] = codepoint;
        info->n_codepoints = 1;
    }
#endif


#if defined MD4C_USE_UTF16
    #define IS_UTF16_SURROGATE_HI(word)     (((WORD)(word) & 0xfc00) == 0xd800)
    #define IS_UTF16_SURROGATE_LO(word)     (((WORD)(word) & 0xfc00) == 0xdc00)
    #define UTF16_DECODE_SURROGATE(hi, lo)  (0x10000 + ((((unsigned)(hi) & 0x3ff) << 10) | (((unsigned)(lo) & 0x3ff) << 0)))

    static unsigned
    md_decode_utf16le__(const CHAR* str, SZ str_size, SZ* p_size)
    {
        if(IS_UTF16_SURROGATE_HI(str[0])) {
            if(1 < str_size && IS_UTF16_SURROGATE_LO(str[1])) {
                if(p_size != NULL)
                    *p_size = 2;
                return UTF16_DECODE_SURROGATE(str[0], str[1]);
            }
        }

        if(p_size != NULL)
            *p_size = 1;
        return str[0];
    }

    static unsigned
    md_decode_utf16le_before__(MD_CTX* ctx, OFF off)
    {
        if(off > 2 && IS_UTF16_SURROGATE_HI(CH(off-2)) && IS_UTF16_SURROGATE_LO(CH(off-1)))
            return UTF16_DECODE_SURROGATE(CH(off-2), CH(off-1));

        return CH(off);
    }

    /* No whitespace uses surrogates, so no decoding needed here. */
    #define ISUNICODEWHITESPACE_(codepoint) md_is_unicode_whitespace__(codepoint)
    #define ISUNICODEWHITESPACE(off)        md_is_unicode_whitespace__(CH(off))
    #define ISUNICODEWHITESPACEBEFORE(off)  md_is_unicode_whitespace__(CH((off)-1))

    #define ISUNICODEPUNCT(off)             md_is_unicode_punct__(md_decode_utf16le__(STR(off), ctx->size - (off), NULL))
    #define ISUNICODEPUNCTBEFORE(off)       md_is_unicode_punct__(md_decode_utf16le_before__(ctx, off))

    static inline int
    md_decode_unicode(const CHAR* str, OFF off, SZ str_size, SZ* p_char_size)
    {
        return md_decode_utf16le__(str+off, str_size-off, p_char_size);
    }
#elif defined MD4C_USE_UTF8
    #define IS_UTF8_LEAD1(byte)     ((unsigned char)(byte) <= 0x7f)
    #define IS_UTF8_LEAD2(byte)     (((unsigned char)(byte) & 0xe0) == 0xc0)
    #define IS_UTF8_LEAD3(byte)     (((unsigned char)(byte) & 0xf0) == 0xe0)
    #define IS_UTF8_LEAD4(byte)     (((unsigned char)(byte) & 0xf8) == 0xf0)
    #define IS_UTF8_TAIL(byte)      (((unsigned char)(byte) & 0xc0) == 0x80)

    static unsigned
    md_decode_utf8__(const CHAR* str, SZ str_size, SZ* p_size)
    {
        if(!IS_UTF8_LEAD1(str[0])) {
            if(IS_UTF8_LEAD2(str[0])) {
                if(1 < str_size && IS_UTF8_TAIL(str[1])) {
                    if(p_size != NULL)
                        *p_size = 2;

                    return (((unsigned int)str[0] & 0x1f) << 6) |
                           (((unsigned int)str[1] & 0x3f) << 0);
                }
            } else if(IS_UTF8_LEAD3(str[0])) {
                if(2 < str_size && IS_UTF8_TAIL(str[1]) && IS_UTF8_TAIL(str[2])) {
                    if(p_size != NULL)
                        *p_size = 3;

                    return (((unsigned int)str[0] & 0x0f) << 12) |
                           (((unsigned int)str[1] & 0x3f) << 6) |
                           (((unsigned int)str[2] & 0x3f) << 0);
                }
            } else if(IS_UTF8_LEAD4(str[0])) {
                if(3 < str_size && IS_UTF8_TAIL(str[1]) && IS_UTF8_TAIL(str[2]) && IS_UTF8_TAIL(str[3])) {
                    if(p_size != NULL)
                        *p_size = 4;

                    return (((unsigned int)str[0] & 0x07) << 18) |
                           (((unsigned int)str[1] & 0x3f) << 12) |
                           (((unsigned int)str[2] & 0x3f) << 6) |
                           (((unsigned int)str[3] & 0x3f) << 0);
                }
            }
        }

        if(p_size != NULL)
            *p_size = 1;
        return (unsigned) str[0];
    }

    static unsigned
    md_decode_utf8_before__(MD_CTX* ctx, OFF off)
    {
        if(!IS_UTF8_LEAD1(CH(off-1))) {
            if(off > 1 && IS_UTF8_LEAD2(CH(off-2)) && IS_UTF8_TAIL(CH(off-1)))
                return (((unsigned int)CH(off-2) & 0x1f) << 6) |
                       (((unsigned int)CH(off-1) & 0x3f) << 0);

            if(off > 2 && IS_UTF8_LEAD3(CH(off-3)) && IS_UTF8_TAIL(CH(off-2)) && IS_UTF8_TAIL(CH(off-1)))
                return (((unsigned int)CH(off-3) & 0x0f) << 12) |
                       (((unsigned int)CH(off-2) & 0x3f) << 6) |
                       (((unsigned int)CH(off-1) & 0x3f) << 0);

            if(off > 3 && IS_UTF8_LEAD4(CH(off-4)) && IS_UTF8_TAIL(CH(off-3)) && IS_UTF8_TAIL(CH(off-2)) && IS_UTF8_TAIL(CH(off-1)))
                return (((unsigned int)CH(off-4) & 0x07) << 18) |
                       (((unsigned int)CH(off-3) & 0x3f) << 12) |
                       (((unsigned int)CH(off-2) & 0x3f) << 6) |
                       (((unsigned int)CH(off-1) & 0x3f) << 0);
        }

        return (unsigned) CH(off-1);
    }

    #define ISUNICODEWHITESPACE_(codepoint) md_is_unicode_whitespace__(codepoint)
    #define ISUNICODEWHITESPACE(off)        md_is_unicode_whitespace__(md_decode_utf8__(STR(off), ctx->size - (off), NULL))
    #define ISUNICODEWHITESPACEBEFORE(off)  md_is_unicode_whitespace__(md_decode_utf8_before__(ctx, off))

    #define ISUNICODEPUNCT(off)             md_is_unicode_punct__(md_decode_utf8__(STR(off), ctx->size - (off), NULL))
    #define ISUNICODEPUNCTBEFORE(off)       md_is_unicode_punct__(md_decode_utf8_before__(ctx, off))

    static inline unsigned
    md_decode_unicode(const CHAR* str, OFF off, SZ str_size, SZ* p_char_size)
    {
        return md_decode_utf8__(str+off, str_size-off, p_char_size);
    }
#else
    #define ISUNICODEWHITESPACE_(codepoint) ISWHITESPACE_(codepoint)
    #define ISUNICODEWHITESPACE(off)        ISWHITESPACE(off)
    #define ISUNICODEWHITESPACEBEFORE(off)  ISWHITESPACE((off)-1)

    #define ISUNICODEPUNCT(off)             ISPUNCT(off)
    #define ISUNICODEPUNCTBEFORE(off)       ISPUNCT((off)-1)

    static inline void
    md_get_unicode_fold_info(unsigned codepoint, MD_UNICODE_FOLD_INFO* info)
    {
        info->codepoints[0] = codepoint;
        if(ISUPPER_(codepoint))
            info->codepoints[0] += 'a' - 'A';
        info->n_codepoints = 1;
    }

    static inline unsigned
    md_decode_unicode(const CHAR* str, OFF off, SZ str_size, SZ* p_size)
    {
        *p_size = 1;
        return (unsigned) str[off];
    }
#endif


/*************************************
 ***  Helper string manipulations  ***
 *************************************/

/* Fill buffer with copy of the string between 'beg' and 'end' but replace any
 * line breaks with given replacement character.
 *
 * NOTE: Caller is responsible to make sure the buffer is large enough.
 * (Given the output is always shorter than input, (end - beg) is good idea
 * what the caller should allocate.)
 */
static void
md_merge_lines(MD_CTX* ctx, OFF beg, OFF end, const MD_LINE* lines, MD_SIZE n_lines,
               CHAR line_break_replacement_char, CHAR* buffer, SZ* p_size)
{
    CHAR* ptr = buffer;
    int line_index = 0;
    OFF off = beg;

    MD_UNUSED(n_lines);

    while(1) {
        const MD_LINE* line = &lines[line_index];
        OFF line_end = line->end;
        if(end < line_end)
            line_end = end;

        while(off < line_end) {
            *ptr = CH(off);
            ptr++;
            off++;
        }

        if(off >= end) {
            *p_size = (MD_SIZE)(ptr - buffer);
            return;
        }

        *ptr = line_break_replacement_char;
        ptr++;

        line_index++;
        off = lines[line_index].beg;
    }
}

/* Wrapper of md_merge_lines() which allocates new buffer for the output string.
 */
static int
md_merge_lines_alloc(MD_CTX* ctx, OFF beg, OFF end, const MD_LINE* lines, MD_SIZE n_lines,
                    CHAR line_break_replacement_char, CHAR** p_str, SZ* p_size)
{
    CHAR* buffer;

    buffer = (CHAR*) malloc(sizeof(CHAR) * (end - beg));
    if(buffer == NULL) {
        MD_LOG("malloc() failed.");
        return -1;
    }

    md_merge_lines(ctx, beg, end, lines, n_lines,
                line_break_replacement_char, buffer, p_size);

    *p_str = buffer;
    return 0;
}

static OFF
md_skip_unicode_whitespace(const CHAR* label, OFF off, SZ size)
{
    SZ char_size;
    unsigned codepoint;

    while(off < size) {
        codepoint = md_decode_unicode(label, off, size, &char_size);
        if(!ISUNICODEWHITESPACE_(codepoint)  &&  !ISNEWLINE_(label[off]))
            break;
        off += char_size;
    }

    return off;
}


/******************************
 ***  Recognizing raw HTML  ***
 ******************************/

/* md_is_html_tag() may be called when processing inlines (inline raw HTML)
 * or when breaking document to blocks (checking for start of HTML block type 7).
 *
 * When breaking document to blocks, we do not yet know line boundaries, but
 * in that case the whole tag has to live on a single line. We distinguish this
 * by n_lines == 0.
 */
static int
md_is_html_tag(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    int attr_state;
    OFF off = beg;
    OFF line_end = (n_lines > 0) ? lines[0].end : ctx->size;
    MD_SIZE line_index = 0;

    MD_ASSERT(CH(beg) == _T('<'));

    if(off + 1 >= line_end)
        return FALSE;
    off++;

    /* For parsing attributes, we need a little state automaton below.
     * State -1: no attributes are allowed.
     * State 0: attribute could follow after some whitespace.
     * State 1: after a whitespace (attribute name may follow).
     * State 2: after attribute name ('=' MAY follow).
     * State 3: after '=' (value specification MUST follow).
     * State 41: in middle of unquoted attribute value.
     * State 42: in middle of single-quoted attribute value.
     * State 43: in middle of double-quoted attribute value.
     */
    attr_state = 0;

    if(CH(off) == _T('/')) {
        /* Closer tag "</ ... >". No attributes may be present. */
        attr_state = -1;
        off++;
    }

    /* Tag name */
    if(off >= line_end  ||  !ISALPHA(off))
        return FALSE;
    off++;
    while(off < line_end  &&  (ISALNUM(off)  ||  CH(off) == _T('-')))
        off++;

    /* (Optional) attributes (if not closer), (optional) '/' (if not closer)
     * and final '>'. */
    while(1) {
        while(off < line_end  &&  !ISNEWLINE(off)) {
            if(attr_state > 40) {
                if(attr_state == 41 && (ISBLANK(off) || ISANYOF(off, _T("\"'=<>`")))) {
                    attr_state = 0;
                    off--;  /* Put the char back for re-inspection in the new state. */
                } else if(attr_state == 42 && CH(off) == _T('\'')) {
                    attr_state = 0;
                } else if(attr_state == 43 && CH(off) == _T('"')) {
                    attr_state = 0;
                }
                off++;
            } else if(ISWHITESPACE(off)) {
                if(attr_state == 0)
                    attr_state = 1;
                off++;
            } else if(attr_state <= 2 && CH(off) == _T('>')) {
                /* End. */
                goto done;
            } else if(attr_state <= 2 && CH(off) == _T('/') && off+1 < line_end && CH(off+1) == _T('>')) {
                /* End with digraph '/>' */
                off++;
                goto done;
            } else if((attr_state == 1 || attr_state == 2) && (ISALPHA(off) || CH(off) == _T('_') || CH(off) == _T(':'))) {
                off++;
                /* Attribute name */
                while(off < line_end && (ISALNUM(off) || ISANYOF(off, _T("_.:-"))))
                    off++;
                attr_state = 2;
            } else if(attr_state == 2 && CH(off) == _T('=')) {
                /* Attribute assignment sign */
                off++;
                attr_state = 3;
            } else if(attr_state == 3) {
                /* Expecting start of attribute value. */
                if(CH(off) == _T('"'))
                    attr_state = 43;
                else if(CH(off) == _T('\''))
                    attr_state = 42;
                else if(!ISANYOF(off, _T("\"'=<>`"))  &&  !ISNEWLINE(off))
                    attr_state = 41;
                else
                    return FALSE;
                off++;
            } else {
                /* Anything unexpected. */
                return FALSE;
            }
        }

        /* We have to be on a single line. See definition of start condition
         * of HTML block, type 7. */
        if(n_lines == 0)
            return FALSE;

        line_index++;
        if(line_index >= n_lines)
            return FALSE;

        off = lines[line_index].beg;
        line_end = lines[line_index].end;

        if(attr_state == 0  ||  attr_state == 41)
            attr_state = 1;

        if(off >= max_end)
            return FALSE;
    }

done:
    if(off >= max_end)
        return FALSE;

    *p_end = off+1;
    return TRUE;
}

static int
md_scan_for_html_closer(MD_CTX* ctx, const MD_CHAR* str, MD_SIZE len,
                        const MD_LINE* lines, MD_SIZE n_lines,
                        OFF beg, OFF max_end, OFF* p_end,
                        OFF* p_scan_horizon)
{
    OFF off = beg;
    MD_SIZE line_index = 0;

    if(off < *p_scan_horizon  &&  *p_scan_horizon >= max_end - len) {
        /* We have already scanned the range up to the max_end so we know
         * there is nothing to see. */
        return FALSE;
    }

    while(TRUE) {
        while(off + len <= lines[line_index].end  &&  off + len <= max_end) {
            if(md_ascii_eq(STR(off), str, len)) {
                /* Success. */
                *p_end = off + len;
                return TRUE;
            }
            off++;
        }

        line_index++;
        if(off >= max_end  ||  line_index >= n_lines) {
            /* Failure. */
            *p_scan_horizon = off;
            return FALSE;
        }

        off = lines[line_index].beg;
    }
}

static int
md_is_html_comment(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg;

    MD_ASSERT(CH(beg) == _T('<'));

    if(off + 4 >= lines[0].end)
        return FALSE;
    if(CH(off+1) != _T('!')  ||  CH(off+2) != _T('-')  ||  CH(off+3) != _T('-'))
        return FALSE;

    /* Skip only "<!" so that we accept also "<!-->" or "<!--->" */
    off += 2;

    /* Scan for ordinary comment closer "-->". */
    return md_scan_for_html_closer(ctx, _T("-->"), 3,
                lines, n_lines, off, max_end, p_end, &ctx->html_comment_horizon);
}

static int
md_is_html_processing_instruction(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg;

    if(off + 2 >= lines[0].end)
        return FALSE;
    if(CH(off+1) != _T('?'))
        return FALSE;
    off += 2;

    return md_scan_for_html_closer(ctx, _T("?>"), 2,
                lines, n_lines, off, max_end, p_end, &ctx->html_proc_instr_horizon);
}

static int
md_is_html_declaration(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg;

    if(off + 2 >= lines[0].end)
        return FALSE;
    if(CH(off+1) != _T('!'))
        return FALSE;
    off += 2;

    /* Declaration name. */
    if(off >= lines[0].end  ||  !ISALPHA(off))
        return FALSE;
    off++;
    while(off < lines[0].end  &&  ISALPHA(off))
        off++;

    return md_scan_for_html_closer(ctx, _T(">"), 1,
                lines, n_lines, off, max_end, p_end, &ctx->html_decl_horizon);
}

static int
md_is_html_cdata(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    static const CHAR open_str[] = _T("<![CDATA[");
    static const SZ open_size = SIZEOF_ARRAY(open_str) - 1;

    OFF off = beg;

    if(off + open_size >= lines[0].end)
        return FALSE;
    if(memcmp(STR(off), open_str, open_size) != 0)
        return FALSE;
    off += open_size;

    return md_scan_for_html_closer(ctx, _T("]]>"), 3,
                lines, n_lines, off, max_end, p_end, &ctx->html_cdata_horizon);
}

static int
md_is_html_any(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, OFF beg, OFF max_end, OFF* p_end)
{
    MD_ASSERT(CH(beg) == _T('<'));
    return (md_is_html_tag(ctx, lines, n_lines, beg, max_end, p_end)  ||
            md_is_html_comment(ctx, lines, n_lines, beg, max_end, p_end)  ||
            md_is_html_processing_instruction(ctx, lines, n_lines, beg, max_end, p_end)  ||
            md_is_html_declaration(ctx, lines, n_lines, beg, max_end, p_end)  ||
            md_is_html_cdata(ctx, lines, n_lines, beg, max_end, p_end));
}


/****************************
 ***  Recognizing Entity  ***
 ****************************/

static int
md_is_hex_entity_contents(MD_CTX* ctx, const CHAR* text, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg;
    MD_UNUSED(ctx);

    while(off < max_end  &&  ISXDIGIT_(text[off])  &&  off - beg <= 8)
        off++;

    if(1 <= off - beg  &&  off - beg <= 6) {
        *p_end = off;
        return TRUE;
    } else {
        return FALSE;
    }
}

static int
md_is_dec_entity_contents(MD_CTX* ctx, const CHAR* text, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg;
    MD_UNUSED(ctx);

    while(off < max_end  &&  ISDIGIT_(text[off])  &&  off - beg <= 8)
        off++;

    if(1 <= off - beg  &&  off - beg <= 7) {
        *p_end = off;
        return TRUE;
    } else {
        return FALSE;
    }
}

static int
md_is_named_entity_contents(MD_CTX* ctx, const CHAR* text, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg;
    MD_UNUSED(ctx);

    if(off < max_end  &&  ISALPHA_(text[off]))
        off++;
    else
        return FALSE;

    while(off < max_end  &&  ISALNUM_(text[off])  &&  off - beg <= 48)
        off++;

    if(2 <= off - beg  &&  off - beg <= 48) {
        *p_end = off;
        return TRUE;
    } else {
        return FALSE;
    }
}

static int
md_is_entity_str(MD_CTX* ctx, const CHAR* text, OFF beg, OFF max_end, OFF* p_end)
{
    int is_contents;
    OFF off = beg;

    MD_ASSERT(text[off] == _T('&'));
    off++;

    if(off+2 < max_end  &&  text[off] == _T('#')  &&  (text[off+1] == _T('x') || text[off+1] == _T('X')))
        is_contents = md_is_hex_entity_contents(ctx, text, off+2, max_end, &off);
    else if(off+1 < max_end  &&  text[off] == _T('#'))
        is_contents = md_is_dec_entity_contents(ctx, text, off+1, max_end, &off);
    else
        is_contents = md_is_named_entity_contents(ctx, text, off, max_end, &off);

    if(is_contents  &&  off < max_end  &&  text[off] == _T(';')) {
        *p_end = off+1;
        return TRUE;
    } else {
        return FALSE;
    }
}

static inline int
md_is_entity(MD_CTX* ctx, OFF beg, OFF max_end, OFF* p_end)
{
    return md_is_entity_str(ctx, ctx->text, beg, max_end, p_end);
}


/******************************
 ***  Attribute Management  ***
 ******************************/

typedef struct MD_ATTRIBUTE_BUILD_tag MD_ATTRIBUTE_BUILD;
struct MD_ATTRIBUTE_BUILD_tag {
    CHAR* text;
    MD_TEXTTYPE* substr_types;
    OFF* substr_offsets;
    int substr_count;
    int substr_alloc;
    MD_TEXTTYPE trivial_types[1];
    OFF trivial_offsets[2];
};


#define MD_BUILD_ATTR_NO_ESCAPES    0x0001

static int
md_build_attr_append_substr(MD_CTX* ctx, MD_ATTRIBUTE_BUILD* build,
                            MD_TEXTTYPE type, OFF off)
{
    if(build->substr_count >= build->substr_alloc) {
        MD_TEXTTYPE* new_substr_types;
        OFF* new_substr_offsets;

        build->substr_alloc = (build->substr_alloc > 0
                ? build->substr_alloc + build->substr_alloc / 2
                : 8);
        new_substr_types = (MD_TEXTTYPE*) realloc(build->substr_types,
                                    build->substr_alloc * sizeof(MD_TEXTTYPE));
        if(new_substr_types == NULL) {
            MD_LOG("realloc() failed.");
            return -1;
        }
        /* Note +1 to reserve space for final offset (== raw_size). */
        new_substr_offsets = (OFF*) realloc(build->substr_offsets,
                                    (build->substr_alloc+1) * sizeof(OFF));
        if(new_substr_offsets == NULL) {
            MD_LOG("realloc() failed.");
            free(new_substr_types);
            return -1;
        }

        build->substr_types = new_substr_types;
        build->substr_offsets = new_substr_offsets;
    }

    build->substr_types[build->substr_count] = type;
    build->substr_offsets[build->substr_count] = off;
    build->substr_count++;
    return 0;
}

static void
md_free_attribute(MD_CTX* ctx, MD_ATTRIBUTE_BUILD* build)
{
    MD_UNUSED(ctx);

    if(build->substr_alloc > 0) {
        free(build->text);
        free(build->substr_types);
        free(build->substr_offsets);
    }
}

static int
md_build_attribute(MD_CTX* ctx, const CHAR* raw_text, SZ raw_size,
                   unsigned flags, MD_ATTRIBUTE* attr, MD_ATTRIBUTE_BUILD* build)
{
    OFF raw_off, off;
    int is_trivial;
    int ret = 0;

    memset(build, 0, sizeof(MD_ATTRIBUTE_BUILD));

    /* If there is no backslash and no ampersand, build trivial attribute
     * without any malloc(). */
    is_trivial = TRUE;
    for(raw_off = 0; raw_off < raw_size; raw_off++) {
        if(ISANYOF3_(raw_text[raw_off], _T('\\'), _T('&'), _T('\0'))) {
            is_trivial = FALSE;
            break;
        }
    }

    if(is_trivial) {
        build->text = (CHAR*) (raw_size ? raw_text : NULL);
        build->substr_types = build->trivial_types;
        build->substr_offsets = build->trivial_offsets;
        build->substr_count = 1;
        build->substr_alloc = 0;
        build->trivial_types[0] = MD_TEXT_NORMAL;
        build->trivial_offsets[0] = 0;
        build->trivial_offsets[1] = raw_size;
        off = raw_size;
    } else {
        build->text = (CHAR*) malloc(raw_size * sizeof(CHAR));
        if(build->text == NULL) {
            MD_LOG("malloc() failed.");
            goto abort;
        }

        raw_off = 0;
        off = 0;

        while(raw_off < raw_size) {
            if(raw_text[raw_off] == _T('\0')) {
                MD_CHECK(md_build_attr_append_substr(ctx, build, MD_TEXT_NULLCHAR, off));
                memcpy(build->text + off, raw_text + raw_off, 1);
                off++;
                raw_off++;
                continue;
            }

            if(raw_text[raw_off] == _T('&')) {
                OFF ent_end;

                if(md_is_entity_str(ctx, raw_text, raw_off, raw_size, &ent_end)) {
                    MD_CHECK(md_build_attr_append_substr(ctx, build, MD_TEXT_ENTITY, off));
                    memcpy(build->text + off, raw_text + raw_off, ent_end - raw_off);
                    off += ent_end - raw_off;
                    raw_off = ent_end;
                    continue;
                }
            }

            if(build->substr_count == 0  ||  build->substr_types[build->substr_count-1] != MD_TEXT_NORMAL)
                MD_CHECK(md_build_attr_append_substr(ctx, build, MD_TEXT_NORMAL, off));

            if(!(flags & MD_BUILD_ATTR_NO_ESCAPES)  &&
               raw_text[raw_off] == _T('\\')  &&  raw_off+1 < raw_size  &&
               (ISPUNCT_(raw_text[raw_off+1]) || ISNEWLINE_(raw_text[raw_off+1])))
                raw_off++;

            build->text[off++] = raw_text[raw_off++];
        }
        build->substr_offsets[build->substr_count] = off;
    }

    attr->text = build->text;
    attr->size = off;
    attr->substr_offsets = build->substr_offsets;
    attr->substr_types = build->substr_types;
    return 0;

abort:
    md_free_attribute(ctx, build);
    return -1;
}


/*********************************************
 ***  Dictionary of Reference Definitions  ***
 *********************************************/

#define MD_FNV1A_BASE       2166136261U
#define MD_FNV1A_PRIME      16777619U

static inline unsigned
md_fnv1a(unsigned base, const void* data, size_t n)
{
    const unsigned char* buf = (const unsigned char*) data;
    unsigned hash = base;
    size_t i;

    for(i = 0; i < n; i++) {
        hash ^= buf[i];
        hash *= MD_FNV1A_PRIME;
    }

    return hash;
}


struct MD_REF_DEF_tag {
    CHAR* label;
    CHAR* title;
    unsigned hash;
    SZ label_size;
    SZ title_size;
    OFF dest_beg;
    OFF dest_end;
    unsigned char label_needs_free : 1;
    unsigned char title_needs_free : 1;
};

/* Label equivalence is quite complicated with regards to whitespace and case
 * folding. This complicates computing a hash of it as well as direct comparison
 * of two labels. */

static unsigned
md_link_label_hash(const CHAR* label, SZ size)
{
    unsigned hash = MD_FNV1A_BASE;
    OFF off;
    unsigned codepoint;
    int is_whitespace = FALSE;

    off = md_skip_unicode_whitespace(label, 0, size);
    while(off < size) {
        SZ char_size;

        codepoint = md_decode_unicode(label, off, size, &char_size);
        is_whitespace = ISUNICODEWHITESPACE_(codepoint) || ISNEWLINE_(label[off]);

        if(is_whitespace) {
            codepoint = ' ';
            hash = md_fnv1a(hash, &codepoint, sizeof(unsigned));
            off = md_skip_unicode_whitespace(label, off, size);
        } else {
            MD_UNICODE_FOLD_INFO fold_info;

            md_get_unicode_fold_info(codepoint, &fold_info);
            hash = md_fnv1a(hash, fold_info.codepoints, fold_info.n_codepoints * sizeof(unsigned));
            off += char_size;
        }
    }

    return hash;
}

static OFF
md_link_label_cmp_load_fold_info(const CHAR* label, OFF off, SZ size,
                                 MD_UNICODE_FOLD_INFO* fold_info)
{
    unsigned codepoint;
    SZ char_size;

    if(off >= size) {
        /* Treat end of a link label as a whitespace. */
        goto whitespace;
    }

    codepoint = md_decode_unicode(label, off, size, &char_size);
    off += char_size;
    if(ISUNICODEWHITESPACE_(codepoint)) {
        /* Treat all whitespace as equivalent */
        goto whitespace;
    }

    /* Get real folding info. */
    md_get_unicode_fold_info(codepoint, fold_info);
    return off;

whitespace:
    fold_info->codepoints[0] = _T(' ');
    fold_info->n_codepoints = 1;
    return md_skip_unicode_whitespace(label, off, size);
}

static int
md_link_label_cmp(const CHAR* a_label, SZ a_size, const CHAR* b_label, SZ b_size)
{
    OFF a_off;
    OFF b_off;
    MD_UNICODE_FOLD_INFO a_fi = { { 0 }, 0 };
    MD_UNICODE_FOLD_INFO b_fi = { { 0 }, 0 };
    OFF a_fi_off = 0;
    OFF b_fi_off = 0;
    int cmp;

    a_off = md_skip_unicode_whitespace(a_label, 0, a_size);
    b_off = md_skip_unicode_whitespace(b_label, 0, b_size);
    while(a_off < a_size || a_fi_off < a_fi.n_codepoints ||
          b_off < b_size || b_fi_off < b_fi.n_codepoints)
    {
        /* If needed, load fold info for next char. */
        if(a_fi_off >= a_fi.n_codepoints) {
            a_fi_off = 0;
            a_off = md_link_label_cmp_load_fold_info(a_label, a_off, a_size, &a_fi);
        }
        if(b_fi_off >= b_fi.n_codepoints) {
            b_fi_off = 0;
            b_off = md_link_label_cmp_load_fold_info(b_label, b_off, b_size, &b_fi);
        }

        cmp = b_fi.codepoints[b_fi_off] - a_fi.codepoints[a_fi_off];
        if(cmp != 0)
            return cmp;

        a_fi_off++;
        b_fi_off++;
    }

    return 0;
}

typedef struct MD_REF_DEF_LIST_tag MD_REF_DEF_LIST;
struct MD_REF_DEF_LIST_tag {
    int n_ref_defs;
    int alloc_ref_defs;
    MD_REF_DEF* ref_defs[];  /* Valid items always  point into ctx->ref_defs[] */
};

static int
md_ref_def_cmp(const void* a, const void* b)
{
    const MD_REF_DEF* a_ref = *(const MD_REF_DEF**)a;
    const MD_REF_DEF* b_ref = *(const MD_REF_DEF**)b;

    if(a_ref->hash < b_ref->hash)
        return -1;
    else if(a_ref->hash > b_ref->hash)
        return +1;
    else
        return md_link_label_cmp(a_ref->label, a_ref->label_size, b_ref->label, b_ref->label_size);
}

static int
md_ref_def_cmp_for_sort(const void* a, const void* b)
{
    int cmp;

    cmp = md_ref_def_cmp(a, b);

    /* Ensure stability of the sorting. */
    if(cmp == 0) {
        const MD_REF_DEF* a_ref = *(const MD_REF_DEF**)a;
        const MD_REF_DEF* b_ref = *(const MD_REF_DEF**)b;

        if(a_ref < b_ref)
            cmp = -1;
        else if(a_ref > b_ref)
            cmp = +1;
        else
            cmp = 0;
    }

    return cmp;
}

static int
md_build_ref_def_hashtable(MD_CTX* ctx)
{
    int i, j;

    if(ctx->n_ref_defs == 0)
        return 0;

    ctx->ref_def_hashtable_size = (ctx->n_ref_defs * 5) / 4;
    ctx->ref_def_hashtable = malloc(ctx->ref_def_hashtable_size * sizeof(void*));
    if(ctx->ref_def_hashtable == NULL) {
        MD_LOG("malloc() failed.");
        goto abort;
    }
    memset(ctx->ref_def_hashtable, 0, ctx->ref_def_hashtable_size * sizeof(void*));

    /* Each member of ctx->ref_def_hashtable[] can be:
     *  -- NULL,
     *  -- pointer to the MD_REF_DEF in ctx->ref_defs[], or
     *  -- pointer to a MD_REF_DEF_LIST, which holds multiple pointers to
     *     such MD_REF_DEFs.
     */
    for(i = 0; i < ctx->n_ref_defs; i++) {
        MD_REF_DEF* def = &ctx->ref_defs[i];
        void* bucket;
        MD_REF_DEF_LIST* list;

        def->hash = md_link_label_hash(def->label, def->label_size);
        bucket = ctx->ref_def_hashtable[def->hash % ctx->ref_def_hashtable_size];

        if(bucket == NULL) {
            /* The bucket is empty. Make it just point to the def. */
            ctx->ref_def_hashtable[def->hash % ctx->ref_def_hashtable_size] = def;
            continue;
        }

        if(ctx->ref_defs <= (MD_REF_DEF*) bucket  &&  (MD_REF_DEF*) bucket < ctx->ref_defs + ctx->n_ref_defs) {
            /* The bucket already contains one ref. def. Lets see whether it
             * is the same label (ref. def. duplicate) or different one
             * (hash conflict). */
            MD_REF_DEF* old_def = (MD_REF_DEF*) bucket;

            if(md_link_label_cmp(def->label, def->label_size, old_def->label, old_def->label_size) == 0) {
                /* Duplicate label: Ignore this ref. def. */
                continue;
            }

            /* Make the bucket complex, i.e. able to hold more ref. defs. */
            list = (MD_REF_DEF_LIST*) malloc(sizeof(MD_REF_DEF_LIST) + 2 * sizeof(MD_REF_DEF*));
            if(list == NULL) {
                MD_LOG("malloc() failed.");
                goto abort;
            }
            list->ref_defs[0] = old_def;
            list->ref_defs[1] = def;
            list->n_ref_defs = 2;
            list->alloc_ref_defs = 2;
            ctx->ref_def_hashtable[def->hash % ctx->ref_def_hashtable_size] = list;
            continue;
        }

        /* Append the def to the complex bucket list.
         *
         * Note in this case we ignore potential duplicates to avoid expensive
         * iterating over the complex bucket. Below, we revisit all the complex
         * buckets and handle it more cheaply after the complex bucket contents
         * is sorted. */
        list = (MD_REF_DEF_LIST*) bucket;
        if(list->n_ref_defs >= list->alloc_ref_defs) {
            int alloc_ref_defs = list->alloc_ref_defs + list->alloc_ref_defs / 2;
            MD_REF_DEF_LIST* list_tmp = (MD_REF_DEF_LIST*) realloc(list,
                        sizeof(MD_REF_DEF_LIST) + alloc_ref_defs * sizeof(MD_REF_DEF*));
            if(list_tmp == NULL) {
                MD_LOG("realloc() failed.");
                goto abort;
            }
            list = list_tmp;
            list->alloc_ref_defs = alloc_ref_defs;
            ctx->ref_def_hashtable[def->hash % ctx->ref_def_hashtable_size] = list;
        }

        list->ref_defs[list->n_ref_defs] = def;
        list->n_ref_defs++;
    }

    /* Sort the complex buckets so we can use bsearch() with them. */
    for(i = 0; i < ctx->ref_def_hashtable_size; i++) {
        void* bucket = ctx->ref_def_hashtable[i];
        MD_REF_DEF_LIST* list;

        if(bucket == NULL)
            continue;
        if(ctx->ref_defs <= (MD_REF_DEF*) bucket  &&  (MD_REF_DEF*) bucket < ctx->ref_defs + ctx->n_ref_defs)
            continue;

        list = (MD_REF_DEF_LIST*) bucket;
        qsort(list->ref_defs, list->n_ref_defs, sizeof(MD_REF_DEF*), md_ref_def_cmp_for_sort);

        /* Disable all duplicates in the complex bucket by forcing all such
         * records to point to the 1st such ref. def. I.e. no matter which
         * record is found during the lookup, it will always point to the right
         * ref. def. in ctx->ref_defs[]. */
        for(j = 1; j < list->n_ref_defs; j++) {
            if(md_ref_def_cmp(&list->ref_defs[j-1], &list->ref_defs[j]) == 0)
                list->ref_defs[j] = list->ref_defs[j-1];
        }
    }

    return 0;

abort:
    return -1;
}

static void
md_free_ref_def_hashtable(MD_CTX* ctx)
{
    if(ctx->ref_def_hashtable != NULL) {
        int i;

        for(i = 0; i < ctx->ref_def_hashtable_size; i++) {
            void* bucket = ctx->ref_def_hashtable[i];
            if(bucket == NULL)
                continue;
            if(ctx->ref_defs <= (MD_REF_DEF*) bucket  &&  (MD_REF_DEF*) bucket < ctx->ref_defs + ctx->n_ref_defs)
                continue;
            free(bucket);
        }

        free(ctx->ref_def_hashtable);
    }
}

static const MD_REF_DEF*
md_lookup_ref_def(MD_CTX* ctx, const CHAR* label, SZ label_size)
{
    unsigned hash;
    void* bucket;

    if(ctx->ref_def_hashtable_size == 0)
        return NULL;

    hash = md_link_label_hash(label, label_size);
    bucket = ctx->ref_def_hashtable[hash % ctx->ref_def_hashtable_size];

    if(bucket == NULL) {
        return NULL;
    } else if(ctx->ref_defs <= (MD_REF_DEF*) bucket  &&  (MD_REF_DEF*) bucket < ctx->ref_defs + ctx->n_ref_defs) {
        const MD_REF_DEF* def = (MD_REF_DEF*) bucket;

        if(md_link_label_cmp(def->label, def->label_size, label, label_size) == 0)
            return def;
        else
            return NULL;
    } else {
        MD_REF_DEF_LIST* list = (MD_REF_DEF_LIST*) bucket;
        MD_REF_DEF key_buf;
        const MD_REF_DEF* key = &key_buf;
        const MD_REF_DEF** ret;

        key_buf.label = (CHAR*) label;
        key_buf.label_size = label_size;
        key_buf.hash = md_link_label_hash(key_buf.label, key_buf.label_size);

        ret = (const MD_REF_DEF**) bsearch(&key, list->ref_defs,
                    list->n_ref_defs, sizeof(MD_REF_DEF*), md_ref_def_cmp);
        if(ret != NULL)
            return *ret;
        else
            return NULL;
    }
}


/***************************
 ***  Recognizing Links  ***
 ***************************/

/* Note this code is partially shared between processing inlines and blocks
 * as reference definitions and links share some helper parser functions.
 */

typedef struct MD_LINK_ATTR_tag MD_LINK_ATTR;
struct MD_LINK_ATTR_tag {
    OFF dest_beg;
    OFF dest_end;

    CHAR* title;
    SZ title_size;
    int title_needs_free;
};


static int
md_is_link_label(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, OFF beg,
                 OFF* p_end, MD_SIZE* p_beg_line_index, MD_SIZE* p_end_line_index,
                 OFF* p_contents_beg, OFF* p_contents_end)
{
    OFF off = beg;
    OFF contents_beg = 0;
    OFF contents_end = 0;
    MD_SIZE line_index = 0;
    int len = 0;

    *p_beg_line_index = 0;

    if(CH(off) != _T('['))
        return FALSE;
    off++;

    while(1) {
        OFF line_end = lines[line_index].end;

        while(off < line_end) {
            if(CH(off) == _T('\\')  &&  off+1 < ctx->size  &&  (ISPUNCT(off+1) || ISNEWLINE(off+1))) {
                if(contents_end == 0) {
                    contents_beg = off;
                    *p_beg_line_index = line_index;
                }
                contents_end = off + 2;
                off += 2;
            } else if(CH(off) == _T('[')) {
                return FALSE;
            } else if(CH(off) == _T(']')) {
                if(contents_beg < contents_end) {
                    /* Success. */
                    *p_contents_beg = contents_beg;
                    *p_contents_end = contents_end;
                    *p_end = off+1;
                    *p_end_line_index = line_index;
                    return TRUE;
                } else {
                    /* Link label must have some non-whitespace contents. */
                    return FALSE;
                }
            } else {
                unsigned codepoint;
                SZ char_size;

                codepoint = md_decode_unicode(ctx->text, off, ctx->size, &char_size);
                if(!ISUNICODEWHITESPACE_(codepoint)) {
                    if(contents_end == 0) {
                        contents_beg = off;
                        *p_beg_line_index = line_index;
                    }
                    contents_end = off + char_size;
                }

                off += char_size;
            }

            len++;
            if(len > 999)
                return FALSE;
        }

        line_index++;
        len++;
        if(line_index < n_lines)
            off = lines[line_index].beg;
        else
            break;
    }

    return FALSE;
}

static int
md_is_link_destination_A(MD_CTX* ctx, OFF beg, OFF max_end, OFF* p_end,
                         OFF* p_contents_beg, OFF* p_contents_end)
{
    OFF off = beg;

    if(off >= max_end  ||  CH(off) != _T('<'))
        return FALSE;
    off++;

    while(off < max_end) {
        if(CH(off) == _T('\\')  &&  off+1 < max_end  &&  ISPUNCT(off+1)) {
            off += 2;
            continue;
        }

        if(ISNEWLINE(off)  ||  CH(off) == _T('<'))
            return FALSE;

        if(CH(off) == _T('>')) {
            /* Success. */
            *p_contents_beg = beg+1;
            *p_contents_end = off;
            *p_end = off+1;
            return TRUE;
        }

        off++;
    }

    return FALSE;
}

static int
md_is_link_destination_B(MD_CTX* ctx, OFF beg, OFF max_end, OFF* p_end,
                         OFF* p_contents_beg, OFF* p_contents_end)
{
    OFF off = beg;
    int parenthesis_level = 0;

    while(off < max_end) {
        if(CH(off) == _T('\\')  &&  off+1 < max_end  &&  ISPUNCT(off+1)) {
            off += 2;
            continue;
        }

        if(ISWHITESPACE(off) || ISCNTRL(off))
            break;

        /* Link destination may include balanced pairs of unescaped '(' ')'.
         * Note we limit the maximal nesting level by 32 to protect us from
         * https://github.com/jgm/cmark/issues/214 */
        if(CH(off) == _T('(')) {
            parenthesis_level++;
            if(parenthesis_level > 32)
                return FALSE;
        } else if(CH(off) == _T(')')) {
            if(parenthesis_level == 0)
                break;
            parenthesis_level--;
        }

        off++;
    }

    if(parenthesis_level != 0  ||  off == beg)
        return FALSE;

    /* Success. */
    *p_contents_beg = beg;
    *p_contents_end = off;
    *p_end = off;
    return TRUE;
}

static inline int
md_is_link_destination(MD_CTX* ctx, OFF beg, OFF max_end, OFF* p_end,
                       OFF* p_contents_beg, OFF* p_contents_end)
{
    if(CH(beg) == _T('<'))
        return md_is_link_destination_A(ctx, beg, max_end, p_end, p_contents_beg, p_contents_end);
    else
        return md_is_link_destination_B(ctx, beg, max_end, p_end, p_contents_beg, p_contents_end);
}

static int
md_is_link_title(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, OFF beg,
                 OFF* p_end, MD_SIZE* p_beg_line_index, MD_SIZE* p_end_line_index,
                 OFF* p_contents_beg, OFF* p_contents_end)
{
    OFF off = beg;
    CHAR closer_char;
    MD_SIZE line_index = 0;

    /* White space with up to one line break. */
    while(off < lines[line_index].end  &&  ISWHITESPACE(off))
        off++;
    if(off >= lines[line_index].end) {
        line_index++;
        if(line_index >= n_lines)
            return FALSE;
        off = lines[line_index].beg;
    }
    if(off == beg)
        return FALSE;

    *p_beg_line_index = line_index;

    /* First char determines how to detect end of it. */
    switch(CH(off)) {
        case _T('"'):   closer_char = _T('"'); break;
        case _T('\''):  closer_char = _T('\''); break;
        case _T('('):   closer_char = _T(')'); break;
        default:        return FALSE;
    }
    off++;

    *p_contents_beg = off;

    while(line_index < n_lines) {
        OFF line_end = lines[line_index].end;

        while(off < line_end) {
            if(CH(off) == _T('\\')  &&  off+1 < ctx->size  &&  (ISPUNCT(off+1) || ISNEWLINE(off+1))) {
                off++;
            } else if(CH(off) == closer_char) {
                /* Success. */
                *p_contents_end = off;
                *p_end = off+1;
                *p_end_line_index = line_index;
                return TRUE;
            } else if(closer_char == _T(')')  &&  CH(off) == _T('(')) {
                /* ()-style title cannot contain (unescaped '(')) */
                return FALSE;
            }

            off++;
        }

        line_index++;
    }

    return FALSE;
}

/* Returns 0 if it is not a reference definition.
 *
 * Returns N > 0 if it is a reference definition. N then corresponds to the
 * number of lines forming it). In this case the definition is stored for
 * resolving any links referring to it.
 *
 * Returns -1 in case of an error (out of memory).
 */
static int
md_is_link_reference_definition(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines)
{
    OFF label_contents_beg;
    OFF label_contents_end;
    MD_SIZE label_contents_line_index;
    int label_is_multiline = FALSE;
    OFF dest_contents_beg;
    OFF dest_contents_end;
    OFF title_contents_beg;
    OFF title_contents_end;
    MD_SIZE title_contents_line_index;
    int title_is_multiline = FALSE;
    OFF off;
    MD_SIZE line_index = 0;
    MD_SIZE tmp_line_index;
    MD_REF_DEF* def = NULL;
    int ret = 0;

    /* Link label. */
    if(!md_is_link_label(ctx, lines, n_lines, lines[0].beg,
                &off, &label_contents_line_index, &line_index,
                &label_contents_beg, &label_contents_end))
        return FALSE;
    label_is_multiline = (label_contents_line_index != line_index);

    /* Colon. */
    if(off >= lines[line_index].end  ||  CH(off) != _T(':'))
        return FALSE;
    off++;

    /* Optional white space with up to one line break. */
    while(off < lines[line_index].end  &&  ISWHITESPACE(off))
        off++;
    if(off >= lines[line_index].end) {
        line_index++;
        if(line_index >= n_lines)
            return FALSE;
        off = lines[line_index].beg;
    }

    /* Link destination. */
    if(!md_is_link_destination(ctx, off, lines[line_index].end,
                &off, &dest_contents_beg, &dest_contents_end))
        return FALSE;

    /* (Optional) title. Note we interpret it as an title only if nothing
     * more follows on its last line. */
    if(md_is_link_title(ctx, lines + line_index, n_lines - line_index, off,
                &off, &title_contents_line_index, &tmp_line_index,
                &title_contents_beg, &title_contents_end)
        &&  off >= lines[line_index + tmp_line_index].end)
    {
        title_is_multiline = (tmp_line_index != title_contents_line_index);
        title_contents_line_index += line_index;
        line_index += tmp_line_index;
    } else {
        /* Not a title. */
        title_is_multiline = FALSE;
        title_contents_beg = off;
        title_contents_end = off;
        title_contents_line_index = 0;
    }

    /* Nothing more can follow on the last line. */
    if(off < lines[line_index].end)
        return FALSE;

    /* So, it _is_ a reference definition. Remember it. */
    if(ctx->n_ref_defs >= ctx->alloc_ref_defs) {
        MD_REF_DEF* new_defs;

        ctx->alloc_ref_defs = (ctx->alloc_ref_defs > 0
                ? ctx->alloc_ref_defs + ctx->alloc_ref_defs / 2
                : 16);
        new_defs = (MD_REF_DEF*) realloc(ctx->ref_defs, ctx->alloc_ref_defs * sizeof(MD_REF_DEF));
        if(new_defs == NULL) {
            MD_LOG("realloc() failed.");
            goto abort;
        }

        ctx->ref_defs = new_defs;
    }
    def = &ctx->ref_defs[ctx->n_ref_defs];
    memset(def, 0, sizeof(MD_REF_DEF));

    if(label_is_multiline) {
        MD_CHECK(md_merge_lines_alloc(ctx, label_contents_beg, label_contents_end,
                    lines + label_contents_line_index, n_lines - label_contents_line_index,
                    _T(' '), &def->label, &def->label_size));
        def->label_needs_free = TRUE;
    } else {
        def->label = (CHAR*) STR(label_contents_beg);
        def->label_size = label_contents_end - label_contents_beg;
    }

    if(title_is_multiline) {
        MD_CHECK(md_merge_lines_alloc(ctx, title_contents_beg, title_contents_end,
                    lines + title_contents_line_index, n_lines - title_contents_line_index,
                    _T('\n'), &def->title, &def->title_size));
        def->title_needs_free = TRUE;
    } else {
        def->title = (CHAR*) STR(title_contents_beg);
        def->title_size = title_contents_end - title_contents_beg;
    }

    def->dest_beg = dest_contents_beg;
    def->dest_end = dest_contents_end;

    /* Success. */
    ctx->n_ref_defs++;
    return line_index + 1;

abort:
    /* Failure. */
    if(def != NULL  &&  def->label_needs_free)
        free(def->label);
    if(def != NULL  &&  def->title_needs_free)
        free(def->title);
    return ret;
}

static int
md_is_link_reference(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines,
                     OFF beg, OFF end, MD_LINK_ATTR* attr)
{
    const MD_REF_DEF* def;
    const MD_LINE* beg_line;
    int is_multiline;
    CHAR* label;
    SZ label_size;
    int ret = FALSE;

    MD_ASSERT(CH(beg) == _T('[') || CH(beg) == _T('!'));
    MD_ASSERT(CH(end-1) == _T(']'));

    if(ctx->max_ref_def_output == 0)
        return FALSE;

    beg += (CH(beg) == _T('!') ? 2 : 1);
    end--;

    /* Find lines corresponding to the beg and end positions. */
    beg_line = md_lookup_line(beg, lines, n_lines, NULL);
    is_multiline = (end > beg_line->end);

    if(is_multiline) {
        MD_CHECK(md_merge_lines_alloc(ctx, beg, end, beg_line,
                 (int)(n_lines - (beg_line - lines)), _T(' '), &label, &label_size));
    } else {
        label = (CHAR*) STR(beg);
        label_size = end - beg;
    }

    def = md_lookup_ref_def(ctx, label, label_size);
    if(def != NULL) {
        attr->dest_beg = def->dest_beg;
        attr->dest_end = def->dest_end;
        attr->title = def->title;
        attr->title_size = def->title_size;
        attr->title_needs_free = FALSE;
    }

    if(is_multiline)
        free(label);

    if(def != NULL) {
        /* See https://github.com/mity/md4c/issues/238 */
        MD_SIZE output_size_estimation = def->label_size + def->title_size + def->dest_end - def->dest_beg;
        if(output_size_estimation < ctx->max_ref_def_output) {
            ctx->max_ref_def_output -= output_size_estimation;
            ret = TRUE;
        } else {
            MD_LOG("Too many link reference definition instantiations.");
            ctx->max_ref_def_output = 0;
        }
    }

abort:
    return ret;
}

static int
md_is_inline_link_spec(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines,
                       OFF beg, OFF* p_end, MD_LINK_ATTR* attr)
{
    MD_SIZE line_index = 0;
    MD_SIZE tmp_line_index;
    OFF title_contents_beg;
    OFF title_contents_end;
    MD_SIZE title_contents_line_index;
    int title_is_multiline;
    OFF off = beg;
    int ret = FALSE;

    md_lookup_line(off, lines, n_lines, &line_index);

    MD_ASSERT(CH(off) == _T('('));
    off++;

    /* Optional white space with up to one line break. */
    while(off < lines[line_index].end  &&  ISWHITESPACE(off))
        off++;
    if(off >= lines[line_index].end  &&  (off >= ctx->size  ||  ISNEWLINE(off))) {
        line_index++;
        if(line_index >= n_lines)
            return FALSE;
        off = lines[line_index].beg;
    }

    /* Link destination may be omitted, but only when not also having a title. */
    if(off < ctx->size  &&  CH(off) == _T(')')) {
        attr->dest_beg = off;
        attr->dest_end = off;
        attr->title = NULL;
        attr->title_size = 0;
        attr->title_needs_free = FALSE;
        off++;
        *p_end = off;
        return TRUE;
    }

    /* Link destination. */
    if(!md_is_link_destination(ctx, off, lines[line_index].end,
                        &off, &attr->dest_beg, &attr->dest_end))
        return FALSE;

    /* (Optional) title. */
    if(md_is_link_title(ctx, lines + line_index, n_lines - line_index, off,
                &off, &title_contents_line_index, &tmp_line_index,
                &title_contents_beg, &title_contents_end))
    {
        title_is_multiline = (tmp_line_index != title_contents_line_index);
        title_contents_line_index += line_index;
        line_index += tmp_line_index;
    } else {
        /* Not a title. */
        title_is_multiline = FALSE;
        title_contents_beg = off;
        title_contents_end = off;
        title_contents_line_index = 0;
    }

    /* Optional whitespace followed with final ')'. */
    while(off < lines[line_index].end  &&  ISWHITESPACE(off))
        off++;
    if(off >= lines[line_index].end) {
        line_index++;
        if(line_index >= n_lines)
            return FALSE;
        off = lines[line_index].beg;
    }
    if(CH(off) != _T(')'))
        goto abort;
    off++;

    if(title_contents_beg >= title_contents_end) {
        attr->title = NULL;
        attr->title_size = 0;
        attr->title_needs_free = FALSE;
    } else if(!title_is_multiline) {
        attr->title = (CHAR*) STR(title_contents_beg);
        attr->title_size = title_contents_end - title_contents_beg;
        attr->title_needs_free = FALSE;
    } else {
        MD_CHECK(md_merge_lines_alloc(ctx, title_contents_beg, title_contents_end,
                    lines + title_contents_line_index, n_lines - title_contents_line_index,
                    _T('\n'), &attr->title, &attr->title_size));
        attr->title_needs_free = TRUE;
    }

    *p_end = off;
    ret = TRUE;

abort:
    return ret;
}

static void
md_free_ref_defs(MD_CTX* ctx)
{
    int i;

    for(i = 0; i < ctx->n_ref_defs; i++) {
        MD_REF_DEF* def = &ctx->ref_defs[i];

        if(def->label_needs_free)
            free(def->label);
        if(def->title_needs_free)
            free(def->title);
    }

    free(ctx->ref_defs);
}


/******************************************
 ***  Processing Inlines (a.k.a Spans)  ***
 ******************************************/

/* We process inlines in few phases:
 *
 * (1) We go through the block text and collect all significant characters
 *     which may start/end a span or some other significant position into
 *     ctx->marks[]. Core of this is what md_collect_marks() does.
 *
 *     We also do some very brief preliminary context-less analysis, whether
 *     it might be opener or closer (e.g. of an emphasis span).
 *
 *     This speeds the other steps as we do not need to re-iterate over all
 *     characters anymore.
 *
 * (2) We analyze each potential mark types, in order by their precedence.
 *
 *     In each md_analyze_XXX() function, we re-iterate list of the marks,
 *     skipping already resolved regions (in preceding precedences) and try to
 *     resolve them.
 *
 * (2.1) For trivial marks, which are single (e.g. HTML entity), we just mark
 *       them as resolved.
 *
 * (2.2) For range-type marks, we analyze whether the mark could be closer
 *       and, if yes, whether there is some preceding opener it could satisfy.
 *
 *       If not we check whether it could be really an opener and if yes, we
 *       remember it so subsequent closers may resolve it.
 *
 * (3) Finally, when all marks were analyzed, we render the block contents
 *     by calling MD_RENDERER::text() callback, interrupting by ::enter_span()
 *     or ::close_span() whenever we reach a resolved mark.
 */


/* The mark structure.
 *
 * '\\': Maybe escape sequence.
 * '\0': NULL char.
 *  '*': Maybe (strong) emphasis start/end.
 *  '_': Maybe (strong) emphasis start/end.
 *  '~': Maybe strikethrough start/end (needs MD_FLAG_STRIKETHROUGH).
 *  '`': Maybe code span start/end.
 *  '&': Maybe start of entity.
 *  ';': Maybe end of entity.
 *  '<': Maybe start of raw HTML or autolink.
 *  '>': Maybe end of raw HTML or autolink.
 *  '[': Maybe start of link label or link text.
 *  '!': Equivalent of '[' for image.
 *  ']': Maybe end of link label or link text.
 *  '@': Maybe permissive e-mail auto-link (needs MD_FLAG_PERMISSIVEEMAILAUTOLINKS).
 *  ':': Maybe permissive URL auto-link (needs MD_FLAG_PERMISSIVEURLAUTOLINKS).
 *  '.': Maybe permissive WWW auto-link (needs MD_FLAG_PERMISSIVEWWWAUTOLINKS).
 *  'D': Dummy mark, it reserves a space for splitting a previous mark
 *       (e.g. emphasis) or to make more space for storing some special data
 *       related to the preceding mark (e.g. link).
 *
 * Note that not all instances of these chars in the text imply creation of the
 * structure. Only those which have (or may have, after we see more context)
 * the special meaning.
 *
 * (Keep this struct as small as possible to fit as much of them into CPU
 * cache line.)
 */
struct MD_MARK_tag {
    OFF beg;
    OFF end;

    /* For unresolved openers, 'next' may be used to form a stack of
     * unresolved open openers.
     *
     * When resolved with MD_MARK_OPENER/CLOSER flag, next/prev is index of the
     * respective closer/opener.
     */
    int prev;
    int next;
    CHAR ch;
    unsigned char flags;
};

/* Mark flags (these apply to ALL mark types). */
#define MD_MARK_POTENTIAL_OPENER            0x01  /* Maybe opener. */
#define MD_MARK_POTENTIAL_CLOSER            0x02  /* Maybe closer. */
#define MD_MARK_OPENER                      0x04  /* Definitely opener. */
#define MD_MARK_CLOSER                      0x08  /* Definitely closer. */
#define MD_MARK_RESOLVED                    0x10  /* Resolved in any definite way. */

/* Mark flags specific for various mark types (so they can share bits). */
#define MD_MARK_EMPH_OC                     0x20  /* Opener/closer mixed candidate. Helper for the "rule of 3". */
#define MD_MARK_EMPH_MOD3_0                 0x40
#define MD_MARK_EMPH_MOD3_1                 0x80
#define MD_MARK_EMPH_MOD3_2                 (0x40 | 0x80)
#define MD_MARK_EMPH_MOD3_MASK              (0x40 | 0x80)
#define MD_MARK_AUTOLINK                    0x20  /* Distinguisher for '<', '>'. */
#define MD_MARK_AUTOLINK_MISSING_MAILTO     0x40
#define MD_MARK_VALIDPERMISSIVEAUTOLINK     0x20  /* For permissive autolinks. */
#define MD_MARK_HASNESTEDBRACKETS           0x20  /* For '[' to rule out invalid link labels early */

static MD_MARKSTACK*
md_emph_stack(MD_CTX* ctx, MD_CHAR ch, unsigned flags)
{
    MD_MARKSTACK* stack;

    switch(ch) {
        case '*':   stack = &ASTERISK_OPENERS_oo_mod3_0; break;
        case '_':   stack = &UNDERSCORE_OPENERS_oo_mod3_0; break;
        default:    MD_UNREACHABLE();
    }

    if(flags & MD_MARK_EMPH_OC)
        stack += 3;

    switch(flags & MD_MARK_EMPH_MOD3_MASK) {
        case MD_MARK_EMPH_MOD3_0:   stack += 0; break;
        case MD_MARK_EMPH_MOD3_1:   stack += 1; break;
        case MD_MARK_EMPH_MOD3_2:   stack += 2; break;
        default:                    MD_UNREACHABLE();
    }

    return stack;
}

static MD_MARKSTACK*
md_opener_stack(MD_CTX* ctx, int mark_index)
{
    MD_MARK* mark = &ctx->marks[mark_index];

    switch(mark->ch) {
        case _T('*'):
        case _T('_'):   return md_emph_stack(ctx, mark->ch, mark->flags);

        case _T('~'):   return (mark->end - mark->beg == 1) ? &TILDE_OPENERS_1 : &TILDE_OPENERS_2;

        case _T('!'):
        case _T('['):   return &BRACKET_OPENERS;

        default:        MD_UNREACHABLE();
    }
}

static MD_MARK*
md_add_mark(MD_CTX* ctx)
{
    if(ctx->n_marks >= ctx->alloc_marks) {
        MD_MARK* new_marks;

        ctx->alloc_marks = (ctx->alloc_marks > 0
                ? ctx->alloc_marks + ctx->alloc_marks / 2
                : 64);
        new_marks = realloc(ctx->marks, ctx->alloc_marks * sizeof(MD_MARK));
        if(new_marks == NULL) {
            MD_LOG("realloc() failed.");
            return NULL;
        }

        ctx->marks = new_marks;
    }

    return &ctx->marks[ctx->n_marks++];
}

#define ADD_MARK_()                                                     \
        do {                                                            \
            mark = md_add_mark(ctx);                                    \
            if(mark == NULL) {                                          \
                ret = -1;                                               \
                goto abort;                                             \
            }                                                           \
        } while(0)

#define ADD_MARK(ch_, beg_, end_, flags_)                               \
        do {                                                            \
            ADD_MARK_();                                                \
            mark->beg = (beg_);                                         \
            mark->end = (end_);                                         \
            mark->prev = -1;                                            \
            mark->next = -1;                                            \
            mark->ch = (char)(ch_);                                     \
            mark->flags = (flags_);                                     \
        } while(0)


static inline void
md_mark_stack_push(MD_CTX* ctx, MD_MARKSTACK* stack, int mark_index)
{
    ctx->marks[mark_index].next = stack->top;
    stack->top = mark_index;
}

static inline int
md_mark_stack_pop(MD_CTX* ctx, MD_MARKSTACK* stack)
{
    int top = stack->top;
    if(top >= 0)
        stack->top = ctx->marks[top].next;
    return top;
}

/* Sometimes, we need to store a pointer into the mark. It is quite rare
 * so we do not bother to make MD_MARK use union, and it can only happen
 * for dummy marks. */
static inline void
md_mark_store_ptr(MD_CTX* ctx, int mark_index, void* ptr)
{
    MD_MARK* mark = &ctx->marks[mark_index];
    MD_ASSERT(mark->ch == 'D');

    /* Check only members beg and end are misused for this. */
    MD_ASSERT(sizeof(void*) <= 2 * sizeof(OFF));
    memcpy(mark, &ptr, sizeof(void*));
}

static inline void*
md_mark_get_ptr(MD_CTX* ctx, int mark_index)
{
    void* ptr;
    MD_MARK* mark = &ctx->marks[mark_index];
    MD_ASSERT(mark->ch == 'D');
    memcpy(&ptr, mark, sizeof(void*));
    return ptr;
}

static inline void
md_resolve_range(MD_CTX* ctx, int opener_index, int closer_index)
{
    MD_MARK* opener = &ctx->marks[opener_index];
    MD_MARK* closer = &ctx->marks[closer_index];

    /* Interconnect opener and closer and mark both as resolved. */
    opener->next = closer_index;
    closer->prev = opener_index;

    opener->flags |= MD_MARK_OPENER | MD_MARK_RESOLVED;
    closer->flags |= MD_MARK_CLOSER | MD_MARK_RESOLVED;
}


#define MD_ROLLBACK_CROSSING    0
#define MD_ROLLBACK_ALL         1

/* In the range ctx->marks[opener_index] ... [closer_index], undo some or all
 * resolvings accordingly to these rules:
 *
 * (1) All stacks of openers are cut so that any pending potential openers
 *     are discarded from future consideration.
 *
 * (2) If 'how' is MD_ROLLBACK_ALL, then ALL resolved marks inside the range
 *     are thrown away and turned into dummy marks ('D').
 *
 * WARNING: Do not call for arbitrary range of opener and closer.
 * This must form (potentially) valid range not crossing nesting boundaries
 * of already resolved ranges.
 */
static void
md_rollback(MD_CTX* ctx, int opener_index, int closer_index, int how)
{
    int i;

    for(i = 0; i < (int) SIZEOF_ARRAY(ctx->opener_stacks); i++) {
        MD_MARKSTACK* stack = &ctx->opener_stacks[i];
        while(stack->top >= opener_index)
            md_mark_stack_pop(ctx, stack);
    }

    if(how == MD_ROLLBACK_ALL) {
        for(i = opener_index + 1; i < closer_index; i++) {
            ctx->marks[i].ch = 'D';
            ctx->marks[i].flags = 0;
        }
    }
}

static void
md_build_mark_char_map(MD_CTX* ctx)
{
    memset(ctx->mark_char_map, 0, sizeof(ctx->mark_char_map));

    ctx->mark_char_map['\\'] = 1;
    ctx->mark_char_map['*'] = 1;
    ctx->mark_char_map['_'] = 1;
    ctx->mark_char_map['`'] = 1;
    ctx->mark_char_map['&'] = 1;
    ctx->mark_char_map[';'] = 1;
    ctx->mark_char_map['<'] = 1;
    ctx->mark_char_map['>'] = 1;
    ctx->mark_char_map['['] = 1;
    ctx->mark_char_map['!'] = 1;
    ctx->mark_char_map[']'] = 1;
    ctx->mark_char_map['\0'] = 1;

    if(ctx->parser.flags & MD_FLAG_STRIKETHROUGH)
        ctx->mark_char_map['~'] = 1;

    if(ctx->parser.flags & MD_FLAG_LATEXMATHSPANS)
        ctx->mark_char_map['$'] = 1;

    if(ctx->parser.flags & MD_FLAG_PERMISSIVEEMAILAUTOLINKS)
        ctx->mark_char_map['@'] = 1;

    if(ctx->parser.flags & MD_FLAG_PERMISSIVEURLAUTOLINKS)
        ctx->mark_char_map[':'] = 1;

    if(ctx->parser.flags & MD_FLAG_PERMISSIVEWWWAUTOLINKS)
        ctx->mark_char_map['.'] = 1;

    if((ctx->parser.flags & MD_FLAG_TABLES) || (ctx->parser.flags & MD_FLAG_WIKILINKS))
        ctx->mark_char_map['|'] = 1;

    if(ctx->parser.flags & MD_FLAG_COLLAPSEWHITESPACE) {
        int i;

        for(i = 0; i < (int) sizeof(ctx->mark_char_map); i++) {
            if(ISWHITESPACE_(i))
                ctx->mark_char_map[i] = 1;
        }
    }
}

static int
md_is_code_span(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, OFF beg,
                MD_MARK* opener, MD_MARK* closer,
                OFF last_potential_closers[CODESPAN_MARK_MAXLEN],
                int* p_reached_paragraph_end)
{
    OFF opener_beg = beg;
    OFF opener_end;
    OFF closer_beg;
    OFF closer_end;
    SZ mark_len;
    OFF line_end;
    int has_space_after_opener = FALSE;
    int has_eol_after_opener = FALSE;
    int has_space_before_closer = FALSE;
    int has_eol_before_closer = FALSE;
    int has_only_space = TRUE;
    MD_SIZE line_index = 0;

    line_end = lines[0].end;
    opener_end = opener_beg;
    while(opener_end < line_end  &&  CH(opener_end) == _T('`'))
        opener_end++;
    has_space_after_opener = (opener_end < line_end && CH(opener_end) == _T(' '));
    has_eol_after_opener = (opener_end == line_end);

    /* The caller needs to know end of the opening mark even if we fail. */
    opener->end = opener_end;

    mark_len = opener_end - opener_beg;
    if(mark_len > CODESPAN_MARK_MAXLEN)
        return FALSE;

    /* Check whether we already know there is no closer of this length.
     * If so, re-scan does no sense. This fixes issue #59. */
    if(last_potential_closers[mark_len-1] >= lines[n_lines-1].end  ||
       (*p_reached_paragraph_end  &&  last_potential_closers[mark_len-1] < opener_end))
        return FALSE;

    closer_beg = opener_end;
    closer_end = opener_end;

    /* Find closer mark. */
    while(TRUE) {
        while(closer_beg < line_end  &&  CH(closer_beg) != _T('`')) {
            if(CH(closer_beg) != _T(' '))
                has_only_space = FALSE;
            closer_beg++;
        }
        closer_end = closer_beg;
        while(closer_end < line_end  &&  CH(closer_end) == _T('`'))
            closer_end++;

        if(closer_end - closer_beg == mark_len) {
            /* Success. */
            has_space_before_closer = (closer_beg > lines[line_index].beg && CH(closer_beg-1) == _T(' '));
            has_eol_before_closer = (closer_beg == lines[line_index].beg);
            break;
        }

        if(closer_end - closer_beg > 0) {
            /* We have found a back-tick which is not part of the closer. */
            has_only_space = FALSE;

            /* But if we eventually fail, remember it as a potential closer
             * of its own length for future attempts. This mitigates needs for
             * rescans. */
            if(closer_end - closer_beg < CODESPAN_MARK_MAXLEN) {
                if(closer_beg > last_potential_closers[closer_end - closer_beg - 1])
                    last_potential_closers[closer_end - closer_beg - 1] = closer_beg;
            }
        }

        if(closer_end >= line_end) {
            line_index++;
            if(line_index >= n_lines) {
                /* Reached end of the paragraph and still nothing. */
                *p_reached_paragraph_end = TRUE;
                return FALSE;
            }
            /* Try on the next line. */
            line_end = lines[line_index].end;
            closer_beg = lines[line_index].beg;
        } else {
            closer_beg = closer_end;
        }
    }

    /* If there is a space or a new line both after and before the opener
     * (and if the code span is not made of spaces only), consume one initial
     * and one trailing space as part of the marks. */
    if(!has_only_space  &&
       (has_space_after_opener || has_eol_after_opener)  &&
       (has_space_before_closer || has_eol_before_closer))
    {
        if(has_space_after_opener)
            opener_end++;
        else
            opener_end = lines[1].beg;

        if(has_space_before_closer)
            closer_beg--;
        else {
            /* Go back to the end of prev line */
            closer_beg = lines[line_index-1].end;
            /* But restore any trailing whitespace */
            while(closer_beg < ctx->size  &&  ISBLANK(closer_beg))
                closer_beg++;
        }
    }

    opener->ch = _T('`');
    opener->beg = opener_beg;
    opener->end = opener_end;
    opener->flags = MD_MARK_POTENTIAL_OPENER;
    closer->ch = _T('`');
    closer->beg = closer_beg;
    closer->end = closer_end;
    closer->flags = MD_MARK_POTENTIAL_CLOSER;
    return TRUE;
}

static int
md_is_autolink_uri(MD_CTX* ctx, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg+1;

    MD_ASSERT(CH(beg) == _T('<'));

    /* Check for scheme. */
    if(off >= max_end  ||  !ISASCII(off))
        return FALSE;
    off++;
    while(1) {
        if(off >= max_end)
            return FALSE;
        if(off - beg > 32)
            return FALSE;
        if(CH(off) == _T(':')  &&  off - beg >= 3)
            break;
        if(!ISALNUM(off) && CH(off) != _T('+') && CH(off) != _T('-') && CH(off) != _T('.'))
            return FALSE;
        off++;
    }

    /* Check the path after the scheme. */
    while(off < max_end  &&  CH(off) != _T('>')) {
        if(ISWHITESPACE(off) || ISCNTRL(off) || CH(off) == _T('<'))
            return FALSE;
        off++;
    }

    if(off >= max_end)
        return FALSE;

    MD_ASSERT(CH(off) == _T('>'));
    *p_end = off+1;
    return TRUE;
}

static int
md_is_autolink_email(MD_CTX* ctx, OFF beg, OFF max_end, OFF* p_end)
{
    OFF off = beg + 1;
    int label_len;

    MD_ASSERT(CH(beg) == _T('<'));

    /* The code should correspond to this regexp:
            /^[a-zA-Z0-9.!#$%&'*+\/=?^_`{|}~-]+
            @[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?
            (?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$/
     */

    /* Username (before '@'). */
    while(off < max_end  &&  (ISALNUM(off) || ISANYOF(off, _T(".!#$%&'*+/=?^_`{|}~-"))))
        off++;
    if(off <= beg+1)
        return FALSE;

    /* '@' */
    if(off >= max_end  ||  CH(off) != _T('@'))
        return FALSE;
    off++;

    /* Labels delimited with '.'; each label is sequence of 1 - 63 alnum
     * characters or '-', but '-' is not allowed as first or last char. */
    label_len = 0;
    while(off < max_end) {
        if(ISALNUM(off))
            label_len++;
        else if(CH(off) == _T('-')  &&  label_len > 0)
            label_len++;
        else if(CH(off) == _T('.')  &&  label_len > 0  &&  CH(off-1) != _T('-'))
            label_len = 0;
        else
            break;

        if(label_len > 63)
            return FALSE;

        off++;
    }

    if(label_len <= 0  || off >= max_end  ||  CH(off) != _T('>') ||  CH(off-1) == _T('-'))
        return FALSE;

    *p_end = off+1;
    return TRUE;
}

static int
md_is_autolink(MD_CTX* ctx, OFF beg, OFF max_end, OFF* p_end, int* p_missing_mailto)
{
    if(md_is_autolink_uri(ctx, beg, max_end, p_end)) {
        *p_missing_mailto = FALSE;
        return TRUE;
    }

    if(md_is_autolink_email(ctx, beg, max_end, p_end)) {
        *p_missing_mailto = TRUE;
        return TRUE;
    }

    return FALSE;
}

static int
md_collect_marks(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, int table_mode)
{
    MD_SIZE line_index;
    int ret = 0;
    MD_MARK* mark;
    OFF codespan_last_potential_closers[CODESPAN_MARK_MAXLEN] = { 0 };
    int codespan_scanned_till_paragraph_end = FALSE;

    for(line_index = 0; line_index < n_lines; line_index++) {
        const MD_LINE* line = &lines[line_index];
        OFF off = line->beg;

        while(TRUE) {
            CHAR ch;

#ifdef MD4C_USE_UTF16
    /* For UTF-16, mark_char_map[] covers only ASCII. */
    #define IS_MARK_CHAR(off)   ((CH(off) < SIZEOF_ARRAY(ctx->mark_char_map))  &&  \
                                (ctx->mark_char_map[(unsigned char) CH(off)]))
#else
    /* For 8-bit encodings, mark_char_map[] covers all 256 elements. */
    #define IS_MARK_CHAR(off)   (ctx->mark_char_map[(unsigned char) CH(off)])
#endif

            /* Optimization: Use some loop unrolling. */
            while(off + 3 < line->end  &&  !IS_MARK_CHAR(off+0)  &&  !IS_MARK_CHAR(off+1)
                                       &&  !IS_MARK_CHAR(off+2)  &&  !IS_MARK_CHAR(off+3))
                off += 4;
            while(off < line->end  &&  !IS_MARK_CHAR(off+0))
                off++;

            if(off >= line->end)
                break;

            ch = CH(off);

            /* A backslash escape.
             * It can go beyond line->end as it may involve escaped new
             * line to form a hard break. */
            if(ch == _T('\\')  &&  off+1 < ctx->size  &&  (ISPUNCT(off+1) || ISNEWLINE(off+1))) {
                /* Hard-break cannot be on the last line of the block. */
                if(!ISNEWLINE(off+1)  ||  line_index+1 < n_lines)
                    ADD_MARK(ch, off, off+2, MD_MARK_RESOLVED);
                off += 2;
                continue;
            }

            /* A potential (string) emphasis start/end. */
            if(ch == _T('*')  ||  ch == _T('_')) {
                OFF tmp = off+1;
                int left_level;     /* What precedes: 0 = whitespace; 1 = punctuation; 2 = other char. */
                int right_level;    /* What follows: 0 = whitespace; 1 = punctuation; 2 = other char. */

                while(tmp < line->end  &&  CH(tmp) == ch)
                    tmp++;

                if(off == line->beg  ||  ISUNICODEWHITESPACEBEFORE(off))
                    left_level = 0;
                else if(ISUNICODEPUNCTBEFORE(off))
                    left_level = 1;
                else
                    left_level = 2;

                if(tmp == line->end  ||  ISUNICODEWHITESPACE(tmp))
                    right_level = 0;
                else if(ISUNICODEPUNCT(tmp))
                    right_level = 1;
                else
                    right_level = 2;

                /* Intra-word underscore doesn't have special meaning. */
                if(ch == _T('_')  &&  left_level == 2  &&  right_level == 2) {
                    left_level = 0;
                    right_level = 0;
                }

                if(left_level != 0  ||  right_level != 0) {
                    unsigned flags = 0;

                    if(left_level > 0  &&  left_level >= right_level)
                        flags |= MD_MARK_POTENTIAL_CLOSER;
                    if(right_level > 0  &&  right_level >= left_level)
                        flags |= MD_MARK_POTENTIAL_OPENER;
                    if(flags == (MD_MARK_POTENTIAL_OPENER | MD_MARK_POTENTIAL_CLOSER))
                        flags |= MD_MARK_EMPH_OC;

                    /* For "the rule of three" we need to remember the original
                     * size of the mark (modulo three), before we potentially
                     * split the mark when being later resolved partially by some
                     * shorter closer. */
                    switch((tmp - off) % 3) {
                        case 0: flags |= MD_MARK_EMPH_MOD3_0; break;
                        case 1: flags |= MD_MARK_EMPH_MOD3_1; break;
                        case 2: flags |= MD_MARK_EMPH_MOD3_2; break;
                    }

                    ADD_MARK(ch, off, tmp, flags);

                    /* During resolving, multiple asterisks may have to be
                     * split into independent span start/ends. Consider e.g.
                     * "**foo* bar*". Therefore we push also some empty dummy
                     * marks to have enough space for that. */
                    off++;
                    while(off < tmp) {
                        ADD_MARK('D', off, off, 0);
                        off++;
                    }
                    continue;
                }

                off = tmp;
                continue;
            }

            /* A potential code span start/end. */
            if(ch == _T('`')) {
                MD_MARK opener;
                MD_MARK closer;
                int is_code_span;

                is_code_span = md_is_code_span(ctx, line, n_lines - line_index, off,
                            &opener, &closer, codespan_last_potential_closers,
                            &codespan_scanned_till_paragraph_end);
                if(is_code_span) {
                    ADD_MARK(opener.ch, opener.beg, opener.end, opener.flags);
                    ADD_MARK(closer.ch, closer.beg, closer.end, closer.flags);
                    md_resolve_range(ctx, ctx->n_marks-2, ctx->n_marks-1);
                    off = closer.end;

                    /* Advance the current line accordingly. */
                    if(off > line->end)
                        line = md_lookup_line(off, lines, n_lines, &line_index);
                    continue;
                }

                off = opener.end;
                continue;
            }

            /* A potential entity start. */
            if(ch == _T('&')) {
                ADD_MARK(ch, off, off+1, MD_MARK_POTENTIAL_OPENER);
                off++;
                continue;
            }

            /* A potential entity end. */
            if(ch == _T(';')) {
                /* We surely cannot be entity unless the previous mark is '&'. */
                if(ctx->n_marks > 0  &&  ctx->marks[ctx->n_marks-1].ch == _T('&'))
                    ADD_MARK(ch, off, off+1, MD_MARK_POTENTIAL_CLOSER);

                off++;
                continue;
            }

            /* A potential autolink or raw HTML start/end. */
            if(ch == _T('<')) {
                int is_autolink;
                OFF autolink_end;
                int missing_mailto;

                if(!(ctx->parser.flags & MD_FLAG_NOHTMLSPANS)) {
                    int is_html;
                    OFF html_end;

                    /* Given the nature of the raw HTML, we have to recognize
                     * it here. Doing so later in md_analyze_lt_gt() could
                     * open can of worms of quadratic complexity. */
                    is_html = md_is_html_any(ctx, line, n_lines - line_index, off,
                                    lines[n_lines-1].end, &html_end);
                    if(is_html) {
                        ADD_MARK(_T('<'), off, off, MD_MARK_OPENER | MD_MARK_RESOLVED);
                        ADD_MARK(_T('>'), html_end, html_end, MD_MARK_CLOSER | MD_MARK_RESOLVED);
                        ctx->marks[ctx->n_marks-2].next = ctx->n_marks-1;
                        ctx->marks[ctx->n_marks-1].prev = ctx->n_marks-2;
                        off = html_end;

                        /* Advance the current line accordingly. */
                        if(off > line->end)
                            line = md_lookup_line(off, lines, n_lines, &line_index);
                        continue;
                    }
                }

                is_autolink = md_is_autolink(ctx, off, lines[n_lines-1].end,
                                    &autolink_end, &missing_mailto);
                if(is_autolink) {
                    unsigned flags = MD_MARK_RESOLVED | MD_MARK_AUTOLINK;
                    if(missing_mailto)
                        flags |= MD_MARK_AUTOLINK_MISSING_MAILTO;

                    ADD_MARK(_T('<'), off, off+1, MD_MARK_OPENER | flags);
                    ADD_MARK(_T('>'), autolink_end-1, autolink_end, MD_MARK_CLOSER | flags);
                    ctx->marks[ctx->n_marks-2].next = ctx->n_marks-1;
                    ctx->marks[ctx->n_marks-1].prev = ctx->n_marks-2;
                    off = autolink_end;
                    continue;
                }

                off++;
                continue;
            }

            /* A potential link or its part. */
            if(ch == _T('[')  ||  (ch == _T('!') && off+1 < line->end && CH(off+1) == _T('['))) {
                OFF tmp = (ch == _T('[') ? off+1 : off+2);
                ADD_MARK(ch, off, tmp, MD_MARK_POTENTIAL_OPENER);
                off = tmp;
                /* Two dummies to make enough place for data we need if it is
                 * a link. */
                ADD_MARK('D', off, off, 0);
                ADD_MARK('D', off, off, 0);
                continue;
            }
            if(ch == _T(']')) {
                ADD_MARK(ch, off, off+1, MD_MARK_POTENTIAL_CLOSER);
                off++;
                continue;
            }

            /* A potential permissive e-mail autolink. */
            if(ch == _T('@')) {
                if(line->beg + 1 <= off  &&  ISALNUM(off-1)  &&
                    off + 3 < line->end  &&  ISALNUM(off+1))
                {
                    ADD_MARK(ch, off, off+1, MD_MARK_POTENTIAL_OPENER);
                    /* Push a dummy as a reserve for a closer. */
                    ADD_MARK('D', line->beg, line->end, 0);
                }

                off++;
                continue;
            }

            /* A potential permissive URL autolink. */
            if(ch == _T(':')) {
                static struct {
                    const CHAR* scheme;
                    SZ scheme_size;
                    const CHAR* suffix;
                    SZ suffix_size;
                } scheme_map[] = {
                    /* In the order from the most frequently used, arguably. */
                    { _T("http"), 4,    _T("//"), 2 },
                    { _T("https"), 5,   _T("//"), 2 },
                    { _T("ftp"), 3,     _T("//"), 2 }
                };
                int scheme_index;

                for(scheme_index = 0; scheme_index < (int) SIZEOF_ARRAY(scheme_map); scheme_index++) {
                    const CHAR* scheme = scheme_map[scheme_index].scheme;
                    const SZ scheme_size = scheme_map[scheme_index].scheme_size;
                    const CHAR* suffix = scheme_map[scheme_index].suffix;
                    const SZ suffix_size = scheme_map[scheme_index].suffix_size;

                    if(line->beg + scheme_size <= off  &&  md_ascii_eq(STR(off-scheme_size), scheme, scheme_size)  &&
                        off + 1 + suffix_size < line->end  &&  md_ascii_eq(STR(off+1), suffix, suffix_size))
                    {
                        ADD_MARK(ch, off-scheme_size, off+1+suffix_size, MD_MARK_POTENTIAL_OPENER);
                        /* Push a dummy as a reserve for a closer. */
                        ADD_MARK('D', line->beg, line->end, 0);
                        off += 1 + suffix_size;
                        break;
                    }
                }

                off++;
                continue;
            }

            /* A potential permissive WWW autolink. */
            if(ch == _T('.')) {
                if(line->beg + 3 <= off  &&  md_ascii_eq(STR(off-3), _T("www"), 3)  &&
                   (off-3 == line->beg || ISUNICODEWHITESPACEBEFORE(off-3) || ISUNICODEPUNCTBEFORE(off-3)))
                {
                    ADD_MARK(ch, off-3, off+1, MD_MARK_POTENTIAL_OPENER);
                    /* Push a dummy as a reserve for a closer. */
                    ADD_MARK('D', line->beg, line->end, 0);
                    off++;
                    continue;
                }

                off++;
                continue;
            }

            /* A potential table cell boundary or wiki link label delimiter. */
            if((table_mode || ctx->parser.flags & MD_FLAG_WIKILINKS) && ch == _T('|')) {
                ADD_MARK(ch, off, off+1, 0);
                off++;
                continue;
            }

            /* A potential strikethrough/equation start/end. */
            if(ch == _T('$') || ch == _T('~')) {
                OFF tmp = off+1;

                while(tmp < line->end && CH(tmp) == ch)
                    tmp++;

                if(tmp - off <= 2) {
                    unsigned flags = MD_MARK_POTENTIAL_OPENER | MD_MARK_POTENTIAL_CLOSER;

                    if(off > line->beg  &&  !ISUNICODEWHITESPACEBEFORE(off)  &&  !ISUNICODEPUNCTBEFORE(off))
                        flags &= ~MD_MARK_POTENTIAL_OPENER;
                    if(tmp < line->end  &&  !ISUNICODEWHITESPACE(tmp)  &&  !ISUNICODEPUNCT(tmp))
                        flags &= ~MD_MARK_POTENTIAL_CLOSER;
                    if(flags != 0)
                        ADD_MARK(ch, off, tmp, flags);
                }

                off = tmp;
                continue;
            }

            /* Turn non-trivial whitespace into single space. */
            if(ISWHITESPACE_(ch)) {
                OFF tmp = off+1;

                while(tmp < line->end  &&  ISWHITESPACE(tmp))
                    tmp++;

                if(tmp - off > 1  ||  ch != _T(' '))
                    ADD_MARK(ch, off, tmp, MD_MARK_RESOLVED);

                off = tmp;
                continue;
            }

            /* NULL character. */
            if(ch == _T('\0')) {
                ADD_MARK(ch, off, off+1, MD_MARK_RESOLVED);
                off++;
                continue;
            }

            off++;
        }
    }

    /* Add a dummy mark at the end of the mark vector to simplify
     * process_inlines(). */
    ADD_MARK(127, ctx->size, ctx->size, MD_MARK_RESOLVED);

abort:
    return ret;
}

static void
md_analyze_bracket(MD_CTX* ctx, int mark_index)
{
    /* We cannot really resolve links here as for that we would need
     * more context. E.g. a following pair of brackets (reference link),
     * or enclosing pair of brackets (if the inner is the link, the outer
     * one cannot be.)
     *
     * Therefore we here only construct a list of '[' ']' pairs ordered by
     * position of the closer. This allows us to analyze what is or is not
     * link in the right order, from inside to outside in case of nested
     * brackets.
     *
     * The resolving itself is deferred to md_resolve_links().
     */

    MD_MARK* mark = &ctx->marks[mark_index];

    if(mark->flags & MD_MARK_POTENTIAL_OPENER) {
        if(BRACKET_OPENERS.top >= 0)
            ctx->marks[BRACKET_OPENERS.top].flags |= MD_MARK_HASNESTEDBRACKETS;

        md_mark_stack_push(ctx, &BRACKET_OPENERS, mark_index);
        return;
    }

    if(BRACKET_OPENERS.top >= 0) {
        int opener_index = md_mark_stack_pop(ctx, &BRACKET_OPENERS);
        MD_MARK* opener = &ctx->marks[opener_index];

        /* Interconnect the opener and closer. */
        opener->next = mark_index;
        mark->prev = opener_index;

        /* Add the pair into a list of potential links for md_resolve_links().
         * Note we misuse opener->prev for this as opener->next points to its
         * closer. */
        if(ctx->unresolved_link_tail >= 0)
            ctx->marks[ctx->unresolved_link_tail].prev = opener_index;
        else
            ctx->unresolved_link_head = opener_index;
        ctx->unresolved_link_tail = opener_index;
        opener->prev = -1;
    }
}

/* Forward declaration. */
static void md_analyze_link_contents(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines,
                                     int mark_beg, int mark_end);

static int
md_resolve_links(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines)
{
    int opener_index = ctx->unresolved_link_head;
    OFF last_link_beg = 0;
    OFF last_link_end = 0;
    OFF last_img_beg = 0;
    OFF last_img_end = 0;

    while(opener_index >= 0) {
        MD_MARK* opener = &ctx->marks[opener_index];
        int closer_index = opener->next;
        MD_MARK* closer = &ctx->marks[closer_index];
        int next_index = opener->prev;
        MD_MARK* next_opener;
        MD_MARK* next_closer;
        MD_LINK_ATTR attr;
        int is_link = FALSE;

        if(next_index >= 0) {
            next_opener = &ctx->marks[next_index];
            next_closer = &ctx->marks[next_opener->next];
        } else {
            next_opener = NULL;
            next_closer = NULL;
        }

        /* If nested ("[ [ ] ]"), we need to make sure that:
         *   - The outer does not end inside of (...) belonging to the inner.
         *   - The outer cannot be link if the inner is link (i.e. not image).
         *
         * (Note we here analyze from inner to outer as the marks are ordered
         * by closer->beg.)
         */
        if((opener->beg < last_link_beg  &&  closer->end < last_link_end)  ||
           (opener->beg < last_img_beg  &&  closer->end < last_img_end)  ||
           (opener->beg < last_link_end  &&  opener->ch == '['))
        {
            opener_index = next_index;
            continue;
        }

        /* Recognize and resolve wiki links.
         * Wiki-links maybe '[[destination]]' or '[[destination|label]]'.
         */
        if ((ctx->parser.flags & MD_FLAG_WIKILINKS) &&
            (opener->end - opener->beg == 1) &&         /* not image */
            next_opener != NULL &&                      /* double '[' opener */
            next_opener->ch == '[' &&
            (next_opener->beg == opener->beg - 1) &&
            (next_opener->end - next_opener->beg == 1) &&
            next_closer != NULL &&                      /* double ']' closer */
            next_closer->ch == ']' &&
            (next_closer->beg == closer->beg + 1) &&
            (next_closer->end - next_closer->beg == 1))
        {
            MD_MARK* delim = NULL;
            int delim_index;
            OFF dest_beg, dest_end;

            is_link = TRUE;

            /* We don't allow destination to be longer than 100 characters.
             * Lets scan to see whether there is '|'. (If not then the whole
             * wiki-link has to be below the 100 characters.) */
            delim_index = opener_index + 1;
            while(delim_index < closer_index) {
                MD_MARK* m = &ctx->marks[delim_index];
                if(m->ch == '|') {
                    delim = m;
                    break;
                }
                if(m->ch != 'D') {
                    if(m->beg - opener->end > 100)
                        break;
                    if(m->ch != 'D'  &&  (m->flags & MD_MARK_OPENER))
                        delim_index = m->next;
                }
                delim_index++;
            }

            dest_beg = opener->end;
            dest_end = (delim != NULL) ? delim->beg : closer->beg;
            if(dest_end - dest_beg == 0 || dest_end - dest_beg > 100)
                is_link = FALSE;

            /* There may not be any new line in the destination. */
            if(is_link) {
                OFF off;
                for(off = dest_beg; off < dest_end; off++) {
                    if(ISNEWLINE(off)) {
                        is_link = FALSE;
                        break;
                    }
                }
            }

            if(is_link) {
                if(delim != NULL) {
                    if(delim->end < closer->beg) {
                        md_rollback(ctx, opener_index, delim_index, MD_ROLLBACK_ALL);
                        md_rollback(ctx, delim_index, closer_index, MD_ROLLBACK_CROSSING);
                        delim->flags |= MD_MARK_RESOLVED;
                        opener->end = delim->beg;
                    } else {
                        /* The pipe is just before the closer: [[foo|]] */
                        md_rollback(ctx, opener_index, closer_index, MD_ROLLBACK_ALL);
                        closer->beg = delim->beg;
                        delim = NULL;
                    }
                }

                opener->beg = next_opener->beg;
                opener->next = closer_index;
                opener->flags |= MD_MARK_OPENER | MD_MARK_RESOLVED;

                closer->end = next_closer->end;
                closer->prev = opener_index;
                closer->flags |= MD_MARK_CLOSER | MD_MARK_RESOLVED;

                last_link_beg = opener->beg;
                last_link_end = closer->end;

                if(delim != NULL)
                    md_analyze_link_contents(ctx, lines, n_lines, delim_index+1, closer_index);

                opener_index = next_opener->prev;
                continue;
            }
        }

        if(next_opener != NULL  &&  next_opener->beg == closer->end) {
            if(next_closer->beg > closer->end + 1) {
                /* Might be full reference link. */
                if(!(next_opener->flags & MD_MARK_HASNESTEDBRACKETS))
                    is_link = md_is_link_reference(ctx, lines, n_lines, next_opener->beg, next_closer->end, &attr);
            } else {
                /* Might be shortcut reference link. */
                if(!(opener->flags & MD_MARK_HASNESTEDBRACKETS))
                    is_link = md_is_link_reference(ctx, lines, n_lines, opener->beg, closer->end, &attr);
            }

            if(is_link < 0)
                return -1;

            if(is_link) {
                /* Eat the 2nd "[...]". */
                closer->end = next_closer->end;

                /* Do not analyze the label as a standalone link in the next
                 * iteration. */
                next_index = ctx->marks[next_index].prev;
            }
        } else {
            if(closer->end < ctx->size  &&  CH(closer->end) == _T('(')) {
                /* Might be inline link. */
                OFF inline_link_end = UINT_MAX;

                is_link = md_is_inline_link_spec(ctx, lines, n_lines, closer->end, &inline_link_end, &attr);
                if(is_link < 0)
                    return -1;

                /* Check the closing ')' is not inside an already resolved range
                 * (i.e. a range with a higher priority), e.g. a code span. */
                if(is_link) {
                    int i = closer_index + 1;

                    while(i < ctx->n_marks) {
                        MD_MARK* mark = &ctx->marks[i];

                        if(mark->beg >= inline_link_end)
                            break;
                        if((mark->flags & (MD_MARK_OPENER | MD_MARK_RESOLVED)) == (MD_MARK_OPENER | MD_MARK_RESOLVED)) {
                            if(ctx->marks[mark->next].beg >= inline_link_end) {
                                /* Cancel the link status. */
                                if(attr.title_needs_free)
                                    free(attr.title);
                                is_link = FALSE;
                                break;
                            }

                            i = mark->next + 1;
                        } else {
                            i++;
                        }
                    }
                }

                if(is_link) {
                    /* Eat the "(...)" */
                    closer->end = inline_link_end;
                }
            }

            if(!is_link) {
                /* Might be collapsed reference link. */
                if(!(opener->flags & MD_MARK_HASNESTEDBRACKETS))
                    is_link = md_is_link_reference(ctx, lines, n_lines, opener->beg, closer->end, &attr);
                if(is_link < 0)
                    return -1;
            }
        }

        if(is_link) {
            /* Resolve the brackets as a link. */
            opener->flags |= MD_MARK_OPENER | MD_MARK_RESOLVED;
            closer->flags |= MD_MARK_CLOSER | MD_MARK_RESOLVED;

            /* If it is a link, we store the destination and title in the two
             * dummy marks after the opener. */
            MD_ASSERT(ctx->marks[opener_index+1].ch == 'D');
            ctx->marks[opener_index+1].beg = attr.dest_beg;
            ctx->marks[opener_index+1].end = attr.dest_end;

            MD_ASSERT(ctx->marks[opener_index+2].ch == 'D');
            md_mark_store_ptr(ctx, opener_index+2, attr.title);
            /* The title might or might not have been allocated for us. */
            if(attr.title_needs_free)
                md_mark_stack_push(ctx, &ctx->ptr_stack, opener_index+2);
            ctx->marks[opener_index+2].prev = attr.title_size;

            if(opener->ch == '[') {
                last_link_beg = opener->beg;
                last_link_end = closer->end;
            } else {
                last_img_beg = opener->beg;
                last_img_end = closer->end;
            }

            md_analyze_link_contents(ctx, lines, n_lines, opener_index+1, closer_index);

            /* If the link text is formed by nothing but permissive autolink,
             * suppress the autolink.
             * See https://github.com/mity/md4c/issues/152 for more info. */
            if(ctx->parser.flags & MD_FLAG_PERMISSIVEAUTOLINKS) {
                MD_MARK* first_nested;
                MD_MARK* last_nested;

                first_nested = opener + 1;
                while(first_nested->ch == _T('D')  &&  first_nested < closer)
                    first_nested++;

                last_nested = closer - 1;
                while(first_nested->ch == _T('D')  &&  last_nested > opener)
                    last_nested--;

                if((first_nested->flags & MD_MARK_RESOLVED)  &&
                   first_nested->beg == opener->end  &&
                   ISANYOF_(first_nested->ch, _T("@:."))  &&
                   first_nested->next == (last_nested - ctx->marks)  &&
                   last_nested->end == closer->beg)
                {
                    first_nested->ch = _T('D');
                    first_nested->flags &= ~MD_MARK_RESOLVED;
                    last_nested->ch = _T('D');
                    last_nested->flags &= ~MD_MARK_RESOLVED;
                }
            }
        }

        opener_index = next_index;
    }

    return 0;
}

/* Analyze whether the mark '&' starts a HTML entity.
 * If so, update its flags as well as flags of corresponding closer ';'. */
static void
md_analyze_entity(MD_CTX* ctx, int mark_index)
{
    MD_MARK* opener = &ctx->marks[mark_index];
    MD_MARK* closer;
    OFF off;

    /* Cannot be entity if there is no closer as the next mark.
     * (Any other mark between would mean strange character which cannot be
     * part of the entity.
     *
     * So we can do all the work on '&' and do not call this later for the
     * closing mark ';'.
     */
    if(mark_index + 1 >= ctx->n_marks)
        return;
    closer = &ctx->marks[mark_index+1];
    if(closer->ch != ';')
        return;

    if(md_is_entity(ctx, opener->beg, closer->end, &off)) {
        MD_ASSERT(off == closer->end);

        md_resolve_range(ctx, mark_index, mark_index+1);
        opener->end = closer->end;
    }
}

static void
md_analyze_table_cell_boundary(MD_CTX* ctx, int mark_index)
{
    MD_MARK* mark = &ctx->marks[mark_index];
    mark->flags |= MD_MARK_RESOLVED;
    mark->next = -1;

    if(ctx->table_cell_boundaries_head < 0)
        ctx->table_cell_boundaries_head = mark_index;
    else
        ctx->marks[ctx->table_cell_boundaries_tail].next = mark_index;
    ctx->table_cell_boundaries_tail = mark_index;
    ctx->n_table_cell_boundaries++;
}

/* Split a longer mark into two. The new mark takes the given count of
 * characters. May only be called if an adequate number of dummy 'D' marks
 * follows.
 */
static int
md_split_emph_mark(MD_CTX* ctx, int mark_index, SZ n)
{
    MD_MARK* mark = &ctx->marks[mark_index];
    int new_mark_index = mark_index + (mark->end - mark->beg - n);
    MD_MARK* dummy = &ctx->marks[new_mark_index];

    MD_ASSERT(mark->end - mark->beg > n);
    MD_ASSERT(dummy->ch == 'D');

    memcpy(dummy, mark, sizeof(MD_MARK));
    mark->end -= n;
    dummy->beg = mark->end;

    return new_mark_index;
}

static void
md_analyze_emph(MD_CTX* ctx, int mark_index)
{
    MD_MARK* mark = &ctx->marks[mark_index];

    /* If we can be a closer, try to resolve with the preceding opener. */
    if(mark->flags & MD_MARK_POTENTIAL_CLOSER) {
        MD_MARK* opener = NULL;
        int opener_index = 0;
        MD_MARKSTACK* opener_stacks[6];
        int i, n_opener_stacks;
        unsigned flags = mark->flags;

        n_opener_stacks = 0;

        /* Apply the rule of 3 */
        opener_stacks[n_opener_stacks++] = md_emph_stack(ctx, mark->ch, MD_MARK_EMPH_MOD3_0 | MD_MARK_EMPH_OC);
        if((flags & MD_MARK_EMPH_MOD3_MASK) != MD_MARK_EMPH_MOD3_2)
            opener_stacks[n_opener_stacks++] = md_emph_stack(ctx, mark->ch, MD_MARK_EMPH_MOD3_1 | MD_MARK_EMPH_OC);
        if((flags & MD_MARK_EMPH_MOD3_MASK) != MD_MARK_EMPH_MOD3_1)
            opener_stacks[n_opener_stacks++] = md_emph_stack(ctx, mark->ch, MD_MARK_EMPH_MOD3_2 | MD_MARK_EMPH_OC);
        opener_stacks[n_opener_stacks++] = md_emph_stack(ctx, mark->ch, MD_MARK_EMPH_MOD3_0);
        if(!(flags & MD_MARK_EMPH_OC)  ||  (flags & MD_MARK_EMPH_MOD3_MASK) != MD_MARK_EMPH_MOD3_2)
            opener_stacks[n_opener_stacks++] = md_emph_stack(ctx, mark->ch, MD_MARK_EMPH_MOD3_1);
        if(!(flags & MD_MARK_EMPH_OC)  ||  (flags & MD_MARK_EMPH_MOD3_MASK) != MD_MARK_EMPH_MOD3_1)
            opener_stacks[n_opener_stacks++] = md_emph_stack(ctx, mark->ch, MD_MARK_EMPH_MOD3_2);

        /* Opener is the most recent mark from the allowed stacks. */
        for(i = 0; i < n_opener_stacks; i++) {
            if(opener_stacks[i]->top >= 0) {
                int m_index = opener_stacks[i]->top;
                MD_MARK* m = &ctx->marks[m_index];

                if(opener == NULL  ||  m->end > opener->end) {
                    opener_index = m_index;
                    opener = m;
                }
            }
        }

        /* Resolve, if we have found matching opener. */
        if(opener != NULL) {
            SZ opener_size = opener->end - opener->beg;
            SZ closer_size = mark->end - mark->beg;
            MD_MARKSTACK* stack = md_opener_stack(ctx, opener_index);

            if(opener_size > closer_size) {
                opener_index = md_split_emph_mark(ctx, opener_index, closer_size);
                md_mark_stack_push(ctx, stack, opener_index);
            } else if(opener_size < closer_size) {
                md_split_emph_mark(ctx, mark_index, closer_size - opener_size);
            }

            /* Above we were only peeking. */
            md_mark_stack_pop(ctx, stack);

            md_rollback(ctx, opener_index, mark_index, MD_ROLLBACK_CROSSING);
            md_resolve_range(ctx, opener_index, mark_index);
            return;
        }
    }

    /* If we could not resolve as closer, we may be yet be an opener. */
    if(mark->flags & MD_MARK_POTENTIAL_OPENER)
        md_mark_stack_push(ctx, md_emph_stack(ctx, mark->ch, mark->flags), mark_index);
}

static void
md_analyze_tilde(MD_CTX* ctx, int mark_index)
{
    MD_MARK* mark = &ctx->marks[mark_index];
    MD_MARKSTACK* stack = md_opener_stack(ctx, mark_index);

    /* We attempt to be Github Flavored Markdown compatible here. GFM accepts
     * only tildes sequences of length 1 and 2, and the length of the opener
     * and closer has to match. */

    if((mark->flags & MD_MARK_POTENTIAL_CLOSER)  &&  stack->top >= 0) {
        int opener_index = stack->top;

        md_mark_stack_pop(ctx, stack);
        md_rollback(ctx, opener_index, mark_index, MD_ROLLBACK_CROSSING);
        md_resolve_range(ctx, opener_index, mark_index);
        return;
    }

    if(mark->flags & MD_MARK_POTENTIAL_OPENER)
        md_mark_stack_push(ctx, stack, mark_index);
}

static void
md_analyze_dollar(MD_CTX* ctx, int mark_index)
{
    MD_MARK* mark = &ctx->marks[mark_index];

    if((mark->flags & MD_MARK_POTENTIAL_CLOSER)  &&  DOLLAR_OPENERS.top >= 0) {
        /* If the potential closer has a non-matching number of $, discard */
        MD_MARK* opener = &ctx->marks[DOLLAR_OPENERS.top];
        int opener_index = DOLLAR_OPENERS.top;
        MD_MARK* closer = mark;
        int closer_index = mark_index;

        if(opener->end - opener->beg == closer->end - closer->beg) {
            /* We are the matching closer */
            md_mark_stack_pop(ctx, &DOLLAR_OPENERS);
            md_rollback(ctx, opener_index, closer_index, MD_ROLLBACK_ALL);
            md_resolve_range(ctx, opener_index, closer_index);

            /* Discard all pending openers: Latex math span do not allow
             * nesting. */
            DOLLAR_OPENERS.top = -1;
            return;
        }
    }

    if(mark->flags & MD_MARK_POTENTIAL_OPENER)
        md_mark_stack_push(ctx, &DOLLAR_OPENERS, mark_index);
}

static MD_MARK*
md_scan_left_for_resolved_mark(MD_CTX* ctx, MD_MARK* mark_from, OFF off, MD_MARK** p_cursor)
{
    MD_MARK* mark;

    for(mark = mark_from; mark >= ctx->marks; mark--) {
        if(mark->ch == 'D'  ||  mark->beg > off)
            continue;
        if(mark->beg <= off  &&  off < mark->end  &&  (mark->flags & MD_MARK_RESOLVED)) {
            if(p_cursor != NULL)
                *p_cursor = mark;
            return mark;
        }
        if(mark->end <= off)
            break;
    }

    if(p_cursor != NULL)
        *p_cursor = mark;
    return NULL;
}

static MD_MARK*
md_scan_right_for_resolved_mark(MD_CTX* ctx, MD_MARK* mark_from, OFF off, MD_MARK** p_cursor)
{
    MD_MARK* mark;

    for(mark = mark_from; mark < ctx->marks + ctx->n_marks; mark++) {
        if(mark->ch == 'D'  ||  mark->end <= off)
            continue;
        if(mark->beg <= off  &&  off < mark->end  &&  (mark->flags & MD_MARK_RESOLVED)) {
            if(p_cursor != NULL)
                *p_cursor = mark;
            return mark;
        }
        if(mark->beg > off)
            break;
    }

    if(p_cursor != NULL)
        *p_cursor = mark;
    return NULL;
}

static void
md_analyze_permissive_autolink(MD_CTX* ctx, int mark_index)
{
    static const struct {
        const MD_CHAR start_char;
        const MD_CHAR delim_char;
        const MD_CHAR* allowed_nonalnum_chars;
        int min_components;
        const MD_CHAR optional_end_char;
    } URL_MAP[] = {
        { _T('\0'), _T('.'),  _T(".-_"),      2, _T('\0') },    /* host, mandatory */
        { _T('/'),  _T('/'),  _T("/.-_"),     0, _T('/') },     /* path */
        { _T('?'),  _T('&'),  _T("&.-+_=()"), 1, _T('\0') },    /* query */
        { _T('#'),  _T('\0'), _T(".-+_") ,    1, _T('\0') }     /* fragment */
    };

    MD_MARK* opener = &ctx->marks[mark_index];
    MD_MARK* closer = &ctx->marks[mark_index + 1];  /* The dummy. */
    OFF line_beg = closer->beg;     /* md_collect_mark() set this for us */
    OFF line_end = closer->end;     /* ditto */
    OFF beg = opener->beg;
    OFF end = opener->end;
    MD_MARK* left_cursor = opener;
    int left_boundary_ok = FALSE;
    MD_MARK* right_cursor = opener;
    int right_boundary_ok = FALSE;
    unsigned i;

    MD_ASSERT(closer->ch == 'D');

    if(opener->ch == '@') {
        MD_ASSERT(CH(opener->beg) == _T('@'));

        /* Scan backwards for the user name (before '@'). */
        while(beg > line_beg) {
            if(ISALNUM(beg-1))
                beg--;
            else if(beg >= line_beg+2  &&  ISALNUM(beg-2)  &&
                        ISANYOF(beg-1, _T(".-_+"))  &&
                        md_scan_left_for_resolved_mark(ctx, left_cursor, beg-1, &left_cursor) == NULL  &&
                        ISALNUM(beg))
                beg--;
            else
                break;
        }
        if(beg == opener->beg)      /* empty user name */
            return;
    }

    /* Verify there's line boundary, whitespace, allowed punctuation or
     * resolved emphasis mark just before the suspected autolink. */
    if(beg == line_beg  ||  ISUNICODEWHITESPACEBEFORE(beg)  ||  ISANYOF(beg-1, _T("({["))) {
        left_boundary_ok = TRUE;
    } else if(ISANYOF(beg-1, _T("*_~"))) {
        MD_MARK* left_mark;

        left_mark = md_scan_left_for_resolved_mark(ctx, left_cursor, beg-1, &left_cursor);
        if(left_mark != NULL  &&  (left_mark->flags & MD_MARK_OPENER))
            left_boundary_ok = TRUE;
    }
    if(!left_boundary_ok)
        return;

    for(i = 0; i < SIZEOF_ARRAY(URL_MAP); i++) {
        int n_components = 0;
        int n_open_brackets = 0;

        if(URL_MAP[i].start_char != _T('\0')) {
            if(end >= line_end  ||  CH(end) != URL_MAP[i].start_char)
                continue;
            if(URL_MAP[i].min_components > 0  &&  (end+1 >= line_end  ||  !ISALNUM(end+1)))
                continue;
            end++;
        }

        while(end < line_end) {
            if(ISALNUM(end)) {
                if(n_components == 0)
                    n_components++;
                end++;
            } else if(end < line_end  &&
                        ISANYOF(end, URL_MAP[i].allowed_nonalnum_chars)  &&
                        md_scan_right_for_resolved_mark(ctx, right_cursor, end, &right_cursor) == NULL  &&
                        ((end > line_beg && (ISALNUM(end-1) || CH(end-1) == _T(')')))  ||  CH(end) == _T('('))  &&
                        ((end+1 < line_end && (ISALNUM(end+1) || CH(end+1) == _T('(')))  ||  CH(end) == _T(')')))
            {
                if(CH(end) == URL_MAP[i].delim_char)
                    n_components++;

                /* brackets have to be balanced. */
                if(CH(end) == _T('(')) {
                    n_open_brackets++;
                } else if(CH(end) == _T(')')) {
                    if(n_open_brackets <= 0)
                        break;
                    n_open_brackets--;
                }

                end++;
            } else {
                break;
            }
        }

        if(end < line_end  &&  URL_MAP[i].optional_end_char != _T('\0')  &&
                CH(end) == URL_MAP[i].optional_end_char)
            end++;

        if(n_components < URL_MAP[i].min_components  ||  n_open_brackets != 0)
            return;

        if(opener->ch == '@')   /* E-mail autolinks wants only the host. */
            break;
    }

    /* Verify there's line boundary, whitespace, allowed punctuation or
     * resolved emphasis mark just after the suspected autolink. */
    if(end == line_end  ||  ISUNICODEWHITESPACE(end)  ||  ISANYOF(end, _T(")}].!?,;"))) {
        right_boundary_ok = TRUE;
    } else {
        MD_MARK* right_mark;

        right_mark = md_scan_right_for_resolved_mark(ctx, right_cursor, end, &right_cursor);
        if(right_mark != NULL  &&  (right_mark->flags & MD_MARK_CLOSER))
            right_boundary_ok = TRUE;
    }
    if(!right_boundary_ok)
        return;

    /* Success, we are an autolink. */
    opener->beg = beg;
    opener->end = beg;
    closer->beg = end;
    closer->end = end;
    closer->ch = opener->ch;
    md_resolve_range(ctx, mark_index, mark_index + 1);
}

#define MD_ANALYZE_NOSKIP_EMPH  0x01

static inline void
md_analyze_marks(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines,
                 int mark_beg, int mark_end, const CHAR* mark_chars, unsigned flags)
{
    int i = mark_beg;
    OFF last_end = lines[0].beg;

    MD_UNUSED(lines);
    MD_UNUSED(n_lines);

    while(i < mark_end) {
        MD_MARK* mark = &ctx->marks[i];

        /* Skip resolved spans. */
        if(mark->flags & MD_MARK_RESOLVED) {
            if((mark->flags & MD_MARK_OPENER)  &&
               !((flags & MD_ANALYZE_NOSKIP_EMPH) && ISANYOF_(mark->ch, "*_~")))
            {
                MD_ASSERT(i < mark->next);
                i = mark->next + 1;
            } else {
                i++;
            }
            continue;
        }

        /* Skip marks we do not want to deal with. */
        if(!ISANYOF_(mark->ch, mark_chars)) {
            i++;
            continue;
        }

        /* The resolving in previous step could have expanded a mark. */
        if(mark->beg < last_end) {
            i++;
            continue;
        }

        /* Analyze the mark. */
        switch(mark->ch) {
            case '[':   /* Pass through. */
            case '!':   /* Pass through. */
            case ']':   md_analyze_bracket(ctx, i); break;
            case '&':   md_analyze_entity(ctx, i); break;
            case '|':   md_analyze_table_cell_boundary(ctx, i); break;
            case '_':   /* Pass through. */
            case '*':   md_analyze_emph(ctx, i); break;
            case '~':   md_analyze_tilde(ctx, i); break;
            case '$':   md_analyze_dollar(ctx, i); break;
            case '.':   /* Pass through. */
            case ':':   /* Pass through. */
            case '@':   md_analyze_permissive_autolink(ctx, i); break;
        }

        if(mark->flags & MD_MARK_RESOLVED) {
            if(mark->flags & MD_MARK_OPENER)
                last_end = ctx->marks[mark->next].end;
            else
                last_end = mark->end;
        }

        i++;
    }
}

/* Analyze marks (build ctx->marks). */
static int
md_analyze_inlines(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines, int table_mode)
{
    int ret;

    /* Reset the previously collected stack of marks. */
    ctx->n_marks = 0;

    /* Collect all marks. */
    MD_CHECK(md_collect_marks(ctx, lines, n_lines, table_mode));

    /* (1) Links. */
    md_analyze_marks(ctx, lines, n_lines, 0, ctx->n_marks, _T("[]!"), 0);
    MD_CHECK(md_resolve_links(ctx, lines, n_lines));
    BRACKET_OPENERS.top = -1;
    ctx->unresolved_link_head = -1;
    ctx->unresolved_link_tail = -1;

    if(table_mode) {
        /* (2) Analyze table cell boundaries. */
        MD_ASSERT(n_lines == 1);
        ctx->n_table_cell_boundaries = 0;
        md_analyze_marks(ctx, lines, n_lines, 0, ctx->n_marks, _T("|"), 0);
        return ret;
    }

    /* (3) Emphasis and strong emphasis; permissive autolinks. */
    md_analyze_link_contents(ctx, lines, n_lines, 0, ctx->n_marks);

abort:
    return ret;
}

static void
md_analyze_link_contents(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines,
                         int mark_beg, int mark_end)
{
    int i;

    md_analyze_marks(ctx, lines, n_lines, mark_beg, mark_end, _T("&"), 0);
    md_analyze_marks(ctx, lines, n_lines, mark_beg, mark_end, _T("*_~$"), 0);

    if((ctx->parser.flags & MD_FLAG_PERMISSIVEAUTOLINKS) != 0) {
        /* These have to be processed last, as they may be greedy and expand
         * from their original mark. Also their implementation must be careful
         * not to cross any (previously) resolved marks when doing so. */
        md_analyze_marks(ctx, lines, n_lines, mark_beg, mark_end, _T("@:."), MD_ANALYZE_NOSKIP_EMPH);
    }

    for(i = 0; i < (int) SIZEOF_ARRAY(ctx->opener_stacks); i++)
        ctx->opener_stacks[i].top = -1;
}

static int
md_enter_leave_span_a(MD_CTX* ctx, int enter, MD_SPANTYPE type,
                      const CHAR* dest, SZ dest_size, int is_autolink,
                      const CHAR* title, SZ title_size)
{
    MD_ATTRIBUTE_BUILD href_build = { 0 };
    MD_ATTRIBUTE_BUILD title_build = { 0 };
    MD_SPAN_A_DETAIL det;
    int ret = 0;

    /* Note we here rely on fact that MD_SPAN_A_DETAIL and
     * MD_SPAN_IMG_DETAIL are binary-compatible. */
    memset(&det, 0, sizeof(MD_SPAN_A_DETAIL));
    MD_CHECK(md_build_attribute(ctx, dest, dest_size,
                    (is_autolink ? MD_BUILD_ATTR_NO_ESCAPES : 0),
                    &det.href, &href_build));
    MD_CHECK(md_build_attribute(ctx, title, title_size, 0, &det.title, &title_build));
    det.is_autolink = is_autolink;
    if(enter)
        MD_ENTER_SPAN(type, &det);
    else
        MD_LEAVE_SPAN(type, &det);

abort:
    md_free_attribute(ctx, &href_build);
    md_free_attribute(ctx, &title_build);
    return ret;
}

static int
md_enter_leave_span_wikilink(MD_CTX* ctx, int enter, const CHAR* target, SZ target_size)
{
    MD_ATTRIBUTE_BUILD target_build = { 0 };
    MD_SPAN_WIKILINK_DETAIL det;
    int ret = 0;

    memset(&det, 0, sizeof(MD_SPAN_WIKILINK_DETAIL));
    MD_CHECK(md_build_attribute(ctx, target, target_size, 0, &det.target, &target_build));

    if (enter)
        MD_ENTER_SPAN(MD_SPAN_WIKILINK, &det);
    else
        MD_LEAVE_SPAN(MD_SPAN_WIKILINK, &det);

abort:
    md_free_attribute(ctx, &target_build);
    return ret;
}


/* Render the output, accordingly to the analyzed ctx->marks. */
static int
md_process_inlines(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines)
{
    MD_TEXTTYPE text_type;
    const MD_LINE* line = lines;
    MD_MARK* prev_mark = NULL;
    MD_MARK* mark;
    OFF off = lines[0].beg;
    OFF end = lines[n_lines-1].end;
    OFF tmp;
    int enforce_hardbreak = 0;
    int ret = 0;

    /* Find first resolved mark. Note there is always at least one resolved
     * mark,  the dummy last one after the end of the latest line we actually
     * never really reach. This saves us of a lot of special checks and cases
     * in this function. */
    mark = ctx->marks;
    while(!(mark->flags & MD_MARK_RESOLVED))
        mark++;

    text_type = MD_TEXT_NORMAL;

    while(1) {
        /* Process the text up to the next mark or end-of-line. */
        tmp = (line->end < mark->beg ? line->end : mark->beg);
        if(tmp > off) {
            MD_TEXT(text_type, STR(off), tmp - off);
            off = tmp;
        }

        /* If reached the mark, process it and move to next one. */
        if(off >= mark->beg) {
            switch(mark->ch) {
                case '\\':      /* Backslash escape. */
                    if(ISNEWLINE(mark->beg+1))
                        enforce_hardbreak = 1;
                    else
                        MD_TEXT(text_type, STR(mark->beg+1), 1);
                    break;

                case ' ':       /* Non-trivial space. */
                    MD_TEXT(text_type, _T(" "), 1);
                    break;

                case '`':       /* Code span. */
                    if(mark->flags & MD_MARK_OPENER) {
                        MD_ENTER_SPAN(MD_SPAN_CODE, NULL);
                        text_type = MD_TEXT_CODE;
                    } else {
                        MD_LEAVE_SPAN(MD_SPAN_CODE, NULL);
                        text_type = MD_TEXT_NORMAL;
                    }
                    break;

                case '_':       /* Underline (or emphasis if we fall through). */
                    if(ctx->parser.flags & MD_FLAG_UNDERLINE) {
                        if(mark->flags & MD_MARK_OPENER) {
                            while(off < mark->end) {
                                MD_ENTER_SPAN(MD_SPAN_U, NULL);
                                off++;
                            }
                        } else {
                            while(off < mark->end) {
                                MD_LEAVE_SPAN(MD_SPAN_U, NULL);
                                off++;
                            }
                        }
                        break;
                    }
                    MD_FALLTHROUGH();

                case '*':       /* Emphasis, strong emphasis. */
                    if(mark->flags & MD_MARK_OPENER) {
                        if((mark->end - off) % 2) {
                            MD_ENTER_SPAN(MD_SPAN_EM, NULL);
                            off++;
                        }
                        while(off + 1 < mark->end) {
                            MD_ENTER_SPAN(MD_SPAN_STRONG, NULL);
                            off += 2;
                        }
                    } else {
                        while(off + 1 < mark->end) {
                            MD_LEAVE_SPAN(MD_SPAN_STRONG, NULL);
                            off += 2;
                        }
                        if((mark->end - off) % 2) {
                            MD_LEAVE_SPAN(MD_SPAN_EM, NULL);
                            off++;
                        }
                    }
                    break;

                case '~':
                    if(mark->flags & MD_MARK_OPENER)
                        MD_ENTER_SPAN(MD_SPAN_DEL, NULL);
                    else
                        MD_LEAVE_SPAN(MD_SPAN_DEL, NULL);
                    break;

                case '$':
                    if(mark->flags & MD_MARK_OPENER) {
                        MD_ENTER_SPAN((mark->end - off) % 2 ? MD_SPAN_LATEXMATH : MD_SPAN_LATEXMATH_DISPLAY, NULL);
                        text_type = MD_TEXT_LATEXMATH;
                    } else {
                        MD_LEAVE_SPAN((mark->end - off) % 2 ? MD_SPAN_LATEXMATH : MD_SPAN_LATEXMATH_DISPLAY, NULL);
                        text_type = MD_TEXT_NORMAL;
                    }
                    break;

                case '[':       /* Link, wiki link, image. */
                case '!':
                case ']':
                {
                    const MD_MARK* opener = (mark->ch != ']' ? mark : &ctx->marks[mark->prev]);
                    const MD_MARK* closer = &ctx->marks[opener->next];
                    const MD_MARK* dest_mark;
                    const MD_MARK* title_mark;

                    if ((opener->ch == '[' && closer->ch == ']') &&
                        opener->end - opener->beg >= 2 &&
                        closer->end - closer->beg >= 2)
                    {
                        int has_label = (opener->end - opener->beg > 2);
                        SZ target_sz;

                        if(has_label)
                            target_sz = opener->end - (opener->beg+2);
                        else
                            target_sz = closer->beg - opener->end;

                        MD_CHECK(md_enter_leave_span_wikilink(ctx, (mark->ch != ']'),
                                 has_label ? STR(opener->beg+2) : STR(opener->end),
                                 target_sz));

                        break;
                    }

                    dest_mark = opener+1;
                    MD_ASSERT(dest_mark->ch == 'D');
                    title_mark = opener+2;
                    MD_ASSERT(title_mark->ch == 'D');

                    MD_CHECK(md_enter_leave_span_a(ctx, (mark->ch != ']'),
                                (opener->ch == '!' ? MD_SPAN_IMG : MD_SPAN_A),
                                STR(dest_mark->beg), dest_mark->end - dest_mark->beg, FALSE,
                                md_mark_get_ptr(ctx, (int)(title_mark - ctx->marks)),
								title_mark->prev));

                    /* link/image closer may span multiple lines. */
                    if(mark->ch == ']') {
                        while(mark->end > line->end)
                            line++;
                    }

                    break;
                }

                case '<':
                case '>':       /* Autolink or raw HTML. */
                    if(!(mark->flags & MD_MARK_AUTOLINK)) {
                        /* Raw HTML. */
                        if(mark->flags & MD_MARK_OPENER)
                            text_type = MD_TEXT_HTML;
                        else
                            text_type = MD_TEXT_NORMAL;
                        break;
                    }
                    /* Pass through, if auto-link. */
                    MD_FALLTHROUGH();

                case '@':       /* Permissive e-mail autolink. */
                case ':':       /* Permissive URL autolink. */
                case '.':       /* Permissive WWW autolink. */
                {
                    MD_MARK* opener = ((mark->flags & MD_MARK_OPENER) ? mark : &ctx->marks[mark->prev]);
                    MD_MARK* closer = &ctx->marks[opener->next];
                    const CHAR* dest = STR(opener->end);
                    SZ dest_size = closer->beg - opener->end;

                    /* For permissive auto-links we do not know closer mark
                     * position at the time of md_collect_marks(), therefore
                     * it can be out-of-order in ctx->marks[].
                     *
                     * With this flag, we make sure that we output the closer
                     * only if we processed the opener. */
                    if(mark->flags & MD_MARK_OPENER)
                        closer->flags |= MD_MARK_VALIDPERMISSIVEAUTOLINK;

                    if(opener->ch == '@' || opener->ch == '.' ||
                        (opener->ch == '<' && (opener->flags & MD_MARK_AUTOLINK_MISSING_MAILTO)))
                    {
                        dest_size += 7;
                        MD_TEMP_BUFFER(dest_size * sizeof(CHAR));
                        memcpy(ctx->buffer,
                                (opener->ch == '.' ? _T("http://") : _T("mailto:")),
                                7 * sizeof(CHAR));
                        memcpy(ctx->buffer + 7, dest, (dest_size-7) * sizeof(CHAR));
                        dest = ctx->buffer;
                    }

                    if(closer->flags & MD_MARK_VALIDPERMISSIVEAUTOLINK)
                        MD_CHECK(md_enter_leave_span_a(ctx, (mark->flags & MD_MARK_OPENER),
                                    MD_SPAN_A, dest, dest_size, TRUE, NULL, 0));
                    break;
                }

                case '&':       /* Entity. */
                    MD_TEXT(MD_TEXT_ENTITY, STR(mark->beg), mark->end - mark->beg);
                    break;

                case '\0':
                    MD_TEXT(MD_TEXT_NULLCHAR, _T(""), 1);
                    break;

                case 127:
                    goto abort;
            }

            off = mark->end;

            /* Move to next resolved mark. */
            prev_mark = mark;
            mark++;
            while(!(mark->flags & MD_MARK_RESOLVED)  ||  mark->beg < off)
                mark++;
        }

        /* If reached end of line, move to next one. */
        if(off >= line->end) {
            /* If it is the last line, we are done. */
            if(off >= end)
                break;

            if(text_type == MD_TEXT_CODE || text_type == MD_TEXT_LATEXMATH) {
                MD_ASSERT(prev_mark != NULL);
                MD_ASSERT(ISANYOF2_(prev_mark->ch, '`', '$')  &&  (prev_mark->flags & MD_MARK_OPENER));
                MD_ASSERT(ISANYOF2_(mark->ch, '`', '$')  &&  (mark->flags & MD_MARK_CLOSER));

                /* Inside a code span, trailing line whitespace has to be
                 * outputted. */
                tmp = off;
                while(off < ctx->size  &&  ISBLANK(off))
                    off++;
                if(off > tmp)
                    MD_TEXT(text_type, STR(tmp), off-tmp);

                /* and new lines are transformed into single spaces. */
                if(off == line->end)
                    MD_TEXT(text_type, _T(" "), 1);
            } else if(text_type == MD_TEXT_HTML) {
                /* Inside raw HTML, we output the new line verbatim, including
                 * any trailing spaces. */
                tmp = off;
                while(tmp < end  &&  ISBLANK(tmp))
                    tmp++;
                if(tmp > off)
                    MD_TEXT(MD_TEXT_HTML, STR(off), tmp - off);
                MD_TEXT(MD_TEXT_HTML, _T("\n"), 1);
            } else {
                /* Output soft or hard line break. */
                MD_TEXTTYPE break_type = MD_TEXT_SOFTBR;

                if(text_type == MD_TEXT_NORMAL) {
                    if(enforce_hardbreak  ||  (ctx->parser.flags & MD_FLAG_HARD_SOFT_BREAKS)) {
                        break_type = MD_TEXT_BR;
                    } else {
                        while(off < ctx->size  &&  ISBLANK(off))
                            off++;
                        if(off >= line->end + 2  &&  CH(off-2) == _T(' ')  &&  CH(off-1) == _T(' ')  &&  ISNEWLINE(off))
                            break_type = MD_TEXT_BR;
                    }
                }

                MD_TEXT(break_type, _T("\n"), 1);
            }

            /* Move to the next line. */
            line++;
            off = line->beg;

            enforce_hardbreak = 0;
        }
    }

abort:
    return ret;
}


/***************************
 ***  Processing Tables  ***
 ***************************/

static void
md_analyze_table_alignment(MD_CTX* ctx, OFF beg, OFF end, MD_ALIGN* align, int n_align)
{
    static const MD_ALIGN align_map[] = { MD_ALIGN_DEFAULT, MD_ALIGN_LEFT, MD_ALIGN_RIGHT, MD_ALIGN_CENTER };
    OFF off = beg;

    while(n_align > 0) {
        int index = 0;  /* index into align_map[] */

        while(CH(off) != _T('-'))
            off++;
        if(off > beg  &&  CH(off-1) == _T(':'))
            index |= 1;
        while(off < end  &&  CH(off) == _T('-'))
            off++;
        if(off < end  &&  CH(off) == _T(':'))
            index |= 2;

        *align = align_map[index];
        align++;
        n_align--;
    }

}

/* Forward declaration. */
static int md_process_normal_block_contents(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines);

static int
md_process_table_cell(MD_CTX* ctx, MD_BLOCKTYPE cell_type, MD_ALIGN align, OFF beg, OFF end)
{
    MD_LINE line;
    MD_BLOCK_TD_DETAIL det;
    int ret = 0;

    while(beg < end  &&  ISWHITESPACE(beg))
        beg++;
    while(end > beg  &&  ISWHITESPACE(end-1))
        end--;

    det.align = align;
    line.beg = beg;
    line.end = end;

    MD_ENTER_BLOCK(cell_type, &det);
    MD_CHECK(md_process_normal_block_contents(ctx, &line, 1));
    MD_LEAVE_BLOCK(cell_type, &det);

abort:
    return ret;
}

static int
md_process_table_row(MD_CTX* ctx, MD_BLOCKTYPE cell_type, OFF beg, OFF end,
                     const MD_ALIGN* align, int col_count)
{
    MD_LINE line;
    OFF* pipe_offs = NULL;
    int i, j, k, n;
    int ret = 0;

    line.beg = beg;
    line.end = end;

    /* Break the line into table cells by identifying pipe characters who
     * form the cell boundary. */
    MD_CHECK(md_analyze_inlines(ctx, &line, 1, TRUE));

    /* We have to remember the cell boundaries in local buffer because
     * ctx->marks[] shall be reused during cell contents processing. */
    n = ctx->n_table_cell_boundaries + 2;
    pipe_offs = (OFF*) malloc(n * sizeof(OFF));
    if(pipe_offs == NULL) {
        MD_LOG("malloc() failed.");
        ret = -1;
        goto abort;
    }
    j = 0;
    pipe_offs[j++] = beg;
    for(i = ctx->table_cell_boundaries_head; i >= 0; i = ctx->marks[i].next) {
        MD_MARK* mark = &ctx->marks[i];
        pipe_offs[j++] = mark->end;
    }
    pipe_offs[j++] = end+1;

    /* Process cells. */
    MD_ENTER_BLOCK(MD_BLOCK_TR, NULL);
    k = 0;
    for(i = 0; i < j-1  &&  k < col_count; i++) {
        if(pipe_offs[i] < pipe_offs[i+1]-1)
            MD_CHECK(md_process_table_cell(ctx, cell_type, align[k++], pipe_offs[i], pipe_offs[i+1]-1));
    }
    /* Make sure we call enough table cells even if the current table contains
     * too few of them. */
    while(k < col_count)
        MD_CHECK(md_process_table_cell(ctx, cell_type, align[k++], 0, 0));
    MD_LEAVE_BLOCK(MD_BLOCK_TR, NULL);

abort:
    free(pipe_offs);

    ctx->table_cell_boundaries_head = -1;
    ctx->table_cell_boundaries_tail = -1;

    return ret;
}

static int
md_process_table_block_contents(MD_CTX* ctx, int col_count, const MD_LINE* lines, MD_SIZE n_lines)
{
    MD_ALIGN* align;
    MD_SIZE line_index;
    int ret = 0;

    /* At least two lines have to be present: The column headers and the line
     * with the underlines. */
    MD_ASSERT(n_lines >= 2);

    align = malloc(col_count * sizeof(MD_ALIGN));
    if(align == NULL) {
        MD_LOG("malloc() failed.");
        ret = -1;
        goto abort;
    }

    md_analyze_table_alignment(ctx, lines[1].beg, lines[1].end, align, col_count);

    MD_ENTER_BLOCK(MD_BLOCK_THEAD, NULL);
    MD_CHECK(md_process_table_row(ctx, MD_BLOCK_TH,
                        lines[0].beg, lines[0].end, align, col_count));
    MD_LEAVE_BLOCK(MD_BLOCK_THEAD, NULL);

    if(n_lines > 2) {
        MD_ENTER_BLOCK(MD_BLOCK_TBODY, NULL);
        for(line_index = 2; line_index < n_lines; line_index++) {
            MD_CHECK(md_process_table_row(ctx, MD_BLOCK_TD,
                     lines[line_index].beg, lines[line_index].end, align, col_count));
        }
        MD_LEAVE_BLOCK(MD_BLOCK_TBODY, NULL);
    }

abort:
    free(align);
    return ret;
}


/**************************
 ***  Processing Block  ***
 **************************/

#define MD_BLOCK_CONTAINER_OPENER   0x01
#define MD_BLOCK_CONTAINER_CLOSER   0x02
#define MD_BLOCK_CONTAINER          (MD_BLOCK_CONTAINER_OPENER | MD_BLOCK_CONTAINER_CLOSER)
#define MD_BLOCK_LOOSE_LIST         0x04
#define MD_BLOCK_SETEXT_HEADER      0x08

struct MD_BLOCK_tag {
    MD_BLOCKTYPE type  :  8;
    unsigned flags     :  8;

    /* MD_BLOCK_H:      Header level (1 - 6)
     * MD_BLOCK_CODE:   Non-zero if fenced, zero if indented.
     * MD_BLOCK_LI:     Task mark character (0 if not task list item, 'x', 'X' or ' ').
     * MD_BLOCK_TABLE:  Column count (as determined by the table underline).
     */
    unsigned data      : 16;

    /* Leaf blocks:     Count of lines (MD_LINE or MD_VERBATIMLINE) on the block.
     * MD_BLOCK_LI:     Task mark offset in the input doc.
     * MD_BLOCK_OL:     Start item number.
     */
    MD_SIZE n_lines;
};

struct MD_CONTAINER_tag {
    CHAR ch;
    unsigned is_loose    : 8;
    unsigned is_task     : 8;
    unsigned start;
    unsigned mark_indent;
    unsigned contents_indent;
    OFF block_byte_off;
    OFF task_mark_off;
};


static int
md_process_normal_block_contents(MD_CTX* ctx, const MD_LINE* lines, MD_SIZE n_lines)
{
    int i;
    int ret;

    MD_CHECK(md_analyze_inlines(ctx, lines, n_lines, FALSE));
    MD_CHECK(md_process_inlines(ctx, lines, n_lines));

abort:
    /* Free any temporary memory blocks stored within some dummy marks. */
    for(i = ctx->ptr_stack.top; i >= 0; i = ctx->marks[i].next)
        free(md_mark_get_ptr(ctx, i));
    ctx->ptr_stack.top = -1;

    return ret;
}

static int
md_process_verbatim_block_contents(MD_CTX* ctx, MD_TEXTTYPE text_type, const MD_VERBATIMLINE* lines, MD_SIZE n_lines)
{
    static const CHAR indent_chunk_str[] = _T("                ");
    static const SZ indent_chunk_size = SIZEOF_ARRAY(indent_chunk_str) - 1;

    MD_SIZE line_index;
    int ret = 0;

    for(line_index = 0; line_index < n_lines; line_index++) {
        const MD_VERBATIMLINE* line = &lines[line_index];
        int indent = line->indent;

        MD_ASSERT(indent >= 0);

        /* Output code indentation. */
        while(indent > (int) indent_chunk_size) {
            MD_TEXT(text_type, indent_chunk_str, indent_chunk_size);
            indent -= indent_chunk_size;
        }
        if(indent > 0)
            MD_TEXT(text_type, indent_chunk_str, indent);

        /* Output the code line itself. */
        MD_TEXT_INSECURE(text_type, STR(line->beg), line->end - line->beg);

        /* Enforce end-of-line. */
        MD_TEXT(text_type, _T("\n"), 1);
    }

abort:
    return ret;
}

static int
md_process_code_block_contents(MD_CTX* ctx, int is_fenced, const MD_VERBATIMLINE* lines, MD_SIZE n_lines)
{
    if(is_fenced) {
        /* Skip the first line in case of fenced code: It is the fence.
         * (Only the starting fence is present due to logic in md_analyze_line().) */
        lines++;
        n_lines--;
    } else {
        /* Ignore blank lines at start/end of indented code block. */
        while(n_lines > 0  &&  lines[0].beg == lines[0].end) {
            lines++;
            n_lines--;
        }
        while(n_lines > 0  &&  lines[n_lines-1].beg == lines[n_lines-1].end) {
            n_lines--;
        }
    }

    if(n_lines == 0)
        return 0;

    return md_process_verbatim_block_contents(ctx, MD_TEXT_CODE, lines, n_lines);
}

static int
md_setup_fenced_code_detail(MD_CTX* ctx, const MD_BLOCK* block, MD_BLOCK_CODE_DETAIL* det,
                            MD_ATTRIBUTE_BUILD* info_build, MD_ATTRIBUTE_BUILD* lang_build)
{
    const MD_VERBATIMLINE* fence_line = (const MD_VERBATIMLINE*)(block + 1);
    OFF beg = fence_line->beg;
    OFF end = fence_line->end;
    OFF lang_end;
    CHAR fence_ch = CH(fence_line->beg);
    int ret = 0;

    /* Skip the fence itself. */
    while(beg < ctx->size  &&  CH(beg) == fence_ch)
        beg++;
    /* Trim initial spaces. */
    while(beg < ctx->size  &&  CH(beg) == _T(' '))
        beg++;

    /* Trim trailing spaces. */
    while(end > beg  &&  CH(end-1) == _T(' '))
        end--;

    /* Build info string attribute. */
    MD_CHECK(md_build_attribute(ctx, STR(beg), end - beg, 0, &det->info, info_build));

    /* Build info string attribute. */
    lang_end = beg;
    while(lang_end < end  &&  !ISWHITESPACE(lang_end))
        lang_end++;
    MD_CHECK(md_build_attribute(ctx, STR(beg), lang_end - beg, 0, &det->lang, lang_build));

    det->fence_char = fence_ch;

abort:
    return ret;
}

static int
md_process_leaf_block(MD_CTX* ctx, const MD_BLOCK* block)
{
    union {
        MD_BLOCK_H_DETAIL header;
        MD_BLOCK_CODE_DETAIL code;
        MD_BLOCK_TABLE_DETAIL table;
    } det;
    MD_ATTRIBUTE_BUILD info_build;
    MD_ATTRIBUTE_BUILD lang_build;
    int is_in_tight_list;
    int clean_fence_code_detail = FALSE;
    int ret = 0;

    memset(&det, 0, sizeof(det));

    if(ctx->n_containers == 0)
        is_in_tight_list = FALSE;
    else
        is_in_tight_list = !ctx->containers[ctx->n_containers-1].is_loose;

    switch(block->type) {
        case MD_BLOCK_H:
            det.header.level = block->data;
            break;

        case MD_BLOCK_CODE:
            /* For fenced code block, we may need to set the info string. */
            if(block->data != 0) {
                memset(&det.code, 0, sizeof(MD_BLOCK_CODE_DETAIL));
                clean_fence_code_detail = TRUE;
                MD_CHECK(md_setup_fenced_code_detail(ctx, block, &det.code, &info_build, &lang_build));
            }
            break;

        case MD_BLOCK_TABLE:
            det.table.col_count = block->data;
            det.table.head_row_count = 1;
            det.table.body_row_count = block->n_lines - 2;
            break;

        default:
            /* Noop. */
            break;
    }

    if(!is_in_tight_list  ||  block->type != MD_BLOCK_P)
        MD_ENTER_BLOCK(block->type, (void*) &det);

    /* Process the block contents accordingly to is type. */
    switch(block->type) {
        case MD_BLOCK_HR:
            /* noop */
            break;

        case MD_BLOCK_CODE:
            MD_CHECK(md_process_code_block_contents(ctx, (block->data != 0),
                            (const MD_VERBATIMLINE*)(block + 1), block->n_lines));
            break;

        case MD_BLOCK_HTML:
            MD_CHECK(md_process_verbatim_block_contents(ctx, MD_TEXT_HTML,
                            (const MD_VERBATIMLINE*)(block + 1), block->n_lines));
            break;

        case MD_BLOCK_TABLE:
            MD_CHECK(md_process_table_block_contents(ctx, block->data,
                            (const MD_LINE*)(block + 1), block->n_lines));
            break;

        default:
            MD_CHECK(md_process_normal_block_contents(ctx,
                            (const MD_LINE*)(block + 1), block->n_lines));
            break;
    }

    if(!is_in_tight_list  ||  block->type != MD_BLOCK_P)
        MD_LEAVE_BLOCK(block->type, (void*) &det);

abort:
    if(clean_fence_code_detail) {
        md_free_attribute(ctx, &info_build);
        md_free_attribute(ctx, &lang_build);
    }
    return ret;
}

static int
md_process_all_blocks(MD_CTX* ctx)
{
    int byte_off = 0;
    int ret = 0;

    /* ctx->containers now is not needed for detection of lists and list items
     * so we reuse it for tracking what lists are loose or tight. We rely
     * on the fact the vector is large enough to hold the deepest nesting
     * level of lists. */
    ctx->n_containers = 0;

    while(byte_off < ctx->n_block_bytes) {
        MD_BLOCK* block = (MD_BLOCK*)((char*)ctx->block_bytes + byte_off);
        union {
            MD_BLOCK_UL_DETAIL ul;
            MD_BLOCK_OL_DETAIL ol;
            MD_BLOCK_LI_DETAIL li;
        } det;

        switch(block->type) {
            case MD_BLOCK_UL:
                det.ul.is_tight = (block->flags & MD_BLOCK_LOOSE_LIST) ? FALSE : TRUE;
                det.ul.mark = (CHAR) block->data;
                break;

            case MD_BLOCK_OL:
                det.ol.start = block->n_lines;
                det.ol.is_tight =  (block->flags & MD_BLOCK_LOOSE_LIST) ? FALSE : TRUE;
                det.ol.mark_delimiter = (CHAR) block->data;
                break;

            case MD_BLOCK_LI:
                det.li.is_task = (block->data != 0);
                det.li.task_mark = (CHAR) block->data;
                det.li.task_mark_offset = (OFF) block->n_lines;
                break;

            default:
                /* noop */
                break;
        }

        if(block->flags & MD_BLOCK_CONTAINER) {
            if(block->flags & MD_BLOCK_CONTAINER_CLOSER) {
                MD_LEAVE_BLOCK(block->type, &det);

                if(block->type == MD_BLOCK_UL || block->type == MD_BLOCK_OL || block->type == MD_BLOCK_QUOTE)
                    ctx->n_containers--;
            }

            if(block->flags & MD_BLOCK_CONTAINER_OPENER) {
                MD_ENTER_BLOCK(block->type, &det);

                if(block->type == MD_BLOCK_UL || block->type == MD_BLOCK_OL) {
                    ctx->containers[ctx->n_containers].is_loose = (block->flags & MD_BLOCK_LOOSE_LIST);
                    ctx->n_containers++;
                } else if(block->type == MD_BLOCK_QUOTE) {
                    /* This causes that any text in a block quote, even if
                     * nested inside a tight list item, is wrapped with
                     * <p>...</p>. */
                    ctx->containers[ctx->n_containers].is_loose = TRUE;
                    ctx->n_containers++;
                }
            }
        } else {
            MD_CHECK(md_process_leaf_block(ctx, block));

            if(block->type == MD_BLOCK_CODE || block->type == MD_BLOCK_HTML)
                byte_off += block->n_lines * sizeof(MD_VERBATIMLINE);
            else
                byte_off += block->n_lines * sizeof(MD_LINE);
        }

        byte_off += sizeof(MD_BLOCK);
    }

    ctx->n_block_bytes = 0;

abort:
    return ret;
}


/************************************
 ***  Grouping Lines into Blocks  ***
 ************************************/

static void*
md_push_block_bytes(MD_CTX* ctx, int n_bytes)
{
    void* ptr;

    if(ctx->n_block_bytes + n_bytes > ctx->alloc_block_bytes) {
        void* new_block_bytes;

        ctx->alloc_block_bytes = (ctx->alloc_block_bytes > 0
                ? ctx->alloc_block_bytes + ctx->alloc_block_bytes / 2
                : 512);
        new_block_bytes = realloc(ctx->block_bytes, ctx->alloc_block_bytes);
        if(new_block_bytes == NULL) {
            MD_LOG("realloc() failed.");
            return NULL;
        }

        /* Fix the ->current_block after the reallocation. */
        if(ctx->current_block != NULL) {
            OFF off_current_block = (OFF) ((char*) ctx->current_block - (char*) ctx->block_bytes);
            ctx->current_block = (MD_BLOCK*) ((char*) new_block_bytes + off_current_block);
        }

        ctx->block_bytes = new_block_bytes;
    }

    ptr = (char*)ctx->block_bytes + ctx->n_block_bytes;
    ctx->n_block_bytes += n_bytes;
    return ptr;
}

static int
md_start_new_block(MD_CTX* ctx, const MD_LINE_ANALYSIS* line)
{
    MD_BLOCK* block;

    MD_ASSERT(ctx->current_block == NULL);

    block = (MD_BLOCK*) md_push_block_bytes(ctx, sizeof(MD_BLOCK));
    if(block == NULL)
        return -1;

    switch(line->type) {
        case MD_LINE_HR:
            block->type = MD_BLOCK_HR;
            break;

        case MD_LINE_ATXHEADER:
        case MD_LINE_SETEXTHEADER:
            block->type = MD_BLOCK_H;
            break;

        case MD_LINE_FENCEDCODE:
        case MD_LINE_INDENTEDCODE:
            block->type = MD_BLOCK_CODE;
            break;

        case MD_LINE_TEXT:
            block->type = MD_BLOCK_P;
            break;

        case MD_LINE_HTML:
            block->type = MD_BLOCK_HTML;
            break;

        case MD_LINE_BLANK:
        case MD_LINE_SETEXTUNDERLINE:
        case MD_LINE_TABLEUNDERLINE:
        default:
            MD_UNREACHABLE();
            break;
    }

    block->flags = 0;
    block->data = line->data;
    block->n_lines = 0;

    ctx->current_block = block;
    return 0;
}

/* Eat from start of current (textual) block any reference definitions and
 * remember them so we can resolve any links referring to them.
 *
 * (Reference definitions can only be at start of it as they cannot break
 * a paragraph.)
 */
static int
md_consume_link_reference_definitions(MD_CTX* ctx)
{
    MD_LINE* lines = (MD_LINE*) (ctx->current_block + 1);
    MD_SIZE n_lines = ctx->current_block->n_lines;
    MD_SIZE n = 0;

    /* Compute how many lines at the start of the block form one or more
     * reference definitions. */
    while(n < n_lines) {
        int n_link_ref_lines;

        n_link_ref_lines = md_is_link_reference_definition(ctx,
                                    lines + n, n_lines - n);
        /* Not a reference definition? */
        if(n_link_ref_lines == 0)
            break;

        /* We fail if it is the ref. def. but it could not be stored due
         * a memory allocation error. */
        if(n_link_ref_lines < 0)
            return -1;

        n += n_link_ref_lines;
    }

    /* If there was at least one reference definition, we need to remove
     * its lines from the block, or perhaps even the whole block. */
    if(n > 0) {
        if(n == n_lines) {
            /* Remove complete block. */
            ctx->n_block_bytes -= n * sizeof(MD_LINE);
            ctx->n_block_bytes -= sizeof(MD_BLOCK);
            ctx->current_block = NULL;
        } else {
            /* Remove just some initial lines from the block. */
            memmove(lines, lines + n, (n_lines - n) * sizeof(MD_LINE));
            ctx->current_block->n_lines -= n;
            ctx->n_block_bytes -= n * sizeof(MD_LINE);
        }
    }

    return 0;
}

static int
md_end_current_block(MD_CTX* ctx)
{
    int ret = 0;

    if(ctx->current_block == NULL)
        return ret;

    /* Check whether there is a reference definition. (We do this here instead
     * of in md_analyze_line() because reference definition can take multiple
     * lines.) */
    if(ctx->current_block->type == MD_BLOCK_P  ||
       (ctx->current_block->type == MD_BLOCK_H  &&  (ctx->current_block->flags & MD_BLOCK_SETEXT_HEADER)))
    {
        MD_LINE* lines = (MD_LINE*) (ctx->current_block + 1);
        if(lines[0].beg < ctx->size  &&  CH(lines[0].beg) == _T('[')) {
            MD_CHECK(md_consume_link_reference_definitions(ctx));
            if(ctx->current_block == NULL)
                return ret;
        }
    }

    if(ctx->current_block->type == MD_BLOCK_H  &&  (ctx->current_block->flags & MD_BLOCK_SETEXT_HEADER)) {
        MD_SIZE n_lines = ctx->current_block->n_lines;

        if(n_lines > 1) {
            /* Get rid of the underline. */
            ctx->current_block->n_lines--;
            ctx->n_block_bytes -= sizeof(MD_LINE);
        } else {
            /* Only the underline has left after eating the ref. defs.
             * Keep the line as beginning of a new ordinary paragraph. */
            ctx->current_block->type = MD_BLOCK_P;
            return 0;
        }
    }

    /* Mark we are not building any block anymore. */
    ctx->current_block = NULL;

abort:
    return ret;
}

static int
md_add_line_into_current_block(MD_CTX* ctx, const MD_LINE_ANALYSIS* analysis)
{
    MD_ASSERT(ctx->current_block != NULL);

    if(ctx->current_block->type == MD_BLOCK_CODE || ctx->current_block->type == MD_BLOCK_HTML) {
        MD_VERBATIMLINE* line;

        line = (MD_VERBATIMLINE*) md_push_block_bytes(ctx, sizeof(MD_VERBATIMLINE));
        if(line == NULL)
            return -1;

        line->indent = analysis->indent;
        line->beg = analysis->beg;
        line->end = analysis->end;
    } else {
        MD_LINE* line;

        line = (MD_LINE*) md_push_block_bytes(ctx, sizeof(MD_LINE));
        if(line == NULL)
            return -1;

        line->beg = analysis->beg;
        line->end = analysis->end;
    }
    ctx->current_block->n_lines++;

    return 0;
}

static int
md_push_container_bytes(MD_CTX* ctx, MD_BLOCKTYPE type, unsigned start,
                        unsigned data, unsigned flags)
{
    MD_BLOCK* block;
    int ret = 0;

    MD_CHECK(md_end_current_block(ctx));

    block = (MD_BLOCK*) md_push_block_bytes(ctx, sizeof(MD_BLOCK));
    if(block == NULL)
        return -1;

    block->type = type;
    block->flags = flags;
    block->data = data;
    block->n_lines = start;

abort:
    return ret;
}



/***********************
 ***  Line Analysis  ***
 ***********************/

static int
md_is_hr_line(MD_CTX* ctx, OFF beg, OFF* p_end, OFF* p_killer)
{
    OFF off = beg + 1;
    int n = 1;

    while(off < ctx->size  &&  (CH(off) == CH(beg) || CH(off) == _T(' ') || CH(off) == _T('\t'))) {
        if(CH(off) == CH(beg))
            n++;
        off++;
    }

    if(n < 3) {
        *p_killer = off;
        return FALSE;
    }

    /* Nothing else can be present on the line. */
    if(off < ctx->size  &&  !ISNEWLINE(off)) {
        *p_killer = off;
        return FALSE;
    }

    *p_end = off;
    return TRUE;
}

static int
md_is_atxheader_line(MD_CTX* ctx, OFF beg, OFF* p_beg, OFF* p_end, unsigned* p_level)
{
    int n;
    OFF off = beg + 1;

    while(off < ctx->size  &&  CH(off) == _T('#')  &&  off - beg < 7)
        off++;
    n = off - beg;

    if(n > 6)
        return FALSE;
    *p_level = n;

    if(!(ctx->parser.flags & MD_FLAG_PERMISSIVEATXHEADERS)  &&  off < ctx->size  &&
       !ISBLANK(off)  &&  !ISNEWLINE(off))
        return FALSE;

    while(off < ctx->size  &&  ISBLANK(off))
        off++;
    *p_beg = off;
    *p_end = off;
    return TRUE;
}

static int
md_is_setext_underline(MD_CTX* ctx, OFF beg, OFF* p_end, unsigned* p_level)
{
    OFF off = beg + 1;

    while(off < ctx->size  &&  CH(off) == CH(beg))
        off++;

    /* Optionally, space(s) or tabs can follow. */
    while(off < ctx->size  &&  ISBLANK(off))
        off++;

    /* But nothing more is allowed on the line. */
    if(off < ctx->size  &&  !ISNEWLINE(off))
        return FALSE;

    *p_level = (CH(beg) == _T('=') ? 1 : 2);
    *p_end = off;
    return TRUE;
}

static int
md_is_table_underline(MD_CTX* ctx, OFF beg, OFF* p_end, unsigned* p_col_count)
{
    OFF off = beg;
    int found_pipe = FALSE;
    unsigned col_count = 0;

    if(off < ctx->size  &&  CH(off) == _T('|')) {
        found_pipe = TRUE;
        off++;
        while(off < ctx->size  &&  ISWHITESPACE(off))
            off++;
    }

    while(1) {
        int delimited = FALSE;

        /* Cell underline ("-----", ":----", "----:" or ":----:") */
        if(off < ctx->size  &&  CH(off) == _T(':'))
            off++;
        if(off >= ctx->size  ||  CH(off) != _T('-'))
            return FALSE;
        while(off < ctx->size  &&  CH(off) == _T('-'))
            off++;
        if(off < ctx->size  &&  CH(off) == _T(':'))
            off++;

        col_count++;
        if(col_count > TABLE_MAXCOLCOUNT) {
            MD_LOG("Suppressing table (column_count >" STRINGIZE(TABLE_MAXCOLCOUNT) ")");
            return FALSE;
        }

        /* Pipe delimiter (optional at the end of line). */
        while(off < ctx->size  &&  ISWHITESPACE(off))
            off++;
        if(off < ctx->size  &&  CH(off) == _T('|')) {
            delimited = TRUE;
            found_pipe =  TRUE;
            off++;
            while(off < ctx->size  &&  ISWHITESPACE(off))
                off++;
        }

        /* Success, if we reach end of line. */
        if(off >= ctx->size  ||  ISNEWLINE(off))
            break;

        if(!delimited)
            return FALSE;
    }

    if(!found_pipe)
        return FALSE;

    *p_end = off;
    *p_col_count = col_count;
    return TRUE;
}

static int
md_is_opening_code_fence(MD_CTX* ctx, OFF beg, OFF* p_end)
{
    OFF off = beg;

    while(off < ctx->size && CH(off) == CH(beg))
        off++;

    /* Fence must have at least three characters. */
    if(off - beg < 3)
        return FALSE;

    ctx->code_fence_length = off - beg;

    /* Optionally, space(s) can follow. */
    while(off < ctx->size  &&  CH(off) == _T(' '))
        off++;

    /* Optionally, an info string can follow. */
    while(off < ctx->size  &&  !ISNEWLINE(off)) {
        /* Backtick-based fence must not contain '`' in the info string. */
        if(CH(beg) == _T('`')  &&  CH(off) == _T('`'))
            return FALSE;
        off++;
    }

    *p_end = off;
    return TRUE;
}

static int
md_is_closing_code_fence(MD_CTX* ctx, CHAR ch, OFF beg, OFF* p_end)
{
    OFF off = beg;
    int ret = FALSE;

    /* Closing fence must have at least the same length and use same char as
     * opening one. */
    while(off < ctx->size  &&  CH(off) == ch)
        off++;
    if(off - beg < ctx->code_fence_length)
        goto out;

    /* Optionally, space(s) can follow */
    while(off < ctx->size  &&  CH(off) == _T(' '))
        off++;

    /* But nothing more is allowed on the line. */
    if(off < ctx->size  &&  !ISNEWLINE(off))
        goto out;

    ret = TRUE;

out:
    /* Note we set *p_end even on failure: If we are not closing fence, caller
     * would eat the line anyway without any parsing. */
    *p_end = off;
    return ret;
}


/* Helper data for md_is_html_block_start_condition() and
 * md_is_html_block_end_condition() */
typedef struct TAG_tag TAG;
struct TAG_tag {
    const CHAR* name;
    unsigned len    : 8;
};

#ifdef X
    #undef X
#endif
#define X(name)     { _T(name), (sizeof(name)-1) / sizeof(CHAR) }
#define Xend        { NULL, 0 }

static const TAG t1[] = { X("pre"), X("script"), X("style"), X("textarea"), Xend };

static const TAG a6[] = { X("address"), X("article"), X("aside"), Xend };
static const TAG b6[] = { X("base"), X("basefont"), X("blockquote"), X("body"), Xend };
static const TAG c6[] = { X("caption"), X("center"), X("col"), X("colgroup"), Xend };
static const TAG d6[] = { X("dd"), X("details"), X("dialog"), X("dir"),
                          X("div"), X("dl"), X("dt"), Xend };
static const TAG f6[] = { X("fieldset"), X("figcaption"), X("figure"), X("footer"),
                          X("form"), X("frame"), X("frameset"), Xend };
static const TAG h6[] = { X("h1"), X("h2"), X("h3"), X("h4"), X("h5"), X("h6"),
                          X("head"), X("header"), X("hr"), X("html"), Xend };
static const TAG i6[] = { X("iframe"), Xend };
static const TAG l6[] = { X("legend"), X("li"), X("link"), Xend };
static const TAG m6[] = { X("main"), X("menu"), X("menuitem"), Xend };
static const TAG n6[] = { X("nav"), X("noframes"), Xend };
static const TAG o6[] = { X("ol"), X("optgroup"), X("option"), Xend };
static const TAG p6[] = { X("p"), X("param"), Xend };
static const TAG s6[] = { X("search"), X("section"), X("summary"), Xend };
static const TAG t6[] = { X("table"), X("tbody"), X("td"), X("tfoot"), X("th"),
                          X("thead"), X("title"), X("tr"), X("track"), Xend };
static const TAG u6[] = { X("ul"), Xend };
static const TAG xx[] = { Xend };

#undef X
#undef Xend

/* Returns type of the raw HTML block, or FALSE if it is not HTML block.
 * (Refer to CommonMark specification for details about the types.)
 */
static int
md_is_html_block_start_condition(MD_CTX* ctx, OFF beg)
{
    /* Type 6 is started by a long list of allowed tags. We use two-level
     * tree to speed-up the search. */
    static const TAG* map6[26] = {
        a6, b6, c6, d6, xx, f6, xx, h6, i6, xx, xx, l6, m6,
        n6, o6, p6, xx, xx, s6, t6, u6, xx, xx, xx, xx, xx
    };
    OFF off = beg + 1;
    int i;

    /* Check for type 1: <script, <pre, or <style */
    for(i = 0; t1[i].name != NULL; i++) {
        if(off + t1[i].len <= ctx->size) {
            if(md_ascii_case_eq(STR(off), t1[i].name, t1[i].len))
                return 1;
        }
    }

    /* Check for type 2: <!-- */
    if(off + 3 < ctx->size  &&  CH(off) == _T('!')  &&  CH(off+1) == _T('-')  &&  CH(off+2) == _T('-'))
        return 2;

    /* Check for type 3: <? */
    if(off < ctx->size  &&  CH(off) == _T('?'))
        return 3;

    /* Check for type 4 or 5: <! */
    if(off < ctx->size  &&  CH(off) == _T('!')) {
        /* Check for type 4: <! followed by uppercase letter. */
        if(off + 1 < ctx->size  &&  ISASCII(off+1))
            return 4;

        /* Check for type 5: <![CDATA[ */
        if(off + 8 < ctx->size) {
            if(md_ascii_eq(STR(off), _T("![CDATA["), 8))
                return 5;
        }
    }

    /* Check for type 6: Many possible starting tags listed above. */
    if(off + 1 < ctx->size  &&  (ISALPHA(off) || (CH(off) == _T('/') && ISALPHA(off+1)))) {
        int slot;
        const TAG* tags;

        if(CH(off) == _T('/'))
            off++;

        slot = (ISUPPER(off) ? CH(off) - 'A' : CH(off) - 'a');
        tags = map6[slot];

        for(i = 0; tags[i].name != NULL; i++) {
            if(off + tags[i].len <= ctx->size) {
                if(md_ascii_case_eq(STR(off), tags[i].name, tags[i].len)) {
                    OFF tmp = off + tags[i].len;
                    if(tmp >= ctx->size)
                        return 6;
                    if(ISBLANK(tmp) || ISNEWLINE(tmp) || CH(tmp) == _T('>'))
                        return 6;
                    if(tmp+1 < ctx->size && CH(tmp) == _T('/') && CH(tmp+1) == _T('>'))
                        return 6;
                    break;
                }
            }
        }
    }

    /* Check for type 7: any COMPLETE other opening or closing tag. */
    if(off + 1 < ctx->size) {
        OFF end;

        if(md_is_html_tag(ctx, NULL, 0, beg, ctx->size, &end)) {
            /* Only optional whitespace and new line may follow. */
            while(end < ctx->size  &&  ISWHITESPACE(end))
                end++;
            if(end >= ctx->size  ||  ISNEWLINE(end))
                return 7;
        }
    }

    return FALSE;
}

/* Case sensitive check whether there is a substring 'what' between 'beg'
 * and end of line. */
static int
md_line_contains(MD_CTX* ctx, OFF beg, const CHAR* what, SZ what_len, OFF* p_end)
{
    OFF i;
    for(i = beg; i + what_len < ctx->size; i++) {
        if(ISNEWLINE(i))
            break;
        if(memcmp(STR(i), what, what_len * sizeof(CHAR)) == 0) {
            *p_end = i + what_len;
            return TRUE;
        }
    }

    *p_end = i;
    return FALSE;
}

/* Returns type of HTML block end condition or FALSE if not an end condition.
 *
 * Note it fills p_end even when it is not end condition as the caller
 * does not need to analyze contents of a raw HTML block.
 */
static int
md_is_html_block_end_condition(MD_CTX* ctx, OFF beg, OFF* p_end)
{
    switch(ctx->html_block_type) {
        case 1:
        {
            OFF off = beg;
            int i;

            while(off+1 < ctx->size  &&  !ISNEWLINE(off)) {
                if(CH(off) == _T('<')  &&  CH(off+1) == _T('/')) {
                    for(i = 0; t1[i].name != NULL; i++) {
                        if(off + 2 + t1[i].len < ctx->size) {
                            if(md_ascii_case_eq(STR(off+2), t1[i].name, t1[i].len)  &&
                               CH(off+2+t1[i].len) == _T('>'))
                            {
                                *p_end = off+2+t1[i].len+1;
                                return TRUE;
                            }
                        }
                    }
                }
                off++;
            }
            *p_end = off;
            return FALSE;
        }

        case 2:
            return (md_line_contains(ctx, beg, _T("-->"), 3, p_end) ? 2 : FALSE);

        case 3:
            return (md_line_contains(ctx, beg, _T("?>"), 2, p_end) ? 3 : FALSE);

        case 4:
            return (md_line_contains(ctx, beg, _T(">"), 1, p_end) ? 4 : FALSE);

        case 5:
            return (md_line_contains(ctx, beg, _T("]]>"), 3, p_end) ? 5 : FALSE);

        case 6:     /* Pass through */
        case 7:
            if(beg >= ctx->size  ||  ISNEWLINE(beg)) {
                /* Blank line ends types 6 and 7. */
                *p_end = beg;
                return ctx->html_block_type;
            }
            return FALSE;

        default:
            MD_UNREACHABLE();
    }
    return FALSE;
}


static int
md_is_container_compatible(const MD_CONTAINER* pivot, const MD_CONTAINER* container)
{
    /* Block quote has no "items" like lists. */
    if(container->ch == _T('>'))
        return FALSE;

    if(container->ch != pivot->ch)
        return FALSE;
    if(container->mark_indent > pivot->contents_indent)
        return FALSE;

    return TRUE;
}

static int
md_push_container(MD_CTX* ctx, const MD_CONTAINER* container)
{
    if(ctx->n_containers >= ctx->alloc_containers) {
        MD_CONTAINER* new_containers;

        ctx->alloc_containers = (ctx->alloc_containers > 0
                ? ctx->alloc_containers + ctx->alloc_containers / 2
                : 16);
        new_containers = realloc(ctx->containers, ctx->alloc_containers * sizeof(MD_CONTAINER));
        if(new_containers == NULL) {
            MD_LOG("realloc() failed.");
            return -1;
        }

        ctx->containers = new_containers;
    }

    memcpy(&ctx->containers[ctx->n_containers++], container, sizeof(MD_CONTAINER));
    return 0;
}

static int
md_enter_child_containers(MD_CTX* ctx, int n_children)
{
    int i;
    int ret = 0;

    for(i = ctx->n_containers - n_children; i < ctx->n_containers; i++) {
        MD_CONTAINER* c = &ctx->containers[i];
        int is_ordered_list = FALSE;

        switch(c->ch) {
            case _T(')'):
            case _T('.'):
                is_ordered_list = TRUE;
                MD_FALLTHROUGH();

            case _T('-'):
            case _T('+'):
            case _T('*'):
                /* Remember offset in ctx->block_bytes so we can revisit the
                 * block if we detect it is a loose list. */
                md_end_current_block(ctx);
                c->block_byte_off = ctx->n_block_bytes;

                MD_CHECK(md_push_container_bytes(ctx,
                                (is_ordered_list ? MD_BLOCK_OL : MD_BLOCK_UL),
                                c->start, c->ch, MD_BLOCK_CONTAINER_OPENER));
                MD_CHECK(md_push_container_bytes(ctx, MD_BLOCK_LI,
                                c->task_mark_off,
                                (c->is_task ? CH(c->task_mark_off) : 0),
                                MD_BLOCK_CONTAINER_OPENER));
                break;

            case _T('>'):
                MD_CHECK(md_push_container_bytes(ctx, MD_BLOCK_QUOTE, 0, 0, MD_BLOCK_CONTAINER_OPENER));
                break;

            default:
                MD_UNREACHABLE();
                break;
        }
    }

abort:
    return ret;
}

static int
md_leave_child_containers(MD_CTX* ctx, int n_keep)
{
    int ret = 0;

    while(ctx->n_containers > n_keep) {
        MD_CONTAINER* c = &ctx->containers[ctx->n_containers-1];
        int is_ordered_list = FALSE;

        switch(c->ch) {
            case _T(')'):
            case _T('.'):
                is_ordered_list = TRUE;
                MD_FALLTHROUGH();

            case _T('-'):
            case _T('+'):
            case _T('*'):
                MD_CHECK(md_push_container_bytes(ctx, MD_BLOCK_LI,
                                c->task_mark_off, (c->is_task ? CH(c->task_mark_off) : 0),
                                MD_BLOCK_CONTAINER_CLOSER));
                MD_CHECK(md_push_container_bytes(ctx,
                                (is_ordered_list ? MD_BLOCK_OL : MD_BLOCK_UL), 0,
                                c->ch, MD_BLOCK_CONTAINER_CLOSER));
                break;

            case _T('>'):
                MD_CHECK(md_push_container_bytes(ctx, MD_BLOCK_QUOTE, 0,
                                0, MD_BLOCK_CONTAINER_CLOSER));
                break;

            default:
                MD_UNREACHABLE();
                break;
        }

        ctx->n_containers--;
    }

abort:
    return ret;
}

static int
md_is_container_mark(MD_CTX* ctx, unsigned indent, OFF beg, OFF* p_end, MD_CONTAINER* p_container)
{
    OFF off = beg;
    OFF max_end;

    if(off >= ctx->size  ||  indent >= ctx->code_indent_offset)
        return FALSE;

    /* Check for block quote mark. */
    if(CH(off) == _T('>')) {
        off++;
        p_container->ch = _T('>');
        p_container->is_loose = FALSE;
        p_container->is_task = FALSE;
        p_container->mark_indent = indent;
        p_container->contents_indent = indent + 1;
        *p_end = off;
        return TRUE;
    }

    /* Check for list item bullet mark. */
    if(ISANYOF(off, _T("-+*"))  &&  (off+1 >= ctx->size || ISBLANK(off+1) || ISNEWLINE(off+1))) {
        p_container->ch = CH(off);
        p_container->is_loose = FALSE;
        p_container->is_task = FALSE;
        p_container->mark_indent = indent;
        p_container->contents_indent = indent + 1;
        *p_end = off+1;
        return TRUE;
    }

    /* Check for ordered list item marks. */
    max_end = off + 9;
    if(max_end > ctx->size)
        max_end = ctx->size;
    p_container->start = 0;
    while(off < max_end  &&  ISDIGIT(off)) {
        p_container->start = p_container->start * 10 + CH(off) - _T('0');
        off++;
    }
    if(off > beg  &&
       off < ctx->size  &&
       (CH(off) == _T('.') || CH(off) == _T(')'))  &&
       (off+1 >= ctx->size || ISBLANK(off+1) || ISNEWLINE(off+1)))
    {
        p_container->ch = CH(off);
        p_container->is_loose = FALSE;
        p_container->is_task = FALSE;
        p_container->mark_indent = indent;
        p_container->contents_indent = indent + off - beg + 1;
        *p_end = off+1;
        return TRUE;
    }

    return FALSE;
}

static unsigned
md_line_indentation(MD_CTX* ctx, unsigned total_indent, OFF beg, OFF* p_end)
{
    OFF off = beg;
    unsigned indent = total_indent;

    while(off < ctx->size  &&  ISBLANK(off)) {
        if(CH(off) == _T('\t'))
            indent = (indent + 4) & ~3;
        else
            indent++;
        off++;
    }

    *p_end = off;
    return indent - total_indent;
}

static const MD_LINE_ANALYSIS md_dummy_blank_line = { MD_LINE_BLANK, 0, 0, 0, 0, 0 };

/* Analyze type of the line and find some its properties. This serves as a
 * main input for determining type and boundaries of a block. */
static int
md_analyze_line(MD_CTX* ctx, OFF beg, OFF* p_end,
                const MD_LINE_ANALYSIS* pivot_line, MD_LINE_ANALYSIS* line)
{
    unsigned total_indent = 0;
    int n_parents = 0;
    int n_brothers = 0;
    int n_children = 0;
    MD_CONTAINER container = { 0 };
    int prev_line_has_list_loosening_effect = ctx->last_line_has_list_loosening_effect;
    OFF off = beg;
    OFF hr_killer = 0;
    int ret = 0;

    line->indent = md_line_indentation(ctx, total_indent, off, &off);
    total_indent += line->indent;
    line->beg = off;
    line->enforce_new_block = FALSE;

    /* Given the indentation and block quote marks '>', determine how many of
     * the current containers are our parents. */
    while(n_parents < ctx->n_containers) {
        MD_CONTAINER* c = &ctx->containers[n_parents];

        if(c->ch == _T('>')  &&  line->indent < ctx->code_indent_offset  &&
            off < ctx->size  &&  CH(off) == _T('>'))
        {
            /* Block quote mark. */
            off++;
            total_indent++;
            line->indent = md_line_indentation(ctx, total_indent, off, &off);
            total_indent += line->indent;

            /* The optional 1st space after '>' is part of the block quote mark. */
            if(line->indent > 0)
                line->indent--;

            line->beg = off;

        } else if(c->ch != _T('>')  &&  line->indent >= c->contents_indent) {
            /* List. */
            line->indent -= c->contents_indent;
        } else {
            break;
        }

        n_parents++;
    }

    if(off >= ctx->size  ||  ISNEWLINE(off)) {
        /* Blank line does not need any real indentation to be nested inside
         * a list. */
        if(n_brothers + n_children == 0) {
            while(n_parents < ctx->n_containers  &&  ctx->containers[n_parents].ch != _T('>'))
                n_parents++;
        }
    }

    while(TRUE) {
        /* Check whether we are fenced code continuation. */
        if(pivot_line->type == MD_LINE_FENCEDCODE) {
            line->beg = off;

            /* We are another MD_LINE_FENCEDCODE unless we are closing fence
             * which we transform into MD_LINE_BLANK. */
            if(line->indent < ctx->code_indent_offset) {
                if(md_is_closing_code_fence(ctx, CH(pivot_line->beg), off, &off)) {
                    line->type = MD_LINE_BLANK;
                    ctx->last_line_has_list_loosening_effect = FALSE;
                    break;
                }
            }

            /* Change indentation accordingly to the initial code fence. */
            if(n_parents == ctx->n_containers) {
                if(line->indent > pivot_line->indent)
                    line->indent -= pivot_line->indent;
                else
                    line->indent = 0;

                line->type = MD_LINE_FENCEDCODE;
                break;
            }
        }

        /* Check whether we are HTML block continuation. */
        if(pivot_line->type == MD_LINE_HTML  &&  ctx->html_block_type > 0) {
            if(n_parents < ctx->n_containers) {
                /* HTML block is implicitly ended if the enclosing container
                 * block ends. */
                ctx->html_block_type = 0;
            } else {
                int html_block_type;

                html_block_type = md_is_html_block_end_condition(ctx, off, &off);
                if(html_block_type > 0) {
                    MD_ASSERT(html_block_type == ctx->html_block_type);

                    /* Make sure this is the last line of the block. */
                    ctx->html_block_type = 0;

                    /* Some end conditions serve as blank lines at the same time. */
                    if(html_block_type == 6 || html_block_type == 7) {
                        line->type = MD_LINE_BLANK;
                        line->indent = 0;
                        break;
                    }
                }

                line->type = MD_LINE_HTML;
                n_parents = ctx->n_containers;
                break;
            }
        }

        /* Check for blank line. */
        if(off >= ctx->size  ||  ISNEWLINE(off)) {
            if(pivot_line->type == MD_LINE_INDENTEDCODE  &&  n_parents == ctx->n_containers) {
                line->type = MD_LINE_INDENTEDCODE;
                if(line->indent > ctx->code_indent_offset)
                    line->indent -= ctx->code_indent_offset;
                else
                    line->indent = 0;
                ctx->last_line_has_list_loosening_effect = FALSE;
            } else {
                line->type = MD_LINE_BLANK;
                ctx->last_line_has_list_loosening_effect = (n_parents > 0  &&
                        n_brothers + n_children == 0  &&
                        ctx->containers[n_parents-1].ch != _T('>'));

    #if 1
                /* See https://github.com/mity/md4c/issues/6
                 *
                 * This ugly checking tests we are in (yet empty) list item but
                 * not its very first line (i.e. not the line with the list
                 * item mark).
                 *
                 * If we are such a blank line, then any following non-blank
                 * line which would be part of the list item actually has to
                 * end the list because according to the specification, "a list
                 * item can begin with at most one blank line."
                 */
                if(n_parents > 0  &&  ctx->containers[n_parents-1].ch != _T('>')  &&
                   n_brothers + n_children == 0  &&  ctx->current_block == NULL  &&
                   ctx->n_block_bytes > (int) sizeof(MD_BLOCK))
                {
                    MD_BLOCK* top_block = (MD_BLOCK*) ((char*)ctx->block_bytes + ctx->n_block_bytes - sizeof(MD_BLOCK));
                    if(top_block->type == MD_BLOCK_LI)
                        ctx->last_list_item_starts_with_two_blank_lines = TRUE;
                }
    #endif
            }
            break;
        } else {
    #if 1
            /* This is the 2nd half of the hack. If the flag is set (i.e. there
             * was a 2nd blank line at the beginning of the list item) and if
             * we would otherwise still belong to the list item, we enforce
             * the end of the list. */
            if(ctx->last_list_item_starts_with_two_blank_lines) {
                if(n_parents > 0  &&  n_parents == ctx->n_containers  &&
                   ctx->containers[n_parents-1].ch != _T('>')  &&
                   n_brothers + n_children == 0  &&  ctx->current_block == NULL  &&
                   ctx->n_block_bytes > (int) sizeof(MD_BLOCK))
                {
                    MD_BLOCK* top_block = (MD_BLOCK*) ((char*)ctx->block_bytes + ctx->n_block_bytes - sizeof(MD_BLOCK));
                    if(top_block->type == MD_BLOCK_LI) {
                        n_parents--;

                        line->indent = total_indent;
                        if(n_parents > 0)
                            line->indent -= MIN(line->indent, ctx->containers[n_parents-1].contents_indent);
                    }
                }

                ctx->last_list_item_starts_with_two_blank_lines = FALSE;
            }
    #endif
            ctx->last_line_has_list_loosening_effect = FALSE;
        }

        /* Check whether we are Setext underline. */
        if(line->indent < ctx->code_indent_offset  &&  pivot_line->type == MD_LINE_TEXT
            &&  off < ctx->size  &&  ISANYOF2(off, _T('='), _T('-'))
            &&  (n_parents == ctx->n_containers))
        {
            unsigned level;

            if(md_is_setext_underline(ctx, off, &off, &level)) {
                line->type = MD_LINE_SETEXTUNDERLINE;
                line->data = level;
                break;
            }
        }

        /* Check for thematic break line. */
        if(line->indent < ctx->code_indent_offset
            &&  off < ctx->size  &&  off >= hr_killer
            &&  ISANYOF(off, _T("-_*")))
        {
            if(md_is_hr_line(ctx, off, &off, &hr_killer)) {
                line->type = MD_LINE_HR;
                break;
            }
        }

        /* Check for "brother" container. I.e. whether we are another list item
         * in already started list. */
        if(n_parents < ctx->n_containers  &&  n_brothers + n_children == 0) {
            OFF tmp;

            if(md_is_container_mark(ctx, line->indent, off, &tmp, &container)  &&
               md_is_container_compatible(&ctx->containers[n_parents], &container))
            {
                pivot_line = &md_dummy_blank_line;

                off = tmp;

                total_indent += container.contents_indent - container.mark_indent;
                line->indent = md_line_indentation(ctx, total_indent, off, &off);
                total_indent += line->indent;
                line->beg = off;

                /* Some of the following whitespace actually still belongs to the mark. */
                if(off >= ctx->size || ISNEWLINE(off)) {
                    container.contents_indent++;
                } else if(line->indent <= ctx->code_indent_offset) {
                    container.contents_indent += line->indent;
                    line->indent = 0;
                } else {
                    container.contents_indent += 1;
                    line->indent--;
                }

                ctx->containers[n_parents].mark_indent = container.mark_indent;
                ctx->containers[n_parents].contents_indent = container.contents_indent;

                n_brothers++;
                continue;
            }
        }

        /* Check for indented code.
         * Note indented code block cannot interrupt a paragraph. */
        if(line->indent >= ctx->code_indent_offset  &&  (pivot_line->type != MD_LINE_TEXT)) {
            line->type = MD_LINE_INDENTEDCODE;
            line->indent -= ctx->code_indent_offset;
            line->data = 0;
            break;
        }

        /* Check for start of a new container block. */
        if(line->indent < ctx->code_indent_offset  &&
           md_is_container_mark(ctx, line->indent, off, &off, &container))
        {
            if(pivot_line->type == MD_LINE_TEXT  &&  n_parents == ctx->n_containers  &&
                        (off >= ctx->size || ISNEWLINE(off))  &&  container.ch != _T('>'))
            {
                /* Noop. List mark followed by a blank line cannot interrupt a paragraph. */
            } else if(pivot_line->type == MD_LINE_TEXT  &&  n_parents == ctx->n_containers  &&
                        ISANYOF2_(container.ch, _T('.'), _T(')'))  &&  container.start != 1)
            {
                /* Noop. Ordered list cannot interrupt a paragraph unless the start index is 1. */
            } else {
                total_indent += container.contents_indent - container.mark_indent;
                line->indent = md_line_indentation(ctx, total_indent, off, &off);
                total_indent += line->indent;

                line->beg = off;
                line->data = container.ch;

                /* Some of the following whitespace actually still belongs to the mark. */
                if(off >= ctx->size || ISNEWLINE(off)) {
                    container.contents_indent++;
                } else if(line->indent <= ctx->code_indent_offset) {
                    container.contents_indent += line->indent;
                    line->indent = 0;
                } else {
                    container.contents_indent += 1;
                    line->indent--;
                }

                if(n_brothers + n_children == 0)
                    pivot_line = &md_dummy_blank_line;

                if(n_children == 0)
                    MD_CHECK(md_leave_child_containers(ctx, n_parents + n_brothers));

                n_children++;
                MD_CHECK(md_push_container(ctx, &container));
                continue;
            }
        }

        /* Check whether we are table continuation. */
        if(pivot_line->type == MD_LINE_TABLE  &&  n_parents == ctx->n_containers) {
            line->type = MD_LINE_TABLE;
            break;
        }

        /* Check for ATX header. */
        if(line->indent < ctx->code_indent_offset  &&
                off < ctx->size  &&  CH(off) == _T('#'))
        {
            unsigned level;

            if(md_is_atxheader_line(ctx, off, &line->beg, &off, &level)) {
                line->type = MD_LINE_ATXHEADER;
                line->data = level;
                break;
            }
        }

        /* Check whether we are starting code fence. */
        if(line->indent < ctx->code_indent_offset  &&
                off < ctx->size  &&  ISANYOF2(off, _T('`'), _T('~')))
        {
            if(md_is_opening_code_fence(ctx, off, &off)) {
                line->type = MD_LINE_FENCEDCODE;
                line->data = 1;
                line->enforce_new_block = TRUE;
                break;
            }
        }

        /* Check for start of raw HTML block. */
        if(off < ctx->size  &&  CH(off) == _T('<')
            &&  !(ctx->parser.flags & MD_FLAG_NOHTMLBLOCKS))
        {
            ctx->html_block_type = md_is_html_block_start_condition(ctx, off);

            /* HTML block type 7 cannot interrupt paragraph. */
            if(ctx->html_block_type == 7  &&  pivot_line->type == MD_LINE_TEXT)
                ctx->html_block_type = 0;

            if(ctx->html_block_type > 0) {
                /* The line itself also may immediately close the block. */
                if(md_is_html_block_end_condition(ctx, off, &off) == ctx->html_block_type) {
                    /* Make sure this is the last line of the block. */
                    ctx->html_block_type = 0;
                }

                line->enforce_new_block = TRUE;
                line->type = MD_LINE_HTML;
                break;
            }
        }

        /* Check for table underline. */
        if((ctx->parser.flags & MD_FLAG_TABLES)  &&  pivot_line->type == MD_LINE_TEXT
            &&  off < ctx->size  &&  ISANYOF3(off, _T('|'), _T('-'), _T(':'))
            &&  n_parents == ctx->n_containers)
        {
            unsigned col_count;

            if(ctx->current_block != NULL  &&  ctx->current_block->n_lines == 1  &&
                md_is_table_underline(ctx, off, &off, &col_count))
            {
                line->data = col_count;
                line->type = MD_LINE_TABLEUNDERLINE;
                break;
            }
        }

        /* By default, we are normal text line. */
        line->type = MD_LINE_TEXT;
        if(pivot_line->type == MD_LINE_TEXT  &&  n_brothers + n_children == 0) {
            /* Lazy continuation. */
            n_parents = ctx->n_containers;
        }

        /* Check for task mark. */
        if((ctx->parser.flags & MD_FLAG_TASKLISTS)  &&  n_brothers + n_children > 0  &&
           ISANYOF_(ctx->containers[ctx->n_containers-1].ch, _T("-+*.)")))
        {
            OFF tmp = off;

            while(tmp < ctx->size  &&  tmp < off + 3  &&  ISBLANK(tmp))
                tmp++;
            if(tmp + 2 < ctx->size  &&  CH(tmp) == _T('[')  &&
               ISANYOF(tmp+1, _T("xX "))  &&  CH(tmp+2) == _T(']')  &&
               (tmp + 3 == ctx->size  ||  ISBLANK(tmp+3)  ||  ISNEWLINE(tmp+3)))
            {
                MD_CONTAINER* task_container = (n_children > 0 ? &ctx->containers[ctx->n_containers-1] : &container);
                task_container->is_task = TRUE;
                task_container->task_mark_off = tmp + 1;
                off = tmp + 3;
                while(off < ctx->size  &&  ISWHITESPACE(off))
                    off++;
                line->beg = off;
            }
        }

        break;
    }

    /* Scan for end of the line.
     *
     * Note this is quite a bottleneck of the parsing as we here iterate almost
     * over compete document.
     */
#if defined __linux__ && !defined MD4C_USE_UTF16
    /* Recent glibc versions have superbly optimized strcspn(), even using
     * vectorization if available. */
    if(ctx->doc_ends_with_newline  &&  off < ctx->size) {
        while(TRUE) {
            off += (OFF) strcspn(STR(off), "\r\n");

            /* strcspn() can stop on zero terminator; but that can appear
             * anywhere in the Markfown input... */
            if(CH(off) == _T('\0'))
                off++;
            else
                break;
        }
    } else
#endif
    {
        /* Optimization: Use some loop unrolling. */
        while(off + 3 < ctx->size  &&  !ISNEWLINE(off+0)  &&  !ISNEWLINE(off+1)
                                   &&  !ISNEWLINE(off+2)  &&  !ISNEWLINE(off+3))
            off += 4;
        while(off < ctx->size  &&  !ISNEWLINE(off))
            off++;
    }

    /* Set end of the line. */
    line->end = off;

    /* But for ATX header, we should exclude the optional trailing mark. */
    if(line->type == MD_LINE_ATXHEADER) {
        OFF tmp = line->end;
        while(tmp > line->beg && ISBLANK(tmp-1))
            tmp--;
        while(tmp > line->beg && CH(tmp-1) == _T('#'))
            tmp--;
        if(tmp == line->beg || ISBLANK(tmp-1) || (ctx->parser.flags & MD_FLAG_PERMISSIVEATXHEADERS))
            line->end = tmp;
    }

    /* Trim trailing spaces. */
    if(line->type != MD_LINE_INDENTEDCODE  &&  line->type != MD_LINE_FENCEDCODE  && line->type != MD_LINE_HTML) {
        while(line->end > line->beg && ISBLANK(line->end-1))
            line->end--;
    }

    /* Eat also the new line. */
    if(off < ctx->size && CH(off) == _T('\r'))
        off++;
    if(off < ctx->size && CH(off) == _T('\n'))
        off++;

    *p_end = off;

    /* If we belong to a list after seeing a blank line, the list is loose. */
    if(prev_line_has_list_loosening_effect  &&  line->type != MD_LINE_BLANK  &&  n_parents + n_brothers > 0) {
        MD_CONTAINER* c = &ctx->containers[n_parents + n_brothers - 1];
        if(c->ch != _T('>')) {
            MD_BLOCK* block = (MD_BLOCK*) (((char*)ctx->block_bytes) + c->block_byte_off);
            block->flags |= MD_BLOCK_LOOSE_LIST;
        }
    }

    /* Leave any containers we are not part of anymore. */
    if(n_children == 0  &&  n_parents + n_brothers < ctx->n_containers)
        MD_CHECK(md_leave_child_containers(ctx, n_parents + n_brothers));

    /* Enter any container we found a mark for. */
    if(n_brothers > 0) {
        MD_ASSERT(n_brothers == 1);
        MD_CHECK(md_push_container_bytes(ctx, MD_BLOCK_LI,
                    ctx->containers[n_parents].task_mark_off,
                    (ctx->containers[n_parents].is_task ? CH(ctx->containers[n_parents].task_mark_off) : 0),
                    MD_BLOCK_CONTAINER_CLOSER));
        MD_CHECK(md_push_container_bytes(ctx, MD_BLOCK_LI,
                    container.task_mark_off,
                    (container.is_task ? CH(container.task_mark_off) : 0),
                    MD_BLOCK_CONTAINER_OPENER));
        ctx->containers[n_parents].is_task = container.is_task;
        ctx->containers[n_parents].task_mark_off = container.task_mark_off;
    }

    if(n_children > 0)
        MD_CHECK(md_enter_child_containers(ctx, n_children));

abort:
    return ret;
}

static int
md_process_line(MD_CTX* ctx, const MD_LINE_ANALYSIS** p_pivot_line, MD_LINE_ANALYSIS* line)
{
    const MD_LINE_ANALYSIS* pivot_line = *p_pivot_line;
    int ret = 0;

    /* Blank line ends current leaf block. */
    if(line->type == MD_LINE_BLANK) {
        MD_CHECK(md_end_current_block(ctx));
        *p_pivot_line = &md_dummy_blank_line;
        return 0;
    }

    if(line->enforce_new_block)
        MD_CHECK(md_end_current_block(ctx));

    /* Some line types form block on their own. */
    if(line->type == MD_LINE_HR || line->type == MD_LINE_ATXHEADER) {
        MD_CHECK(md_end_current_block(ctx));

        /* Add our single-line block. */
        MD_CHECK(md_start_new_block(ctx, line));
        MD_CHECK(md_add_line_into_current_block(ctx, line));
        MD_CHECK(md_end_current_block(ctx));
        *p_pivot_line = &md_dummy_blank_line;
        return 0;
    }

    /* MD_LINE_SETEXTUNDERLINE changes meaning of the current block and ends it. */
    if(line->type == MD_LINE_SETEXTUNDERLINE) {
        MD_ASSERT(ctx->current_block != NULL);
        ctx->current_block->type = MD_BLOCK_H;
        ctx->current_block->data = line->data;
        ctx->current_block->flags |= MD_BLOCK_SETEXT_HEADER;
        MD_CHECK(md_add_line_into_current_block(ctx, line));
        MD_CHECK(md_end_current_block(ctx));
        if(ctx->current_block == NULL) {
            *p_pivot_line = &md_dummy_blank_line;
        } else {
            /* This happens if we have consumed all the body as link ref. defs.
             * and downgraded the underline into start of a new paragraph block. */
            line->type = MD_LINE_TEXT;
            *p_pivot_line = line;
        }
        return 0;
    }

    /* MD_LINE_TABLEUNDERLINE changes meaning of the current block. */
    if(line->type == MD_LINE_TABLEUNDERLINE) {
        MD_ASSERT(ctx->current_block != NULL);
        MD_ASSERT(ctx->current_block->n_lines == 1);
        ctx->current_block->type = MD_BLOCK_TABLE;
        ctx->current_block->data = line->data;
        MD_ASSERT(pivot_line != &md_dummy_blank_line);
        ((MD_LINE_ANALYSIS*)pivot_line)->type = MD_LINE_TABLE;
        MD_CHECK(md_add_line_into_current_block(ctx, line));
        return 0;
    }

    /* The current block also ends if the line has different type. */
    if(line->type != pivot_line->type)
        MD_CHECK(md_end_current_block(ctx));

    /* The current line may start a new block. */
    if(ctx->current_block == NULL) {
        MD_CHECK(md_start_new_block(ctx, line));
        *p_pivot_line = line;
    }

    /* In all other cases the line is just a continuation of the current block. */
    MD_CHECK(md_add_line_into_current_block(ctx, line));

abort:
    return ret;
}

static int
md_process_doc(MD_CTX *ctx)
{
    const MD_LINE_ANALYSIS* pivot_line = &md_dummy_blank_line;
    MD_LINE_ANALYSIS line_buf[2];
    MD_LINE_ANALYSIS* line = &line_buf[0];
    OFF off = 0;
    int ret = 0;

    MD_ENTER_BLOCK(MD_BLOCK_DOC, NULL);

    while(off < ctx->size) {
        if(line == pivot_line)
            line = (line == &line_buf[0] ? &line_buf[1] : &line_buf[0]);

        MD_CHECK(md_analyze_line(ctx, off, &off, pivot_line, line));
        MD_CHECK(md_process_line(ctx, &pivot_line, line));
    }

    md_end_current_block(ctx);

    MD_CHECK(md_build_ref_def_hashtable(ctx));

    /* Process all blocks. */
    MD_CHECK(md_leave_child_containers(ctx, 0));
    MD_CHECK(md_process_all_blocks(ctx));

    MD_LEAVE_BLOCK(MD_BLOCK_DOC, NULL);

abort:

#if 0
    /* Output some memory consumption statistics. */
    {
        char buffer[256];
        sprintf(buffer, "Alloced %u bytes for block buffer.",
                    (unsigned)(ctx->alloc_block_bytes));
        MD_LOG(buffer);

        sprintf(buffer, "Alloced %u bytes for containers buffer.",
                    (unsigned)(ctx->alloc_containers * sizeof(MD_CONTAINER)));
        MD_LOG(buffer);

        sprintf(buffer, "Alloced %u bytes for marks buffer.",
                    (unsigned)(ctx->alloc_marks * sizeof(MD_MARK)));
        MD_LOG(buffer);

        sprintf(buffer, "Alloced %u bytes for aux. buffer.",
                    (unsigned)(ctx->alloc_buffer * sizeof(MD_CHAR)));
        MD_LOG(buffer);
    }
#endif

    return ret;
}


/********************
 ***  Public API  ***
 ********************/

int
md_parse(const MD_CHAR* text, MD_SIZE size, const MD_PARSER* parser, void* userdata)
{
    MD_CTX ctx;
    int i;
    int ret;

    if(parser->abi_version != 0) {
        if(parser->debug_log != NULL)
            parser->debug_log("Unsupported abi_version.", userdata);
        return -1;
    }

    /* Setup context structure. */
    memset(&ctx, 0, sizeof(MD_CTX));
    ctx.text = text;
    ctx.size = size;
    memcpy(&ctx.parser, parser, sizeof(MD_PARSER));
    ctx.userdata = userdata;
    ctx.code_indent_offset = (ctx.parser.flags & MD_FLAG_NOINDENTEDCODEBLOCKS) ? (OFF)(-1) : 4;
    md_build_mark_char_map(&ctx);
    ctx.doc_ends_with_newline = (size > 0  &&  ISNEWLINE_(text[size-1]));
    ctx.max_ref_def_output = MIN(MIN(16 * (uint64_t)size, (uint64_t)(1024 * 1024)), (uint64_t)SZ_MAX);

    /* Reset all mark stacks and lists. */
    for(i = 0; i < (int) SIZEOF_ARRAY(ctx.opener_stacks); i++)
        ctx.opener_stacks[i].top = -1;
    ctx.ptr_stack.top = -1;
    ctx.unresolved_link_head = -1;
    ctx.unresolved_link_tail = -1;
    ctx.table_cell_boundaries_head = -1;
    ctx.table_cell_boundaries_tail = -1;

    /* All the work. */
    ret = md_process_doc(&ctx);

    /* Clean-up. */
    md_free_ref_defs(&ctx);
    md_free_ref_def_hashtable(&ctx);
    free(ctx.buffer);
    free(ctx.marks);
    free(ctx.block_bytes);
    free(ctx.containers);

    return ret;
}
