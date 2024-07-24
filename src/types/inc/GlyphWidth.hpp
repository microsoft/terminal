/*++
Copyright (c) Microsoft Corporation

Module Name:
- GlyphWidth.hpp

Abstract:
- Helpers for determining the width of a particular string of chars.

*/
#pragma once

#include <functional>
#include <string_view>

#include "convert.hpp"

bool IsGlyphFullWidth(const std::wstring_view& glyph) noexcept;
bool IsGlyphFullWidth(const wchar_t wch) noexcept;
void SetGlyphWidthFallback(std::function<bool(const std::wstring_view&)> pfnFallback) noexcept;
void NotifyGlyphWidthFontChanged() noexcept;
