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
    bool ActionExecute(const wchar_t /* wch */) override { return true; };
    bool ActionExecuteFromEscape(const wchar_t /* wch */) override { return true; };
    bool ActionPrint(const wchar_t /* wch */) override { return true; };
    bool ActionPrintString(const std::wstring_view /* string */) override { return true; };

    bool ActionPassThroughString(const std::wstring_view /* string */) override { return true; };

    bool ActionEscDispatch(const wchar_t /* wch */,
                           const std::basic_string_view<wchar_t> /* intermediates */) override { return true; };

    bool ActionClear() override { return true; };

    bool ActionIgnore() override { return true; };

    bool ActionOscDispatch(const wchar_t /* wch */,
                           const size_t /* parameter */,
                           const std::wstring_view /* string */) override { return true; };

    bool ActionSs3Dispatch(const wchar_t /* wch */,
                           const std::basic_string_view<size_t> /* parameters */) override { return true; };

    bool FlushAtEndOfString() const override { return false; };
    bool DispatchControlCharsFromEscape() const override { return false; };
    bool DispatchIntermediatesFromEscape() const override { return false; };

    // ActionCsiDispatch is the only method that's actually implemented.
    bool ActionCsiDispatch(const wchar_t /*wch*/,
                           const std::basic_string_view<wchar_t> /*intermediates*/,
                           const std::basic_string_view<size_t> parameters) override
    {
        csiParams.emplace(parameters.cbegin(), parameters.cend());
        return true;
    }

    // This will only be populated if ActionCsiDispatch is called.
    std::optional<std::vector<size_t>> csiParams;
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
