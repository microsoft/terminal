/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/

#pragma once

#include "MediaResourceHelper.g.h"
#include "Native/MediaResource.h"
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
    struct MediaResource : winrt::implements<MediaResource, winrt::Microsoft::Terminal::Settings::Model::IMediaResource>
    {
        using NativeResource = ::Microsoft::Terminal::Settings::Model::Native::MediaResource;

        explicit MediaResource(winrt::com_ptr<NativeResource> resource);
        ~MediaResource();

        winrt::hstring Path() const { return _resource->Path(); }
        winrt::hstring Resolved() const { return _resource->Resolved(); }
        bool Ok() const { return _resource->Ok(); }
        void Resolve(const winrt::hstring& path) { _resource->Resolve(path); }
        void Reject() { _resource->Reject(); }

        const winrt::com_ptr<NativeResource>& Native() const noexcept { return _resource; }

        static IMediaResource Empty();
        static IMediaResource FromString(const winrt::hstring& string);

    private:
        winrt::com_ptr<NativeResource> _resource;
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

            const std::wstring_view pathView{ path };
            const auto commaIndex = pathView.rfind(L',');
            if (!resource.Ok() && commaIndex != std::wstring_view::npos)
            {
                const auto pathWithoutIndex = pathView.substr(0, commaIndex);
                const auto index = til::parse_signed<int>(pathView.substr(commaIndex + 1));
                if (index &&
                    (til::ends_with(pathWithoutIndex, L".exe") ||
                     til::ends_with(pathWithoutIndex, L".dll") ||
                     til::ends_with(pathWithoutIndex, L".lnk")))
                {
                    const auto binaryResource{ MediaResource::FromString(winrt::hstring{ pathWithoutIndex }) };
                    resolver(origin, basePath, binaryResource);
                    if (binaryResource.Ok())
                    {
                        std::wstring resolvedPath{ binaryResource.Resolved() };
                        resolvedPath.append(pathView.substr(commaIndex));
                        resource.Resolve(winrt::hstring{ resolvedPath });
                    }
                    else
                    {
                        resource.Reject();
                    }
                    return;
                }
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
