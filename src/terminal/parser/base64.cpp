// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "base64.hpp"

#include <til/u8u16convert.h>

using namespace Microsoft::Console::VirtualTerminal;

// clang-format off
static constexpr uint8_t decodeTable[128] = {
	255 /* NUL */, 255 /* SOH */, 255 /* STX */, 255 /* ETX */, 255 /* EOT */, 255 /* ENQ */, 255 /* ACK */, 255 /* BEL */, 255 /* BS  */, 255 /* HT  */, 64  /* LF  */, 255 /* VT  */, 255 /* FF  */, 64  /* CR  */, 255 /* SO  */, 255 /* SI  */,
	255 /* DLE */, 255 /* DC1 */, 255 /* DC2 */, 255 /* DC3 */, 255 /* DC4 */, 255 /* NAK */, 255 /* SYN */, 255 /* ETB */, 255 /* CAN */, 255 /* EM  */, 255 /* SUB */, 255 /* ESC */, 255 /* FS  */, 255 /* GS  */, 255 /* RS  */, 255 /* US  */,
	255 /* SP  */, 255 /* !   */, 255 /* "   */, 255 /* #   */, 255 /* $   */, 255 /* %   */, 255 /* &   */, 255 /* '   */, 255 /* (   */, 255 /* )   */, 255 /* *   */, 62  /* +   */, 255 /* ,   */, 62  /* -   */, 255 /* .   */, 63  /* /   */,
	52  /* 0   */, 53  /* 1   */, 54  /* 2   */, 55  /* 3   */, 56  /* 4   */, 57  /* 5   */, 58  /* 6   */, 59  /* 7   */, 60  /* 8   */, 61  /* 9   */, 255 /* :   */, 255 /* ;   */, 255 /* <   */, 255 /* =   */, 255 /* >   */, 255 /* ?   */,
	255 /* @   */, 0   /* A   */, 1   /* B   */, 2   /* C   */, 3   /* D   */, 4   /* E   */, 5   /* F   */, 6   /* G   */, 7   /* H   */, 8   /* I   */, 9   /* J   */, 10  /* K   */, 11  /* L   */, 12  /* M   */, 13  /* N   */, 14  /* O   */,
	15  /* P   */, 16  /* Q   */, 17  /* R   */, 18  /* S   */, 19  /* T   */, 20  /* U   */, 21  /* V   */, 22  /* W   */, 23  /* X   */, 24  /* Y   */, 25  /* Z   */, 255 /* [   */, 255 /* \   */, 255 /* ]   */, 255 /* ^   */, 63  /* _   */,
	255 /* `   */, 26  /* a   */, 27  /* b   */, 28  /* c   */, 29  /* d   */, 30  /* e   */, 31  /* f   */, 32  /* g   */, 33  /* h   */, 34  /* i   */, 35  /* j   */, 36  /* k   */, 37  /* l   */, 38  /* m   */, 39  /* n   */, 40  /* o   */,
	41  /* p   */, 42  /* q   */, 43  /* r   */, 44  /* s   */, 45  /* t   */, 46  /* u   */, 47  /* v   */, 48  /* w   */, 49  /* x   */, 50  /* y   */, 51  /* z   */, 255 /* {   */, 255 /* |   */, 255 /* }   */, 255 /* ~   */, 255 /* DEL */,
};
// clang-format on

// Routine Description:
// - Decode a base64 string. This requires the base64 string is properly padded.
//      Otherwise, false will be returned.
// Arguments:
// - src - String to decode.
// - dst - Destination to decode into.
// Return Value:
// - true if decoding successfully, otherwise false.
void Base64::s_Decode(const std::wstring_view src, std::wstring& dst)
{
    std::string result;
    result.resize((src.size() / 4) * 3);

    auto in = src.data();
    const auto inEnd = in + src.size();
    const auto inEndBatched = inEnd - 3;
    const auto outBeg = reinterpret_cast<uint8_t*>(result.data());
    auto out = outBeg;
    uint_fast32_t r = 0;
    uint_fast8_t ri = 0;
    uint_fast16_t error = 0;

#define accumulate(ch)                         \
    do                                         \
    {                                          \
        const auto n = decodeTable[ch & 0x7f]; \
                                               \
        error |= (ch | n) & 0xff80;            \
                                               \
        if ((n & 0b01000000) == 0)             \
        {                                      \
            r = r << 6 | n;                    \
            ri++;                              \
        }                                      \
    } while (0)

    while (in < inEndBatched)
    {
        const auto a = in[0];
        const auto b = in[1];
        const auto c = in[2];
        const auto d = in[3];

        accumulate(a);
        accumulate(b);
        accumulate(c);
        accumulate(d);

        switch (ri)
        {
        case 2:
            out[0] = uint8_t(r >> 4);
            out += 1;
            ri = 1;
            break;
        case 3:
            out[0] = uint8_t(r >> 10);
            out[1] = uint8_t(r >> 2);
            out += 2;
            ri = 1;
            break;
        case 4:
            out[0] = uint8_t(r >> 16);
            out[1] = uint8_t(r >> 8);
            out[2] = uint8_t(r >> 0);
            out += 3;
            ri = 0;
            break;
        }

        in += 4;
    }

    for (size_t i = 0, remaining = inEnd - in; i < remaining; i++)
    {
        const auto ch = in[i];
        accumulate(ch);
    }

    switch (ri)
    {
    case 2:
        out[0] = uint8_t(r >> 4);
        break;
    case 3:
        out[0] = uint8_t(r >> 10);
        out[1] = uint8_t(r >> 2);
        break;
    case 4:
        out[0] = uint8_t(r >> 16);
        out[1] = uint8_t(r >> 8);
        out[2] = uint8_t(r >> 0);
        break;
    }

    if (error)
    {
        throw std::runtime_error("invalid base64");
    }

    result.resize(out - outBeg);
    til::u8u16(result, dst);
}
