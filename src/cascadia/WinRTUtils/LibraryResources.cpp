// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ScopedResourceLoader.h"
#include "LibraryResources.h"

/*
CHECKED RESOURCES
This is the support infrastructure for "checked resources", a system that lets
us immediately failfast (on launch) when a library makes a static reference to
a resource that doesn't exist at runtime.

Resource checking relies on diligent use of USES_RESOURCE() and RS_() (which
uses USES_RESOURCE), but can make sure we don't ship something that'll blow up
at runtime.

It works like this:
** IN DEBUG MODE **
- All resource names referenced through USES_RESOURCE() are emitted alongside
  their referencing filenames and line numbers into a static section of the
  binary.
  That section is named .util$res$m.

- We emit two sentinel values into two different sections, .util$res$a and
  .util$res$z.

- The linker sorts all sections alphabetically before crushing them together
  into the final binary.

- When we first construct our library's scoped resource loader, we iterate over
  every resource reference between $a and $z and check residency.

** IN RELEASE MODE **
- All checked resource code is compiled out.
*/

extern const wchar_t* g_WinRTUtilsLibraryResourceScope;

#ifdef _DEBUG
#pragma detect_mismatch("winrt_utils_debug", "1")

#pragma section(".util$res$a", read)
#pragma section(".util$res$z", read)

__declspec(allocate(".util$res$a")) static const ::Microsoft::Console::Utils::StaticResource* debugResFirst{ nullptr };
__declspec(allocate(".util$res$z")) static const ::Microsoft::Console::Utils::StaticResource* debugResLast{ nullptr };

static void EnsureAllResourcesArePresent(const ScopedResourceLoader& loader)
{
    for (auto resp = &debugResFirst; resp != &debugResLast; ++resp)
    {
        if (*resp)
        {
            const auto& res = **resp;
            if (!loader.HasResourceWithName(res.resourceKey))
            {
                auto filename = wcsrchr(res.filename, L'\\');
                if (!filename)
                {
                    filename = res.filename;
                }
                else
                {
                    filename++; // skip the '\'
                }

                FAIL_FAST_MSG("Resource %ls not found in scope %ls (%ls:%u)", res.resourceKey, g_WinRTUtilsLibraryResourceScope, filename, res.line);
            }
        }
    }
}
#else // _DEBUG

#pragma detect_mismatch("winrt_utils_debug", "0")

#endif

static ScopedResourceLoader GetLibraryResourceLoader() UTILS_NONDEBUG_NOEXCEPT
{
    ScopedResourceLoader loader{ g_WinRTUtilsLibraryResourceScope };
#ifdef _DEBUG
    EnsureAllResourcesArePresent(loader);
#endif
    return loader;
}

winrt::hstring GetLibraryResourceString(const std::wstring_view key) UTILS_NONDEBUG_NOEXCEPT
{
    static auto loader{ GetLibraryResourceLoader() };
    return loader.GetLocalizedString(key);
}
