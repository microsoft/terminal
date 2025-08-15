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
#include "MediaResourceSupport.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct FolderEntry : FolderEntryT<FolderEntry, NewTabMenuEntry, IPathlessMediaResourceContainer>
    {
    public:
        FolderEntry() noexcept;
        explicit FolderEntry(const winrt::hstring& name) noexcept;

        Model::NewTabMenuEntry Copy() const override;

        Json::Value ToJson() const override;
        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);

        // In JSON, we can set arbitrarily many profiles or nested profiles, that might not all
        // be rendered; for example, when a profile entry is invalid, or when a folder is empty.
        // Therefore, we will store the JSON entries list internally, and then expose only the
        // entries to be rendered to WinRT.
        winrt::Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry> Entries() const;

        void ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const Model::MediaResourceResolver& resolver) override;

        IMediaResource Icon() const noexcept
        {
            return _icon ? _icon : MediaResource::Empty();
        }

        void Icon(const IMediaResource& val)
        {
            _icon = val;
        }

        WINRT_PROPERTY(winrt::hstring, Name);
        WINRT_PROPERTY(FolderEntryInlining, Inlining, FolderEntryInlining::Never);
        WINRT_PROPERTY(bool, AllowEmpty, false);
        WINRT_PROPERTY(winrt::Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry>, RawEntries);

    private:
        IMediaResource _icon;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(FolderEntry);
}
