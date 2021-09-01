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

namespace Microsoft::Terminal::Settings::Model::HashUtils
{
    // This is a helper template function for hashing multiple variables in conjunction to each other.
    template<typename T>
    constexpr size_t HashProperty(const T& val)
    {
        std::hash<T> hashFunc;
        return hashFunc(val);
    }

    template<typename T, typename... Args>
    constexpr size_t HashProperty(const T& val, Args&&... more)
    {
        // Inspired by boost::hash_combine, which causes this effect...
        //   seed ^= hash_value(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        // Source: https://www.boost.org/doc/libs/1_35_0/doc/html/boost/hash_combine_id241013.html
        const auto seed{ HashProperty(val) };
        return seed ^ (0x9e3779b9 + (seed << 6) + (seed >> 2) + HashProperty(std::forward<Args>(more)...));
    }

    template<>
    constexpr size_t HashProperty(const til::color& val)
    {
        return HashProperty(val.a, val.r, val.g, val.b);
    }

#ifdef WINRT_Windows_Foundation_H
    template<typename T>
    constexpr size_t HashProperty(const winrt::Windows::Foundation::IReference<T>& val)
    {
        return val ? HashProperty(val.Value()) : 0;
    }
#endif

#ifdef WINRT_Windows_UI_H
    template<>
    constexpr size_t HashProperty(const winrt::Windows::UI::Color& val)
    {
        return HashProperty(til::color{ val });
    }
#endif

#ifdef WINRT_Microsoft_Terminal_Core_H
    template<>
    constexpr size_t HashProperty(const winrt::Microsoft::Terminal::Core::Color& val)
    {
        return HashProperty(til::color{ val });
    }
#endif

};
