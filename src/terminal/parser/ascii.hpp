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
        FS = 0x1C, // File Separator
        GS = 0x1D, // Group Separator
        RS = 0x1E, // Record Separator
        US = 0x1F, // Unit Separator
        SPC = 0x20, // Space, first printable character
        DEL = 0x7F, // Delete

        // Uppercase letters
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,

        // Lowercase letters
        a = 97,
        b = 98,
        c = 99,
        d = 100,
        e = 101,
        f = 102,
        g = 103,
        h = 104,
        i = 105,
        j = 106,
        k = 107,
        l = 108,
        m = 109,
        n = 110,
        o = 111,
        p = 112,
        q = 113,
        r = 114,
        s = 115,
        t = 116,
        u = 117,
        v = 118,
        w = 119,
        x = 120,
        y = 121,
        z = 122
    };
}
