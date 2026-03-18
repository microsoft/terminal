#include "pch.h"
#include "InputBuffer.h"

#pragma warning(disable : 4100)

void InputBuffer::write(std::string_view text)
{
    m_buf.append(text);
}

bool InputBuffer::hasData() const noexcept
{
    return m_readPos < m_buf.size();
}

size_t InputBuffer::pendingEventCount() const noexcept
{
    // Rough lower-bound: each byte in the buffer can generate at least 1 record,
    // and we may already have pre-parsed records queued.
    const auto queued = (m_records.size() > m_recordReadPos) ? (m_records.size() - m_recordReadPos) : 0;
    const auto rawBytes = (m_buf.size() > m_readPos) ? (m_buf.size() - m_readPos) : 0;
    // Each raw byte produces at most 2 records (key down + up), so this is
    // an over-estimate. But for GetNumberOfConsoleInputEvents we need a
    // lower bound that's non-zero when data is available.
    return queued + rawBytes;
}

size_t InputBuffer::readRawText(char* dst, size_t dstCapacity)
{
    const auto available = m_buf.size() - m_readPos;
    const auto toCopy = std::min(available, dstCapacity);
    if (toCopy > 0)
    {
        memcpy(dst, m_buf.data() + m_readPos, toCopy);
        m_readPos += toCopy;
        compact();
    }
    return toCopy;
}

size_t InputBuffer::readInputRecords(INPUT_RECORD* dst, size_t maxRecords, bool peek)
{
    // Parse any new data from the raw buffer into m_records.
    if (m_readPos < m_buf.size())
    {
        m_readPos = parse(m_readPos);
    }

    const auto available = m_records.size() - m_recordReadPos;
    const auto toCopy = std::min(available, maxRecords);
    if (toCopy > 0)
    {
        memcpy(dst, m_records.data() + m_recordReadPos, toCopy * sizeof(INPUT_RECORD));
        if (!peek)
        {
            m_recordReadPos += toCopy;

            // If we've consumed all records, reset the vector.
            if (m_recordReadPos == m_records.size())
            {
                m_records.clear();
                m_recordReadPos = 0;
            }
        }
    }

    if (!peek)
    {
        compact();
    }

    return toCopy;
}

void InputBuffer::flush()
{
    m_buf.clear();
    m_readPos = 0;
    m_records.clear();
    m_recordReadPos = 0;
}

void InputBuffer::compact()
{
    // Only compact when we've consumed a meaningful prefix.
    if (m_readPos > 4096 || (m_readPos > 0 && m_readPos == m_buf.size()))
    {
        m_buf.erase(0, m_readPos);
        m_readPos = 0;
    }
}

// ============================================================================
// VT Parser
// ============================================================================
//
// Grammar (simplified):
//   Ground:
//     0x1B        → Escape
//     0x00..0x1A, 0x1C..0x7F  → plain character (emit key event)
//     0x80..0xFF  → UTF-8 lead byte (consume full codepoint, emit key event)
//
//   Escape:
//     '['         → CsiEntry
//     'O'         → Ss3
//     0x20..0x7E  → Alt+<char> (emit key event with LEFT_ALT_PRESSED)
//     timeout/buf-end → bare ESC key
//
//   CsiEntry:
//     '0'..'9',';' → accumulate parameters
//     'A'..'Z','a'..'z','~','_' → dispatch CSI sequence
//
//   Ss3:
//     'A'..'Z'    → dispatch SS3 key

size_t InputBuffer::parse(size_t pos)
{
    while (pos < m_buf.size())
    {
        const auto consumed = parseGround(pos);
        if (consumed == 0)
            break; // Need more data.
        pos += consumed;
    }
    return pos;
}

size_t InputBuffer::parseGround(size_t pos)
{
    const auto ch = static_cast<uint8_t>(m_buf[pos]);

    if (ch == 0x1B) // ESC
    {
        // Look ahead to decide if this is ESC-prefix or bare ESC.
        if (pos + 1 >= m_buf.size())
        {
            // TODO: ESC timeout for out-of-proc hosts (e.g. sshd).
            // For now, treat a lone ESC at the end of the buffer as a bare ESC key.
            // In-proc hosts always deliver complete sequences, so this is fine.
            ParsedKey key;
            key.vk = VK_ESCAPE;
            key.scanCode = vkToScanCode(VK_ESCAPE);
            key.ch = 0x1B;
            emitKey(key);
            return 1;
        }

        const auto next = static_cast<uint8_t>(m_buf[pos + 1]);

        if (next == '[')
        {
            // ESC [ → CSI
            const auto consumed = parseCsi(pos + 2);
            if (consumed == 0)
                return 0; // Incomplete CSI, need more data.
            return 2 + consumed;
        }

        if (next == 'O')
        {
            // ESC O → SS3
            const auto consumed = parseSs3(pos + 2);
            if (consumed == 0)
                return 0; // Incomplete SS3, need more data.
            return 2 + consumed;
        }

        if (next >= 0x20 && next <= 0x7E)
        {
            // Alt+character.
            ParsedKey key;
            key.ch = static_cast<wchar_t>(next);
            key.vk = LOBYTE(VkKeyScanW(key.ch));
            key.scanCode = vkToScanCode(key.vk);
            key.modifiers = LEFT_ALT_PRESSED;
            emitKey(key);
            return 2;
        }

        // ESC followed by something unexpected — just emit bare ESC.
        ParsedKey key;
        key.vk = VK_ESCAPE;
        key.scanCode = vkToScanCode(VK_ESCAPE);
        key.ch = 0x1B;
        emitKey(key);
        return 1;
    }

    if (ch < 0x20) // C0 control characters.
    {
        ParsedKey key;
        key.ch = static_cast<wchar_t>(ch);

        switch (ch)
        {
        case '\r': // Enter
            key.vk = VK_RETURN;
            key.scanCode = vkToScanCode(VK_RETURN);
            key.ch = L'\r';
            break;
        case '\n': // Ctrl+Enter or LF
            key.vk = VK_RETURN;
            key.scanCode = vkToScanCode(VK_RETURN);
            key.ch = L'\n';
            key.modifiers = LEFT_CTRL_PRESSED;
            break;
        case '\t': // Tab
            key.vk = VK_TAB;
            key.scanCode = vkToScanCode(VK_TAB);
            key.ch = L'\t';
            break;
        case '\b': // Backspace
            key.vk = VK_BACK;
            key.scanCode = vkToScanCode(VK_BACK);
            key.ch = L'\b';
            break;
        case 0x00: // Ctrl+Space or Ctrl+@
            key.vk = VK_SPACE;
            key.scanCode = vkToScanCode(VK_SPACE);
            key.ch = L'\0';
            key.modifiers = LEFT_CTRL_PRESSED;
            break;
        default:
            // Ctrl+A..Ctrl+Z: ch = 0x01..0x1A
            if (ch >= 0x01 && ch <= 0x1A)
            {
                key.ch = static_cast<wchar_t>(ch);
                key.vk = static_cast<WORD>('A' + ch - 1);
                key.scanCode = vkToScanCode(key.vk);
                key.modifiers = LEFT_CTRL_PRESSED;
            }
            break;
        }

        emitKey(key);
        return 1;
    }

    if (ch == 0x7F) // DEL — treat as backspace.
    {
        ParsedKey key;
        key.vk = VK_BACK;
        key.scanCode = vkToScanCode(VK_BACK);
        key.ch = L'\b';
        emitKey(key);
        return 1;
    }

    // ASCII printable or UTF-8 multi-byte sequence.
    // Decode one UTF-8 codepoint.
    size_t seqLen = 1;
    uint32_t codepoint = ch;

    if (ch >= 0x80)
    {
        if ((ch & 0xE0) == 0xC0) { seqLen = 2; codepoint = ch & 0x1F; }
        else if ((ch & 0xF0) == 0xE0) { seqLen = 3; codepoint = ch & 0x0F; }
        else if ((ch & 0xF8) == 0xF0) { seqLen = 4; codepoint = ch & 0x07; }
        else { seqLen = 1; codepoint = 0xFFFD; } // Invalid lead byte.

        if (pos + seqLen > m_buf.size())
            return 0; // Incomplete codepoint, need more data.

        for (size_t i = 1; i < seqLen; i++)
        {
            const auto cont = static_cast<uint8_t>(m_buf[pos + i]);
            if ((cont & 0xC0) != 0x80)
            {
                codepoint = 0xFFFD;
                seqLen = i; // Consume up to the bad byte.
                break;
            }
            codepoint = (codepoint << 6) | (cont & 0x3F);
        }
    }

    // Emit one or two INPUT_RECORDs (surrogate pair for codepoints > 0xFFFF).
    if (codepoint <= 0xFFFF)
    {
        ParsedKey key;
        key.ch = static_cast<wchar_t>(codepoint);
        key.vk = LOBYTE(VkKeyScanW(key.ch));
        if (key.vk == 0xFF) key.vk = 0; // No VK mapping.
        key.scanCode = vkToScanCode(key.vk);
        emitKey(key);
    }
    else if (codepoint <= 0x10FFFF)
    {
        // Surrogate pair.
        const auto hi = static_cast<wchar_t>(0xD800 + ((codepoint - 0x10000) >> 10));
        const auto lo = static_cast<wchar_t>(0xDC00 + ((codepoint - 0x10000) & 0x3FF));

        ParsedKey key;
        key.ch = hi;
        emitKey(key);
        key.ch = lo;
        emitKey(key);
    }
    else
    {
        // Invalid codepoint → U+FFFD.
        ParsedKey key;
        key.ch = 0xFFFD;
        emitKey(key);
    }

    return seqLen;
}

size_t InputBuffer::parseCsi(size_t pos)
{
    // Parse parameters and find the final byte.
    const auto paramEnd = parseCsiParams(pos);
    if (paramEnd >= m_buf.size())
        return 0; // Incomplete, need more data.

    const auto finalByte = static_cast<uint8_t>(m_buf[paramEnd]);
    const auto totalConsumed = paramEnd - pos + 1;

    // Win32 Input Mode: CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
    if (finalByte == '_')
    {
        if (m_paramCount >= 4) // At minimum: Vk, Sc, Uc, Kd
        {
            ParsedKey key;
            key.vk = static_cast<WORD>(m_params[0]);
            key.scanCode = static_cast<WORD>(m_params[1]);
            key.ch = static_cast<wchar_t>(m_params[2]);
            key.keyDown = m_params[3] != 0;
            key.modifiers = (m_paramCount >= 5) ? static_cast<DWORD>(m_params[4]) : 0;
            key.repeatCount = (m_paramCount >= 6) ? static_cast<WORD>(m_params[5]) : 1;
            if (key.repeatCount == 0)
                key.repeatCount = 1;
            key.isW32IM = true;
            emitKey(key);
        }
        return totalConsumed;
    }

    // Cursor keys and simple CSI sequences: CSI [params] <letter>
    if (finalByte >= 'A' && finalByte <= 'Z')
    {
        WORD vk = 0;
        switch (finalByte)
        {
        case 'A': vk = VK_UP; break;
        case 'B': vk = VK_DOWN; break;
        case 'C': vk = VK_RIGHT; break;
        case 'D': vk = VK_LEFT; break;
        case 'H': vk = VK_HOME; break;
        case 'F': vk = VK_END; break;
        case 'P': vk = VK_F1; break;
        case 'Q': vk = VK_F2; break;
        case 'R': vk = VK_F3; break;
        case 'S': vk = VK_F4; break;
        case 'Z': // Shift+Tab (CSI Z = "backtab")
        {
            ParsedKey key;
            key.vk = VK_TAB;
            key.scanCode = vkToScanCode(VK_TAB);
            key.ch = L'\t';
            key.modifiers = SHIFT_PRESSED;
            emitKey(key);
            return totalConsumed;
        }
        default:
            // Unknown CSI sequence — discard.
            return totalConsumed;
        }

        if (vk != 0)
        {
            ParsedKey key;
            key.vk = vk;
            key.scanCode = vkToScanCode(vk);
            key.modifiers = ENHANCED_KEY;

            // Modifier in second parameter: CSI 1;{mod} A
            if (m_paramCount >= 2)
                key.modifiers |= vtModifierToControlKeyState(m_params[1]);

            emitKey(key);
        }
        return totalConsumed;
    }

    // Generic keys: CSI {num} ~ (Insert, Delete, PgUp, PgDn, F5–F12)
    if (finalByte == '~' && m_paramCount >= 1)
    {
        WORD vk = 0;
        switch (m_params[0])
        {
        case 1:  vk = VK_HOME; break;
        case 2:  vk = VK_INSERT; break;
        case 3:  vk = VK_DELETE; break;
        case 4:  vk = VK_END; break;
        case 5:  vk = VK_PRIOR; break;  // PageUp
        case 6:  vk = VK_NEXT; break;   // PageDown
        case 15: vk = VK_F5; break;
        case 17: vk = VK_F6; break;
        case 18: vk = VK_F7; break;
        case 19: vk = VK_F8; break;
        case 20: vk = VK_F9; break;
        case 21: vk = VK_F10; break;
        case 23: vk = VK_F11; break;
        case 24: vk = VK_F12; break;
        default:
            return totalConsumed; // Unknown — discard.
        }

        ParsedKey key;
        key.vk = vk;
        key.scanCode = vkToScanCode(vk);
        key.modifiers = ENHANCED_KEY;

        // Modifier in second parameter: CSI {num};{mod} ~
        if (m_paramCount >= 2)
            key.modifiers |= vtModifierToControlKeyState(m_params[1]);

        emitKey(key);
        return totalConsumed;
    }

    // Unrecognised CSI — discard.
    return totalConsumed;
}

size_t InputBuffer::parseSs3(size_t pos)
{
    if (pos >= m_buf.size())
        return 0; // Need more data.

    const auto finalByte = static_cast<uint8_t>(m_buf[pos]);

    if (finalByte >= 'A' && finalByte <= 'Z')
    {
        WORD vk = 0;
        switch (finalByte)
        {
        case 'A': vk = VK_UP; break;
        case 'B': vk = VK_DOWN; break;
        case 'C': vk = VK_RIGHT; break;
        case 'D': vk = VK_LEFT; break;
        case 'H': vk = VK_HOME; break;
        case 'F': vk = VK_END; break;
        case 'P': vk = VK_F1; break;
        case 'Q': vk = VK_F2; break;
        case 'R': vk = VK_F3; break;
        case 'S': vk = VK_F4; break;
        default:
            return 1; // Unknown — discard.
        }

        ParsedKey key;
        key.vk = vk;
        key.scanCode = vkToScanCode(vk);
        key.modifiers = ENHANCED_KEY;
        emitKey(key);
        return 1;
    }

    // Unknown SS3 sequence — discard.
    return 1;
}

size_t InputBuffer::parseCsiParams(size_t pos)
{
    m_paramCount = 0;
    memset(m_params, 0, sizeof(m_params));

    int current = 0;
    bool hasDigit = false;

    while (pos < m_buf.size())
    {
        const auto ch = static_cast<uint8_t>(m_buf[pos]);

        if (ch >= '0' && ch <= '9')
        {
            current = current * 10 + (ch - '0');
            hasDigit = true;
            pos++;
            continue;
        }

        if (ch == ';')
        {
            if (m_paramCount < MaxParams)
                m_params[m_paramCount++] = current;
            current = 0;
            hasDigit = false;
            pos++;
            continue;
        }

        // Final byte or intermediate byte — stop parameter parsing.
        // Store the last accumulated parameter.
        if (hasDigit || m_paramCount > 0)
        {
            if (m_paramCount < MaxParams)
                m_params[m_paramCount++] = current;
        }

        return pos; // pos now points at the final byte.
    }

    // Ran off the end of the buffer — incomplete.
    return pos;
}

void InputBuffer::emitKey(const ParsedKey& key)
{
    if (key.isW32IM)
    {
        // W32IM: emit exactly one record as specified.
        INPUT_RECORD rec{};
        rec.EventType = KEY_EVENT;
        rec.Event.KeyEvent.bKeyDown = key.keyDown ? TRUE : FALSE;
        rec.Event.KeyEvent.wRepeatCount = key.repeatCount;
        rec.Event.KeyEvent.wVirtualKeyCode = key.vk;
        rec.Event.KeyEvent.wVirtualScanCode = key.scanCode;
        rec.Event.KeyEvent.uChar.UnicodeChar = key.ch;
        rec.Event.KeyEvent.dwControlKeyState = key.modifiers;
        m_records.push_back(rec);
        return;
    }

    // Standard path: emit a key-down and key-up pair.
    INPUT_RECORD down{};
    down.EventType = KEY_EVENT;
    down.Event.KeyEvent.bKeyDown = TRUE;
    down.Event.KeyEvent.wRepeatCount = key.repeatCount;
    down.Event.KeyEvent.wVirtualKeyCode = key.vk;
    down.Event.KeyEvent.wVirtualScanCode = key.scanCode;
    down.Event.KeyEvent.uChar.UnicodeChar = key.ch;
    down.Event.KeyEvent.dwControlKeyState = key.modifiers;
    m_records.push_back(down);

    INPUT_RECORD up = down;
    up.Event.KeyEvent.bKeyDown = FALSE;
    m_records.push_back(up);
}

WORD InputBuffer::vkToScanCode(WORD vk)
{
    return static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
}

DWORD InputBuffer::vtModifierToControlKeyState(int vtMod)
{
    // VT modifier parameter is 1-based: value = 1 + (flags).
    // Bit 0 = Shift, Bit 1 = Alt, Bit 2 = Ctrl.
    if (vtMod <= 1)
        return 0;

    const auto flags = vtMod - 1;
    DWORD state = 0;
    if (flags & 1) state |= SHIFT_PRESSED;
    if (flags & 2) state |= LEFT_ALT_PRESSED;
    if (flags & 4) state |= LEFT_CTRL_PRESSED;
    return state;
}
