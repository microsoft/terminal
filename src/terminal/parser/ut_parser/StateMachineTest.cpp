// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "stateMachine.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class StateMachineTest;
            class TestStateMachineEngine;
        };
    };
};

using namespace Microsoft::Console::VirtualTerminal;

class Microsoft::Console::VirtualTerminal::TestStateMachineEngine : public IStateMachineEngine
{
public:
    void ResetTestState()
    {
        printed.clear();
        passedThrough.clear();
        csiParams.reset();
    }

    bool ActionExecute(const wchar_t /* wch */) override { return true; };
    bool ActionExecuteFromEscape(const wchar_t /* wch */) override { return true; };
    bool ActionPrint(const wchar_t /* wch */) override { return true; };
    bool ActionPrintString(const std::wstring_view string) override
    {
        printed += string;
        return true;
    };

    bool ActionPassThroughString(const std::wstring_view string) override
    {
        passedThrough += string;
        return true;
    };

    bool ActionEscDispatch(const wchar_t /* wch */,
                           const std::basic_string_view<wchar_t> /* intermediates */) override { return true; };

    bool ActionVt52EscDispatch(const wchar_t /*wch*/,
                               const std::basic_string_view<wchar_t> /*intermediates*/,
                               const std::basic_string_view<size_t> /*parameters*/) override { return true; };

    bool ActionClear() override { return true; };

    bool ActionIgnore() override { return true; };

    bool ActionOscDispatch(const wchar_t /* wch */,
                           const size_t /* parameter */,
                           const std::wstring_view /* string */) override
    {
        if (pfnFlushToTerminal)
        {
            pfnFlushToTerminal();
            return true;
        }
        return true;
    };

    bool ActionSs3Dispatch(const wchar_t /* wch */,
                           const std::basic_string_view<size_t> /* parameters */) override { return true; };

    bool ParseControlSequenceAfterSs3() const override { return false; }
    bool FlushAtEndOfString() const override { return false; };
    bool DispatchControlCharsFromEscape() const override { return false; };
    bool DispatchIntermediatesFromEscape() const override { return false; };

    // ActionCsiDispatch is the only method that's actually implemented.
    bool ActionCsiDispatch(const wchar_t /*wch*/,
                           const std::basic_string_view<wchar_t> /*intermediates*/,
                           const std::basic_string_view<size_t> parameters) override
    {
        // If flush to terminal is registered for a test, then use it.
        if (pfnFlushToTerminal)
        {
            pfnFlushToTerminal();
            return true;
        }
        else
        {
            csiParams.emplace(parameters.cbegin(), parameters.cend());
            return true;
        }
    }

    // This will only be populated if ActionCsiDispatch is called.
    std::optional<std::vector<size_t>> csiParams;

    // Flush function for pass-through test.
    std::function<bool()> pfnFlushToTerminal;

    // Passed through string.
    std::wstring passedThrough;

    // Printed string.
    std::wstring printed;
};

class Microsoft::Console::VirtualTerminal::StateMachineTest
{
    TEST_CLASS(StateMachineTest);

    TEST_CLASS_SETUP(ClassSetup)
    {
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        return true;
    }

    TEST_METHOD(TwoStateMachinesDoNotInterfereWithEachother);

    TEST_METHOD(PassThroughUnhandled);
    TEST_METHOD(RunStorageBeforeEscape);
    TEST_METHOD(BulkTextPrint);
    TEST_METHOD(PassThroughUnhandledSplitAcrossWrites);
};

void StateMachineTest::TwoStateMachinesDoNotInterfereWithEachother()
{
    auto firstEnginePtr{ std::make_unique<TestStateMachineEngine>() };
    // this dance is required because StateMachine presumes to take ownership of its engine.
    const auto& firstEngine{ *firstEnginePtr.get() };
    StateMachine firstStateMachine{ std::move(firstEnginePtr) };

    auto secondEnginePtr{ std::make_unique<TestStateMachineEngine>() };
    const auto& secondEngine{ *secondEnginePtr.get() };
    StateMachine secondStateMachine{ std::move(secondEnginePtr) };

    firstStateMachine.ProcessString(L"\x1b[12"); // partial sequence
    secondStateMachine.ProcessString(L"\x1b[3C"); // full sequence on second parser
    firstStateMachine.ProcessString(L";34m"); // completion to previous partial sequence on first parser

    std::vector<size_t> expectedFirstCsi{ 12u, 34u };
    std::vector<size_t> expectedSecondCsi{ 3u };

    VERIFY_ARE_EQUAL(expectedFirstCsi, firstEngine.csiParams);
    VERIFY_ARE_EQUAL(expectedSecondCsi, secondEngine.csiParams);
}

void StateMachineTest::PassThroughUnhandled()
{
    auto enginePtr{ std::make_unique<TestStateMachineEngine>() };
    // this dance is required because StateMachine presumes to take ownership of its engine.
    auto& engine{ *enginePtr.get() };
    StateMachine machine{ std::move(enginePtr) };

    // Hook up the passthrough function.
    engine.pfnFlushToTerminal = std::bind(&StateMachine::FlushToTerminal, &machine);

    machine.ProcessString(L"\x1b[?999h 12345 Hello World");

    VERIFY_ARE_EQUAL(String(L"\x1b[?999h"), String(engine.passedThrough.c_str()));
    VERIFY_ARE_EQUAL(String(L" 12345 Hello World"), String(engine.printed.c_str()));
}

void StateMachineTest::RunStorageBeforeEscape()
{
    auto enginePtr{ std::make_unique<TestStateMachineEngine>() };
    // this dance is required because StateMachine presumes to take ownership of its engine.
    auto& engine{ *enginePtr.get() };
    StateMachine machine{ std::move(enginePtr) };

    // Hook up the passthrough function.
    engine.pfnFlushToTerminal = std::bind(&StateMachine::FlushToTerminal, &machine);

    // Print a bunch of regular text to build up the run buffer before transitioning state.
    machine.ProcessString(L"12345 Hello World\x1b[?999h");

    // Then ensure the entire buffered run was printed all at once back to us.
    VERIFY_ARE_EQUAL(String(L"12345 Hello World"), String(engine.printed.c_str()));
    VERIFY_ARE_EQUAL(String(L"\x1b[?999h"), String(engine.passedThrough.c_str()));
}

void StateMachineTest::BulkTextPrint()
{
    auto enginePtr{ std::make_unique<TestStateMachineEngine>() };
    // this dance is required because StateMachine presumes to take ownership of its engine.
    auto& engine{ *enginePtr.get() };
    StateMachine machine{ std::move(enginePtr) };

    // Print a bunch of regular text to build up the run buffer before transitioning state.
    machine.ProcessString(L"12345 Hello World");

    // Then ensure the entire buffered run was printed all at once back to us.
    VERIFY_ARE_EQUAL(String(L"12345 Hello World"), String(engine.printed.c_str()));
}

void StateMachineTest::PassThroughUnhandledSplitAcrossWrites()
{
    auto enginePtr{ std::make_unique<TestStateMachineEngine>() };
    // this dance is required because StateMachine presumes to take ownership of its engine.
    auto& engine{ *enginePtr.get() };
    StateMachine machine{ std::move(enginePtr) };

    // Hook up the passthrough function.
    engine.pfnFlushToTerminal = std::bind(&StateMachine::FlushToTerminal, &machine);

    // Broken in two pieces (test case from GH#3081)
    machine.ProcessString(L"\x1b[?12");
    VERIFY_ARE_EQUAL(L"", engine.passedThrough); // nothing out yet
    VERIFY_ARE_EQUAL(L"", engine.printed);

    machine.ProcessString(L"34h");
    VERIFY_ARE_EQUAL(L"\x1b[?1234h", engine.passedThrough); // whole sequence out, no other output
    VERIFY_ARE_EQUAL(L"", engine.printed);

    engine.ResetTestState();

    // Three pieces
    machine.ProcessString(L"\x1b[?2");
    VERIFY_ARE_EQUAL(L"", engine.passedThrough); // nothing out yet
    VERIFY_ARE_EQUAL(L"", engine.printed);

    machine.ProcessString(L"34");
    VERIFY_ARE_EQUAL(L"", engine.passedThrough); // nothing out yet
    VERIFY_ARE_EQUAL(L"", engine.printed);

    machine.ProcessString(L"5h");
    VERIFY_ARE_EQUAL(L"\x1b[?2345h", engine.passedThrough); // whole sequence out, no other output
    VERIFY_ARE_EQUAL(L"", engine.printed);

    engine.ResetTestState();

    // Split during OSC terminator (test case from GH#3080)
    machine.ProcessString(L"\x1b]99;foo\x1b");
    VERIFY_ARE_EQUAL(L"", engine.passedThrough); // nothing out yet
    VERIFY_ARE_EQUAL(L"", engine.printed);

    machine.ProcessString(L"\\");
    VERIFY_ARE_EQUAL(L"\x1b]99;foo\x1b\\", engine.passedThrough);
    VERIFY_ARE_EQUAL(L"", engine.printed);
}
