// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "MediaResource.h"

#include <cassert>
#include <mutex>
#include <utility>

namespace Microsoft::Terminal::Settings::Model::Native
{
    namespace
    {
        struct EmptyResourceCache
        {
            std::mutex mutex;
            winrt::weak_ref<MediaResource> weak;
            const MediaResource* identity = nullptr;
        };

        auto& emptyResourceCache()
        {
            static EmptyResourceCache cache;
            return cache;
        }
    }

    HRESULT __stdcall MediaResource::QueryInterface(REFIID iid, void** object) noexcept
    {
        return base_type::QueryInterface(iid, object);
    }

    ULONG __stdcall MediaResource::AddRef() noexcept
    {
        return base_type::AddRef();
    }

    ULONG __stdcall MediaResource::Release() noexcept
    {
        return base_type::Release();
    }

    winrt::com_ptr<MediaResource> MediaResource::get_strong() noexcept
    {
        return base_type::get_strong();
    }

    winrt::weak_ref<MediaResource> MediaResource::get_weak()
    {
        return base_type::get_weak();
    }

    MediaResource::MediaResource(ConstructionToken, winrt::hstring path, const bool empty) :
        _path{ std::move(path) },
        _empty{ empty }
    {
    }

    MediaResource::~MediaResource() noexcept
    {
        if (_empty)
        {
            auto& cache = emptyResourceCache();
            const std::lock_guard lock{ cache.mutex };
            if (cache.identity == this)
            {
                // C++/WinRT weak-reference control blocks also hold a module lock.
                cache.weak = {};
                cache.identity = nullptr;
            }
        }
    }

    winrt::com_ptr<MediaResource> MediaResource::FromString(const winrt::hstring& path)
    {
        return winrt::make_self<MediaResource>(ConstructionToken{}, path, false);
    }

    winrt::com_ptr<MediaResource> MediaResource::Empty()
    {
        auto& cache = emptyResourceCache();
        winrt::com_ptr<MediaResource> result;
        const std::lock_guard lock{ cache.mutex };
        result = cache.weak.get();
        if (!result)
        {
            result = winrt::make_self<MediaResource>(ConstructionToken{}, winrt::hstring{}, true);
            cache.weak = result->get_weak();
            cache.identity = result.get();
        }
        return result;
    }

    winrt::hstring MediaResource::Path() const
    {
        return _path;
    }

    winrt::hstring MediaResource::Resolved() const
    {
        return _resolved ? _resolvedPath : _path;
    }

    bool MediaResource::Ok() const noexcept
    {
        return _ok;
    }

    void MediaResource::Resolve(const winrt::hstring& path)
    {
        assert(!_empty);
        if (!_empty)
        {
            _resolvedPath = path;
            _ok = true;
            _resolved = true;
        }
    }

    void MediaResource::Reject()
    {
        assert(!_empty);
        if (!_empty)
        {
            _resolvedPath = {};
            _ok = false;
            _resolved = true;
        }
    }
}
