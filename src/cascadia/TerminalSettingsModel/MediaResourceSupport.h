/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/

#pragma once

#include "MediaResourceHelper.g.h"

struct
    __declspec(uuid("6068ee1b-1ea0-4804-993a-42ef0c58d867"))
        IMediaResourceContainer : public IUnknown
{
    virtual void ResolveMediaResources(const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver) = 0;
};

struct
    __declspec(uuid("9f11361c-7c8f-45c9-8948-36b66d67eca8"))
        IPathlessMediaResourceContainer : public IUnknown
{
    virtual void ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver) = 0;
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct EmptyMediaResource : winrt::implements<EmptyMediaResource, winrt::Microsoft::Terminal::Settings::Model::IMediaResource, winrt::non_agile, winrt::no_weak_ref, winrt::no_module_lock>
    {
        winrt::hstring Path() { return {}; };
        winrt::hstring Resolved() { return {}; }

        bool Ok() const { return false; }

        void Resolve(const winrt::hstring&)
        {
            assert(false); // Somebody tried to resolve the empty media resource
        }

        void Reject()
        {
            assert(false); // Somebody tried to resolve the empty media resource
        }
    };

    struct MediaResource : winrt::implements<MediaResource, winrt::Microsoft::Terminal::Settings::Model::IMediaResource, winrt::non_agile, winrt::no_weak_ref, winrt::no_module_lock>
    {
        MediaResource() {}
        MediaResource(const winrt::hstring& p) :
            value{ p } {}

        winrt::hstring Path() { return value; };
        winrt::hstring Resolved() { return resolved ? resolvedValue : value; }

        bool Ok() const { return ok; }

        void Resolve(const winrt::hstring& newPath)
        {
            resolvedValue = newPath;
            ok = true;
            resolved = true;
        }

        void Reject()
        {
            resolvedValue = {};
            ok = false;
            resolved = true;
        }

        winrt::hstring value{};
        winrt::hstring resolvedValue{};
        bool ok{ false };
        bool resolved{ false };

        static IMediaResource Empty()
        {
            static IMediaResource emptyResource{ winrt::make<EmptyMediaResource>() };
            return emptyResource;
        }

        static IMediaResource FromString(const winrt::hstring& string)
        {
            return winrt::make<MediaResource>(string);
        }
    };

    _TIL_INLINEPREFIX void ResolveMediaResource(const winrt::hstring& basePath, const Model::IMediaResource& resource, const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver)
    {
        resolver(basePath, resource);
    }

    _TIL_INLINEPREFIX void ResolveIconMediaResource(const winrt::hstring& basePath, const Model::IMediaResource& resource, const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver)
    {
        if (const winrt::hstring path{ resource.Path() }; !path.empty())
        {
            std::wstring_view pathView{ path };
            if (pathView.size() <= 2 || pathView.find_first_of(L'\u200D') <= 8)
            {
                // **HEURISTIC**
                // If it's 2 code units long or contains a zero-width joiner in the first 8 code units, it is
                // PROBABLY an Emoji. Just pass it through.
                resource.Resolve(path);
                return;
            }

            ResolveMediaResource(basePath, resource, resolver);
        }
    }

    // This exists to allow external consumers to call this code via WinRT
    struct MediaResourceHelper
    {
        static winrt::Microsoft::Terminal::Settings::Model::IMediaResource FromString(hstring const& s)
        {
            return MediaResource::FromString(s);
        }
        static winrt::Microsoft::Terminal::Settings::Model::IMediaResource Empty()
        {
            return MediaResource::Empty();
        }
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    struct MediaResourceHelper : MediaResourceHelperT<MediaResourceHelper, implementation::MediaResourceHelper>
    {
    };
}
