/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/

#pragma once

#include "MediaResourceHelper.g.h"
#include "../types/inc/utils.hpp"

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
    struct EmptyMediaResource : winrt::implements<EmptyMediaResource, winrt::Microsoft::Terminal::Settings::Model::IMediaResource, winrt::no_weak_ref, winrt::no_module_lock>
    {
        // Micro-optimization: having one empty resource that contains no actual paths saves us a few bytes per object
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

    /* MEDIA RESOURCES
     *
     * A media resource is a container for two strings: one pre-validation path and one post-validation path.
     * It is expected that, before they are used, they are passed through a resolver.
     *
     * A resolver may Resolve() a media resource to a path or Reject() it.
     * - If it is Resolved, the new path is accessible via the Resolved() method.
     * - If it is Rejected, the Resolved() method will return the empty string.
     *
     * A media resource is considered `Ok` if it has been Resolved to a real path.
     *
     * As a special case, if it has been neither resolved nor rejected, it will return the pre-validation
     * path--this is intended to aid its use in places where the risk of using an unresolved media path
     * is fine.
     */
    struct MediaResource : winrt::implements<MediaResource, winrt::Microsoft::Terminal::Settings::Model::IMediaResource, winrt::no_weak_ref, winrt::no_module_lock>
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
        bool ok{ false }; // Path() was transformed into a final and valid Resolved path
        bool resolved{ false }; // This resource has been visited by a resolver, regardless of the outcome.

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

    _TIL_INLINEPREFIX void ResolveMediaResource(const winrt::Microsoft::Terminal::Settings::Model::OriginTag origin, const winrt::hstring& basePath, const Model::IMediaResource& resource, const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver)
    {
        const auto path{ resource.Path() };
        if (path.empty() || resource.Ok())
        {
            // Don't resolve empty resources *or* resources which have already been found.
            return;
        }
        resolver(origin, basePath, resource);
    }

    _TIL_INLINEPREFIX void ResolveIconMediaResource(const winrt::Microsoft::Terminal::Settings::Model::OriginTag origin, const winrt::hstring& basePath, const Model::IMediaResource& resource, const winrt::Microsoft::Terminal::Settings::Model::MediaResourceResolver& resolver)
    {
        if (const winrt::hstring path{ resource.Path() }; !path.empty())
        {
            if (::Microsoft::Console::Utils::IsLikelyToBeEmojiOrSymbolIcon(path))
            {
                resource.Resolve(path);
                return;
            }

            ResolveMediaResource(origin, basePath, resource, resolver);
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
