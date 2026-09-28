// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <string>

// Pure password protection helpers, extracted from the UI code so the logic
// can be exercised by a lightweight test project without any WinRT/XAML
// dependencies. Passwords are protected with DPAPI (current-user scope); values
// that were written before this scheme existed (plaintext) survive intact.
//
// Note on portability: DPAPI blobs are tied to the user+account that produced
// them, so a config copied to another machine cannot be decrypted there. The
// trade-off is intentional: it keeps the plaintext out of the config file for
// casual readers without requiring a master password.
namespace SftpPassword
{
    // Encrypts a password with DPAPI and prefixes a marker so the serializer
    // can tell encrypted values from legacy plaintext ones. Empty -> empty.
    std::wstring Encrypt(const std::wstring& password);

    // Returns the plaintext for a value produced by Encrypt. Values without
    // the DPAPI marker (legacy plaintext) are returned unchanged. Returns
    // empty if the blob cannot be decrypted (different account/machine).
    std::wstring Decrypt(const std::wstring& password);
}