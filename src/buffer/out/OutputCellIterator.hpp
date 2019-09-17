/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- OutputCellIterator.hpp

Abstract:
- Read-only view into an entire batch of data to be written into the output buffer.
- This is done for performance reasons (avoid heap allocs and copies).

Author:
- Michael Niksa (MiNiksa) 06-Oct-2018

Revision History:
- Based on work from OutputCell.hpp/cpp by Austin Diviness (AustDi)

--*/

#pragma once

#include "TextAttribute.hpp"

#include "OutputCell.hpp"
#include "OutputCellView.hpp"

class OutputCellIterator final
{
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = OutputCellView;
    using difference_type = ptrdiff_t;
    using pointer = OutputCellView*;
    using reference = OutputCellView&;

    OutputCellIterator(const wchar_t& wch, const size_t fillLimit = 0) noexcept;
    OutputCellIterator(const TextAttribute& attr, const size_t fillLimit = 0) noexcept;
    OutputCellIterator(const wchar_t& wch, const TextAttribute& attr, const size_t fillLimit = 0) noexcept;
    OutputCellIterator(const CHAR_INFO& charInfo, const size_t fillLimit = 0) noexcept;
    OutputCellIterator(const std::wstring_view utf16Text);
    OutputCellIterator(const std::wstring_view utf16Text, const TextAttribute attribute);
    OutputCellIterator(const std::basic_string_view<WORD> legacyAttributes, const bool unused) noexcept;
    OutputCellIterator(const std::basic_string_view<CHAR_INFO> charInfos) noexcept;
    OutputCellIterator(const std::basic_string_view<OutputCell> cells);
    ~OutputCellIterator() = default;

    OutputCellIterator& operator=(const OutputCellIterator& it) = default;

    operator bool() const noexcept;

    ptrdiff_t GetCellDistance(OutputCellIterator other) const noexcept;
    ptrdiff_t GetInputDistance(OutputCellIterator other) const noexcept;
    friend ptrdiff_t operator-(OutputCellIterator one, OutputCellIterator two) = delete;

    OutputCellIterator& operator++();
    OutputCellIterator operator++(int);

    const OutputCellView& operator*() const noexcept;
    const OutputCellView* operator->() const noexcept;

private:
    enum class Mode
    {
        // Loose mode is where we're given text and attributes in a raw sort of form
        // like while data is being inserted from an API call.
        Loose,

        // Loose mode with only text is where we're given just text and we want
        // to use the attribute already in the buffer when writing
        LooseTextOnly,

        // Fill mode is where we were given one thing and we just need to keep giving
        // that back over and over for eternity.
        Fill,

        // Given a run of legacy attributes, convert each of them and insert only attribute data.
        LegacyAttr,

        // CharInfo mode is where we've been given a pair of text and attribute for each
        // cell in the legacy format from an API call.
        CharInfo,

        // Cell mode is where we have an already fully structured cell data usually
        // from accessing/copying data already put into the OutputBuffer.
        Cell,
    };
    Mode _mode;

    std::basic_string_view<WORD> _legacyAttrs;

    std::variant<
        std::wstring_view,
        std::basic_string_view<CHAR_INFO>,
        std::basic_string_view<OutputCell>,
        std::monostate>
        _run;

    TextAttribute _attr;

    bool _TryMoveTrailing() noexcept;

    static OutputCellView s_GenerateView(const std::wstring_view view);

    static OutputCellView s_GenerateView(const std::wstring_view view,
                                         const TextAttribute attr);

    static OutputCellView s_GenerateView(const std::wstring_view view,
                                         const TextAttribute attr,
                                         const TextAttributeBehavior behavior);

    static OutputCellView s_GenerateView(const wchar_t& wch) noexcept;
    static OutputCellView s_GenerateViewLegacyAttr(const WORD& legacyAttr) noexcept;
    static OutputCellView s_GenerateView(const TextAttribute& attr) noexcept;
    static OutputCellView s_GenerateView(const wchar_t& wch, const TextAttribute& attr) noexcept;
    static OutputCellView s_GenerateView(const CHAR_INFO& charInfo) noexcept;

    static OutputCellView s_GenerateView(const OutputCell& cell);

    OutputCellView _currentView;

    size_t _pos;
    size_t _distance;
    size_t _fillLimit;
};
