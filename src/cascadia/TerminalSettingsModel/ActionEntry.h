/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ActionEntry.h

Abstract:
- An action entry in the "new tab" dropdown menu

Author(s):
- Pankaj Bhojwani - May 2024

--*/
#pragma once

#include "NewTabMenuEntry.h"
#include "ActionEntry.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ActionEntry : ActionEntryT<ActionEntry, NewTabMenuEntry>
    {
    public:
        ActionEntry() noexcept;

        Model::NewTabMenuEntry Copy() const;

        Json::Value ToJson() const override;
        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);

        WINRT_PROPERTY(winrt::hstring, ActionId);
        WINRT_PROPERTY(winrt::hstring, Icon);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ActionEntry);
}
