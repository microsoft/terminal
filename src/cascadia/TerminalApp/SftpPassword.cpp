// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "SftpPassword.h"

#include <windows.h>
#include <dpapi.h>
#include <wincrypt.h>

#include <string_view>
#include <vector>

#pragma comment(lib, "Crypt32.lib")

namespace
{
    // Marker used to distinguish a DPAPI-encrypted password from a legacy
    // plaintext one, so old configuration files keep working.
    constexpr std::wstring_view EncryptedPasswordMarker{ L"sftp-dpapi:" };

    // Minimal UTF-16 <-> UTF-8 conversions. Deliberately independent of til so
    // this file can be compiled by a small standalone test project.
    [[nodiscard]] std::string ToUtf8(const std::wstring& text)
    {
        if (text.empty())
        {
            return {};
        }
        const auto required{ WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr) };
        if (required <= 0)
        {
            return {};
        }
        std::string out(required, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), required, nullptr, nullptr);
        return out;
    }

    [[nodiscard]] std::wstring FromUtf8(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }
        const auto required{ MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0) };
        if (required <= 0)
        {
            return {};
        }
        std::wstring out(required, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), required);
        return out;
    }
}

namespace SftpPassword
{
    std::wstring Encrypt(const std::wstring& password)
    {
        if (password.empty())
        {
            return L"";
        }

        const auto utf8{ ToUtf8(password) };
        DATA_BLOB plain{};
        plain.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(utf8.data()));
        plain.cbData = static_cast<DWORD>(utf8.size());

        DATA_BLOB encrypted{};
        if (!CryptProtectData(&plain,
                              L"windows-terminal-sftp-password",
                              nullptr,
                              nullptr,
                              nullptr,
                              CRYPTPROTECT_UI_FORBIDDEN,
                              &encrypted))
        {
            return L"";
        }

        std::wstring encoded;
        DWORD encodedSize{ 0 };
        if (!CryptBinaryToStringW(encrypted.pbData,
                                  encrypted.cbData,
                                  CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                  nullptr,
                                  &encodedSize))
        {
            LocalFree(encrypted.pbData);
            return L"";
        }
        encoded.resize(encodedSize);
        if (!CryptBinaryToStringW(encrypted.pbData,
                                  encrypted.cbData,
                                  CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                  encoded.data(),
                                  &encodedSize))
        {
            LocalFree(encrypted.pbData);
            return L"";
        }
        LocalFree(encrypted.pbData);

        // CryptBinaryToStringW writes a trailing NUL; drop it.
        if (!encoded.empty() && encoded.back() == L'\0')
        {
            encoded.pop_back();
        }

        return std::wstring{ EncryptedPasswordMarker } + encoded;
    }

    std::wstring Decrypt(const std::wstring& password)
    {
        if (password.size() < EncryptedPasswordMarker.size() ||
            password.compare(0, EncryptedPasswordMarker.size(), EncryptedPasswordMarker) != 0)
        {
            return password;
        }

        const auto encoded{ password.substr(EncryptedPasswordMarker.size()) };

        DWORD blobSize{ 0 };
        if (!CryptStringToBinaryW(encoded.c_str(),
                                  static_cast<DWORD>(encoded.size()),
                                  CRYPT_STRING_BASE64,
                                  nullptr,
                                  &blobSize,
                                  nullptr,
                                  nullptr))
        {
            return L"";
        }
        std::vector<BYTE> blob(blobSize);
        if (!CryptStringToBinaryW(encoded.c_str(),
                                  static_cast<DWORD>(encoded.size()),
                                  CRYPT_STRING_BASE64,
                                  blob.data(),
                                  &blobSize,
                                  nullptr,
                                  nullptr))
        {
            return L"";
        }

        DATA_BLOB encrypted{};
        encrypted.pbData = blob.data();
        encrypted.cbData = blobSize;

        DATA_BLOB plain{};
        if (!CryptUnprotectData(&encrypted,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                CRYPTPROTECT_UI_FORBIDDEN,
                                &plain))
        {
            // Password was encrypted on a different account/machine and cannot
            // be recovered here.
            return L"";
        }

        std::string utf8{ reinterpret_cast<const char*>(plain.pbData), plain.cbData };
        LocalFree(plain.pbData);

        return FromUtf8(utf8);
    }
}