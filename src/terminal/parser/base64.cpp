// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "base64.hpp"

#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::VirtualTerminal;

// clang-format off
static constexpr uint8_t decodeTable[128] = {
    255 /* NUL */, 255 /* SOH */, 255 /* STX */, 255 /* ETX */, 255 /* EOT */, 255 /* ENQ */, 255 /* ACK */, 255 /* BEL */, 255 /* BS  */, 255 /* HT  */, 255 /* LF  */, 255 /* VT  */, 255 /* FF  */, 255 /* CR  */, 255 /* SO  */, 255 /* SI  */,
    255 /* DLE */, 255 /* DC1 */, 255 /* DC2 */, 255 /* DC3 */, 255 /* DC4 */, 255 /* NAK */, 255 /* SYN */, 255 /* ETB */, 255 /* CAN */, 255 /* EM  */, 255 /* SUB */, 255 /* ESC */, 255 /* FS  */, 255 /* GS  */, 255 /* RS  */, 255 /* US  */,
    255 /* SP  */, 255 /* !   */, 255 /* "   */, 255 /* #   */, 255 /* $   */, 255 /* %   */, 255 /* &   */, 255 /* '   */, 255 /* (   */, 255 /* )   */, 255 /* *   */, 62  /* +   */, 255 /* ,   */, 62  /* -   */, 255 /* .   */, 63  /* /   */,
    52  /* 0   */, 53  /* 1   */, 54  /* 2   */, 55  /* 3   */, 56  /* 4   */, 57  /* 5   */, 58  /* 6   */, 59  /* 7   */, 60  /* 8   */, 61  /* 9   */, 255 /* :   */, 255 /* ;   */, 255 /* <   */, 255 /* =   */, 255 /* >   */, 255 /* ?   */,
    255 /* @   */, 0   /* A   */, 1   /* B   */, 2   /* C   */, 3   /* D   */, 4   /* E   */, 5   /* F   */, 6   /* G   */, 7   /* H   */, 8   /* I   */, 9   /* J   */, 10  /* K   */, 11  /* L   */, 12  /* M   */, 13  /* N   */, 14  /* O   */,
    15  /* P   */, 16  /* Q   */, 17  /* R   */, 18  /* S   */, 19  /* T   */, 20  /* U   */, 21  /* V   */, 22  /* W   */, 23  /* X   */, 24  /* Y   */, 25  /* Z   */, 255 /* [   */, 255 /* \   */, 255 /* ]   */, 255 /* ^   */, 63  /* _   */,
    255 /* `   */, 26  /* a   */, 27  /* b   */, 28  /* c   */, 29  /* d   */, 30  /* e   */, 31  /* f   */, 32  /* g   */, 33  /* h   */, 34  /* i   */, 35  /* j   */, 36  /* k   */, 37  /* l   */, 38  /* m   */, 39  /* n   */, 40  /* o   */,
    41  /* p   */, 42  /* q   */, 43  /* r   */, 44  /* s   */, 45  /* t   */, 46  /* u   */, 47  /* v   */, 48  /* w   */, 49  /* x   */, 50  /* y   */, 51  /* z   */, 255 /* {   */, 255 /* |   */, 255 /* }   */, 255 /* ~   */, 255 /* DEL */,
};
// clang-format on

// Decodes an UTF8 string encoded with RFC 4648 (Base64) and returns it as UTF16 in dst.
// It supports both variants of the RFC (base64 and base64url), but
// throws an error for non-alphabet characters, including newlines.
// * Throws an exception for all invalid base64 inputs.
// * Doesn't support whitespace and will throw an exception for such strings.
// * Doesn't validate the number of trailing "=". Those are basically ignored.
//   Strings like "YQ===" will be accepted as valid input and simply result in "a".
void Base64::Decode(const std::wstring_view& src, std::wstring& dst)
{
    std::string result;
    result.resize(((src.size() + 3) / 4) * 3);

    // in and inEnd may be nullptr if src.empty().
    // The remaining code in this function ensures not to read from in if src.empty().
#pragma warning(suppress : 26429) // Symbol 'in' is never tested for nullness, it can be marked as not_null (f.23).
    auto in = src.data();
    auto inEnd = in + src.size();
    // Sometimes in programming you have to ask yourself what the right offset for a pointer is.
    // Is 4 enough? Certainly not. 6 on the other hand is just way too much. Clearly 5 is just right.
    //
    // In all seriousness however the offset is 5, because the batched loop reads 4 characters at a time,
    // a base64 string can end with two "=" and the batched loop doesn't handle any such "=".
    // Additionally the while() condition of the batched loop would make a lot more sense if it were using <=,
    // but for reasons outlined below it needs to use < so we need to add 1 back again.
    // We thus get -4-2+1 which is -5.
    //
    // There's a special reason we need to use < and not <= for the loop:
    // In C++ it's undefined behavior to perform any pointer arithmetic that leads to unallocated memory,
    // which is why we can't just write `inEnd - 6` as that might be UB if `src.size()` is less than 6.
    // We thus would need write `inEnd - min(6, src.size())` in combination with `<=` for the batched loop.
    // But if `src.size()` is actually less than 6 then `inEnd` is equal to the initial `in`, aka: an empty range.
    // In such cases we'd enter the batched loop and read from `in` despite us not wanting to enter the loop.
    // We can fix the issue by using < instead and adding +1 to the offset.
    //
    // Yes this works.
    const auto inEndBatched = inEnd - std::min<size_t>(5, src.size());

    // outBeg and out may be nullptr if src.empty().
    // The remaining code in this function ensures not to write to out if src.empty().
    const auto outBeg = result.data();
#pragma warning(suppress : 26429) // Symbol 'out' is never tested for nullness, it can be marked as not_null (f.23).
    auto out = outBeg;

    uint_fast32_t r = 0;
    uint_fast16_t error = 0;

#define accumulate(ch)                         \
    do                                         \
    {                                          \
        const auto n = decodeTable[ch & 0x7f]; \
        error |= (ch | n) & 0xff80;            \
        r = r << 6 | n;                        \
    } while (0)

    // If src.empty() then `in == inEndBatched == nullptr` and this is skipped.
    while (in < inEndBatched)
    {
        const auto a = *in++;
        const auto b = *in++;
        const auto c = *in++;
        const auto d = *in++;

        accumulate(a);
        accumulate(b);
        accumulate(c);
        accumulate(d);

        *out++ = gsl::narrow_cast<char>(r >> 16);
        *out++ = gsl::narrow_cast<char>(r >> 8);
        *out++ = gsl::narrow_cast<char>(r >> 0);
    }

    {
        uint_fast8_t ri = 0;

        // If src.empty() then `in == inEnd == nullptr` and this is skipped.
        for (; in < inEnd; ++in)
        {
            if (const auto ch = *in; ch != '=')
            {
                accumulate(ch);
                ri++;
            }
        }

        switch (ri)
        {
        case 2:
            *out++ = gsl::narrow_cast<char>(r >> 4);
            break;
        case 3:
            *out++ = gsl::narrow_cast<char>(r >> 10);
            *out++ = gsl::narrow_cast<char>(r >> 2);
            break;
        case 4:
            *out++ = gsl::narrow_cast<char>(r >> 16);
            *out++ = gsl::narrow_cast<char>(r >> 8);
            *out++ = gsl::narrow_cast<char>(r >> 0);
            break;
        default:
            error |= ri;
            break;
        }
    }

#undef accumulate

    if (error)
    {
        throw std::runtime_error("invalid base64");
    }

    result.resize(out - outBeg);
    THROW_IF_FAILED(til::u8u16(result, dst));
}
