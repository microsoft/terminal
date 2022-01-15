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

        VERIFY_ARE_EQUAL(expected.size(), actual.size());
        for (const auto& [expectedKey, expectedValue] : expected)
        {
            VERIFY_IS_TRUE(actual.find(expectedKey) != actual.end());
            VERIFY_ARE_EQUAL(expectedValue, actual[expectedKey]);
        }
    }

    TEST_METHOD(ToString)
    {
        til::env environment;
        environment.insert_or_assign(L"A", L"Apple");
        environment.insert_or_assign(L"B", L"Banana");
        environment.insert_or_assign(L"C", L"Cassowary");

        wchar_t expectedArray[] = L"A=Apple\0B=Banana\0C=Cassowary\0";

        const std::wstring expected{ expectedArray, ARRAYSIZE(expectedArray) };
        const auto& actual = environment.to_string();

        VERIFY_ARE_EQUAL(expected.size(), actual.size());
        for (size_t i = 0; i < expected.size(); ++i)
        {
            VERIFY_ARE_EQUAL(expected[i], actual[i]);
        }
    }
};
