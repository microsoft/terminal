// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../../WinRTUtils/inc/WtExeUtils.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalAppLocalTests
{
    class QuoteAndEscapeTest
    {
        BEGIN_TEST_CLASS(QuoteAndEscapeTest)
        END_TEST_CLASS()

        TEST_METHOD(TestBasicQuoting);
        TEST_METHOD(TestEscapingQuotes);
        TEST_METHOD(TestEscapingSemicolons);
        TEST_METHOD(TestEscapingSquareBrackets);
        TEST_METHOD(TestPathWithSquareBrackets);
        TEST_METHOD(TestBackslashes);
    };

    void QuoteAndEscapeTest::TestBasicQuoting()
    {
        const auto result = QuoteAndEscapeCommandlineArg(L"simple");
        VERIFY_ARE_EQUAL(L"\"simple\"", result);
    }

    void QuoteAndEscapeTest::TestEscapingQuotes()
    {
        const auto result = QuoteAndEscapeCommandlineArg(L"has\"quote");
        VERIFY_ARE_EQUAL(L"\"has\\\"quote\"", result);
    }

    void QuoteAndEscapeTest::TestEscapingSemicolons()
    {
        const auto result = QuoteAndEscapeCommandlineArg(L"has;semicolon");
        VERIFY_ARE_EQUAL(L"\"has\\;semicolon\"", result);
    }

    void QuoteAndEscapeTest::TestEscapingSquareBrackets()
    {
        // Square brackets should be escaped with backticks for PowerShell compatibility
        const auto result = QuoteAndEscapeCommandlineArg(L"has[brackets]");
        VERIFY_ARE_EQUAL(L"\"has`[brackets`]\"", result);
    }

    void QuoteAndEscapeTest::TestPathWithSquareBrackets()
    {
        // Test a realistic path with square brackets like "C:\Users\Name\Downloads\Windows 11 [DVD]\"
        const auto result = QuoteAndEscapeCommandlineArg(L"C:\\Users\\Name\\Downloads\\Windows 11 [DVD]\\");
        VERIFY_ARE_EQUAL(L"\"C:\\Users\\Name\\Downloads\\Windows 11 `[DVD`]\\\\\"", result);
    }

    void QuoteAndEscapeTest::TestBackslashes()
    {
        // Backslashes before quotes should be escaped
        const auto result = QuoteAndEscapeCommandlineArg(L"path\\with\\\"quote");
        VERIFY_ARE_EQUAL(L"\"path\\with\\\\\"quote\"", result);
    }
}
