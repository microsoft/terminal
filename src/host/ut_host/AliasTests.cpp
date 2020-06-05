// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

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
            PCWSTR newline = L"\r\n";
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
        int sepIndex = targetExpectedPair.Find(L'=');
        target = targetExpectedPair.Left(sepIndex);
        expected = targetExpectedPair.Mid(sepIndex + 1);
    }

    TEST_METHOD(TestMatchAndCopy)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:exeName", L"{test.exe}")
            TEST_METHOD_PROPERTY(L"Data:aliasName", L"{foo}")
            TEST_METHOD_PROPERTY(L"Data:originalString", L"{ foo one two three four five six seven eight nine ten eleven twelve }")
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
                                 L"bar $*=bar one two three four five six seven eight nine ten eleven twelve%,"
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
                                 L"MyMoney$$$$$$App=MyMoney$$$$$$App%," // this is a long standing bug, $$ isn't replaced with $.
                                 L"Invalid$Apple=Invalid$Apple%," // An invalid macro $A is copied through
                                 L"IEndInA$=IEndInA$%," // Ending in a $ is copied through.
                                 L"megamix $7$Gfun $1 $b test $9 $L $2.txt$tall$$the$$things $*$tat$g$gonce.log=megamix seven>fun one | test nine < two.txt%all$$the$$things one two three four five six seven eight nine ten eleven twelve%at>>once.log%"
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

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        DWORD linesExpected = _ReplacePercentWithCRLF(expected);

        std::wstring original(originalString);

        Alias::s_TestAddAlias(exe, alias, target);

        // Fill classic wchar_t[] buffer for interfacing with the MatchAndCopyAlias function
        const USHORT bufferSize = 160ui16;
        auto buffer = std::make_unique<wchar_t[]>(bufferSize);
        wcscpy_s(buffer.get(), bufferSize, original.data());

        const size_t cbBuffer = bufferSize * sizeof(wchar_t);
        size_t bufferUsed = 0;
        DWORD linesActual = 0;

        // Run the match and copy function.
        Alias::s_MatchAndCopyAliasLegacy(buffer.get(),
                                         wcslen(buffer.get()) * sizeof(wchar_t),
                                         buffer.get(),
                                         cbBuffer,
                                         bufferUsed,
                                         exe,
                                         linesActual);

        // Null terminate buffer for comparison
        buffer[bufferUsed / sizeof(wchar_t)] = L'\0';

        Log::Comment(String().Format(L"Expected: '%s'", expected.data()));
        Log::Comment(String().Format(L"Actual  : '%s'", buffer.get()));

        VERIFY_ARE_EQUAL(WEX::Common::String(expected.data()), WEX::Common::String(buffer.get()));

        VERIFY_ARE_EQUAL(linesExpected, linesActual);
    }

    TEST_METHOD(TestMatchAndCopyTrailingCRLF)
    {
        PWSTR pwszSource = L"SourceWithoutCRLF\r\n";
        const size_t cbSource = wcslen(pwszSource) * sizeof(wchar_t);

        const size_t cchTarget = 60;
        auto rgwchTarget = std::make_unique<wchar_t[]>(cchTarget);
        const size_t cbTarget = cchTarget * sizeof(wchar_t);
        wcscpy_s(rgwchTarget.get(), cchTarget, L"testtesttesttesttest");
        auto rgwchTargetBefore = std::make_unique<wchar_t[]>(cchTarget);
        wcscpy_s(rgwchTargetBefore.get(), cchTarget, rgwchTarget.get());

        size_t cbTargetUsed = 0;
        auto const cbTargetUsedBefore = cbTargetUsed;

        DWORD dwLines = 0;
        auto const dwLinesBefore = dwLines;

        // Register the wrong alias name before we try.
        std::wstring exe(L"exe.exe");
        std::wstring sourceWithoutCRLF(L"SourceWithoutCRLF");
        std::wstring target(L"someTarget");
        Alias::s_TestAddAlias(exe, sourceWithoutCRLF, target);

        const auto targetExpected = target + L"\r\n";
        const size_t cbTargetExpected = targetExpected.size() * sizeof(wchar_t); // +2 for \r\n that will be added on replace.

        Alias::s_MatchAndCopyAliasLegacy(pwszSource,
                                         cbSource,
                                         rgwchTarget.get(),
                                         cbTarget,
                                         cbTargetUsed,
                                         exe,
                                         dwLines);

        // Terminate target buffer with \0 for comparison
        rgwchTarget[cbTargetUsed] = L'\0';

        VERIFY_ARE_EQUAL(cbTargetExpected, cbTargetUsed, L"Target bytes should be filled with target size.");
        VERIFY_ARE_EQUAL(String(targetExpected.data()), String(rgwchTarget.get(), gsl::narrow<int>(cbTargetUsed / sizeof(wchar_t))), L"Target string should be filled with target data.");
        VERIFY_ARE_EQUAL(1u, dwLines, L"Line count should be 1.");
    }

    TEST_METHOD(TestMatchAndCopyInvalidExeName)
    {
        PWSTR pwszSource = L"Source";
        const size_t cbSource = wcslen(pwszSource) * sizeof(wchar_t);

        const size_t cchTarget = 12;
        auto rgwchTarget = std::make_unique<wchar_t[]>(cchTarget);
        wcscpy_s(rgwchTarget.get(), cchTarget, L"testtestabc");
        auto rgwchTargetBefore = std::make_unique<wchar_t[]>(cchTarget);
        wcscpy_s(rgwchTargetBefore.get(), cchTarget, rgwchTarget.get());

        const size_t cbTarget = cchTarget * sizeof(wchar_t);
        size_t cbTargetUsed = cbTarget;

        DWORD dwLines = 0;
        auto const dwLinesBefore = dwLines;

        std::wstring exeName;

        Alias::s_MatchAndCopyAliasLegacy(pwszSource,
                                         cbSource,
                                         rgwchTarget.get(),
                                         cbTarget,
                                         cbTargetUsed,
                                         exeName,
                                         dwLines);

        VERIFY_ARE_EQUAL(cbTarget, cbTargetUsed, L"Byte count shouldn't have changed with failure.");
        VERIFY_ARE_EQUAL(dwLinesBefore, dwLines, L"Line count shouldn't have changed with failure.");
        VERIFY_ARE_EQUAL(String(rgwchTargetBefore.get(), cchTarget), String(rgwchTarget.get(), cchTarget), L"Target string shouldn't have changed with failure.");
    }

    TEST_METHOD(TestMatchAndCopyExeNotFound)
    {
        PWSTR pwszSource = L"Source";
        const size_t cbSource = wcslen(pwszSource) * sizeof(wchar_t);

        const size_t cchTarget = 12;
        auto rgwchTarget = std::make_unique<wchar_t[]>(cchTarget);
        const size_t cbTarget = cchTarget * sizeof(wchar_t);
        wcscpy_s(rgwchTarget.get(), cchTarget, L"testtestabc");
        auto rgwchTargetBefore = std::make_unique<wchar_t[]>(cchTarget);
        wcscpy_s(rgwchTargetBefore.get(), cchTarget, rgwchTarget.get());
        size_t cbTargetUsed = 0;
        auto const cbTargetUsedBefore = cbTargetUsed;

        std::wstring exeName(L"exe.exe");

        DWORD dwLines = 0;
        auto const dwLinesBefore = dwLines;

        Alias::s_MatchAndCopyAliasLegacy(pwszSource,
                                         cbSource,
                                         rgwchTarget.get(),
                                         cbTarget,
                                         cbTargetUsed,
                                         exeName, // we didn't pre-set-up the exe name
                                         dwLines);

        VERIFY_ARE_EQUAL(cbTargetUsedBefore, cbTargetUsed, L"No bytes should have been written.");
        VERIFY_ARE_EQUAL(String(rgwchTargetBefore.get(), cchTarget), String(rgwchTarget.get(), cchTarget), L"Target string should be unmodified.");
        VERIFY_ARE_EQUAL(dwLinesBefore, dwLines, L"Line count should pass through.");
    }

    TEST_METHOD(TestMatchAndCopyAliasNotFound)
    {
        PWSTR pwszSource = L"Source";
        const size_t cbSource = wcslen(pwszSource) * sizeof(wchar_t);

        const size_t cchTarget = 12;
        auto rgwchTarget = std::make_unique<wchar_t[]>(cchTarget);
        const size_t cbTarget = cchTarget * sizeof(wchar_t);
        wcscpy_s(rgwchTarget.get(), cchTarget, L"testtestabc");
        auto rgwchTargetBefore = std::make_unique<wchar_t[]>(cchTarget);
        wcscpy_s(rgwchTargetBefore.get(), cchTarget, rgwchTarget.get());

        size_t cbTargetUsed = 0;
        auto const cbTargetUsedBefore = cbTargetUsed;

        DWORD dwLines = 0;
        auto const dwLinesBefore = dwLines;

        // Register the wrong alias name before we try.
        std::wstring exe(L"exe.exe");
        std::wstring badSource(L"wrongSource");
        std::wstring target(L"someTarget");
        Alias::s_TestAddAlias(exe, badSource, target);

        Alias::s_MatchAndCopyAliasLegacy(pwszSource,
                                         cbSource,
                                         rgwchTarget.get(),
                                         cbTarget,
                                         cbTargetUsed,
                                         exe,
                                         dwLines);

        VERIFY_ARE_EQUAL(cbTargetUsedBefore, cbTargetUsed, L"No bytes should be used if nothing was found.");
        VERIFY_ARE_EQUAL(String(rgwchTargetBefore.get(), cchTarget), String(rgwchTarget.get(), cchTarget), L"Target string should be unmodified.");
        VERIFY_ARE_EQUAL(dwLinesBefore, dwLines, L"Line count should pass through.");
    }

    TEST_METHOD(TestMatchAndCopyTargetTooSmall)
    {
        PWSTR pwszSource = L"Source";
        const size_t cbSource = wcslen(pwszSource) * sizeof(wchar_t);

        const size_t cchTarget = 12;
        auto rgwchTarget = std::make_unique<wchar_t[]>(cchTarget);
        wcscpy_s(rgwchTarget.get(), cchTarget, L"testtestabc");
        auto rgwchTargetBefore = std::make_unique<wchar_t[]>(cchTarget);
        wcscpy_s(rgwchTargetBefore.get(), cchTarget, rgwchTarget.get());

        const size_t cbTarget = cchTarget * sizeof(wchar_t);
        size_t cbTargetUsed = 0;
        auto const cbTargetUsedBefore = cbTargetUsed;

        DWORD dwLines = 0;
        auto const dwLinesBefore = dwLines;

        // Register the correct alias name before we try.
        std::wstring exe(L"exe.exe");
        std::wstring source(pwszSource);
        std::wstring target(L"someTarget");
        Alias::s_TestAddAlias(exe, source, target);

        Alias::s_MatchAndCopyAliasLegacy(pwszSource,
                                         cbSource,
                                         rgwchTarget.get(),
                                         1, // Make the target size too small
                                         cbTargetUsed,
                                         exe,
                                         dwLines);

        VERIFY_ARE_EQUAL(cbTargetUsedBefore, cbTargetUsed, L"Byte count shouldn't have changed with failure.");
        VERIFY_ARE_EQUAL(dwLinesBefore, dwLines, L"Line count shouldn't have changed with failure.");
        VERIFY_ARE_EQUAL(String(rgwchTargetBefore.get(), cchTarget), String(rgwchTarget.get(), cchTarget), L"Target string shouldn't have changed with failure.");
    }

    TEST_METHOD(TestMatchAndCopyLeadingSpaces)
    {
        PWSTR pwszSource = L" Source";
        const size_t cbSource = wcslen(pwszSource) * sizeof(wchar_t);

        const size_t cchTarget = 12;
        auto rgwchTarget = std::make_unique<wchar_t[]>(cchTarget);
        const size_t cbTarget = cchTarget * sizeof(wchar_t);
        wcscpy_s(rgwchTarget.get(), cchTarget, L"testtestabc");
        auto rgwchTargetBefore = std::make_unique<wchar_t[]>(cchTarget);
        wcscpy_s(rgwchTargetBefore.get(), cchTarget, rgwchTarget.get());
        size_t cbTargetUsed = 0;
        auto const cbTargetUsedExpected = cbTarget;

        DWORD dwLines = 0;
        auto const dwLinesExpected = dwLines + 1;

        // Register the correct alias name before we try.
        std::wstring exe(L"exe.exe");
        std::wstring source(L"Source");
        std::wstring target(L"someTarget");
        Alias::s_TestAddAlias(exe, source, target);

        std::wstring targetExpected = target + L"\r\n";

        // We should be able to match through the leading spaces. They should be stripped.
        Alias::s_MatchAndCopyAliasLegacy(pwszSource,
                                         cbSource,
                                         rgwchTarget.get(),
                                         cbTarget,
                                         cbTargetUsed,
                                         exe,
                                         dwLines);

        VERIFY_ARE_EQUAL(cbTargetUsedExpected, cbTargetUsed, L"No target bytes should be used.");
        VERIFY_ARE_EQUAL(String(targetExpected.data(), gsl::narrow<int>(targetExpected.size())), String(rgwchTarget.get(), cchTarget), L"Target string should match expected.");
        VERIFY_ARE_EQUAL(dwLinesExpected, dwLines, L"Line count be updated to 1.");
    }

    TEST_METHOD(TrimTrailing)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:targetExpectedPair",
                                 L"{"
                                 L"bar%=bar," // The character % will be turned into an \r\n
                                 L"bar=bar"
                                 L"}")
        END_TEST_METHOD_PROPERTIES()

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        // Substitute %s from metadata into \r\n (since metadata can't hold \r\n)
        _ReplacePercentWithCRLF(target);
        _ReplacePercentWithCRLF(expected);

        Alias::s_TrimTrailingCrLf(target);

        VERIFY_ARE_EQUAL(String(expected.data()), String(target.data()));
    }

    TEST_METHOD(Tokenize)
    {
        std::wstring tokenStr(L"one two three");
        std::deque<std::wstring> tokensExpected;
        tokensExpected.emplace_back(L"one");
        tokensExpected.emplace_back(L"two");
        tokensExpected.emplace_back(L"three");

        auto tokensActual = Alias::s_Tokenize(tokenStr);

        VERIFY_ARE_EQUAL(tokensExpected.size(), tokensActual.size());

        for (size_t i = 0; i < tokensExpected.size(); i++)
        {
            VERIFY_ARE_EQUAL(String(tokensExpected[i].data()), String(tokensActual[i].data()));
        }
    }

    TEST_METHOD(TokenizeNothing)
    {
        std::wstring tokenStr(L"alias");
        std::deque<std::wstring> tokensExpected;
        tokensExpected.emplace_back(tokenStr);

        auto tokensActual = Alias::s_Tokenize(tokenStr);

        VERIFY_ARE_EQUAL(tokensExpected.size(), tokensActual.size());

        for (size_t i = 0; i < tokensExpected.size(); i++)
        {
            VERIFY_ARE_EQUAL(String(tokensExpected[i].data()), String(tokensActual[i].data()));
        }
    }

    TEST_METHOD(GetArgString)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:targetExpectedPair",
                                 L"{"
                                 L"alias arg1 arg2 arg3=arg1 arg2 arg3,"
                                 L"aliasOnly="
                                 L"}")
        END_TEST_METHOD_PROPERTIES()

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        std::wstring actual = Alias::s_GetArgString(target);

        VERIFY_ARE_EQUAL(String(expected.data()), String(actual.data()));
    }

    TEST_METHOD(NumberedArgMacro)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:targetExpectedPair",
                                 L"{"
                                 L"1=one,"
                                 L"2=two,"
                                 L"3=three,"
                                 L"4=four,"
                                 L"5=five,"
                                 L"6=six,"
                                 L"7=seven,"
                                 L"8=eight,"
                                 L"9=nine,"
                                 L"A=,"
                                 L"0=,"
                                 L"}")
        END_TEST_METHOD_PROPERTIES()

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        std::deque<std::wstring> tokens;
        tokens.emplace_back(L"alias");
        tokens.emplace_back(L"one");
        tokens.emplace_back(L"two");
        tokens.emplace_back(L"three");
        tokens.emplace_back(L"four");
        tokens.emplace_back(L"five");
        tokens.emplace_back(L"six");
        tokens.emplace_back(L"seven");
        tokens.emplace_back(L"eight");
        tokens.emplace_back(L"nine");
        tokens.emplace_back(L"ten");

        // if we expect non-empty results, then we should get a bool back saying it was processed
        const bool returnExpected = !expected.empty();

        std::wstring actual;
        const bool returnActual = Alias::s_TryReplaceNumberedArgMacro(target[0], actual, tokens);

        VERIFY_ARE_EQUAL(returnExpected, returnActual);
        VERIFY_ARE_EQUAL(String(expected.data()), String(actual.data()));
    }

    TEST_METHOD(WildcardArgMacro)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:targetExpectedPair",
                                 L"{"
                                 L"*=one two three,"
                                 L"A=,"
                                 L"0=,"
                                 L"}")
        END_TEST_METHOD_PROPERTIES()

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        std::wstring fullArgString(L"one two three");

        // if we expect non-empty results, then we should get a bool back saying it was processed
        const bool returnExpected = !expected.empty();

        std::wstring actual;
        const bool returnActual = Alias::s_TryReplaceWildcardArgMacro(target[0], actual, fullArgString);

        VERIFY_ARE_EQUAL(returnExpected, returnActual);
        VERIFY_ARE_EQUAL(String(expected.data()), String(actual.data()));
    }

    TEST_METHOD(InputRedirMacro)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:targetExpectedPair",
                                 L"{"
                                 L"L=<,"
                                 L"l=<,"
                                 L"A=,"
                                 L"a=,"
                                 L"0=,"
                                 L"}")
        END_TEST_METHOD_PROPERTIES()

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        // if we expect non-empty results, then we should get a bool back saying it was processed
        const bool returnExpected = !expected.empty();

        std::wstring actual;
        const bool returnActual = Alias::s_TryReplaceInputRedirMacro(target[0], actual);

        VERIFY_ARE_EQUAL(returnExpected, returnActual);
        VERIFY_ARE_EQUAL(String(expected.data()), String(actual.data()));
    }

    TEST_METHOD(OutputRedirMacro)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:targetExpectedPair",
                                 L"{"
                                 L"G=>,"
                                 L"g=>,"
                                 L"A=,"
                                 L"a=,"
                                 L"0=,"
                                 L"}")
        END_TEST_METHOD_PROPERTIES()

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        // if we expect non-empty results, then we should get a bool back saying it was processed
        const bool returnExpected = !expected.empty();

        std::wstring actual;
        const bool returnActual = Alias::s_TryReplaceOutputRedirMacro(target[0], actual);

        VERIFY_ARE_EQUAL(returnExpected, returnActual);
        VERIFY_ARE_EQUAL(String(expected.data()), String(actual.data()));
    }

    TEST_METHOD(PipeRedirMacro)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:targetExpectedPair",
                                 L"{"
                                 L"B=|,"
                                 L"b=|,"
                                 L"A=,"
                                 L"a=,"
                                 L"0=,"
                                 L"}")
        END_TEST_METHOD_PROPERTIES()

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        // if we expect non-empty results, then we should get a bool back saying it was processed
        const bool returnExpected = !expected.empty();

        std::wstring actual;
        const bool returnActual = Alias::s_TryReplacePipeRedirMacro(target[0], actual);

        VERIFY_ARE_EQUAL(returnExpected, returnActual);
        VERIFY_ARE_EQUAL(String(expected.data()), String(actual.data()));
    }

    TEST_METHOD(NextCommandMacro)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:targetExpectedPair",
                                 L"{"
                                 L"T=%,"
                                 L"t=%,"
                                 L"A=,"
                                 L"a=,"
                                 L"0=,"
                                 L"}")
        END_TEST_METHOD_PROPERTIES()

        std::wstring target;
        std::wstring expected;
        _RetrieveTargetExpectedPair(target, expected);

        _ReplacePercentWithCRLF(expected);

        // if we expect non-empty results, then we should get a bool back saying it was processed
        const bool returnExpected = !expected.empty();

        std::wstring actual;
        size_t lineCountActual = 0;

        const auto lineCountExpected = lineCountActual + (returnExpected ? 1 : 0);

        const bool returnActual = Alias::s_TryReplaceNextCommandMacro(target[0], actual, lineCountActual);

        VERIFY_ARE_EQUAL(returnExpected, returnActual);
        VERIFY_ARE_EQUAL(String(expected.data()), String(actual.data()));
        VERIFY_ARE_EQUAL(lineCountExpected, lineCountActual);
    }

    TEST_METHOD(AppendCrLf)
    {
        std::wstring actual;
        size_t lineCountActual = 0;

        const std::wstring expected(L"\r\n");
        const auto lineCountExpected = lineCountActual + 1;

        Alias::s_AppendCrLf(actual, lineCountActual);
        VERIFY_ARE_EQUAL(String(expected.data()), String(actual.data()));
        VERIFY_ARE_EQUAL(lineCountExpected, lineCountActual);
    }
};
