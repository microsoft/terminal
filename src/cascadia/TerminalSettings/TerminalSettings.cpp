// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalSettings.h"

#include "TerminalSettings.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::implementation
{
    uint32_t TerminalSettings::GetColorTableEntry(int32_t index) const noexcept
    {
        return _colorTable.at(index);
    }

    void TerminalSettings::SetColorTableEntry(int32_t index, uint32_t value)
    {
        auto const colorTableCount = gsl::narrow_cast<decltype(index)>(_colorTable.size());
        THROW_HR_IF(E_INVALIDARG, index >= colorTableCount);
        _colorTable.at(index) = value;
    }
}
