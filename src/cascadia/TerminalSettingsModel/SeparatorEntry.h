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

#include "NewTabMenuEntry.h"
#include "SeparatorEntry.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct SeparatorEntry : SeparatorEntryT<SeparatorEntry, NewTabMenuEntry>
    {
    public:
        SeparatorEntry() noexcept;

        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(SeparatorEntry);
}
