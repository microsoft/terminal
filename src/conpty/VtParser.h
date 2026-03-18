// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// A reusable VT tokenizer, ported from the Rust implementation in Microsoft Edit.
//
// The parser produces tokens from a UTF-8 byte stream. It handles chunked input
// correctly — if a sequence is split across two feed() calls, the parser buffers
// the incomplete prefix and completes it on the next call.
//
// Usage:
//   VtParser parser;
//   VtParser::Stream stream = parser.parse(input);
//   VtToken token;
//   while (stream.next(token)) {
//       switch (token.type) { ... }
//   }

#pragma once

#include <cstdint>
#include <string_view>

// A single CSI sequence, parsed for your convenience.
struct VtCsi
{
    // The parameters of the CSI sequence.
    uint16_t params[32]{};
    // The number of parameters stored in params[].
    size_t paramCount = 0;
    // The private byte, if any. '\0' if none.
    // The private byte is the first character right after the ESC [ sequence.
    // It is usually a '?' or '<'.
    char privateByte = '\0';
    // The final byte of the CSI sequence.
    // This is the last character of the sequence, e.g. 'm' or 'H'.
    char finalByte = '\0';
};

struct VtToken
{
    enum Type : uint8_t
    {
        // A bunch of text. Doesn't contain any control characters.
        Text,
        // A single control character, like backspace or return.
        Ctrl,
        // We encountered ESC x and this contains x (in `ch`).
        Esc,
        // We encountered ESC O x and this contains x (in `ch`).
        SS3,
        // A CSI sequence started with ESC [. See `csi`.
        Csi,
        // An OSC sequence started with ESC ]. May be partial (chunked).
        Osc,
        // A DCS sequence started with ESC P. May be partial (chunked).
        Dcs,
    };

    Type type = Text;
    // For Ctrl: the control byte itself.
    // For Esc/SS3: the character after ESC / ESC O.
    char ch = '\0';
    // For Csi: pointer to the parser's Csi struct. Valid until the next next() call.
    const VtCsi* csi = nullptr;
    // For Text/Osc/Dcs: the string payload (points into the input buffer, zero-copy).
    std::string_view payload;
    // For Osc/Dcs: true if the sequence is incomplete (split across chunks).
    bool partial = false;
};

class VtParser
{
public:
    class Stream;

    VtParser() = default;

    // Returns true if the parser is in the middle of an ESC sequence,
    // meaning the caller should apply a timeout before the next parse() call.
    // If the timeout fires, call parse("") to flush the bare ESC.
    bool hasEscTimeout() const noexcept;

    // Begin parsing the given input. Returns a Stream that yields tokens.
    // The returned Stream borrows from both `this` and `input` — do not
    // modify either while the Stream is alive.
    Stream parse(std::string_view input) noexcept;

private:
    enum class State : uint8_t
    {
        Ground,
        Esc,
        Ss3,
        Csi,
        Osc,
        Dcs,
        OscEsc,
        DcsEsc,
    };

    State m_state = State::Ground;
    VtCsi m_csi;
};

// An iterator that yields VtTokens from a single parse() call.
// This is a "lending iterator" — the token references data owned by
// the parser and the input string_view.
class VtParser::Stream
{
public:
    Stream(VtParser* parser, std::string_view input) noexcept
        : m_parser(parser), m_input(input) {}

    // The input being parsed.
    std::string_view input() const noexcept { return m_input; }

    // Current byte offset into the input.
    size_t offset() const noexcept { return m_off; }

    // True if all input has been consumed.
    bool done() const noexcept { return m_off >= m_input.size(); }

    // Get the next token. Returns false when no more complete tokens
    // can be extracted (remaining bytes are an incomplete sequence).
    bool next(VtToken& out);

    // Decode and consume one UTF-8 codepoint. Returns '\0' at end.
    char32_t nextChar();

private:
    VtParser* m_parser;
    std::string_view m_input;
    size_t m_off = 0;
};
