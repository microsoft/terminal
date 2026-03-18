#include "pch.h"
#include "InputBuffer.h"

void InputBuffer::write(std::string_view text)
{
    m_buf.append(text);
}

bool InputBuffer::hasData() const noexcept
{
    return m_readPos < m_buf.size() || m_recordReadPos < m_records.size();
}

size_t InputBuffer::pendingEventCount() const noexcept
{
    const auto queued = (m_records.size() > m_recordReadPos) ? (m_records.size() - m_recordReadPos) : 0;
    const auto rawBytes = (m_buf.size() > m_readPos) ? (m_buf.size() - m_readPos) : 0;
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
        parseTokensToRecords();

    const auto available = m_records.size() - m_recordReadPos;
    const auto toCopy = std::min(available, maxRecords);
    if (toCopy > 0)
    {
        memcpy(dst, m_records.data() + m_recordReadPos, toCopy * sizeof(INPUT_RECORD));
        if (!peek)
        {
            m_recordReadPos += toCopy;
            if (m_recordReadPos == m_records.size())
            {
                m_records.clear();
                m_recordReadPos = 0;
            }
        }
    }

    if (!peek)
        compact();

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
    if (m_readPos > 4096 || (m_readPos > 0 && m_readPos == m_buf.size()))
    {
        m_buf.erase(0, m_readPos);
        m_readPos = 0;
    }
}

// ============================================================================
// Token → INPUT_RECORD conversion
// ============================================================================

void InputBuffer::parseTokensToRecords()
{
    const auto input = std::string_view{ m_buf.data() + m_readPos, m_buf.size() - m_readPos };
    auto stream = m_parser.parse(input);

    VtToken token;
    while (stream.next(token))
    {
        switch (token.type)
        {
        case VtToken::Text:
            handleText(token.payload);
            break;
        case VtToken::Ctrl:
            handleCtrl(token.ch);
            break;
        case VtToken::Esc:
            handleEsc(token.ch);
            break;
        case VtToken::SS3:
            handleSs3(token.ch);
            break;
        case VtToken::Csi:
            handleCsi(*token.csi);
            break;
        default:
            // Osc/Dcs — irrelevant for input records, skip.
            break;
        }
    }

    // Advance m_readPos by what the parser consumed.
    m_readPos += stream.offset();
}

void InputBuffer::handleText(std::string_view text)
{
    // Decode UTF-8 codepoints and emit key events.
    const auto* bytes = reinterpret_cast<const uint8_t*>(text.data());
    size_t i = 0;

    while (i < text.size())
    {
        uint32_t cp;
        size_t seqLen;
        const auto b = bytes[i];

        if (b < 0x80) { cp = b; seqLen = 1; }
        else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; seqLen = 2; }
        else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; seqLen = 3; }
        else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; seqLen = 4; }
        else { cp = 0xFFFD; seqLen = 1; i++; continue; }

        if (i + seqLen > text.size()) break;

        for (size_t j = 1; j < seqLen; j++)
        {
            const auto cont = bytes[i + j];
            if ((cont & 0xC0) != 0x80) { cp = 0xFFFD; break; }
            cp = (cp << 6) | (cont & 0x3F);
        }
        i += seqLen;

        // Emit one or two INPUT_RECORDs (surrogate pair for supplementary plane).
        if (cp <= 0xFFFF)
        {
            ParsedKey key;
            key.ch = static_cast<wchar_t>(cp);
            key.vk = LOBYTE(VkKeyScanW(key.ch));
            if (key.vk == 0xFF) key.vk = 0;
            key.scanCode = vkToScanCode(key.vk);
            emitKey(key);
        }
        else if (cp <= 0x10FFFF)
        {
            ParsedKey key;
            key.ch = static_cast<wchar_t>(0xD800 + ((cp - 0x10000) >> 10));
            emitKey(key);
            key.ch = static_cast<wchar_t>(0xDC00 + ((cp - 0x10000) & 0x3FF));
            emitKey(key);
        }
    }
}

void InputBuffer::handleCtrl(char ch)
{
    ParsedKey key;

    switch (static_cast<uint8_t>(ch))
    {
    case '\r':
        key.vk = VK_RETURN;
        key.scanCode = vkToScanCode(VK_RETURN);
        key.ch = L'\r';
        break;
    case '\n':
        key.vk = VK_RETURN;
        key.scanCode = vkToScanCode(VK_RETURN);
        key.ch = L'\n';
        key.modifiers = LEFT_CTRL_PRESSED;
        break;
    case '\t':
        key.vk = VK_TAB;
        key.scanCode = vkToScanCode(VK_TAB);
        key.ch = L'\t';
        break;
    case '\b':
        key.vk = VK_BACK;
        key.scanCode = vkToScanCode(VK_BACK);
        key.ch = L'\b';
        break;
    case 0x7F:
        key.vk = VK_BACK;
        key.scanCode = vkToScanCode(VK_BACK);
        key.ch = L'\b';
        break;
    case 0x00:
        key.vk = VK_SPACE;
        key.scanCode = vkToScanCode(VK_SPACE);
        key.ch = L'\0';
        key.modifiers = LEFT_CTRL_PRESSED;
        break;
    default:
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
}

void InputBuffer::handleEsc(char ch)
{
    ParsedKey key;

    if (ch == '\0')
    {
        // Bare ESC (timeout or end of buffer).
        key.vk = VK_ESCAPE;
        key.scanCode = vkToScanCode(VK_ESCAPE);
        key.ch = 0x1B;
    }
    else if (ch >= 0x20 && ch <= 0x7E)
    {
        // Alt+character.
        key.ch = static_cast<wchar_t>(ch);
        key.vk = LOBYTE(VkKeyScanW(key.ch));
        key.scanCode = vkToScanCode(key.vk);
        key.modifiers = LEFT_ALT_PRESSED;
    }
    else
    {
        // Unexpected — emit bare ESC.
        key.vk = VK_ESCAPE;
        key.scanCode = vkToScanCode(VK_ESCAPE);
        key.ch = 0x1B;
    }

    emitKey(key);
}

void InputBuffer::handleSs3(char ch)
{
    static constexpr WORD keypadLut[] = {
        VK_UP,   // A
        VK_DOWN, // B
        VK_RIGHT,// C
        VK_LEFT, // D
        0,       // E
        VK_END,  // F
        0,       // G
        VK_HOME, // H
    };

    ParsedKey key;
    key.modifiers = ENHANCED_KEY;

    if (ch >= 'A' && ch <= 'H')
    {
        key.vk = keypadLut[ch - 'A'];
        if (key.vk == 0) return;
        key.scanCode = vkToScanCode(key.vk);
    }
    else if (ch >= 'P' && ch <= 'S')
    {
        key.vk = static_cast<WORD>(VK_F1 + (ch - 'P'));
        key.scanCode = vkToScanCode(key.vk);
    }
    else
    {
        return; // Unknown SS3.
    }

    emitKey(key);
}

void InputBuffer::handleCsi(const VtCsi& csi)
{
    // Win32 Input Mode: CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
    if (csi.finalByte == '_' && csi.paramCount >= 4)
    {
        ParsedKey key;
        key.vk = static_cast<WORD>(csi.params[0]);
        key.scanCode = static_cast<WORD>(csi.params[1]);
        key.ch = static_cast<wchar_t>(csi.params[2]);
        key.keyDown = csi.params[3] != 0;
        key.modifiers = (csi.paramCount >= 5) ? static_cast<DWORD>(csi.params[4]) : 0;
        key.repeatCount = (csi.paramCount >= 6) ? static_cast<WORD>(csi.params[5]) : 1;
        if (key.repeatCount == 0) key.repeatCount = 1;
        key.isW32IM = true;
        emitKey(key);
        return;
    }

    // Cursor keys: CSI [1;mod] A-H
    static constexpr WORD keypadLut[] = {
        VK_UP, VK_DOWN, VK_RIGHT, VK_LEFT, 0, VK_END, 0, VK_HOME,
    };

    if (csi.finalByte >= 'A' && csi.finalByte <= 'H')
    {
        const auto vk = keypadLut[csi.finalByte - 'A'];
        if (vk == 0) return;

        ParsedKey key;
        key.vk = vk;
        key.scanCode = vkToScanCode(vk);
        key.modifiers = ENHANCED_KEY;
        if (csi.paramCount >= 2)
            key.modifiers |= vtModifierToControlKeyState(csi.params[1]);
        emitKey(key);
        return;
    }

    // Shift+Tab: CSI Z
    if (csi.finalByte == 'Z')
    {
        ParsedKey key;
        key.vk = VK_TAB;
        key.scanCode = vkToScanCode(VK_TAB);
        key.ch = L'\t';
        key.modifiers = SHIFT_PRESSED;
        emitKey(key);
        return;
    }

    // Generic keys: CSI {num} [;mod] ~
    if (csi.finalByte == '~' && csi.paramCount >= 1)
    {
        static constexpr struct { uint16_t param; WORD vk; } genericLut[] = {
            { 1,  VK_HOME },   { 2,  VK_INSERT }, { 3,  VK_DELETE },
            { 4,  VK_END },    { 5,  VK_PRIOR },  { 6,  VK_NEXT },
            { 15, VK_F5 },     { 17, VK_F6 },     { 18, VK_F7 },
            { 19, VK_F8 },     { 20, VK_F9 },     { 21, VK_F10 },
            { 23, VK_F11 },    { 24, VK_F12 },
        };

        for (const auto& entry : genericLut)
        {
            if (csi.params[0] == entry.param)
            {
                ParsedKey key;
                key.vk = entry.vk;
                key.scanCode = vkToScanCode(entry.vk);
                key.modifiers = ENHANCED_KEY;
                if (csi.paramCount >= 2)
                    key.modifiers |= vtModifierToControlKeyState(csi.params[1]);
                emitKey(key);
                return;
            }
        }
    }

    // Unrecognised CSI — discard.
}

void InputBuffer::emitKey(const ParsedKey& key)
{
    if (key.isW32IM)
    {
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

DWORD InputBuffer::vtModifierToControlKeyState(uint16_t vtMod)
{
    if (vtMod <= 1) return 0;
    const auto flags = vtMod - 1;
    DWORD state = 0;
    if (flags & 1) state |= SHIFT_PRESSED;
    if (flags & 2) state |= LEFT_ALT_PRESSED;
    if (flags & 4) state |= LEFT_CTRL_PRESSED;
    return state;
}
