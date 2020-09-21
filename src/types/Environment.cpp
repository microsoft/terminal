// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/Environment.hpp"

using namespace ::Microsoft::Console::Utils;

// We cannot use spand or not_null because we're dealing with \0\0-terminated buffers of unknown length
#pragma warning(disable : 26481 26429)

// Function Description:
// - Updates an EnvironmentVariableMapW with the current process's unicode
//   environment variables ignoring ones already set in the provided map.
// Arguments:
// - map: The map to populate with the current processes's environment variables.
// Return Value:
// - S_OK if we succeeded, or an appropriate HRESULT for failing
HRESULT Microsoft::Console::Utils::UpdateEnvironmentMapW(EnvironmentVariableMapW& map) noexcept
try
{
    LPWCH currentEnvVars{};
    auto freeCurrentEnv = wil::scope_exit([&] {
        if (currentEnvVars)
        {
            FreeEnvironmentStringsW(currentEnvVars);
            currentEnvVars = nullptr;
        }
    });

    currentEnvVars = ::GetEnvironmentStringsW();
    RETURN_HR_IF_NULL(E_OUTOFMEMORY, currentEnvVars);

    // Each entry is NULL-terminated; block is guaranteed to be double-NULL terminated at a minimum.
    for (wchar_t const* lastCh{ currentEnvVars }; *lastCh != '\0'; ++lastCh)
    {
        // Copy current entry into temporary map.
        const size_t cchEntry{ ::wcslen(lastCh) };
        const std::wstring_view entry{ lastCh, cchEntry };

        // Every entry is of the form "name=value\0".
        const auto pos = entry.find_first_of(L"=", 0, 1);
        RETURN_HR_IF(E_UNEXPECTED, pos == std::wstring::npos);

        std::wstring name{ entry.substr(0, pos) }; // portion before '='
        std::wstring value{ entry.substr(pos + 1) }; // portion after '='

        // Don't replace entries that already exist.
        map.try_emplace(std::move(name), std::move(value));
        lastCh += cchEntry;
    }

    return S_OK;
}
CATCH_RETURN();

// Function Description:
// - Creates a new environment block using the provided vector as appropriate
//   (resizing if needed) based on the provided environment variable map
//   matching the format of GetEnvironmentStringsW.
// Arguments:
// - map: The map to populate the new environment block vector with.
// - newEnvVars: The vector that will be used to create the new environment block.
// Return Value:
// - S_OK if we succeeded, or an appropriate HRESULT for failing
HRESULT Microsoft::Console::Utils::EnvironmentMapToEnvironmentStringsW(EnvironmentVariableMapW& map, std::vector<wchar_t>& newEnvVars) noexcept
try
{
    // Clear environment block before use.
    constexpr size_t cbChar{ sizeof(decltype(newEnvVars.begin())::value_type) };

    if (!newEnvVars.empty())
    {
        ::SecureZeroMemory(newEnvVars.data(), newEnvVars.size() * cbChar);
    }

    // Resize environment block to fit map.
    size_t cchEnv{ 2 }; // For the block's double NULL-terminator.
    for (const auto& [name, value] : map)
    {
        // Final form of "name=value\0".
        cchEnv += name.size() + 1 + value.size() + 1;
    }
    newEnvVars.resize(cchEnv);

    // Ensure new block is wiped if we exit due to failure.
    auto zeroNewEnv = wil::scope_exit([&]() noexcept {
        ::SecureZeroMemory(newEnvVars.data(), newEnvVars.size() * cbChar);
    });

    // Transform each map entry and copy it into the new environment block.
    LPWCH pEnvVars{ newEnvVars.data() };
    size_t cbRemaining{ cchEnv * cbChar };
    for (const auto& [name, value] : map)
    {
        // Final form of "name=value\0" for every entry.
        {
            const size_t cchSrc{ name.size() };
            const size_t cbSrc{ cchSrc * cbChar };
            RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, name.c_str(), cbSrc) != 0);
            pEnvVars += cchSrc;
            cbRemaining -= cbSrc;
        }

        RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, L"=", cbChar) != 0);
        ++pEnvVars;
        cbRemaining -= cbChar;

        {
            const size_t cchSrc{ value.size() };
            const size_t cbSrc{ cchSrc * cbChar };
            RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, value.c_str(), cbSrc) != 0);
            pEnvVars += cchSrc;
            cbRemaining -= cbSrc;
        }

        RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, L"\0", cbChar) != 0);
        ++pEnvVars;
        cbRemaining -= cbChar;
    }

    // Environment block only has to be NULL-terminated, but double NULL-terminate anyway.
    RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, L"\0\0", cbChar * 2) != 0);
    cbRemaining -= cbChar * 2;

    RETURN_HR_IF(E_UNEXPECTED, cbRemaining != 0);

    zeroNewEnv.release(); // success; don't wipe new environment block on exit

    return S_OK;
}
CATCH_RETURN();
