// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    enum AsciiChars : wchar_t
    {
        NUL = 0x0, // Null
        SOH = 0x1, // Start of Heading
        STX = 0x2, // Start of Text
        ETX = 0x3, // End of Text
        EOT = 0x4, // End of Transmission
        ENQ = 0x5, // Enquiry
        ACK = 0x6, // Acknowledge
        BEL = 0x7, // Bell
        BS = 0x8, // Backspace
        TAB = 0x9, // Horizontal Tab
        LF = 0xA, // Line Feed (new line)
        VT = 0xB, // Vertical Tab
        FF = 0xC, // Form Feed (new page)
        CR = 0xD, // Carriage Return
        SO = 0xE, // Shift Out
        SI = 0xF, // Shift In
        DLE = 0x10, // Data Link Escape
        DC1 = 0x11, // Device Control 1
        DC2 = 0x12, // Device Control 2
        DC3 = 0x13, // Device Control 3
        DC4 = 0x14, // Device Control 4
        NAK = 0x15, // Negative Acknowledge
        SYN = 0x16, // Synchronous Idle
        ETB = 0x17, // End of Transmission Block
        CAN = 0x18, // Cancel
        EM = 0x19, // End of Medium
        SUB = 0x1A, // Substitute
        ESC = 0x1B, // Escape
        FS = 0x1C, // File Seperator
        GS = 0x1D, // Group Seperator
        RS = 0x1E, // Record Seperator
        US = 0x1F, // Unit Seperator
        SPC = 0x20, // Space, first printable character
        DEL = 0x7F, // Delete
    };
}
