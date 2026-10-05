#include "pch.h"
#include "MediaResourceSupport.h"
#include "MediaResourceHelper.g.cpp"

#include "AdapterCache.h"
#include "MediaResourceAdapter.h"

namespace
{
    using MediaResourceAdapter = winrt::Microsoft::Terminal::Settings::Model::implementation::MediaResource;
    using NativeResource = Microsoft::Terminal::Settings::Model::Native::MediaResource;

    auto& mediaResourceAdapters()
    {
        static Microsoft::Terminal::Settings::Model::Adapters::AdapterCache<NativeResource, MediaResourceAdapter> cache;
        return cache;
    }
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    MediaResource::MediaResource(winrt::com_ptr<NativeResource> resource) :
        _resource{ std::move(resource) }
    {
    }

    MediaResource::~MediaResource()
    {
        mediaResourceAdapters().Remove(_resource.get(), this);
    }

    IMediaResource MediaResource::Empty()
    {
        return ::Microsoft::Terminal::Settings::Model::Adapters::ToProjected(NativeResource::Empty());
    }

    IMediaResource MediaResource::FromString(const winrt::hstring& string)
    {
        return ::Microsoft::Terminal::Settings::Model::Adapters::ToProjected(NativeResource::FromString(string));
    }
}

namespace Microsoft::Terminal::Settings::Model::Adapters
{
    winrt::com_ptr<Native::MediaResource> ToNative(const winrt::Microsoft::Terminal::Settings::Model::IMediaResource& resource)
    {
        return resource ? winrt::get_self<MediaResourceAdapter>(resource)->Native() : nullptr;
    }

    winrt::Microsoft::Terminal::Settings::Model::IMediaResource ToProjected(const winrt::com_ptr<Native::MediaResource>& resource)
    {
        const auto adapter = mediaResourceAdapters().Get(resource);
        if (adapter)
        {
            return *adapter;
        }
        return nullptr;
    }
}
