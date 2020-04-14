// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

class ObjectTests
{
    CommonState* m_state;

    TEST_CLASS(ObjectTests);

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->InitEvents();
        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalScreenBuffer();
        m_state->PrepareGlobalInputBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalFont();
        m_state->CleanupGlobalInputBuffer();

        delete m_state;

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        return true;
    }

    TEST_METHOD(TestFailedHandleAllocationWhenNotShared)
    {
        Log::Comment(L"Create a new output buffer modeled from the default/active one.");
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& existingOutput = gci.GetActiveOutputBuffer();

        SCREEN_INFORMATION* newOutput;

        VERIFY_NT_SUCCESS(SCREEN_INFORMATION::CreateInstance(existingOutput.GetViewport().Dimensions(),
                                                             existingOutput.GetCurrentFont(),
                                                             existingOutput.GetBufferSize().Dimensions(),
                                                             existingOutput.GetAttributes(),
                                                             *existingOutput.GetPopupAttributes(),
                                                             existingOutput.GetTextBuffer().GetCursor().GetSize(),
                                                             &newOutput));

        ConsoleObjectHeader* newOutputAsHeader = static_cast<ConsoleObjectHeader*>(newOutput);

        VERIFY_ARE_EQUAL(0ul, newOutputAsHeader->_ulOpenCount);
        VERIFY_ARE_EQUAL(0ul, newOutputAsHeader->_ulReaderCount);
        VERIFY_ARE_EQUAL(0ul, newOutputAsHeader->_ulReadShareCount);
        VERIFY_ARE_EQUAL(0ul, newOutputAsHeader->_ulWriterCount);
        VERIFY_ARE_EQUAL(0ul, newOutputAsHeader->_ulWriteShareCount);

        std::unique_ptr<ConsoleHandleData> unsharedHandle;
        VERIFY_SUCCEEDED(newOutput->AllocateIoHandle(ConsoleHandleData::HandleType::Output,
                                                     GENERIC_READ | GENERIC_WRITE,
                                                     0,
                                                     unsharedHandle));
        unsharedHandle.release(); // leak the pointer because destruction will attempt count decrement

        VERIFY_ARE_EQUAL(1ul, newOutputAsHeader->_ulOpenCount);
        VERIFY_ARE_EQUAL(1ul, newOutputAsHeader->_ulReaderCount);
        VERIFY_ARE_EQUAL(0ul, newOutputAsHeader->_ulReadShareCount);
        VERIFY_ARE_EQUAL(1ul, newOutputAsHeader->_ulWriterCount);
        VERIFY_ARE_EQUAL(0ul, newOutputAsHeader->_ulWriteShareCount);

        std::unique_ptr<ConsoleHandleData> secondHandleAttempt;
        VERIFY_ARE_EQUAL(HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION), newOutput->AllocateIoHandle(ConsoleHandleData::HandleType::Output, GENERIC_READ | GENERIC_WRITE, 0, secondHandleAttempt));

        VERIFY_ARE_EQUAL(1ul, newOutputAsHeader->_ulOpenCount);
        VERIFY_ARE_EQUAL(1ul, newOutputAsHeader->_ulReaderCount);
        VERIFY_ARE_EQUAL(0ul, newOutputAsHeader->_ulReadShareCount);
        VERIFY_ARE_EQUAL(1ul, newOutputAsHeader->_ulWriterCount);
        VERIFY_ARE_EQUAL(0ul, newOutputAsHeader->_ulWriteShareCount);
    }
};
