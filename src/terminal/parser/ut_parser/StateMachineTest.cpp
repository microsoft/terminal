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
    bool ActionPrintString(const wchar_t* const /* rgwch */,
                           size_t const /* cch */) override { return true; };

    bool ActionPassThroughString(const wchar_t* const /* rgwch */,
                                 size_t const /* cch */) override { return true; };

    bool ActionEscDispatch(const wchar_t /* wch */,
                           const unsigned short /* cIntermediate */,
                           const wchar_t /* wchIntermediate */) override { return true; };

    bool ActionClear() override { return true; };

    bool ActionIgnore() override { return true; };

    bool ActionOscDispatch(const wchar_t /* wch */,
                           const unsigned short /* sOscParam */,
                           wchar_t* const /* pwchOscStringBuffer */,
                           const unsigned short /* cchOscString */) override { return true; };

    bool ActionSs3Dispatch(const wchar_t /* wch */,
                           const unsigned short* const /* rgusParams */,
                           const unsigned short /* cParams */) override { return true; };

    bool FlushAtEndOfString() const override { return false; };
    bool DispatchControlCharsFromEscape() const override { return false; };
    bool DispatchIntermediatesFromEscape() const override { return false; };

    // ActionCsiDispatch is the only method that's actually implemented.
    bool ActionCsiDispatch(const wchar_t /* wch */,
                           const unsigned short /* cIntermediate */,
                           const wchar_t /* wchIntermediate */,
                           _In_reads_(cParams) const unsigned short* const rgusParams,
                           const unsigned short cParams) override
    {
        csiParams.emplace(rgusParams, rgusParams + cParams);
        return true;
    }

    // This will only be populated if ActionCsiDispatch is called.
    std::optional<std::vector<unsigned short>> csiParams;
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
    StateMachine firstStateMachine{ firstEnginePtr.release() };

    auto secondEnginePtr{ std::make_unique<TestStateMachineEngine>() };
    const auto& secondEngine{ *secondEnginePtr.get() };
    StateMachine secondStateMachine{ secondEnginePtr.release() };

    firstStateMachine.ProcessString(L"\x1b[12"); // partial sequence
    secondStateMachine.ProcessString(L"\x1b[3C"); // full sequence on second parser
    firstStateMachine.ProcessString(L";34m"); // completion to previous partial sequence on first parser

    std::vector<unsigned short> expectedFirstCsi{ 12u, 34u };
    std::vector<unsigned short> expectedSecondCsi{ 3u };

    VERIFY_ARE_EQUAL(expectedFirstCsi, firstEngine.csiParams);
    VERIFY_ARE_EQUAL(expectedSecondCsi, secondEngine.csiParams);
}
