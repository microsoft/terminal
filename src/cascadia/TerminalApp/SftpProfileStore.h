// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "SftpPassword.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Pure SFTP profile persistence, extracted from the UI code so the logic can
// be exercised by a lightweight test project without any WinRT/XAML
// dependencies. The on-disk format is a UTF-16 (no BOM) JSON array of
// connection profiles; passwords are stored encrypted (see SftpPassword).
namespace SftpProfileStore
{
    struct SftpConnectionProfile
    {
        std::wstring name{};
        std::wstring host{};
        uint32_t port{ 22 };
        std::wstring username{};
        std::wstring password{};
        std::wstring keyPath{};
    };

    static_assert(sizeof(wchar_t) == 2, "profile store assumes UTF-16 wchar_t");

    // Escapes a string so it can be stored as a JSON string literal.
    std::wstring JsonEscape(const std::wstring& in);

    // Decodes the JSON escape sequences used when writing profiles. Returns the
    // decoded text, or std::nullopt on a trailing/backslash-invalid input so
    // callers can detect malformed configuration instead of guessing.
    std::optional<std::wstring> JsonUnescape(const std::wstring& in);

    // Serializes profiles into the on-disk JSON text (passwords encrypted).
    std::wstring SerializeProfiles(const std::vector<SftpConnectionProfile>& profiles);

    // Parses the JSON text written by SerializeProfiles. Passwords are
    // decrypted in place. Tolerates unknown keys; returns false only on input
    // that is not well-formed enough to make progress.
    bool DeserializeProfiles(const std::wstring& text, std::vector<SftpConnectionProfile>& out);
}