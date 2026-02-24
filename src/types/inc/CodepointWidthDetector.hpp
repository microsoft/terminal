// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

enum class TextMeasurementMode
{
    // Uses a method very similar to the official UAX #29 Extended Grapheme Cluster algorithm.
    Graphemes,
    // wcswidth() is a popular method on UNIX to measure text width. It basically treats
    // any zero-width character as a continuation of a preceding non-zero-width character.
    Wcswidth,
    // The old conhost algorithm is UCS-2 based and assigns a minimum width of 1 to all codepoints.
    // It had been extended to support UTF-16 after the introduction of Windows Terminal,
    // but retained the behavior of not supporting zero-width characters.
    Console,
};

// NOTE: The same GraphemeState instance should be passed for a series of GraphemeNext *or* GraphemePrev calls,
// but NOT between GraphemeNext *and* GraphemePrev ("exclusive OR"). They're also not reusable when the
// CodepointWidthDetector::_legacy flag changes. Different functions treat these members differently.
struct GraphemeState
{
    // These are the [out] parameters for GraphemeNext/Prev.
    //
    // If a previous call returned false (= reached the end of the string), then on the first call with
    // the next string, beg/len will contain the parts of the grapheme cluster that are found in that
    // new string argument. That's true even if the two strings don't join to form a single cluster.
    // In that case beg/len will simply be an empty string. It basically tells you
    // "Yup, that cluster in the last string was complete after all".
    //
    // However, width will always be updated to represent the width of the current cluster.
    //
    // For instance, if the first string is a narrow emoji and the second one is U+FE0F, the first call will return
    // the emoji with a width of 1, and the second call will return U+FE0F with a width of 2.
    // You know these two belong together because the first call returned false.
    // The total width is not 1+2 but rather just 2.
    const wchar_t* beg = nullptr;
    int len = 0;
    // width will always be between 0 or 2.
    int width = 0;

    // If GraphemeNext/Prev return false (= reached the end of the string), they'll fill these struct
    // members with some info so that we can check if it joins with the start of the next string argument.
    // _state is stored ~flipped, so that we can differentiate between it being unset (0) and it being set to 0 (~0 = 255).
    int _state = 0;
    int _last = 0;
};

struct CodepointWidthDetector
{
    static CodepointWidthDetector& Singleton() noexcept;

    // Returns false if the end of the string has been reached.
    bool GraphemeNext(GraphemeState& s, const std::wstring_view& str) noexcept;
    bool GraphemePrev(GraphemeState& s, const std::wstring_view& str) noexcept;

    TextMeasurementMode GetMode() const noexcept;
    void SetFallbackMethod(std::function<bool(const std::wstring_view&)> pfnFallback) noexcept;
    void Reset(TextMeasurementMode mode) noexcept;

private:
    bool _graphemeNext(GraphemeState& s, const std::wstring_view& str) const noexcept;
    bool _graphemePrev(GraphemeState& s, const std::wstring_view& str) const noexcept;
    bool _graphemeNextWcswidth(GraphemeState& s, const std::wstring_view& str) const noexcept;
    bool _graphemePrevWcswidth(GraphemeState& s, const std::wstring_view& str) const noexcept;
    bool _graphemeNextConsole(GraphemeState& s, const std::wstring_view& str) noexcept;
    bool _graphemePrevConsole(GraphemeState& s, const std::wstring_view& str) noexcept;
    __declspec(noinline) int _checkFallbackViaCache(char32_t codepoint) noexcept;

    std::unordered_map<char32_t, int> _fallbackCache;
    std::function<bool(const std::wstring_view&)> _pfnFallbackMethod;
    TextMeasurementMode _mode = TextMeasurementMode::Graphemes;
    int _ambiguousWidth = 1;
};
