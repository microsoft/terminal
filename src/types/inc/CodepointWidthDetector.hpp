// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

struct CodepointWidthDetector
{
    static CodepointWidthDetector& Singleton() noexcept;

    size_t GraphemeNext(const std::wstring_view& str, size_t offset, int* width) noexcept;
    size_t GraphemePrev(const std::wstring_view& str, size_t offset, int* width) noexcept;

    void SetFallbackMethod(std::function<bool(const std::wstring_view&)> pfnFallback) noexcept;
    void ClearFallbackCache() noexcept;

private:
    __declspec(noinline) int _checkFallbackViaCache(char32_t codepoint) noexcept;

    size_t _graphemeNextOld(const std::wstring_view& str, size_t offset, int* width) noexcept;
    size_t _graphemePrevOld(const std::wstring_view& str, size_t offset, int* width) noexcept;
    int _getWidthOld(char32_t cp) noexcept;

    std::unordered_map<char32_t, uint8_t> _fallbackCache;
    std::function<bool(const std::wstring_view&)> _pfnFallbackMethod;
};
