// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "globals.h"
#include "../ConsoleArguments.hpp"
#include "../../types/inc/utils.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace Microsoft::Console::Utils;

class ConsoleArgumentsTests
{
public:
    TEST_CLASS(ConsoleArgumentsTests);

    TEST_METHOD(ArgSplittingTests);
    TEST_METHOD(ClientCommandlineTests);
    TEST_METHOD(LegacyFormatsTests);

    TEST_METHOD(IsUsingVtHandleTests);
    TEST_METHOD(CombineVtPipeHandleTests);
    TEST_METHOD(IsVtHandleValidTests);

    TEST_METHOD(InitialSizeTests);

    TEST_METHOD(HeadlessArgTests);
    TEST_METHOD(SignalHandleTests);
    TEST_METHOD(FeatureArgTests);
};

ConsoleArguments CreateAndParse(std::wstring& commandline, HANDLE hVtIn, HANDLE hVtOut)
{
    ConsoleArguments args = ConsoleArguments(commandline, hVtIn, hVtOut);
    VERIFY_SUCCEEDED(args.ParseCommandline());
    return args;
}

// Used when you expect args to be invalid
ConsoleArguments CreateAndParseUnsuccessfully(std::wstring& commandline, HANDLE hVtIn, HANDLE hVtOut)
{
    ConsoleArguments args = ConsoleArguments(commandline, hVtIn, hVtOut);
    VERIFY_FAILED(args.ParseCommandline());
    return args;
}

void ArgTestsRunner(LPCWSTR comment, std::wstring commandline, HANDLE hVtIn, HANDLE hVtOut, const ConsoleArguments& expected, bool shouldBeSuccessful)
{
    Log::Comment(comment);
    Log::Comment(commandline.c_str());
    const ConsoleArguments actual = shouldBeSuccessful ?
                                        CreateAndParse(commandline, hVtIn, hVtOut) :
                                        CreateAndParseUnsuccessfully(commandline, hVtIn, hVtOut);

    VERIFY_ARE_EQUAL(expected, actual);
}

void ConsoleArgumentsTests::ArgSplittingTests()
{
    std::wstring commandline;

    commandline = L"conhost.exe --headless this is the commandline";
    ArgTestsRunner(L"#1 look for a valid commandline",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"this is the commandline", // clientCommandLine,
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    true, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe \"this is the commandline\"";
    ArgTestsRunner(L"#2 a commandline with quotes",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"\"this is the commandline\"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --headless \"--vtmode bar this is the commandline\"";
    ArgTestsRunner(L"#3 quotes on an arg",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"\"--vtmode bar this is the commandline\"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    true, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --headless   --server    0x4       this      is the    commandline";
    ArgTestsRunner(L"#4 Many spaces",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"this is the commandline", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    true, // headless
                                    false, // createServerHandle
                                    0x4, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --headless\t--vtmode\txterm\tthis\tis\tthe\tcommandline";
    ArgTestsRunner(L"#5\ttab\tdelimit",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"this is the commandline", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"xterm", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    true, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --headless\\ foo\\ --outpipe\\ bar\\ this\\ is\\ the\\ commandline";
    ArgTestsRunner(L"#6 back-slashes won't escape spaces",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"--headless\\ foo\\ --outpipe\\ bar\\ this\\ is\\ the\\ commandline", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --headless\\\tfoo\\\t--outpipe\\\tbar\\\tthis\\\tis\\\tthe\\\tcommandline";
    ArgTestsRunner(L"#7 back-slashes won't escape tabs (but the tabs are still converted to spaces)",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"--headless\\ foo\\ --outpipe\\ bar\\ this\\ is\\ the\\ commandline", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --vtmode a\\\\\\\\\"b c\" d e";
    ArgTestsRunner(L"#8 Combo of backslashes and quotes from msdn",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"d e", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"a\\\\b c", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe this is the commandline";
    ArgTestsRunner(L"#9 commandline no quotes",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"this is the commandline", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?
}

void ConsoleArgumentsTests::ClientCommandlineTests()
{
    std::wstring commandline;

    commandline = L"conhost.exe -- foo";
    ArgTestsRunner(L"#1 Check that a simple explicit commandline is found",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"foo", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe foo";
    ArgTestsRunner(L"#2 Check that a simple implicit commandline is found",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"foo", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe foo -- bar";
    ArgTestsRunner(L"#3 Check that a implicit commandline with other expected args is treated as a whole client commandline (1)",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"foo -- bar", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --vtmode foo foo -- bar";
    ArgTestsRunner(L"#4 Check that a implicit commandline with other expected args is treated as a whole client commandline (2)",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"foo -- bar", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"foo", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe console --vtmode foo foo -- bar";
    ArgTestsRunner(L"#5 Check that a implicit commandline with other expected args is treated as a whole client commandline (3)",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"console --vtmode foo foo -- bar", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe console --vtmode foo --outpipe foo -- bar";
    ArgTestsRunner(L"#6 Check that a implicit commandline with other expected args is treated as a whole client commandline (4)",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"console --vtmode foo --outpipe foo -- bar", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --vtmode foo -- --outpipe foo bar";
    ArgTestsRunner(L"#7 Check splitting vt pipes across the explicit commandline does not pull both pipe names out",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"--outpipe foo bar", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"foo", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --vtmode -- --headless bar";
    ArgTestsRunner(L"#8 Let -- be used as a value of a parameter",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"bar", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"--", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    true, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --";
    ArgTestsRunner(L"#9 -- by itself does nothing successfully",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe";
    ArgTestsRunner(L"#10 An empty commandline should parse as an empty commandline",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?
}

void ConsoleArgumentsTests::LegacyFormatsTests()
{
    std::wstring commandline;

    commandline = L"conhost.exe 0x4";
    ArgTestsRunner(L"#1 Check that legacy launch mechanisms via the system loader with a server handle ID work",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --server 0x4";
    ArgTestsRunner(L"#2 Check that launch mechanism with parameterized server handle ID works",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe 0x4 0x8";
    ArgTestsRunner(L"#3 Check that two handle IDs fails (1)",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe --server 0x4 0x8";
    ArgTestsRunner(L"#4 Check that two handle IDs fails (2)",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe 0x4 --server 0x8";
    ArgTestsRunner(L"#5 Check that two handle IDs fails (3)",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe --server 0x4 --server 0x8";
    ArgTestsRunner(L"#6 Check that two handle IDs fails (4)",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe 0x4 -ForceV1";
    ArgTestsRunner(L"#7 Check that ConDrv handle + -ForceV1 succeeds",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    true, // forceV1
                                    false, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe -ForceV1";
    ArgTestsRunner(L"#8 Check that -ForceV1 parses on its own",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    true, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?
}

void ConsoleArgumentsTests::IsUsingVtHandleTests()
{
    ConsoleArguments args(L"", INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE);
    VERIFY_IS_FALSE(args.HasVtHandles());

    // Just some assorted positive values that could be valid handles. No specific correlation to anything.
    args._vtInHandle = UlongToHandle(0x12);
    VERIFY_IS_FALSE(args.HasVtHandles());

    args._vtOutHandle = UlongToHandle(0x16);
    VERIFY_IS_TRUE(args.HasVtHandles());

    args._vtInHandle = UlongToHandle(0ul);
    VERIFY_IS_FALSE(args.HasVtHandles());

    args._vtInHandle = UlongToHandle(0x20);
    args._vtOutHandle = UlongToHandle(0ul);
    VERIFY_IS_FALSE(args.HasVtHandles());
}

void ConsoleArgumentsTests::CombineVtPipeHandleTests()
{
    std::wstring commandline;

    // Just some assorted positive values that could be valid handles. No specific correlation to anything.
    HANDLE hInSample = UlongToHandle(0x10);
    HANDLE hOutSample = UlongToHandle(0x24);

    commandline = L"conhost.exe";
    ArgTestsRunner(L"#1 Check that handles with no mode is OK",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --vtmode telnet";
    ArgTestsRunner(L"#2 Check that handles with mode is OK",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    hInSample,
                                    hOutSample,
                                    L"telnet", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?
}

void ConsoleArgumentsTests::IsVtHandleValidTests()
{
    // We use both 0 and INVALID_HANDLE_VALUE as invalid handles since we're not sure
    // exactly what will get passed in on the STDIN/STDOUT handles as it can vary wildly
    // depending on who is passing it.
    VERIFY_IS_FALSE(IsValidHandle(0), L"Zero handle invalid.");
    VERIFY_IS_FALSE(IsValidHandle(INVALID_HANDLE_VALUE), L"Invalid handle invalid.");
    VERIFY_IS_TRUE(IsValidHandle(UlongToHandle(0x4)), L"0x4 is valid.");
}

void ConsoleArgumentsTests::InitialSizeTests()
{
    std::wstring commandline;

    commandline = L"conhost.exe --width 120 --height 30";
    ArgTestsRunner(L"#1 look for a valid commandline with both width and height",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    120, // width
                                    30, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --width 120";
    ArgTestsRunner(L"#2 look for a valid commandline with only width",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    120, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --height 30";
    ArgTestsRunner(L"#3 look for a valid commandline with only height",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    30, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --width 0";
    ArgTestsRunner(L"#4 look for a valid commandline passing 0",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --width -1";
    ArgTestsRunner(L"#5 look for a valid commandline passing -1",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    -1, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --width foo";
    ArgTestsRunner(L"#6 look for an ivalid commandline passing a string",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe --width 2foo";
    ArgTestsRunner(L"#7 look for an ivalid commandline passing a string with a number at the start",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe --width 65535";
    ArgTestsRunner(L"#7 look for an ivalid commandline passing a value that's too big",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?
}

void ConsoleArgumentsTests::HeadlessArgTests()
{
    std::wstring commandline;

    commandline = L"conhost.exe --headless";
    ArgTestsRunner(L"#1 Check that the headless arg works",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    true, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --headless 0x4";
    ArgTestsRunner(L"#2 Check that headless arg works with the server param",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    true, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --headless --headless";
    ArgTestsRunner(L"#3 multiple --headless params are all treated as one",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    true, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe -- foo.exe --headless";
    ArgTestsRunner(L"#4 ---headless as a client commandline does not make us headless",
                   commandline,
                   INVALID_HANDLE_VALUE,
                   INVALID_HANDLE_VALUE,
                   ConsoleArguments(commandline,
                                    L"foo.exe --headless", // clientCommandLine
                                    INVALID_HANDLE_VALUE,
                                    INVALID_HANDLE_VALUE,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?
}

void ConsoleArgumentsTests::SignalHandleTests()
{
    // Just some assorted positive values that could be valid handles. No specific correlation to anything.
    HANDLE hInSample = UlongToHandle(0x10);
    HANDLE hOutSample = UlongToHandle(0x24);

    std::wstring commandline;

    commandline = L"conhost.exe --server 0x4 --signal 0x8";
    ArgTestsRunner(L"#1 Normal case, pass both server and signal handle",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"",
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    8ul, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --server 0x4 --signal ASDF";
    ArgTestsRunner(L"#2 Pass bad signal handle",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"",
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    false, // createServerHandle
                                    4ul, // serverHandle
                                    0ul, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe --signal --server 0x4";
    ArgTestsRunner(L"#3 Pass null signal handle",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"",
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0ul, // serverHandle
                                    0ul, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?
}

void ConsoleArgumentsTests::FeatureArgTests()
{
    // Just some assorted positive values that could be valid handles. No specific correlation to anything.
    HANDLE hInSample = UlongToHandle(0x10);
    HANDLE hOutSample = UlongToHandle(0x24);

    std::wstring commandline;

    commandline = L"conhost.exe --feature pty";
    ArgTestsRunner(L"#1 Normal case, pass a supported feature",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"",
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?
    commandline = L"conhost.exe --feature tty";
    ArgTestsRunner(L"#2 Error case, pass an unsupported feature",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"",
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe --feature pty --feature pty";
    ArgTestsRunner(L"#3 Many supported features",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"",
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   true); // successful parse?

    commandline = L"conhost.exe --feature pty --feature tty";
    ArgTestsRunner(L"#4 At least one unsupported feature",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"",
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe --feature pty --feature";
    ArgTestsRunner(L"#5 no value to the feature flag",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"",
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?

    commandline = L"conhost.exe --feature pty --feature --signal foo";
    ArgTestsRunner(L"#6 a invalid feature value that is otherwise a valid arg",
                   commandline,
                   hInSample,
                   hOutSample,
                   ConsoleArguments(commandline,
                                    L"",
                                    hInSample,
                                    hOutSample,
                                    L"", // vtMode
                                    0, // width
                                    0, // height
                                    false, // forceV1
                                    false, // headless
                                    true, // createServerHandle
                                    0, // serverHandle
                                    0, // signalHandle
                                    false), // inheritCursor
                   false); // successful parse?
}
