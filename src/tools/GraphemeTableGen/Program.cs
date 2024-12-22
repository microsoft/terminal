using System.Text;
using System.Runtime.InteropServices;
using System.Xml.Linq;

using TrieType = uint;

// Used as an indicator in our rules for ÷ ("does not join").
// Underscore is one of the few characters that are permitted as an identifier,
// are monospace in most fonts and also visually distinct from the digits.
const int _ = -1;

// @formatter:off

// joinRules doesn't quite follow UAX #29, as it states:
// > Note: Testing two adjacent characters is insufficient for determining a boundary.
//
// I completely agree, however it makes the implementation complex and slow, and it only benefits what can be considered
// edge cases in the context of terminals. By using a lookup table anyway this results in a >100MB/s throughput,
// before adding any fast-passes whatsoever. This is 2x as fast as any standards conforming implementation I found.
//
// This affects the following rules:
// * GB9c: \p{InCB=Consonant} [\p{InCB=Extend}\p{InCB=Linker}]* \p{InCB=Linker} [\p{InCB=Extend}\p{InCB=Linker}]* × \p{InCB=Consonant}
//   "Do not break within certain combinations with Indic_Conjunct_Break (InCB)=Linker."
//   Our implementation does this:
//                     × \p{InCB=Linker}
//     \p{InCB=Linker} × \p{InCB=Consonant}
//   In other words, it doesn't check for a leading \p{InCB=Consonant} or a series of Extenders/Linkers in between.
//   I suspect that these simplified rules are sufficient for the vast majority of terminal use cases.
// * GB11: \p{Extended_Pictographic} Extend* ZWJ × \p{Extended_Pictographic}
//   "Do not break within emoji modifier sequences or emoji zwj sequences."
//   Our implementation does this:
//     ZWJ × \p{Extended_Pictographic}
//   In other words, it doesn't check whether the ZWJ is led by another \p{InCB=Extended_Pictographic}.
//   Again, I suspect that a trailing, standalone ZWJ is a rare occurrence and joining it with any Emoji is fine.
//   GB11 could be implemented properly quite easily for forward iteration by forbidding "ZWJ × \p{Extended_Pictographic}"
//   in the base table and switching to a second joinRules table when encountering any valid "\p{Extended_Pictographic} ×".
//   Only in that secondary table "ZWJ × \p{Extended_Pictographic}" would be permitted. This then properly implements
//   "\p{Extended_Pictographic} × ZWJ × \p{Extended_Pictographic}". However, this makes backward iteration difficult,
//   because knowing whether to transition to the secondary table requires looking ahead, which we want to avoid.
// * GB12: sot (RI RI)* RI × RI
//   GB13: [^RI] (RI RI)* RI × RI
//   "Do not break within emoji flag sequences. That is, do not break between regional indicator
//   (RI) symbols if there is an odd number of RI characters before the break point."
//   Our implementation does this (this is not a real notation):
//     RI ÷ RI × RI ÷ RI
//   In other words, it joins any pair of RIs and then immediately aborts further RI joins.
//   Unlike the above two cases, this is a bit more risky, because it's much more likely to be encountered in practice.
//   Imagine a shell that doesn't understand graphemes for instance. You type 2 flags (= 4 RIs) and backspace.
//   You'll now have 3 RIs. If iterating through it forwards, you'd join the first two, then get 1 lone RI at the end,
//   whereas if you iterate backwards you'd join the last two, then get 1 lone RI at the start.
//   This asymmetry may have some subtle effects, but I suspect that it's still rare enough to not matter much.
//
// This is a great reference for the resulting table:
// https://www.unicode.org/Public/UCD/latest/ucd/auxiliary/GraphemeBreakTest.html
int[][][] joinRules =
[
    // Base table
    [
        /* ↓ leading        → trailing codepoint                                                                                                                                                                   */
        /*               |   Other  |    CR    |    LF    |  Control |  Extend  |    RI    | Prepend  |  HangulL |  HangulV |  HangulT | HangulLV | HangulLVT | InCBLinker | InCBConsonant |  ExtPic  |    ZWJ   | */
        /* Other         | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* CR            | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, _ /*  |   */, _ /*    | */, _ /* | */, _ /* | */],
        /* LF            | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, _ /*  |   */, _ /*    | */, _ /* | */, _ /* | */],
        /* Control       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, _ /*  |   */, _ /*    | */, _ /* | */, _ /* | */],
        /* Extend        | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* RI            | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 1 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* Prepend       | */ [0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /*  |  */, 0 /*  |   */, 0 /*    | */, 0 /* | */, 0 /* | */],
        /* HangulL       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, 0 /* | */, 0 /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulV       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulT       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulLV      | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulLVT     | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* InCBLinker    | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, 0 /*    | */, _ /* | */, 0 /* | */],
        /* InCBConsonant | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* ExtPic        | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* ZWJ           | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, 0 /* | */, 0 /* | */],
    ],
    // Once we have encountered a Regional Indicator pair we'll enter this table.
    // It's a copy of the base table, but instead of RI × RI, we're RI ÷ RI.
    [
        /* ↓ leading         → trailing codepoint                                                                                                                                                                  */
        /*               |   Other  |    CR    |    LF    |  Control |  Extend  |    RI    | Prepend  |  HangulL |  HangulV |  HangulT | HangulLV | HangulLVT | InCBLinker | InCBConsonant |  ExtPic  |    ZWJ   | */
        /* Other         | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* CR            | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, _ /*  |   */, _ /*    | */, _ /* | */, _ /* | */],
        /* LF            | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, _ /*  |   */, _ /*    | */, _ /* | */, _ /* | */],
        /* Control       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, _ /*  |   */, _ /*    | */, _ /* | */, _ /* | */],
        /* Extend        | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* RI            | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* Prepend       | */ [0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /*  |  */, 0 /*  |   */, 0 /*    | */, 0 /* | */, 0 /* | */],
        /* HangulL       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, 0 /* | */, 0 /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulV       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulT       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulLV      | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulLVT     | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* InCBLinker    | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, 0 /*    | */, _ /* | */, 0 /* | */],
        /* InCBConsonant | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* ExtPic        | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* ZWJ           | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, 0 /* | */, 0 /* | */],
    ],
];

// Documentation for our UAX #14 line break implementation based on Unicode 16.1,
// but heavily modified to allow for use with lookup tables:
//
// NOTE: If you convert these rules into a lookup table, you must apply them in reverse order.
//       This is because the rules are ordered from most to least important (e.g. LB8 overrides LB18).
//
// Resolve line breaking classes:
// LB1:   Assign a line breaking class [...].
//        ❌ Unicode does that for us via the "lb" attribute.
//
// Start and end of text:
// LB2:   Never break at the start of text.
//        ❌ Functionality not needed.
// LB3:   Always break at the end of text.
//        ❌ Functionality not needed.
//
// Mandatory breaks:
// LB4:   Always break after hard line breaks.
//        ❌ Handled by our ucd_* functions.
// LB5:   Treat CR followed by LF, as well as CR, LF, and NL as hard line breaks.
//        ❌ Handled by our ucd_* functions.
// LB6:   Do not break before hard line breaks.
//        ❌ Handled by our ucd_* functions.
//
// Explicit breaks and non-breaks:
// LB7:   Do not break before spaces or zero width space.
//        ❌ It's way simpler to treat spaces as if they always break.
// LB8:   Break before any character following a zero-width space, even if one or more spaces intervene.
//        ⍻ ZW ÷    modified from    ZW SP* ÷    because it's not worth being this anal about accuracy here.
// LB8a:  Do not break after a zero width joiner.
//        ❌ Our ucd_* functions never break within grapheme clusters.
//
// Combining marks:
// LB9:   Do not break a combining character sequence; treat it as if it has the line breaking class of the base character in all of the following rules. Treat ZWJ as if it were CM.
//        ❌ Our ucd_* functions never break within grapheme clusters.
// LB10:  Treat any remaining combining mark or ZWJ as AL.
//        ❌ To be honest, I'm not entirely sure, I understand the implications of this rule.
//
// Word joiner:
// LB11:  Do not break before or after Word joiner and related characters.
//        ✔ × WJ
//        ✔ WJ ×
//
// Non-breaking characters:
// LB12:  Do not break after NBSP and related characters.
//        ✔ GL ×
// LB12a: Do not break before NBSP and related characters, except after spaces and hyphens.
//        ✔ [^SP BA HY] × GL
//
// Opening and closing:
// LB13:  Do not break before ']' or '!' or '/', even after spaces.
//        ✔ × CL
//        ✔ × CP
//        ✔ × EX
//        ✔ × SY
// LB14:  Do not break after '[', even after spaces.
//        ⍻ OP ×    modified from    OP SP* ×    just because it's simpler. It would be nice to address this.
// LB15a: Do not break after an unresolved initial punctuation that lies at the start of the line, after a space, after opening punctuation, or after an unresolved quotation mark, even after spaces.
//        ❌ Not implemented. Seemed too complex for little gain?
// LB15b: Do not break before an unresolved final punctuation that lies at the end of the line, before a space, before a prohibited break, or before an unresolved quotation mark, even after spaces.
//        ❌ Not implemented. Seemed too complex for little gain?
// LB15c: Break before a decimal mark that follows a space, for instance, in 'subtract .5'.
//        ⍻ SP ÷ IS    modified from    SP ÷ IS NU    because this fits neatly with LB15d.
// LB15d: Otherwise, do not break before ';', ',', or '.', even after spaces.
//        ✔ × IS
// LB16:  Do not break between closing punctuation and a nonstarter (lb=NS), even with intervening spaces.
//        ❌ Not implemented. Could be useful in the future, but its usefulness seemed limited to me.
// LB17:  Do not break within '——', even with intervening spaces.
//        ❌ Not implemented. Terminal applications nor code use em-dashes much anyway.
//
// Spaces:
// LB18:  Break after spaces.
//        ❌ Implemented because we didn't implement LB7.
//
// Special case rules:
// LB19:  Do not break before non-initial unresolved quotation marks, such as ' ” ' or ' " ', nor after non-final unresolved quotation marks, such as ' “ ' or ' " '.
//        ⍻ × QU    modified from    × [ QU - \p{Pi} ]
//        ⍻ QU ×    modified from    [ QU - \p{Pf} ] ×
//        We implement the Unicode 16.0 instead of 16.1 rules, because it's simpler and allows us to use a LUT.
// LB19a: Unless surrounded by East Asian characters, do not break either side of any unresolved quotation marks.
//        ❌ [^$EastAsian] × QU
//        ❌ × QU ( [^$EastAsian] | eot )
//        ❌ QU × [^$EastAsian]
//        ❌ ( sot | [^$EastAsian] ) QU ×
//        Same as LB19.
// LB20:  Break before and after unresolved CB.
//        ❌ We break by default. Unicode inline objects are super irrelevant in a terminal in either case.
// LB20a: Do not break after a word-initial hyphen.
//        ❌ Not implemented. Seemed not worth the hassle as the window will almost always be >1 char wide.
// LB21:  Do not break before hyphen-minus, other hyphens, fixed-width spaces, small kana, and other non-starters, or after acute accents.
//        ✔ × BA
//        ✔ × HY
//        ✔ × NS
//        ✔ BB ×
// LB21a: Do not break after the hyphen in Hebrew + Hyphen + non-Hebrew.
//        ❌ Not implemented. Perhaps in the future.
// LB21b: Do not break between Solidus and Hebrew letters.
//        ❌ Not implemented. Perhaps in the future.
// LB22:  Do not break before ellipses.
//        ✔ × IN
//
// Numbers:
// LB23:  Do not break between digits and letters.
//        ✔ (AL | HL) × NU
//        ✔ NU × (AL | HL)
// LB23a: Do not break between numeric prefixes and ideographs, or between ideographs and numeric postfixes.
//        ✔ PR × (ID | EB | EM)
//        ✔ (ID | EB | EM) × PO
// LB24:  Do not break between numeric prefix/postfix and letters, or between letters and prefix/postfix.
//        ✔ (PR | PO) × (AL | HL)
//        ✔ (AL | HL) × (PR | PO)
// LB25:  Do not break numbers:
//        ⍻ CL × PO                  modified from    NU ( SY | IS )* CL × PO
//        ⍻ CP × PO                  modified from    NU ( SY | IS )* CP × PO
//        ⍻ CL × PR                  modified from    NU ( SY | IS )* CL × PR
//        ⍻ CP × PR                  modified from    NU ( SY | IS )* CP × PR
//        ⍻ ( NU | SY | IS ) × PO    modified from    NU ( SY | IS )* × PO
//        ⍻ ( NU | SY | IS ) × PR    modified from    NU ( SY | IS )* × PR
//        ⍻ PO × OP                  modified from    PO × OP NU
//        ⍻ PO × OP                  modified from    PO × OP IS NU
//        ✔ PO × NU
//        ⍻ PR × OP                  modified from    PR × OP NU
//        ⍻ PR × OP                  modified from    PR × OP IS NU
//        ✔ PR × NU
//        ✔ HY × NU
//        ✔ IS × NU
//        ⍻ ( NU | SY | IS ) × NU    modified from    NU ( SY | IS )* × NU
//        Most were simplified because the cases this additionally allows don't matter much here.
//
// Korean syllable blocks
// LB26:  Do not break a Korean syllable.
//        ❌ Our ucd_* functions never break within grapheme clusters.
// LB27:  Treat a Korean Syllable Block the same as ID.
//        ❌ Our ucd_* functions never break within grapheme clusters.
//
// Finally, join alphabetic letters into words and break everything else.
// LB28:  Do not break between alphabetics ("at").
//        ✔ (AL | HL) × (AL | HL)
// LB28a: Do not break inside the orthographic syllables of Brahmic scripts.
//        ❌ Our ucd_* functions never break within grapheme clusters.
// LB29:  Do not break between numeric punctuation and alphabetics ("e.g.").
//        ✔ IS × (AL | HL)
// LB30:  Do not break between letters, numbers, or ordinary symbols and opening or closing parentheses.
//        ✔ (AL | HL | NU) × [OP-$EastAsian]
//        ✔ [CP-$EastAsian] × (AL | HL | NU)
// LB30a: Break between two regional indicator symbols if and only if there are an even number of regional indicators preceding the position of the break.
//        ❌ Our ucd_* functions never break within grapheme clusters.
// LB30b: Do not break between an emoji base (or potential emoji) and an emoji modifier.
//        ❌ Our ucd_* functions never break within grapheme clusters.
// LB31:  Break everywhere else.
//        ❌ Our default behavior.
int[][] joinRulesLineBreak =
[
    /* ↓ leading                    → trailing codepoint                                                                                                                                                                                                                                                                                                                                                                               */
    /*                           |   Other  | WordJoiner | ZeroWidthSpace |   Glue   |   Space  | BreakAfter | BreakBefore | Hyphen   | ClosePunctuation | CloseParenthesis_EA | CloseParenthesis_NotEA | Exclamation | Inseparable | Nonstarter | OpenPunctuation_EA | OpenPunctuation_NotEA | Quotation | InfixNumericSeparator | Numeric  | PostfixNumeric | PrefixNumeric | SymbolsAllowingBreakAfter | Alphabetic | Ideographic | */
    /* Other                     | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, _ /* |   */, _ /*     |   */, _ /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* WordJoiner                | */ [1 /* |  */, 1 /*  |    */, 1 /*    | */, 1 /* | */, 1 /* |  */, 1 /*  |  */, 1 /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, 1 /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, 1 /*   | */],
    /* ZeroWidthSpace            | */ [_ /* |  */, _ /*  |    */, _ /*    | */, _ /* | */, _ /* |  */, _ /*  |  */, _ /*   | */, _ /* |    */, _ /*      |      */, _ /*       |        */, _ /*        |  */, _ /*   |  */, _ /*   |  */, _ /*  |      */, _ /*      |        */, _ /*       | */, _ /*  |       */, _ /*        | */, _ /* |   */, _ /*     |   */, _ /*    |         */, _ /*          | */, _ /*   |  */, _ /*   | */],
    /* Glue                      | */ [1 /* |  */, 1 /*  |    */, 1 /*    | */, 1 /* | */, 1 /* |  */, 1 /*  |  */, 1 /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, 1 /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, 1 /*   | */],
    /* Space                     | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, _ /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, _ /*        | */, _ /* |   */, _ /*     |   */, _ /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* BreakAfter                | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, _ /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, _ /* |   */, _ /*     |   */, _ /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* BreakBefore               | */ [1 /* |  */, 1 /*  |    */, 1 /*    | */, 1 /* | */, 1 /* |  */, 1 /*  |  */, 1 /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, 1 /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, 1 /*   | */],
    /* Hyphen                    | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, _ /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, _ /*     |   */, _ /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* ClosePunctuation          | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, _ /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* CloseParenthesis_EA       | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, _ /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* CloseParenthesis_NotEA    | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, _ /*   | */],
    /* Exclamation               | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, _ /* |   */, _ /*     |   */, _ /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* Inseparable               | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, _ /* |   */, _ /*     |   */, _ /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* Nonstarter                | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, _ /* |   */, _ /*     |   */, _ /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* OpenPunctuation_EA        | */ [1 /* |  */, 1 /*  |    */, 1 /*    | */, 1 /* | */, 1 /* |  */, 1 /*  |  */, 1 /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, 1 /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, 1 /*   | */],
    /* OpenPunctuation_NotEA     | */ [1 /* |  */, 1 /*  |    */, 1 /*    | */, 1 /* | */, 1 /* |  */, 1 /*  |  */, 1 /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, 1 /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, 1 /*   | */],
    /* Quotation                 | */ [1 /* |  */, 1 /*  |    */, 1 /*    | */, 1 /* | */, 1 /* |  */, 1 /*  |  */, 1 /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, 1 /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, 1 /*   | */],
    /* InfixNumericSeparator     | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, _ /*   | */],
    /* Numeric                   | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, _ /*   | */],
    /* PostfixNumeric            | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, 1 /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, _ /*     |   */, _ /*    |         */, 1 /*          | */, 1 /*   |  */, _ /*   | */],
    /* PrefixNumeric             | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, 1 /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, _ /*     |   */, _ /*    |         */, 1 /*          | */, 1 /*   |  */, 1 /*   | */],
    /* SymbolsAllowingBreakAfter | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
    /* Alphabetic                | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, 1 /*       | */, 1 /*  |       */, 1 /*        | */, 1 /* |   */, 1 /*     |   */, 1 /*    |         */, 1 /*          | */, 1 /*   |  */, _ /*   | */],
    /* Ideographic               | */ [_ /* |  */, 1 /*  |    */, _ /*    | */, 1 /* | */, _ /* |  */, 1 /*  |  */, _ /*   | */, 1 /* |    */, 1 /*      |      */, 1 /*       |        */, 1 /*        |  */, 1 /*   |  */, 1 /*   |  */, 1 /*  |      */, _ /*      |        */, _ /*       | */, 1 /*  |       */, 1 /*        | */, _ /* |   */, 1 /*     |   */, _ /*    |         */, 1 /*          | */, _ /*   |  */, _ /*   | */],
];

// @formatter:on

if (args.Length != 1)
{
    Console.WriteLine(
        """
        Usage: GraphemeTableGen <path to ucd.nounihan.grouped.xml>

        You can download the latest ucd.nounihan.grouped.xml from:
            https://www.unicode.org/Public/UCD/latest/ucdxml/ucd.nounihan.grouped.zip
        """
    );
    Environment.Exit(1);
}

var ucd = ExtractValuesFromUcd(args[0]);

// Find the best trie configuration over the given block sizes (2^2 - 2^8) and stages (4).
// More stages = Less size. The trajectory roughly follows a+b*c^stages, where c < 1.
// 4 still gives ~30% savings over 3 stages and going beyond 5 gives diminishing returns (<10%).
var trie = BuildBestTrie(ucd.Values, 2, 8, 4);
// The joinRules above has 2 bits per value. This packs it into 32-bit integers to save space.
var rules = joinRules.Select(table => PrepareRulesTable(table, 2, 3)).ToArray();
// Each rules item has the same length. Each item is 32 bits = 4 bytes.
var totalSize = trie.TotalSize + rules.Length * rules[0].Length * sizeof(TrieType);

// Run a quick sanity check to ensure that the trie works as expected.
foreach (var (expected, cp) in ucd.Values.Select((v, i) => (v, i)))
{
    TrieType v = 0;
    foreach (var s in trie.Stages)
    {
        v = s.Values[(int)v + ((cp >> s.Shift) & s.Mask)];
    }

    if (v != expected)
    {
        throw new Exception($"trie sanity check failed for {cp:X}");
    }
}

// All the remaining code starting here simply generates the C++ output.
var buf = new StringBuilder();
buf.Append($$"""
// Generated by GraphemeTableGen
// on {{DateTime.UtcNow.ToString("yyyy'-'MM'-'dd'T'HH':'mm':'ssK")}}, from {{ucd.Description}}, {{totalSize}} bytes
// clang-format off

""");

foreach (var stage in trie.Stages)
{
    var fmt = $" 0x{{0:x{stage.Bits / 4}}},";
    var width = 16;
    if (stage.Index != 0)
    {
        width = stage.Mask + 1;
    }

    buf.Append($"static const uint{stage.Bits}_t s_stage{stage.Index}[] = {{");
    foreach (var (value, j) in stage.Values.Select((v, j) => (v, j)))
    {
        if (j % width == 0)
        {
            buf.Append("\n   ");
        }
        buf.AppendFormat(fmt, value);
    }
    buf.Append("\n};\n");
}

buf.Append($"static const uint32_t s_join_rules[{rules.Length}][{rules[0].Length}] = {{\n");
foreach (var table in rules)
{
    buf.Append("    {\n");
    foreach (var r in table)
    {
        buf.Append($"        0b{r:b32},\n");
    }
    buf.Append("    },\n");
}
buf.Append("};\n");

buf.Append("inline int ucd_lookup(const uint32_t cp)\n");
buf.Append("{\n");
foreach (var stage in trie.Stages)
{
    buf.Append($"    const uint{stage.Bits}_t s{stage.Index} = s_stage{stage.Index}[");
    if (stage.Index == 0)
    {
        buf.Append($"cp >> {stage.Shift}");
    }
    else
    {
        buf.Append($"s{stage.Index - 1} + ((cp >> {stage.Shift}) & {stage.Mask})");
    }

    buf.Append("];\n");
}
buf.Append($"    return s{trie.Stages.Count - 1};\n");
buf.Append("}\n");

buf.Append($$"""
inline int ucd_grapheme_joins(const int state, const int lead, const int trail)
{
    const int l = lead & 15;
    const int t = trail & 15;
    return (s_join_rules[state][l] >> (t * 2)) & 3;
}
inline bool ucd_grapheme_done(const int state)
{
    return state == 3;
}
inline int ucd_to_character_width(const int val)
{
    return val >> 6;
}
inline int ucd_is_newline(const int val)
{
    return val > {{(int)ClusterBreak.Control}};
}
// clang-format on
""");

Console.Write(buf);
return;

// This reads the given ucd.nounihan.grouped.xml file and extracts the
// CharacterWidth and ClusterBreak properties for all codepoints.
static Ucd ExtractValuesFromUcd(string path)
{
    var values = new TrieType[1114112];
    Array.Fill(values, TrieValue(ClusterBreak.Other, CharacterWidth.Narrow));

    XNamespace ns = "http://www.unicode.org/ns/2003/ucd/1.0";
    var doc = XDocument.Load(path);
    var root = doc.Root!;
    var description = root.Element(ns + "description")!.Value;

    foreach (var group in doc.Root!.Descendants(ns + "group"))
    {
        var groupGeneralCategory = group.Attribute("gc")?.Value;
        var groupLineBreak = group.Attribute("lb")?.Value;
        var groupGraphemeClusterBreak = group.Attribute("GCB")?.Value;
        var groupIndicConjunctBreak = group.Attribute("InCB")?.Value;
        var groupExtendedPictographic = group.Attribute("ExtPict")?.Value;
        var groupEastAsian = group.Attribute("ea")?.Value;

        foreach (var ch in group.Elements())
        {
            int firstCp;
            int lastCp;
            if (ch.Attribute("cp") is { } val)
            {
                var cp = Convert.ToInt32(val.Value, 16);
                firstCp = cp;
                lastCp = cp;
            }
            else
            {
                firstCp = Convert.ToInt32(ch.Attribute("first-cp")!.Value, 16);
                lastCp = Convert.ToInt32(ch.Attribute("last-cp")!.Value, 16);
            }

            var generalCategory = ch.Attribute("gc")?.Value ?? groupGeneralCategory ?? "";
            var lineBreak = ch.Attribute("lb")?.Value ?? groupLineBreak ?? "";
            var graphemeClusterBreak = ch.Attribute("GCB")?.Value ?? groupGraphemeClusterBreak ?? "";
            var indicConjunctBreak = ch.Attribute("InCB")?.Value ?? groupIndicConjunctBreak ?? "";
            var extendedPictographic = ch.Attribute("ExtPict")?.Value ?? groupExtendedPictographic ?? "";
            var eastAsian = ch.Attribute("ea")?.Value ?? groupEastAsian ?? "";

            var cb = graphemeClusterBreak switch
            {
                "XX" => ClusterBreak.Other, // Anything else
                // We ignore GB3 which demands that CR × LF do not break apart, because
                // * these control characters won't normally reach our text storage
                // * otherwise we're in a raw write mode and historically conhost stores them in separate cells
                "CR" => ClusterBreak.CR, // Carriage Return
                "LF" => ClusterBreak.LF, // Line Feed
                "CN" => ClusterBreak.Control, // Control
                "EX" or "SM" => ClusterBreak.Extend, // Extend, SpacingMark
                "PP" => ClusterBreak.Prepend, // Prepend
                "ZWJ" => ClusterBreak.ZWJ, // Zero Width Joiner
                "RI" => ClusterBreak.RI, // Regional Indicator
                "L" => ClusterBreak.HangulL, // Hangul Syllable Type L
                "V" => ClusterBreak.HangulV, // Hangul Syllable Type V
                "T" => ClusterBreak.HangulT, // Hangul Syllable Type T
                "LV" => ClusterBreak.HangulLV, // Hangul Syllable Type LV
                "LVT" => ClusterBreak.HangulLVT, // Hangul Syllable Type LVT
                _ => throw new Exception($"Unrecognized GCB {graphemeClusterBreak} for U+{firstCp:X4} to U+{lastCp:X4}")
            };

            if (extendedPictographic == "Y")
            {
                // Currently every single Extended_Pictographic codepoint happens to be GCB=XX.
                // This is fantastic for us because it means we can stuff it into the ClusterBreak enum
                // and treat it as an alias of EXTEND, but with the special GB11 properties.
                if (cb != ClusterBreak.Other)
                {
                    throw new Exception(
                        $"Unexpected GCB {graphemeClusterBreak} with ExtPict=Y for U+{firstCp:X4} to U+{lastCp:X4}");
                }

                cb = ClusterBreak.ExtPic;
            }

            cb = indicConjunctBreak switch
            {
                "None" or "Extend" => cb,
                "Linker" => ClusterBreak.InCBLinker,
                "Consonant" => ClusterBreak.InCBConsonant,
                _ => throw new Exception($"Unrecognized InCB {indicConjunctBreak} for U+{firstCp:X4} to U+{lastCp:X4}")
            };

            var width = eastAsian switch
            {
                "N" or "Na" or "H" => CharacterWidth.Narrow, // Half-width, Narrow, Neutral
                "F" or "W" => CharacterWidth.Wide, // Wide, Full-width
                "A" => CharacterWidth.Narrow, // Ambiguous
                _ => throw new Exception($"Unrecognized ea {eastAsian} for U+{firstCp:X4} to U+{lastCp:X4}")
            };

            // There's no "ea" attribute for "zero width" so we need to do that ourselves. This matches:
            //   Me: Mark, enclosing
            //   Mn: Mark, non-spacing
            //   Cf: Control, format
            switch (generalCategory)
            {
                case "Cf" when cb == ClusterBreak.Control:
                    // A significant portion of Cf characters are not just gc=Cf (= commonly considered zero-width),
                    // but also GCB=CN (= does not join). This is a bit of a problem for terminals,
                    // because they don't support zero-width graphemes, as zero-width columns can't exist.
                    // So, we turn all of them into Extend, which is roughly how wcswidth() would treat them.
                    cb = ClusterBreak.Extend;
                    width = CharacterWidth.ZeroWidth;
                    break;
                case "Me" or "Mn" or "Cf":
                width = CharacterWidth.ZeroWidth;
                    break;
            }

            var lbEa = eastAsian is "F" or "W" or "H";
            var lb = lineBreak switch
            {
                "WJ" => LineBreak.WordJoiner,
                "ZW" => LineBreak.ZeroWidthSpace,
                "GL" => LineBreak.Glue,
                "SP" => LineBreak.Space,

                "BA" => LineBreak.BreakAfter,
                "BB" => LineBreak.BreakBefore,
                "HY" => LineBreak.Hyphen,

                "CL" => LineBreak.ClosePunctuation,
                "CP" when lbEa => LineBreak.CloseParenthesis_EA,
                "CP" => LineBreak.CloseParenthesis_NotEA,
                "EX" => LineBreak.Exclamation,
                "IN" => LineBreak.Inseparable,
                "NS" => LineBreak.Nonstarter,
                "OP" when lbEa => LineBreak.OpenPunctuation_EA,
                "OP" => LineBreak.OpenPunctuation_NotEA,
                "QU" => LineBreak.Quotation,

                "IS" => LineBreak.InfixNumericSeparator,
                "NU" => LineBreak.Numeric,
                "PO" => LineBreak.PostfixNumeric,
                "PR" => LineBreak.PrefixNumeric,
                "SY" => LineBreak.SymbolsAllowingBreakAfter,

                "AL" or "HL" => LineBreak.Alphabetic,
                "ID" or "EB" or "EM" => LineBreak.Ideographic,

                _ => LineBreak.Other,
            };

            Fill(firstCp, lastCp, TrieValue(cb, width));
        }
    }

    // U+00AD: Soft Hyphen
    // A soft hyphen is a hint that a word break is allowed at that position.
    // By default, the glyph is supposed to be invisible, and only if
    // a word break occurs, the text renderer should display a hyphen.
    // A terminal does not support computerized typesetting, but unlike the other
    // gc=Cf cases we give it a Narrow width, because that matches wcswidth().
    Fill(0x00AD, 0x00AD, TrieValue(ClusterBreak.Control, CharacterWidth.Narrow));

    // U+2500 to U+257F: Box Drawing block
    // U+2580 to U+259F: Block Elements block
    // By default, CharacterWidth.Ambiguous, but by convention .Narrow in terminals.
    Fill(0x2500, 0x259F, TrieValue(ClusterBreak.Other, CharacterWidth.Narrow));

    // U+FE0F Variation Selector-16 is used to turn unqualified Emojis into qualified ones.
    // By convention, this turns them from being ambiguous width (= narrow) into wide ones.
    // We achieve this here by explicitly giving this codepoint a wide width.
    // Later down below we'll clamp width back to <= 2.
    Fill(0xFE0F, 0xFE0F, TrieValue(ClusterBreak.Extend, CharacterWidth.Wide));

    return new Ucd
    {
        Description = description,
        Values = values.ToList(),
    };

    // Packs the arguments into a single integer that's stored as-is in the final trie stage.
    TrieType TrieValue(ClusterBreak cb, CharacterWidth width)
    {
        return (TrieType)cb | (TrieType)width << 6;
    }

    void Fill(int first, int last, TrieType value)
    {
        Array.Fill(values, value, first, last - first + 1);
    }
}

// Because each item in the list of 2D rule tables only uses 1-2 bits,
// this function packs them into chunks of 32-bit integers to save space.
static uint[] PrepareRulesTable(int[][] rules, int bitWidth, int nonJoinerValue)
{
    var compressed = new uint[rules.Length];

    foreach (var lead in Enumerable.Range(0, rules.Length))
    {
        var row = rules[lead];
        uint nextIndices = 0;

        if (row.Length > 32 / bitWidth)
        {
            throw new Exception("Can't pack row into 32 bits");
        }

        foreach (var trail in Enumerable.Range(0, row.Length))
        {
            var value = row[trail];
            if (value < 0)
            {
                value = nonJoinerValue;
            }
            if (value > (1 << bitWidth) - 1)
            {
                throw new Exception("Can't pack table index into 2 bits");
            }
            nextIndices |= (uint)(value << (trail * bitWidth));
        }

        compressed[lead] = nextIndices;
    }

    return compressed;
}

// This tries all possible trie configurations and returns the one with the smallest size. It's brute force.
static Trie BuildBestTrie(List<TrieType> uncompressed, int minShift, int maxShift, int stages)
{
    var depth = stages - 1;
    var delta = maxShift - minShift + 1;
    var total = 1;
    for (var i = 0; i < depth; i++)
    {
        total *= delta;
    }

    var tasks = new int[total][];
    for (var i = 0; i < total; i++)
    {
        // Given minShift=2, maxShift=3, depth=3 this generates
        //   [2 2 2]
        //   [3 2 2]
        //   [2 3 2]
        //   [3 3 2]
        //   [2 2 3]
        //   [3 2 3]
        //   [2 3 3]
        //   [3 3 3]
        var shifts = new int[depth];
        for (int j = 0, index = i; j < depth; j++, index /= delta)
        {
            shifts[j] = minShift + index % delta;
        }

        tasks[i] = shifts;
    }

    return tasks.AsParallel().Select(shifts => BuildTrie(uncompressed, shifts)).MinBy(t => t.TotalSize)!;
}

// Compresses the given uncompressed data into a multi-level trie with shifts.Count+1 stages.
// shifts defines the power-of-two sizes of the deduplicated chunks in each stage.
// The final output receives no deduplication which is why this returns shifts.Count+1 stages.
static Trie BuildTrie(List<TrieType> uncompressed, Span<int> shifts)
{
    var cumulativeShift = 0;
    var stages = new List<Stage>();

    for (var i = 0; i < shifts.Length; i++)
    {
        var shift = shifts[i];
        var chunkSize = 1 << shift;
        var cache = new Dictionary<ReadOnlyTrieTypeSpan, TrieType>();
        var compressed = new List<TrieType>();
        var offsets = new List<TrieType>();

        for (var off = 0; off < uncompressed.Count; off += chunkSize)
        {
            var key = new ReadOnlyTrieTypeSpan(uncompressed, off, Math.Min(chunkSize, uncompressed.Count - off));

            // Cast the integer slice to a string so that it can be hashed.

            if (!cache.TryGetValue(key, out var offset))
            {
                // For a 4-stage trie searching for existing occurrences of chunk in compressed yields a ~10%
                // compression improvement. Checking for overlaps with the tail end of compressed yields another ~15%.
                // FYI I tried to shuffle the order of compressed chunks but found that this has a negligible impact.
                var haystack = CollectionsMarshal.AsSpan(compressed);
                var needle = key.AsSpan();
                var existing = FindExisting(haystack, needle);

                if (existing >= 0)
                {
                    offset = (TrieType)existing;
                }
                else
                {
                    var overlap = MeasureOverlap(CollectionsMarshal.AsSpan(compressed), needle);
                    compressed.AddRange(needle[overlap..]);
                    offset = (TrieType)(compressed.Count - needle.Length);
                }

                cache[key] = offset;
            }

            offsets.Add(offset);
        }

        stages.Add(new Stage
        {
            Values = compressed,
            Index = shifts.Length - i,
            Shift = cumulativeShift,
            Mask = chunkSize - 1,
            Bits = 0,
        });

        uncompressed = offsets;
        cumulativeShift += shift;
    }

    stages.Add(new Stage
    {
        Values = uncompressed,
        Index = 0,
        Shift = cumulativeShift,
        Mask = int.MaxValue,
        Bits = 0,
    });

    stages.Reverse();

    foreach (var s in stages)
    {
        var m = s.Values.Max();
        s.Bits = m switch
        {
            <= 0xff => 8,
            <= 0xffff => 16,
            _ => 32
        };
    }

    return new Trie
    {
        Stages = stages,
        TotalSize = stages.Sum(s => (s.Bits / 8) * s.Values.Count)
    };
}

// Finds needle in haystack. Returns -1 if it couldn't be found.
static int FindExisting(ReadOnlySpan<TrieType> haystack, ReadOnlySpan<TrieType> needle)
{
    var idx = haystack.IndexOf(needle);
    return idx;
}

// Given two slices, this returns the amount by which `prev`s end overlaps with `next`s start.
// That is, given [0,1,2,3,4] and [2,3,4,5] this returns 3 because [2,3,4] is the "overlap".
static int MeasureOverlap(ReadOnlySpan<TrieType> prev, ReadOnlySpan<TrieType> next)
{
    for (var overlap = Math.Min(prev.Length, next.Length); overlap >= 0; overlap--)
    {
        if (prev[^overlap..].SequenceEqual(next[..overlap]))
        {
            return overlap;
        }
    }

    return 0;
}

internal enum CharacterWidth
{
    ZeroWidth,
    Narrow,
    Wide,
    Ambiguous
}

internal enum ClusterBreak
{
    Other,         // GB999
    Extend,        // GB9, GB9a -- includes SpacingMark
    RI,            // GB12, GB13
    Prepend,       // GB9b
    HangulL,       // GB6, GB7, GB8
    HangulV,       // GB6, GB7, GB8
    HangulT,       // GB6, GB7, GB8
    HangulLV,      // GB6, GB7, GB8
    HangulLVT,     // GB6, GB7, GB8
    InCBLinker,    // GB9c
    InCBConsonant, // GB9c
    ExtPic,        // GB11
    ZWJ,           // GB9, GB11

    // These are intentionally ordered last, as this allows
    // us to simplify the ucd_is_newline implementation.
    Control,       // GB4, GB5
    CR,            // GB3, GB4, GB5
    LF,            // GB3, GB4, GB5
}

internal enum LineBreak
{
    Other, // Anything else

    // Non-tailorable Line Breaking Classes
    WordJoiner, // WJ
    ZeroWidthSpace, // ZW
    Glue, // GL
    Space, // SP

    // Break Opportunities
    BreakAfter, // BA
    BreakBefore, // BB
    Hyphen, // HY

    // Characters Prohibiting Certain Breaks
    ClosePunctuation, // CL
    CloseParenthesis_EA, // CP, East Asian
    CloseParenthesis_NotEA, // CP, not East Asian
    Exclamation, // EX
    Inseparable, // IN
    Nonstarter, // NS
    OpenPunctuation_EA, // OP, East Asian
    OpenPunctuation_NotEA, // OP, not East Asian
    Quotation, // QU

    // Numeric Context
    InfixNumericSeparator, // IS
    Numeric, // NU
    PostfixNumeric, // PO
    PrefixNumeric, // PR
    SymbolsAllowingBreakAfter, // SY

    // Other Characters
    Alphabetic, // AL & HL
    Ideographic, // ID & EB & EM
}

internal class Ucd
{
    public required string Description;
    public required List<TrieType> Values;
}

internal class Stage
{
    public required List<TrieType> Values;
    public required int Index;
    public required int Shift;
    public required int Mask;
    public required int Bits;
}

internal class Trie
{
    public required List<Stage> Stages;
    public required int TotalSize;
}

// Because you can't put a Span<TrieType> into a Dictionary.
// This works around that by simply keeping a reference to the List<TrieType> around.
internal readonly struct ReadOnlyTrieTypeSpan(List<TrieType> list, int start, int length)
{
    public ReadOnlySpan<TrieType> AsSpan() => CollectionsMarshal.AsSpan(list).Slice(start, length);

    public override bool Equals(object? obj)
    {
        return obj is ReadOnlyTrieTypeSpan other && AsSpan().SequenceEqual(other.AsSpan());
    }

    public override int GetHashCode()
    {
        HashCode hashCode = default;
        hashCode.AddBytes(MemoryMarshal.AsBytes(AsSpan()));
        return hashCode.ToHashCode();
    }
}
