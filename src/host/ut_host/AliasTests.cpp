// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

#include "alias.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class AliasTests
{
    TEST_CLASS(AliasTests);

    TEST_CLASS_SETUP(ClassSetup)
    {
        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        // Don't let aliases spill across test functions.
        Alias::s_TestClearAliases();
        return true;
    }

    DWORD _ReplacePercentWithCRLF(std::wstring& string)
    {
        DWORD linesExpected = 0;

        auto pos = string.find(L'%');

        while (pos != std::wstring::npos)
        {
            auto newline = L"\r\n";
            string = string.erase(pos, 1);
            string = string.insert(pos, newline);
            linesExpected++; // we expect one "line" per newline character returned.
            pos = string.find(L'%');
        }

        return linesExpected;
    }

    void _RetrieveTargetExpectedPair(std::wstring& target,
                                     std::wstring& expected)
    {
        // Get test parameters
        String targetExpectedPair;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"targetExpectedPair", targetExpectedPair));

        // Convert WEX strings into the wstrings
        auto sepIndex = targetExpectedPair.Find(L'=');
        target = targetExpectedPair.Left(sepIndex);
        expected = targetExpectedPair.Mid(sepIndex + 1);
    }

    TEST_METHOD(TestMatchAndCopy)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:exeName", L"{test.exe}")
            TEST_METHOD_PROPERTY(L"Data:aliasName", L"{foo}")
            TEST_METHOD_PROPERTY(L"Data:originalString", L"{ foo one two three four five six    seven eight nine ten eleven twelve }")
            TEST_METHOD_PROPERTY(L"Data:targetExpectedPair",
                                 L"{" // Each of these is a human-generated test of macro before and after.
                                 L"bar=bar%," // The character % will be turned into an \r\n
                                 L"bar $1=bar one%,"
                                 L"bar $2=bar two%,"
                                 L"bar $3=bar three%,"
                                 L"bar $4=bar four%,"
                                 L"bar $5=bar five%,"
                                 L"bar $6=bar six%,"
                                 L"bar $7=bar seven%,"
                                 L"bar $8=bar eight%,"
                                 L"bar $9=bar nine%,"
                                 L"bar $3 $1 $4 $1 $5 $9=bar three one four one five nine%," // assorted mixed order parameters with a repeat
                                 L"bar $*=bar one two three four five six    seven eight nine ten eleven twelve%,"
                                 L"longer=longer%," // replace with a target longer than the original alias
                                 L"redirect $1$goutput $2=redirect one>output two%," // doing these without spaces between some commands
                                 L"REDIRECT $1$GOUTPUT $2=REDIRECT one>OUTPUT two%," // also notice we're checking both upper and lowercase
                                 L"append $1$g$goutput $2=append one>>output two%,"
                                 L"APPEND $1$G$GOUTPUT $2=APPEND one>>OUTPUT two%,"
                                 L"redirect $1$linputfile.$2=redirect one<inputfile.two%,"
                                 L"REDIRECT $1$LINPUTFILE.$2=REDIRECT one<INPUTFILE.two%,"
                                 L"pipe $1$boutput $2=pipe one|output two%,"
                                 L"PIPE $1$BOUTPUT $2=PIPE one|OUTPUT two%,"
                                 L"run$tmultiple$tcommands=run%multiple%commands%,"
                                 L"MyMoney$$$$$$App=MyMoney$$$$$$App%," // this is a long-standing bug, $$ isn't replaced with $.
                                 L"Invalid$Apple=Invalid$Apple%," // An invalid macro $A is copied through
                                 L"IEndInA$=IEndInA$%," // Ending in a $ is copied through.
                                 L"megamix $7$Gfun $1 $b test $9 $L $2.txt$tall$$the$$things $*$tat$g$gonce.log=megamix seven>fun one | test nine < two.txt%all$$the$$things one two three four five six    seven eight nine ten eleven twelve%at>>once.log%"
                                 L"}")
        END_TEST_METHOD_PROPERTIES()

        // Get test parameters
        String exeName;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"exeName", exeName));

        String aliasName;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"aliasName", aliasName));

        String originalString;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"originalString", originalString));

        // Prepare internal alias structures

        // Convert WEX strings into the wstrings we will use to feed into the Alias structures
        // and match to our expected values.
        std::wstring alias(aliasName);
        std::wstring exe(exeName);
        std::wstring original(originalString);

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        const auto linesExpected = _ReplacePercentWithCRLF(expected);

        Alias::s_TestAddAlias(exe, alias, target);

        // Run the match and copy function.
        size_t linesActual = 0;
        const auto actual = Alias::s_MatchAndCopyAlias(original, exe, linesActual);

        VERIFY_ARE_EQUAL(expected, actual);
        VERIFY_ARE_EQUAL(linesExpected, linesActual);
    }

    TEST_METHOD(TestMatchAndCopyInvalidExeName)
    {
        const auto pwszSource = L"Source";
        size_t dwLines = 1;
        std::wstring exeName;
        const auto buffer = Alias::s_MatchAndCopyAlias(pwszSource, exeName, dwLines);
        VERIFY_IS_TRUE(buffer.empty());
        VERIFY_ARE_EQUAL(1u, dwLines);
    }

    TEST_METHOD(TestMatchAndCopyExeNotFound)
    {
        const auto pwszSource = L"Source";
        size_t dwLines = 1;
        const std::wstring exeName(L"exe.exe");
        const auto buffer = Alias::s_MatchAndCopyAlias(pwszSource, exeName, dwLines);
        VERIFY_IS_TRUE(buffer.empty());
        VERIFY_ARE_EQUAL(1u, dwLines);
    }

    TEST_METHOD(TestMatchAndCopyAliasNotFound)
    {
        const auto pwszSource = L"Source";
        size_t dwLines = 1;

        // Register the wrong alias name before we try.
        std::wstring exe(L"exe.exe");
        std::wstring badSource(L"wrongSource");
        std::wstring target(L"someTarget");
        Alias::s_TestAddAlias(exe, badSource, target);

        const auto buffer = Alias::s_MatchAndCopyAlias(pwszSource, exe, dwLines);
        VERIFY_IS_TRUE(buffer.empty());
        VERIFY_ARE_EQUAL(1u, dwLines);
    }

    TEST_METHOD(TestMatchAndCopyLeadingSpaces)
    {
        const auto pwszSource = L" Source";
        size_t dwLines = 1;

        // Register the correct alias name before we try.
        std::wstring exe(L"exe.exe");
        std::wstring source(L"Source");
        std::wstring target(L"someTarget");
        Alias::s_TestAddAlias(exe, source, target);

        // Leading spaces should bypass the alias. This should not match anything.
        const auto buffer = Alias::s_MatchAndCopyAlias(pwszSource, exe, dwLines);
        VERIFY_IS_TRUE(buffer.empty());
        VERIFY_ARE_EQUAL(1u, dwLines);
    }
};
