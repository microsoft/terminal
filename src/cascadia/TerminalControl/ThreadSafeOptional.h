// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "pch.h"

template<typename T>
class ThreadSafeOptional
{
public:
    template<class... Args>
    bool Emplace(Args&&... args)
    {
        std::lock_guard guard{ _lock };

        bool hadValue = _inner.has_value();
        _inner.emplace(std::forward<Args>(args)...);
        return !hadValue;
    }

    std::optional<T> Take()
    {
        std::lock_guard guard{ _lock };

        std::optional<T> value;
        _inner.swap(value);

        return value;
    }

    template<typename F>
    void ModifyValue(F f)
    {
        std::lock_guard guard{ _lock };

        if (_inner.has_value())
        {
            f(_inner.value());
        }
    }

private:
    std::mutex _lock;
    std::optional<T> _inner;
};
