#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "VtParser.h"

// InputBuffer: Text-based input buffer with an integrated VT parser.
//
// The hosting terminal writes UTF-8 VT sequences via write(). Consumers
// (console API handlers) dequeue data in one of two forms:
//
//   1. As raw VT text (ENABLE_VIRTUAL_TERMINAL_INPUT is set):
//      readRawText() returns the raw bytes 1:1.
//
//   2. As INPUT_RECORDs (ENABLE_VIRTUAL_TERMINAL_INPUT is NOT set):
//      readInputRecords() uses VtParser to tokenize sequences on-demand
//      and converts them to key events:
//        - Win32InputMode (W32IM, CSI ... _) for full INPUT_RECORD fidelity
//        - Standard VT520 cursor/function keys (CSI A-D, CSI ~, SS3)
//        - Plain text (each codepoint -> key down + key up pair)
//
// Thread safety: NOT thread-safe. All calls must be serialized by the caller.

class InputBuffer
{
public:
    InputBuffer() = default;

    // Append UTF-8 text to the input buffer.
    void write(std::string_view text);

    // True if there is any data available for reading.
    bool hasData() const noexcept;

    // Returns the number of pending INPUT_RECORDs (rough estimate).
    size_t pendingEventCount() const noexcept;

    // Read raw VT text (for ENABLE_VIRTUAL_TERMINAL_INPUT mode).
    size_t readRawText(char* dst, size_t dstCapacity);

    // Generate INPUT_RECORDs from the buffer (for legacy mode).
    size_t readInputRecords(INPUT_RECORD* dst, size_t maxRecords, bool peek = false);

    // Discard all buffered data.
    void flush();

private:
    struct ParsedKey
    {
        WORD vk = 0;
        WORD scanCode = 0;
        wchar_t ch = 0;
        DWORD modifiers = 0;
        WORD repeatCount = 1;
        bool keyDown = true;
        bool isW32IM = false;
    };

    // Convert VtTokens into INPUT_RECORDs.
    void parseTokensToRecords();

    // Token interpretation helpers.
    void handleText(std::string_view text);
    void handleCtrl(char ch);
    void handleEsc(char ch);
    void handleSs3(char ch);
    void handleCsi(const VtCsi& csi);

    void emitKey(const ParsedKey& key);

    static WORD vkToScanCode(WORD vk);
    static DWORD vtModifierToControlKeyState(uint16_t vtMod);

    void compact();

    std::string m_buf;
    size_t m_readPos = 0;

    VtParser m_parser;

    std::vector<INPUT_RECORD> m_records;
    size_t m_recordReadPos = 0;
};
