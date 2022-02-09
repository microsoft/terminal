// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "Monarch.h"

// This seems like a hack, but it works.
//
// This class factory works so that there's only ever one instance of a Monarch
// per-process. Once the first monarch is created, we'll stash it in g_weak.
// Future callers who try to instantiate a Monarch will get the one that's
// already been made.

struct MonarchFactory : winrt::implements<MonarchFactory, ::IClassFactory>
{
    MonarchFactory() = default;

    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept
    {
        static winrt::weak_ref<winrt::Microsoft::Terminal::Remoting::implementation::Monarch> g_weak{ nullptr };

        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        // Lock the ref immediately. We don't want it freed from out beneath us
        auto strong = g_weak.get();
        if (!strong)
        {
            // Create a new Monarch instance
            strong = winrt::make_self<winrt::Microsoft::Terminal::Remoting::implementation::Monarch>();
            g_weak = (*strong).get_weak();
            return strong.as(iid, result);
        }
        else
        {
            // We already instantiated one Monarch, let's just return that one!
            return strong.as(iid, result);
        }
    }

    HRESULT __stdcall LockServer(BOOL) noexcept
    {
        return S_OK;
    }
};
