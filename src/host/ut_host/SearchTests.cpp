// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "../buffer/out/search.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

class SearchTests
{
    TEST_CLASS(SearchTests);

    CommonState* m_state;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = new CommonState();

        m_state->PrepareGlobalFont();
        m_state->PrepareGlobalRenderer();
        m_state->PrepareGlobalScreenBuffer();

        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalScreenBuffer();
        m_state->CleanupGlobalRenderer();
        m_state->CleanupGlobalFont();

        delete m_state;

        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        m_state->PrepareNewTextBufferInfo();
        m_state->FillTextBuffer();

        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        m_state->CleanupNewTextBufferInfo();
        Selection::Instance().ClearSelection();
        return true;
    }

    static void DoFoundChecks(Search& s, til::point coordStartExpected, til::CoordType lineDelta)
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        auto coordEndExpected = coordStartExpected;
        coordEndExpected.x += 1;

        VERIFY_IS_TRUE(s.SelectCurrent());
        VERIFY_ARE_EQUAL(coordStartExpected, gci.renderData.GetSelectionAnchor());
        VERIFY_ARE_EQUAL(coordEndExpected, gci.renderData.GetSelectionEnd());

        coordStartExpected.y += lineDelta;
        coordEndExpected.y += lineDelta;
        s.FindNext();

        VERIFY_IS_TRUE(s.SelectCurrent());
        VERIFY_ARE_EQUAL(coordStartExpected, gci.renderData.GetSelectionAnchor());
        VERIFY_ARE_EQUAL(coordEndExpected, gci.renderData.GetSelectionEnd());

        coordStartExpected.y += lineDelta;
        coordEndExpected.y += lineDelta;
        s.FindNext();

        VERIFY_IS_TRUE(s.SelectCurrent());
        VERIFY_ARE_EQUAL(coordStartExpected, gci.renderData.GetSelectionAnchor());
        VERIFY_ARE_EQUAL(coordEndExpected, gci.renderData.GetSelectionEnd());

        coordStartExpected.y += lineDelta;
        coordEndExpected.y += lineDelta;
        s.FindNext();

        VERIFY_IS_TRUE(s.SelectCurrent());
        VERIFY_ARE_EQUAL(coordStartExpected, gci.renderData.GetSelectionAnchor());
        VERIFY_ARE_EQUAL(coordEndExpected, gci.renderData.GetSelectionEnd());
    }

    TEST_METHOD(ForwardCaseSensitive)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        Search s;
        s.ResetIfStale(gci.renderData, L"AB", false, false);
        DoFoundChecks(s, {}, 1);
    }

    TEST_METHOD(ForwardCaseSensitiveJapanese)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        Search s;
        s.ResetIfStale(gci.renderData, L"\x304b", false, false);
        DoFoundChecks(s, { 2, 0 }, 1);
    }

    TEST_METHOD(ForwardCaseInsensitive)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        Search s;
        s.ResetIfStale(gci.renderData, L"ab", false, true);
        DoFoundChecks(s, {}, 1);
    }

    TEST_METHOD(ForwardCaseInsensitiveJapanese)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        Search s;
        s.ResetIfStale(gci.renderData, L"\x304b", false, true);
        DoFoundChecks(s, { 2, 0 }, 1);
    }

    TEST_METHOD(BackwardCaseSensitive)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        Search s;
        s.ResetIfStale(gci.renderData, L"AB", true, false);
        DoFoundChecks(s, { 0, 3 }, -1);
    }

    TEST_METHOD(BackwardCaseSensitiveJapanese)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        Search s;
        s.ResetIfStale(gci.renderData, L"\x304b", true, false);
        DoFoundChecks(s, { 2, 3 }, -1);
    }

    TEST_METHOD(BackwardCaseInsensitive)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        Search s;
        s.ResetIfStale(gci.renderData, L"ab", true, true);
        DoFoundChecks(s, { 0, 3 }, -1);
    }

    TEST_METHOD(BackwardCaseInsensitiveJapanese)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        Search s;
        s.ResetIfStale(gci.renderData, L"\x304b", true, true);
        DoFoundChecks(s, { 2, 3 }, -1);
    }
};
