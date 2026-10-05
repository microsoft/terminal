// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../pch.h"
#include "MediaResourceResolver.h"
#include "../../../types/inc/utils.hpp"

namespace Microsoft::Terminal::Settings::Model::Native
{
    void ResolveMediaResource(const OriginTag origin, const winrt::hstring& basePath, const winrt::com_ptr<MediaResource>& resource, const MediaResourceResolver& resolver)
    {
        if (resource->Path().empty() || resource->Ok())
        {
            return;
        }
        resolver(origin, basePath, resource);
    }

    void ResolveIconMediaResource(const OriginTag origin, const winrt::hstring& basePath, const winrt::com_ptr<MediaResource>& resource, const MediaResourceResolver& resolver)
    {
        const auto path = resource->Path();
        if (path.empty())
        {
            return;
        }
        if (::Microsoft::Console::Utils::IsLikelyToBeEmojiOrSymbolIcon(path))
        {
            resource->Resolve(path);
            return;
        }

        const std::wstring_view pathView{ path };
        const auto commaIndex = pathView.rfind(L',');
        if (!resource->Ok() && commaIndex != std::wstring_view::npos)
        {
            const auto pathWithoutIndex = pathView.substr(0, commaIndex);
            const auto index = til::parse_signed<int>(pathView.substr(commaIndex + 1));
            if (index &&
                (til::ends_with(pathWithoutIndex, L".exe") ||
                 til::ends_with(pathWithoutIndex, L".dll") ||
                 til::ends_with(pathWithoutIndex, L".lnk")))
            {
                const auto binaryResource = MediaResource::FromString(winrt::hstring{ pathWithoutIndex });
                resolver(origin, basePath, binaryResource);
                if (binaryResource->Ok())
                {
                    std::wstring resolvedPath{ binaryResource->Resolved() };
                    resolvedPath.append(pathView.substr(commaIndex));
                    resource->Resolve(winrt::hstring{ resolvedPath });
                }
                else
                {
                    resource->Reject();
                }
                return;
            }
        }
        ResolveMediaResource(origin, basePath, resource, resolver);
    }
}
