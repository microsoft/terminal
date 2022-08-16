/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SeparatorEntry.h

Abstract:
- A separator entry in the "new tab" dropdown menu

Author(s):
- Floris Westerman - August 2022

--*/
#pragma once

#include "pch.h"
#include "SeparatorEntry.g.h"
#include "JsonUtils.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct SeparatorEntry : SeparatorEntryT<SeparatorEntry>
    {
    public:
        explicit SeparatorEntry() noexcept {

        }
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(SeparatorEntry);
}
