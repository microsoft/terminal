// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/env.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

BOOL APIENTRY RegenerateUserEnvironment(void** pNewEnv, BOOL bSetCurrentEnv);

class EnvTests
{
    TEST_CLASS(EnvTests);

    TEST_METHOD(Construct)
    {
        til::env environment;
    }

    TEST_METHOD(Generate)
    {
        // Be sneaky and pull the method that @eryksun told us about to see if we've effectively recreated it.
        wil::unique_hmodule shell32{ LoadLibraryW(L"shell32.dll") };
        auto func = GetProcAddressByFunctionDeclaration(shell32.get(), RegenerateUserEnvironment);

        // Use a WIL pointer to free it on scope exit.
        wil::unique_environstrings_ptr newEnv;
        VERIFY_WIN32_BOOL_SUCCEEDED(func((void**)&newEnv, FALSE));

        // Parse the string into our environment table.
        til::env expected{ newEnv.get() };

        // Set up an empty table and tell it to generate the environment with a similar algorithm.
        til::env actual;
        actual.regenerate();

        VERIFY_ARE_EQUAL(expected.as_map().size(), actual.as_map().size());
        for (const auto& [expectedKey, expectedValue] : expected.as_map())
        {
            Log::Comment(String().Format(L"Environment Variable: '%s'", expectedKey.data()));
            const auto actualValue = actual.as_map()[expectedKey];
            VERIFY_IS_TRUE(actual.as_map().find(expectedKey) != actual.as_map().end());
            VERIFY_ARE_EQUAL(expectedValue, actual.as_map()[expectedKey]);
        }
    }

    TEST_METHOD(ToString)
    {
        til::env environment;
        environment.as_map().insert_or_assign(L"A", L"Apple");
        environment.as_map().insert_or_assign(L"B", L"Banana");
        environment.as_map().insert_or_assign(L"C", L"Cassowary");

        wchar_t expectedArray[] = L"A=Apple\0B=Banana\0C=Cassowary\0";

        const std::wstring expected{ expectedArray, ARRAYSIZE(expectedArray) };
        const auto& actual = environment.to_string();

        VERIFY_ARE_EQUAL(expected.size(), actual.size());
        for (size_t i = 0; i < expected.size(); ++i)
        {
            VERIFY_ARE_EQUAL(expected[i], actual[i]);
        }
    }

    TEST_METHOD(TestExpandEnvironmentStrings)
    {
        {
            til::env environment;
            environment.as_map().insert_or_assign(L"ENV", L"Bar");

            VERIFY_ARE_EQUAL(L"FooBarBaz", environment.expand_environment_strings(L"Foo%ENV%Baz"));
        }

        {
            til::env environment;

            VERIFY_ARE_EQUAL(L"Foo%ENV%Baz", environment.expand_environment_strings(L"Foo%ENV%Baz"));
        }

        {
            til::env environment;

            VERIFY_ARE_EQUAL(L"Foo%ENV", environment.expand_environment_strings(L"Foo%ENV"));
        }
    }
};
