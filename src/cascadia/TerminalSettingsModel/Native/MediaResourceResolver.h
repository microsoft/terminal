// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profile.h"

namespace Microsoft::Terminal::Settings::Model::Native
{
    TSM_NATIVE_API void ResolveMediaResource(OriginTag origin, const winrt::hstring& basePath, const winrt::com_ptr<MediaResource>& resource, const MediaResourceResolver& resolver);
    TSM_NATIVE_API void ResolveIconMediaResource(OriginTag origin, const winrt::hstring& basePath, const winrt::com_ptr<MediaResource>& resource, const MediaResourceResolver& resolver);
}
