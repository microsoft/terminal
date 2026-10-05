// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <mutex>
#include <unordered_map>
#include <winrt/base.h>

namespace Microsoft::Terminal::Settings::Model::Adapters
{
    template<typename Native, typename Adapter>
    class AdapterCache
    {
    public:
        void Register(const Native* native, Adapter* adapter)
        {
            const std::lock_guard lock{ _mutex };
            _entries.insert_or_assign(native, Entry{ adapter, adapter->get_weak() });
        }

        winrt::com_ptr<Adapter> Get(const winrt::com_ptr<Native>& native)
        {
            if (!native)
            {
                return nullptr;
            }

            // Release the lock before destroying a failed or replaced adapter.
            winrt::com_ptr<Adapter> result;
            const std::lock_guard lock{ _mutex };
            const auto it = _entries.find(native.get());
            if (it != _entries.end())
            {
                result = it->second.weak.get();
            }
            if (!result)
            {
                result = winrt::make_self<Adapter>(native);
                _entries.insert_or_assign(native.get(), Entry{ result.get(), result->get_weak() });
            }
            return result;
        }

        void Remove(const Native* native, const Adapter* adapter) noexcept
        {
            const std::lock_guard lock{ _mutex };
            const auto it = _entries.find(native);
            if (it != _entries.end() && it->second.identity == adapter)
            {
                _entries.erase(it);
            }
        }

    private:
        struct Entry
        {
            const Adapter* identity;
            winrt::weak_ref<Adapter> weak;
        };

        std::mutex _mutex;
        std::unordered_map<const Native*, Entry> _entries;
    };
}
