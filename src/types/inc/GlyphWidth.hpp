/*++
Copyright (c) Microsoft Corporation

Module Name:
- GlyphWidth.hpp

Abstract:
- Helpers for determining the width of a particular string of chars.

*/
#pragma once

bool IsGlyphFullWidth(const std::wstring_view& glyph) noexcept;
bool IsGlyphFullWidth(const wchar_t wch) noexcept;
