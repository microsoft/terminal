// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*++
Module Name:
- HashUtils.h

Abstract:
- This module is used for hashing data consistently

Author(s):
- Carlos Zamora (CaZamor) 15-Apr-2021

Revision History:
- N/A
--*/

#pragma once

#include <til/hash.h>

namespace til
{
    template<typename T>
    struct hash_trait<winrt::Windows::Foundation::IReference<T>>
    {
        constexpr void operator()(hasher& h, const winrt::Windows::Foundation::IReference<T>& v) const noexcept
        {
            if (v)
            {
                h.write(v.Value());
            }
        }
    };

    template<>
    struct hash_trait<winrt::hstring>
    {
        void operator()(hasher& h, const winrt::hstring& value) const noexcept
        {
            h.write(reinterpret_cast<const uint8_t*>(value.data()), value.size() * sizeof(wchar_t));
        }
    };
}
