// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

enum class VtIoMode
{
    INVALID,
    XTERM,
    XTERM_256,
    WIN_TELNET,
    XTERM_ASCII
};

const wchar_t* const XTERM_STRING = L"xterm";
const wchar_t* const XTERM_256_STRING = L"xterm-256color";
const wchar_t* const WIN_TELNET_STRING = L"win-telnet";
const wchar_t* const XTERM_ASCII_STRING = L"xterm-ascii";
const wchar_t* const DEFAULT_STRING = L"";
