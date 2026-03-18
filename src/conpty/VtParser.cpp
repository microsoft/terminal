// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "VtParser.h"

bool VtParser::hasEscTimeout() const noexcept
{
    return m_state == State::Esc;
}

VtParser::Stream VtParser::parse(std::string_view input) noexcept
{
    return Stream{ this, input };
}

// Decode one UTF-8 codepoint from the input at the current offset.
// Advances m_off past the consumed bytes. Returns U+0000 at end-of-input.
char32_t VtParser::Stream::nextChar()
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(m_input.data());
    const auto len = m_input.size();

    if (m_off >= len)
        return U'\0';

    const auto b0 = bytes[m_off];

    // ASCII fast path.
    if (b0 < 0x80)
    {
        m_off++;
        return static_cast<char32_t>(b0);
    }

    // Determine sequence length and initial bits.
    size_t seqLen;
    char32_t cp;
    if ((b0 & 0xE0) == 0xC0) { seqLen = 2; cp = b0 & 0x1F; }
    else if ((b0 & 0xF0) == 0xE0) { seqLen = 3; cp = b0 & 0x0F; }
    else if ((b0 & 0xF8) == 0xF0) { seqLen = 4; cp = b0 & 0x07; }
    else { m_off++; return U'\xFFFD'; } // Invalid lead byte.

    if (m_off + seqLen > len)
    {
        // Incomplete codepoint at end of input — consume what we have.
        m_off = len;
        return U'\xFFFD';
    }

    for (size_t i = 1; i < seqLen; i++)
    {
        const auto cont = bytes[m_off + i];
        if ((cont & 0xC0) != 0x80)
        {
            m_off += i;
            return U'\xFFFD';
        }
        cp = (cp << 6) | (cont & 0x3F);
    }

    m_off += seqLen;
    return cp;
}

bool VtParser::Stream::next(VtToken& out)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(m_input.data());
    const auto len = m_input.size();

    // If the previous input ended with an escape character, and we're called
    // with empty input (timeout fired), return the bare ESC.
    if (len == 0 && m_parser->m_state == State::Esc)
    {
        m_parser->m_state = State::Ground;
        out.type = VtToken::Esc;
        out.ch = '\0';
        return true;
    }

    while (m_off < len)
    {
        switch (m_parser->m_state)
        {
        case State::Ground:
        {
            const auto b = bytes[m_off];

            if (b == 0x1B)
            {
                m_parser->m_state = State::Esc;
                m_off++;
                break; // Continue the outer loop to process the Esc state.
            }

            if (b < 0x20 || b == 0x7F)
            {
                m_off++;
                out.type = VtToken::Ctrl;
                out.ch = static_cast<char>(b);
                return true;
            }

            // Bulk scan printable text (>= 0x20, != 0x7F, != 0x1B).
            const auto beg = m_off;
            do {
                m_off++;
            } while (m_off < len && bytes[m_off] >= 0x20 && bytes[m_off] != 0x7F && bytes[m_off] != 0x1B);

            out.type = VtToken::Text;
            out.payload = m_input.substr(beg, m_off - beg);
            return true;
        }

        case State::Esc:
        {
            const auto ch = nextChar();

            switch (ch)
            {
            case '[':
                m_parser->m_state = State::Csi;
                m_parser->m_csi.privateByte = '\0';
                m_parser->m_csi.finalByte = '\0';
                // Clear only params that were used last time.
                while (m_parser->m_csi.paramCount > 0)
                {
                    m_parser->m_csi.paramCount--;
                    m_parser->m_csi.params[m_parser->m_csi.paramCount] = 0;
                }
                break;
            case ']':
                m_parser->m_state = State::Osc;
                break;
            case 'O':
                m_parser->m_state = State::Ss3;
                break;
            case 'P':
                m_parser->m_state = State::Dcs;
                break;
            default:
                m_parser->m_state = State::Ground;
                out.type = VtToken::Esc;
                // Truncate to char. For the sequences we care about this is always ASCII.
                out.ch = static_cast<char>(ch);
                return true;
            }
            break;
        }

        case State::Ss3:
        {
            m_parser->m_state = State::Ground;
            const auto ch = nextChar();
            out.type = VtToken::SS3;
            out.ch = static_cast<char>(ch);
            return true;
        }

        case State::Csi:
        {
            for (;;)
            {
                // Parse parameter digits.
                if (m_parser->m_csi.paramCount < std::size(m_parser->m_csi.params))
                {
                    auto& dst = m_parser->m_csi.params[m_parser->m_csi.paramCount];
                    while (m_off < len && bytes[m_off] >= '0' && bytes[m_off] <= '9')
                    {
                        const uint32_t add = bytes[m_off] - '0';
                        const uint32_t value = static_cast<uint32_t>(dst) * 10 + add;
                        dst = static_cast<uint16_t>(std::min(value, static_cast<uint32_t>(UINT16_MAX)));
                        m_off++;
                    }
                }
                else
                {
                    // Overflow: skip digits.
                    while (m_off < len && bytes[m_off] >= '0' && bytes[m_off] <= '9')
                        m_off++;
                }

                // Need more data?
                if (m_off >= len)
                    return false;

                const auto c = bytes[m_off];
                m_off++;

                if (c >= 0x40 && c <= 0x7E)
                {
                    // Final byte.
                    m_parser->m_state = State::Ground;
                    m_parser->m_csi.finalByte = static_cast<char>(c);
                    if (m_parser->m_csi.paramCount != 0 || m_parser->m_csi.params[0] != 0)
                        m_parser->m_csi.paramCount++;
                    out.type = VtToken::Csi;
                    out.csi = &m_parser->m_csi;
                    return true;
                }

                if (c == ';')
                {
                    m_parser->m_csi.paramCount++;
                }
                else if (c >= '<' && c <= '?')
                {
                    m_parser->m_csi.privateByte = static_cast<char>(c);
                }
                // else: intermediate bytes (0x20-0x2F) or unknown — silently skip.
            }
        }

        case State::Osc:
        case State::Dcs:
        {
            const auto beg = m_off;
            std::string_view data;
            bool partial;

            for (;;)
            {
                // Scan for BEL (0x07) or ESC (0x1B) — potential terminators.
                while (m_off < len && bytes[m_off] != 0x07 && bytes[m_off] != 0x1B)
                    m_off++;

                data = m_input.substr(beg, m_off - beg);
                partial = m_off >= len;

                if (partial)
                    break;

                const auto c = bytes[m_off];
                m_off++;

                if (c == 0x1B)
                {
                    // ESC might start ST (ESC \). Check next byte.
                    if (m_off >= len)
                    {
                        // At end of input — save state for next chunk.
                        m_parser->m_state = (m_parser->m_state == State::Osc) ? State::OscEsc : State::DcsEsc;
                        partial = true;
                        break;
                    }

                    if (bytes[m_off] != '\\')
                        continue; // False alarm, not ST.

                    m_off++; // Consume the backslash.
                }

                // BEL or ESC \ — sequence is complete.
                break;
            }

            const auto wasOsc = (m_parser->m_state == State::Osc);
            if (!partial)
                m_parser->m_state = State::Ground;

            out.type = wasOsc ? VtToken::Osc : VtToken::Dcs;
            out.payload = data;
            out.partial = partial;
            return true;
        }

        case State::OscEsc:
        case State::DcsEsc:
        {
            // Previous chunk ended with ESC inside an OSC/DCS.
            // Check if this chunk starts with '\' to complete the ST.
            if (bytes[m_off] == '\\')
            {
                const auto wasOsc = (m_parser->m_state == State::OscEsc);
                m_parser->m_state = State::Ground;
                m_off++;

                out.type = wasOsc ? VtToken::Osc : VtToken::Dcs;
                out.payload = {};
                out.partial = false;
                return true;
            }
            else
            {
                // False alarm — the ESC was not a string terminator.
                // Return it as partial payload and resume the string state.
                const auto wasOsc = (m_parser->m_state == State::OscEsc);
                m_parser->m_state = wasOsc ? State::Osc : State::Dcs;

                out.type = wasOsc ? VtToken::Osc : VtToken::Dcs;
                out.payload = "\x1b";
                out.partial = true;
                return true;
            }
        }
        } // switch
    } // while

    return false;
}
