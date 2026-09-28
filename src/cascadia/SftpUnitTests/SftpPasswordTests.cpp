// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "SftpPassword.h"

#include <string>

using namespace WEX::TestExecution;
using namespace WEX::Common;

class SftpPasswordTests
{
    TEST_CLASS(SftpPasswordTests);

    TEST_METHOD(TestPasswordRoundTrip);
};

void SftpPasswordTests::TestPasswordRoundTrip()
{
    const std::wstring sensitive{ L"h\u00e9llo \u0441\u0435\u043a\u0440\u0435\u0442 passw0rd!" };
    const auto encrypted{ SftpPassword::Encrypt(sensitive) };
    VERIFY_IS_FALSE(encrypted.empty(), L"Encrypt returns non-empty marker+base64");
    VERIFY_ARE_EQUAL(encrypted.substr(0, 11), std::wstring{ L"sftp-dpapi:" }, L"Encrypt prefixes marker");
    VERIFY_IS_FALSE(encrypted == sensitive, L"Encrypt does not leak plaintext");

    const auto decrypted{ SftpPassword::Decrypt(encrypted) };
    VERIFY_ARE_EQUAL(decrypted, sensitive, L"Decrypt(Encrypt(x)) == x");

    VERIFY_ARE_EQUAL(SftpPassword::Decrypt(L"plaintext"), std::wstring{ L"plaintext" }, L"Decrypt passes through non-marker text");
    VERIFY_ARE_EQUAL(SftpPassword::Encrypt(L""), std::wstring{}, L"Encrypt of empty string is empty");
    VERIFY_ARE_EQUAL(SftpPassword::Decrypt(L""), std::wstring{}, L"Decrypt of empty string is empty");

    // A marker with invalid base64 must not crash and must produce empty.
    VERIFY_ARE_EQUAL(SftpPassword::Decrypt(L"sftp-dpapi:!!!not-base64!!!"), std::wstring{}, L"Decrypt tolerates bad base64");
}