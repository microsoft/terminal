// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Api.h"
#include <optional>
#include <utility>

#define TSM_NATIVE_LIFETIME_DECLARATIONS(type)                                     \
    TSM_NATIVE_API HRESULT __stdcall QueryInterface(REFIID iid, void** object) noexcept; \
    TSM_NATIVE_API ULONG __stdcall AddRef() noexcept;                              \
    TSM_NATIVE_API ULONG __stdcall Release() noexcept;                             \
    TSM_NATIVE_API winrt::com_ptr<type> get_strong() noexcept;                      \
    TSM_NATIVE_API winrt::weak_ref<type> get_weak()

#define TSM_NATIVE_PROPERTY(owner, type, name)                                    \
public:                                                                         \
    TSM_NATIVE_API type name() const;                                            \
    TSM_NATIVE_API void name(const type& value);                                 \
    TSM_NATIVE_API bool Has##name() const;                                       \
    TSM_NATIVE_API void Clear##name();                                          \
    TSM_NATIVE_API winrt::com_ptr<owner> name##OverrideSource() const;            \
                                                                                \
private:                                                                        \
    std::optional<type> _##name;                                                \
    std::optional<type> _get##name##Impl() const;                                \
    winrt::com_ptr<owner> _get##name##OverrideSourceImpl();                       \
    std::pair<winrt::com_ptr<owner>, std::optional<type>> _get##name##OverrideSourceAndValueImpl()
