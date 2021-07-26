// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

enum class VtIoMode
{
    INVALID,
    XTERM,
    XTERM_256,
    XTERM_ASCII,
    XTERM_256_PT
};

const wchar_t* const XTERM_STRING = L"xterm";
const wchar_t* const XTERM_256_STRING_PT = L"xterm-256color-pt";
const wchar_t* const XTERM_256_STRING = L"xterm-256color";
const wchar_t* const XTERM_ASCII_STRING = L"xterm-ascii";
const wchar_t* const DEFAULT_STRING = L"";
