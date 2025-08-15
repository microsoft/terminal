// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

/*
USING RESOURCES
To use PRI resources that are included alongside your library:
- In one file, use
  UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"ResourceScope"). The
  provided scope will be used as the basename for all library
  resource lookups with RS_() or GetLibraryLocalizedString().
- Use RS_(L"ResourceName") for all statically-known resource
  names.
- Use GetLibraryResourceString(string) for all resource lookups
  for keys known only at runtime.
- For any static resource lookups that are deferred through
  another function call, use USES_RESOURCE(L"Key") to ensure the
  key is tracked.
*/

#ifdef _DEBUG

/*
The definitions in this section exist to support checked resources.
Check out the comment in LibraryResources.cpp to learn more.
*/

// Don't let non-debug and debug builds live together.
#pragma detect_mismatch("winrt_utils_debug", "1")

#pragma section(".util$res$m", read)

namespace Microsoft::Console::Utils
{
    struct StaticResource
    {
        const wchar_t* resourceKey;
        const wchar_t* filename;
        unsigned int line;
    };
}

#define USES_RESOURCE(x) ([]() {                                        \
    static const ::Microsoft::Console::Utils::StaticResource res{       \
        (x), __FILEW__, __LINE__                                        \
    };                                                                  \
    __declspec(allocate(".util$res$m")) static const auto pRes{ &res }; \
    return pRes->resourceKey;                                           \
}())
#define RS_(x) GetLibraryResourceString(USES_RESOURCE(x))

#else // _DEBUG

#pragma detect_mismatch("winrt_utils_debug", "0")

#define USES_RESOURCE(x) (x)
#define RS_(x) GetLibraryResourceString((x))

#endif

// For things that need UTF-8 strings
#define RS_A(x) (winrt::to_string(RS_(x)))

// Array-to-pointer decay might technically be avoidable, but this is elegant and clean.
#define UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(x) \
    __pragma(warning(suppress : 26485));       \
    __declspec(selectany) extern const wchar_t* g_WinRTUtilsLibraryResourceScope{ (x) };

class ScopedResourceLoader;

const ScopedResourceLoader& GetLibraryResourceLoader();
winrt::hstring GetLibraryResourceString(const std::wstring_view key);
bool HasLibraryResourceWithName(const std::wstring_view key);

#define RS_fmt(x, ...) RS_fmt_impl(USES_RESOURCE(x), __VA_ARGS__)

template<typename... Args>
std::wstring RS_fmt_impl(std::wstring_view key, Args&&... args)
{
    const auto format = GetLibraryResourceString(key);
    return fmt::format(fmt::runtime(std::wstring_view{ format }), std::forward<Args>(args)...);
}
