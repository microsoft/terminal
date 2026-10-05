// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <unknwn.h>
#include <winrt/base.h>

#include "Api.h"

namespace Microsoft::Terminal::Settings::Model::Native
{
    class MediaResource : public winrt::implements<MediaResource, winrt::Windows::Foundation::IInspectable>
    {
        class ConstructionToken
        {
        public:
            ConstructionToken(const ConstructionToken&) = default;

        private:
            ConstructionToken() = default;
            friend class MediaResource;
        };

    public:
        TSM_NATIVE_API HRESULT __stdcall QueryInterface(REFIID iid, void** object) noexcept;
        TSM_NATIVE_API ULONG __stdcall AddRef() noexcept;
        TSM_NATIVE_API ULONG __stdcall Release() noexcept;
        TSM_NATIVE_API winrt::com_ptr<MediaResource> get_strong() noexcept;
        TSM_NATIVE_API winrt::weak_ref<MediaResource> get_weak();

        TSM_NATIVE_API static winrt::com_ptr<MediaResource> FromString(const winrt::hstring& path);
        TSM_NATIVE_API static winrt::com_ptr<MediaResource> Empty();
        TSM_NATIVE_API ~MediaResource() noexcept;

        MediaResource(ConstructionToken, winrt::hstring path, bool empty);

        TSM_NATIVE_API winrt::hstring Path() const;
        TSM_NATIVE_API winrt::hstring Resolved() const;
        TSM_NATIVE_API bool Ok() const noexcept;
        TSM_NATIVE_API void Resolve(const winrt::hstring& path);
        TSM_NATIVE_API void Reject();

    private:
        using base_type = winrt::implements<MediaResource, winrt::Windows::Foundation::IInspectable>;

        winrt::hstring _path;
        winrt::hstring _resolvedPath;
        bool _empty;
        bool _resolved = false;
        bool _ok = false;
    };
}
