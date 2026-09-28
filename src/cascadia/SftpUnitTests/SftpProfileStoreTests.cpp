// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "SftpProfileStore.h"
#include "SftpPassword.h"

#include <string>
#include <vector>

using namespace WEX::TestExecution;
using namespace WEX::Common;

class SftpProfileStoreTests
{
    TEST_CLASS(SftpProfileStoreTests);

    TEST_METHOD(TestJsonEscapeRoundTrip);
    TEST_METHOD(TestJsonUnescapeInvalid);
    TEST_METHOD(TestSerializeRoundTrip);
    TEST_METHOD(TestDeserializeToleratesLegacyAndMalformed);
};

void SftpProfileStoreTests::TestJsonEscapeRoundTrip()
{
    const std::wstring samples[]{
        L"plain",
        L"with \"quotes\" and \\ backslash",
        L"line\nfeed\r\nand\ttabs",
        L"unicode \u0441\u043b\u043e\u0432\u043e (\u00e9\u00e8)",
        L"",
    };
    for (const auto& sample : samples)
    {
        const auto escaped{ SftpProfileStore::JsonEscape(sample) };
        const auto unescaped{ SftpProfileStore::JsonUnescape(escaped) };
        VERIFY_IS_TRUE(unescaped.has_value() && *unescaped == sample, L"JsonEscape/JsonUnescape round trip");
    }
}

void SftpProfileStoreTests::TestJsonUnescapeInvalid()
{
    VERIFY_IS_FALSE(SftpProfileStore::JsonUnescape(L"trailing\\").has_value(), L"JsonUnescape rejects trailing backslash");
    VERIFY_IS_FALSE(SftpProfileStore::JsonUnescape(L"\\u12").has_value(), L"JsonUnescape rejects short \\uXXXX");
    VERIFY_IS_TRUE(SftpProfileStore::JsonUnescape(L"\\n\\r\\t\\\"\\\\").has_value(), L"JsonUnescape handles escapes");
}

void SftpProfileStoreTests::TestSerializeRoundTrip()
{
    std::vector<SftpProfileStore::SftpConnectionProfile> profiles;
    profiles.push_back({
        L"prod",
        L"example.com",
        22,
        L"alice",
        L"swordfish",
        L"C:\\keys\\id_ed25519",
    });
    profiles.push_back({
        L"custom port with \"special\" chars \\ and \n newline",
        L"[2001:db8::1]",
        2222,
        L"\u043f\u043e\u043b\u044c\u0437\u043e\u0432\u0430\u0442\u0435\u043b\u044c",
        L"p\u00e4ss \u0441 \u043a\u0438\u0440\u0438\u043b\u043b\u0438\u0446\u0435\u0439",
        L"",
    });
    profiles.push_back({ L"empty password", L"host", 22, L"user", L"", L"" });

    const auto json{ SftpProfileStore::SerializeProfiles(profiles) };

    std::vector<SftpProfileStore::SftpConnectionProfile> parsed;
    const bool ok{ SftpProfileStore::DeserializeProfiles(json, parsed) };
    VERIFY_IS_TRUE(ok, L"DeserializeProfiles succeeds on serialized data");
    VERIFY_ARE_EQUAL(parsed.size(), profiles.size(), L"DeserializeProfiles round trips element count");

    for (size_t i = 0; i < profiles.size(); ++i)
    {
        VERIFY_ARE_EQUAL(parsed[i].name, profiles[i].name, L"round trip: name");
        VERIFY_ARE_EQUAL(parsed[i].host, profiles[i].host, L"round trip: host");
        VERIFY_ARE_EQUAL(parsed[i].port, profiles[i].port, L"round trip: port");
        VERIFY_ARE_EQUAL(parsed[i].username, profiles[i].username, L"round trip: username");
        VERIFY_ARE_EQUAL(parsed[i].password, profiles[i].password, L"round trip: password (decrypted)");
        VERIFY_ARE_EQUAL(parsed[i].keyPath, profiles[i].keyPath, L"round trip: keyPath");
    }
}

void SftpProfileStoreTests::TestDeserializeToleratesLegacyAndMalformed()
{
    // Legacy plaintext password (no DPAPI marker) must pass through.
    std::vector<SftpProfileStore::SftpConnectionProfile> parsed;
    const std::wstring legacy{
        L"[{\"name\":\"old\",\"host\":\"h\",\"port\":22,\"username\":\"u\","
        L"\"password\":\"plaintext-pw\",\"keyPath\":\"\"}]"
    };
    VERIFY_IS_TRUE(SftpProfileStore::DeserializeProfiles(legacy, parsed), L"legacy plaintext config parses");
    VERIFY_IS_TRUE(parsed.size() == 1 && parsed[0].password == L"plaintext-pw", L"legacy plaintext password preserved");

    // Totally malformed input must not crash and must not fabricate profiles.
    parsed.clear();
    VERIFY_IS_TRUE(SftpProfileStore::DeserializeProfiles(L"this is not json at all", parsed), L"garbage input is tolerated");
    parsed.clear();
    VERIFY_IS_TRUE(SftpProfileStore::DeserializeProfiles(L"[]", parsed) && parsed.empty(), L"empty array yields no profiles");
    parsed.clear();
    VERIFY_IS_TRUE(SftpProfileStore::DeserializeProfiles(L"{\"name\":\"unterminated", parsed), L"unterminated input is tolerated");
}