// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Native/MediaResource.h"
#include "Native/MediaResourceList.h"
#include <winrt/Microsoft.Terminal.Settings.Model.h>

namespace Microsoft::Terminal::Settings::Model::Adapters
{
    TSM_NATIVE_API winrt::com_ptr<Native::MediaResource> ToNative(const winrt::Microsoft::Terminal::Settings::Model::IMediaResource& resource);
    TSM_NATIVE_API winrt::Microsoft::Terminal::Settings::Model::IMediaResource ToProjected(const winrt::com_ptr<Native::MediaResource>& resource);
    TSM_NATIVE_API winrt::com_ptr<Native::MediaResourceList> ToNative(const winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::IMediaResource>& resources);
    TSM_NATIVE_API winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::IMediaResource> ToProjected(const winrt::com_ptr<Native::MediaResourceList>& resources);
}
