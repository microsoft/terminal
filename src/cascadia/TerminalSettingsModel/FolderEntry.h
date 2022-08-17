/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FolderEntry.h

Abstract:
- A folder entry in the "new tab" dropdown menu, 

Author(s):
- Floris Westerman - August 2022

--*/
#pragma once

#include "pch.h"
#include "NewTabMenuEntry.h"
#include "FolderEntry.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct FolderEntry : FolderEntryT<FolderEntry, NewTabMenuEntry>
    {
    public:
        FolderEntry() noexcept;
        explicit FolderEntry(const winrt::hstring& name) noexcept;

        Json::Value ToJson() const override;
        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);

        WINRT_PROPERTY(winrt::hstring, Name);
        WINRT_PROPERTY(winrt::hstring, Icon);
        WINRT_PROPERTY(winrt::Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry>, Entries);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(FolderEntry);
}
