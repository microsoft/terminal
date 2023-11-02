// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "screenInfo.hpp"

// Word delimiters
bool IsWordDelim(wchar_t wch) noexcept;
bool IsWordDelim(const std::wstring_view& charData) noexcept;
int DelimiterClass(wchar_t wch) noexcept;
