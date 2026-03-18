#pragma once

#include <string>
#include <string_view>
#include <vector>

// InputBuffer: Text-based input buffer with an integrated VT parser.
//
// The hosting terminal writes UTF-8 VT sequences via write(). Consumers
// (console API handlers) dequeue data in one of two forms:
//
//   1. As raw VT text (ENABLE_VIRTUAL_TERMINAL_INPUT is set):
//      readRawText() returns the raw bytes 1:1.
//
//   2. As INPUT_RECORDs (ENABLE_VIRTUAL_TERMINAL_INPUT is NOT set):
//      readInputRecords() parses VT sequences on-demand and converts them
//      to key events. The parser recognises:
//        - Win32InputMode (W32IM, CSI ... _) for full INPUT_RECORD fidelity
//        - Standard VT520 cursor/function keys (CSI A–D, CSI ~, SS3)
//        - Plain text (each grapheme cluster → key down + key up pair)
//
// The parser is intentionally minimal — just enough to cover what terminals
// actually send. It does NOT handle mouse, focus, or DCS sequences since
// those are irrelevant for the input record path.
//
// Thread safety: NOT thread-safe. All calls must be serialized by the caller
// (Server's message loop + WriteInput are on the same thread or the
//  caller holds the console lock).

class InputBuffer
{
public:
    InputBuffer() = default;

    // Append UTF-8 text to the input buffer.
    void write(std::string_view text);

    // True if there is any data available for reading.
    bool hasData() const noexcept;

    // Returns the number of pending INPUT_RECORDs.
    // This is an estimate — it counts the *minimum* number of records
    // currently parseable from the buffer.
    size_t pendingEventCount() const noexcept;

    // Read raw VT text (for ENABLE_VIRTUAL_TERMINAL_INPUT mode).
    // Returns the number of bytes copied. `dst` is the output buffer
    // of `dstCapacity` bytes.
    size_t readRawText(char* dst, size_t dstCapacity);

    // Generate INPUT_RECORDs from the buffer (for legacy mode).
    // Fills `dst` with up to `maxRecords` records.
    // Returns the number of records written.
    // If `peek` is true, the buffer position is not advanced.
    size_t readInputRecords(INPUT_RECORD* dst, size_t maxRecords, bool peek = false);

    // Discard all buffered data.
    void flush();

private:
    // The VT parser state machine.
    enum class ParserState
    {
        Ground,    // Normal text.
        Escape,    // After ESC.
        CsiEntry,  // After ESC [, collecting parameters.
        Ss3,       // After ESC O.
    };

    // A parsed input event (intermediate form between VT and INPUT_RECORD).
    struct ParsedKey
    {
        WORD vk = 0;
        WORD scanCode = 0;
        wchar_t ch = 0;
        DWORD modifiers = 0;
        WORD repeatCount = 1;
        bool keyDown = true;
        bool isW32IM = false; // If true, emit exactly as specified (no synthetic up/down).
    };

    // Parse as many events as possible from m_buf starting at m_readPos.
    // Appends to m_records. Returns the new read position.
    size_t parse(size_t pos);

    // Parse helpers — each returns the number of bytes consumed (0 = need more data).
    size_t parseGround(size_t pos);
    size_t parseCsi(size_t pos);
    size_t parseSs3(size_t pos);

    // CSI parameter parsing. Parses semicolon-separated decimal integers
    // starting at `pos`. Returns the position after the last parameter digit.
    // Populates m_params and m_paramCount.
    size_t parseCsiParams(size_t pos);

    // Emit an INPUT_RECORD pair (key down + key up) into m_records.
    void emitKey(const ParsedKey& key);

    // Map a VK + modifiers to a scan code via MapVirtualKey.
    static WORD vkToScanCode(WORD vk);

    // Map VT modifier parameter (1-based) to dwControlKeyState flags.
    static DWORD vtModifierToControlKeyState(int vtMod);

    // Compact the buffer: discard consumed bytes.
    void compact();

    // Raw byte buffer. Text appended by write(), consumed by read*/parse.
    std::string m_buf;
    size_t m_readPos = 0;

    // Pre-parsed INPUT_RECORD queue. Filled by parse(), drained by readInputRecords().
    std::vector<INPUT_RECORD> m_records;
    size_t m_recordReadPos = 0;

    // CSI parameter scratch space.
    static constexpr size_t MaxParams = 8;
    int m_params[MaxParams]{};
    size_t m_paramCount = 0;
};
