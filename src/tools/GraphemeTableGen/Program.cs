using System.Text;
using System.Runtime.InteropServices;
using System.Xml.Linq;

using TrieType = uint;

// Used as an indicator in joinRules for ÷ ("does not join").
// Underscore is one of the few characters that are permitted as an identifier,
// are monospace in most fonts and also visually distinct from the digits.
const byte _ = 3;

// JoinRules doesn't quite follow UAX #29, as it states:
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
byte[][][] joinRules =
[
    // Base table
    [
        /* | leading       -> trailing codepoint                                                                                                                                             */
        /* v             |   Other  |  Control |  Extend  |    RI    |  Prepend |  HangulL |  HangulV |  HangulT | HangulLV | HangulLVT | InCBLinker | InCBConsonant |  ExtPic  |    ZWJ   | */
        /* Other         | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* Control       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, _ /*  |   */, _ /*    | */, _ /* | */, _ /* | */],
        /* Extend        | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* RI            | */ [_ /* | */, _ /* | */, 0 /* | */, 1 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* Prepend       | */ [0 /* | */, _ /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /*  |  */, 0 /*  |   */, 0 /*    | */, 0 /* | */, 0 /* | */],
        /* HangulL       | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, 0 /* | */, 0 /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulV       | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulT       | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulLV      | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulLVT     | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* InCBLinker    | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, 0 /*    | */, _ /* | */, 0 /* | */],
        /* InCBConsonant | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* ExtPic        | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* ZWJ           | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, 0 /* | */, 0 /* | */],
    ],
    // Once we have encountered a Regional Indicator pair we'll enter this table.
    // It's a copy of the base table, but instead of RI × RI, we're RI ÷ RI.
    [
        /* | leading       -> trailing codepoint                                                                                                                                             */
        /* v             |   Other  |  Control |  Extend  |    RI    |  Prepend |  HangulL |  HangulV |  HangulT | HangulLV | HangulLVT | InCBLinker | InCBConsonant |  ExtPic  |    ZWJ   | */
        /* Other         | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* Control       | */ [_ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, _ /*  |   */, _ /*    | */, _ /* | */, _ /* | */],
        /* Extend        | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* RI            | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* Prepend       | */ [0 /* | */, _ /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /* | */, 0 /*  |  */, 0 /*  |   */, 0 /*    | */, 0 /* | */, 0 /* | */],
        /* HangulL       | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, 0 /* | */, 0 /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulV       | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulT       | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulLV      | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* HangulLVT     | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* InCBLinker    | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, 0 /*    | */, _ /* | */, 0 /* | */],
        /* InCBConsonant | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* ExtPic        | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, _ /* | */, 0 /* | */],
        /* ZWJ           | */ [_ /* | */, _ /* | */, 0 /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /* | */, _ /*  |  */, 0 /*  |   */, _ /*    | */, 0 /* | */, 0 /* | */],
    ],
];

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
var rules = PrepareRulesTable(joinRules);
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
buf.Append("// Generated by GraphemeTableGen\n");
buf.Append($"// on {DateTime.UtcNow.ToString("yyyy'-'MM'-'dd'T'HH':'mm':'ssK")}, from {ucd.Description}, {totalSize} bytes\n");
buf.Append("// clang-format off\n");

foreach (var stage in trie.Stages)
{
    var fmt = $" 0x{{0:x{stage.Bits / 4}}},";
    var width = 16;
    if (stage.Index != 0)
    {
        width = stage.Mask + 1;
    }

    buf.Append($"static constexpr uint{stage.Bits}_t s_stage{stage.Index}[] = {{");
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

buf.Append($"static constexpr uint32_t s_joinRules[{rules.Length}][{rules[0].Length}] = {{\n");
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

buf.Append("constexpr int ucdLookup(const char32_t cp) noexcept\n");
buf.Append("{\n");
foreach (var stage in trie.Stages)
{
    buf.Append($"    const auto s{stage.Index} = s_stage{stage.Index}[");
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

buf.Append("constexpr int ucdGraphemeJoins(const int state, const int lead, const int trail) noexcept\n");
buf.Append("{\n");
buf.Append("    const auto l = lead & 15;\n");
buf.Append("    const auto t = trail & 15;\n");
buf.Append($"    return (s_joinRules[state][l] >> (t * 2)) & 3;\n");
buf.Append("}\n");
buf.Append("constexpr bool ucdGraphemeDone(const int state) noexcept\n");
buf.Append("{\n");
buf.Append($"    return state == 3;\n");
buf.Append("}\n");
buf.Append("constexpr int ucdToCharacterWidth(const int val) noexcept\n");
buf.Append("{\n");
buf.Append("    return val >> 6;\n");
buf.Append("}\n");
buf.Append("// clang-format on\n");

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
                "CR" or "LF" or "CN" => ClusterBreak.Control, // Carriage Return, Line Feed, Control
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
                "A" => CharacterWidth.Ambiguous, // Ambiguous
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

// Because each item in the list of 2D rule tables only uses 2 bits and not all 8 in each byte,
// this function packs them into chunks of 32-bit integers to save space.
static uint[][] PrepareRulesTable(byte[][][] rules)
{
    var compressed = new uint[rules.Length][];
    for (var i = 0; i < compressed.Length; i++)
    {
        compressed[i] = new uint[16];
    }

    foreach (var (table, prevIndex) in rules.Select((v, i) => (v, i)))
    {
        foreach (var (row, lead) in table.Select((v, i) => (v, i)))
        {
            if (table[lead].Length > 16)
            {
                throw new Exception("Can't pack row into 32 bits");
            }

            uint nextIndices = 0;
            foreach (var (nextIndex, trail) in row.Select((v, i) => (v, i)))
            {
                if (nextIndex > 3)
                {
                    throw new Exception("Can't pack table index into 2 bits");
                }

                nextIndices |= (uint)(nextIndex << (trail * 2));
            }

            compressed[prevIndex][lead] = nextIndices;
        }
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
    Control,       // GB3, GB4, GB5 -- includes CR, LF
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
