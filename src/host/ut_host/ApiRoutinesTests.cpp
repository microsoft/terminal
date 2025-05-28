// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "ApiRoutines.h"
#include "getset.h"
#include "dbcs.h"
#include "misc.h"

#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Types;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

class ApiRoutinesTests
{
    TEST_CLASS(ApiRoutinesTests);

    std::unique_ptr<CommonState> m_state;

    ApiRoutines _Routines;
    IApiRoutines* _pApiRoutines = &_Routines;
    CommandHistory* m_pHistory;

    TEST_METHOD_SETUP(MethodSetup)
    {
        m_state = std::make_unique<CommonState>();

        m_state->PrepareGlobalInputBuffer();
        m_state->PrepareGlobalScreenBuffer();

        m_pHistory = CommandHistory::s_Allocate(L"cmd.exe", nullptr);
        if (!m_pHistory)
        {
            return false;
        }
        // History must be prepared before COOKED_READ
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        CommandHistory::s_Free(nullptr);
        m_pHistory = nullptr;

        m_state->CleanupGlobalInputBuffer();

        m_state->CleanupGlobalScreenBuffer();

        m_state.reset(nullptr);

        return true;
    }

    BOOL _fPrevInsertMode;
    void PrepVerifySetConsoleInputModeImpl(const ULONG ulOriginalInputMode)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.Flags = 0;
        gci.pInputBuffer->InputMode = ulOriginalInputMode & ~(ENABLE_QUICK_EDIT_MODE | ENABLE_AUTO_POSITION | ENABLE_INSERT_MODE | ENABLE_EXTENDED_FLAGS);
        gci.SetInsertMode(WI_IsFlagSet(ulOriginalInputMode, ENABLE_INSERT_MODE));
        WI_UpdateFlag(gci.Flags, CONSOLE_QUICK_EDIT_MODE, WI_IsFlagSet(ulOriginalInputMode, ENABLE_QUICK_EDIT_MODE));
        WI_UpdateFlag(gci.Flags, CONSOLE_AUTO_POSITION, WI_IsFlagSet(ulOriginalInputMode, ENABLE_AUTO_POSITION));

        // Set cursor DB to on so we can verify that it turned off when the Insert Mode changes.
        gci.GetActiveOutputBuffer().SetCursorDBMode(true);

        // Record the insert mode at this time to see if it changed.
        _fPrevInsertMode = gci.GetInsertMode();
    }

    void VerifySetConsoleInputModeImpl(const HRESULT hrExpected,
                                       const ULONG ulNewMode)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto pii = gci.pInputBuffer;

        // The expected mode set in the buffer is the mode given minus the flags that are stored in different fields.
        auto ulModeExpected = ulNewMode;
        WI_ClearAllFlags(ulModeExpected, (ENABLE_QUICK_EDIT_MODE | ENABLE_AUTO_POSITION | ENABLE_INSERT_MODE | ENABLE_EXTENDED_FLAGS));
        const auto fQuickEditExpected = WI_IsFlagSet(ulNewMode, ENABLE_QUICK_EDIT_MODE);
        const auto fAutoPositionExpected = WI_IsFlagSet(ulNewMode, ENABLE_AUTO_POSITION);
        const auto fInsertModeExpected = WI_IsFlagSet(ulNewMode, ENABLE_INSERT_MODE);

        // If the insert mode changed, we expect the cursor to have turned off.
        const auto fCursorDBModeExpected = ((!!_fPrevInsertMode) == fInsertModeExpected);

        // Call the API
        const auto hrActual = _pApiRoutines->SetConsoleInputModeImpl(*pii, ulNewMode);

        // Now do verifications of final state.
        VERIFY_ARE_EQUAL(hrExpected, hrActual);
        VERIFY_ARE_EQUAL(ulModeExpected, pii->InputMode);
        VERIFY_ARE_EQUAL(fQuickEditExpected, WI_IsFlagSet(gci.Flags, CONSOLE_QUICK_EDIT_MODE));
        VERIFY_ARE_EQUAL(fAutoPositionExpected, WI_IsFlagSet(gci.Flags, CONSOLE_AUTO_POSITION));
        VERIFY_ARE_EQUAL(!!fInsertModeExpected, !!gci.GetInsertMode());
        VERIFY_ARE_EQUAL(fCursorDBModeExpected, gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor().IsDouble());
    }

    TEST_METHOD(ApiSetConsoleInputModeImplValidNonExtended)
    {
        Log::Comment(L"Set some perfectly valid, non-extended flags.");
        PrepVerifySetConsoleInputModeImpl(0);
        Log::Comment(L"Success code should result from setting valid flags.");
        Log::Comment(L"Flags should be set exactly as given.");
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    }

    TEST_METHOD(ApiSetConsoleInputModeImplValidExtended)
    {
        Log::Comment(L"Set some perfectly valid, extended flags.");
        PrepVerifySetConsoleInputModeImpl(0);
        Log::Comment(L"Success code should result from setting valid flags.");
        Log::Comment(L"Flags should be set exactly as given.");
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE | ENABLE_AUTO_POSITION);
    }

    TEST_METHOD(ApiSetConsoleInputModeImplExtendedTurnOff)
    {
        Log::Comment(L"Try to turn off extended flags.");
        PrepVerifySetConsoleInputModeImpl(ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE | ENABLE_AUTO_POSITION);
        Log::Comment(L"Success code should result from setting valid flags.");
        Log::Comment(L"Flags should be set exactly as given.");
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_EXTENDED_FLAGS);
    }

    TEST_METHOD(ApiSetConsoleInputModeImplInvalid)
    {
        Log::Comment(L"Set some invalid flags.");
        PrepVerifySetConsoleInputModeImpl(0);
        Log::Comment(L"Should get invalid argument code because we set invalid flags.");
        Log::Comment(L"Flags should be set anyway despite invalid code.");
        VerifySetConsoleInputModeImpl(E_INVALIDARG, 0x8000000);
    }

    TEST_METHOD(ApiSetConsoleInputModeImplInsertNoCookedRead)
    {
        Log::Comment(L"Turn on insert mode without cooked read data.");
        PrepVerifySetConsoleInputModeImpl(0);
        Log::Comment(L"Success code should result from setting valid flags.");
        Log::Comment(L"Flags should be set exactly as given.");
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE);
        Log::Comment(L"Turn back off and verify.");
        PrepVerifySetConsoleInputModeImpl(0);
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_EXTENDED_FLAGS);
    }

    TEST_METHOD(ApiSetConsoleInputModeImplInsertCookedRead)
    {
        Log::Comment(L"Turn on insert mode with cooked read data.");
        m_state->PrepareReadHandle();
        auto cleanupReadHandle = wil::scope_exit([&]() { m_state->CleanupReadHandle(); });
        m_state->PrepareCookedReadData();
        auto cleanupCookedRead = wil::scope_exit([&]() { m_state->CleanupCookedReadData(); });

        PrepVerifySetConsoleInputModeImpl(0);
        Log::Comment(L"Success code should result from setting valid flags.");
        Log::Comment(L"Flags should be set exactly as given.");
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE);
        Log::Comment(L"Turn back off and verify.");
        PrepVerifySetConsoleInputModeImpl(0);
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_EXTENDED_FLAGS);
    }

    TEST_METHOD(ApiSetConsoleInputModeImplEchoOnLineOff)
    {
        Log::Comment(L"Set ECHO on with LINE off. It's invalid, but it should get set anyway and return an error code.");
        PrepVerifySetConsoleInputModeImpl(0);
        Log::Comment(L"Setting ECHO without LINE should return an invalid argument code.");
        Log::Comment(L"Input mode should be set anyway despite FAILED return code.");
        VerifySetConsoleInputModeImpl(E_INVALIDARG, ENABLE_ECHO_INPUT);
    }

    TEST_METHOD(ApiSetConsoleInputModeExtendedFlagBehaviors)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        Log::Comment(L"Verify that we can set various extended flags even without the ENABLE_EXTENDED_FLAGS flag.");
        PrepVerifySetConsoleInputModeImpl(0);
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_INSERT_MODE);
        PrepVerifySetConsoleInputModeImpl(0);
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_QUICK_EDIT_MODE);
        PrepVerifySetConsoleInputModeImpl(0);
        VerifySetConsoleInputModeImpl(S_OK, ENABLE_AUTO_POSITION);

        Log::Comment(L"Verify that we cannot unset various extended flags without the ENABLE_EXTENDED_FLAGS flag.");
        PrepVerifySetConsoleInputModeImpl(ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE | ENABLE_AUTO_POSITION);
        const auto pii = gci.pInputBuffer;
        const auto hr = _pApiRoutines->SetConsoleInputModeImpl(*pii, 0);

        VERIFY_ARE_EQUAL(S_OK, hr);
        VERIFY_ARE_EQUAL(true, !!gci.GetInsertMode());
        VERIFY_ARE_EQUAL(true, WI_IsFlagSet(gci.Flags, CONSOLE_QUICK_EDIT_MODE));
        VERIFY_ARE_EQUAL(true, WI_IsFlagSet(gci.Flags, CONSOLE_AUTO_POSITION));
    }

    TEST_METHOD(ApiSetConsoleInputModeImplPSReadlineScenario)
    {
        Log::Comment(L"Set Powershell PSReadline expected modes.");
        PrepVerifySetConsoleInputModeImpl(0x1F7);
        Log::Comment(L"Should return an invalid argument code because ECHO is set without LINE.");
        Log::Comment(L"Input mode should be set anyway despite FAILED return code.");
        VerifySetConsoleInputModeImpl(E_INVALIDARG, 0x1E4);
    }

    TEST_METHOD(ApiGetConsoleTitleA)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        // SetTitle() runs some extra code. Let's not skip it since this is a test.
        gci.SetTitle(L"Test window title.");
        const auto pszExpected = til::u16u8(gci.GetTitle());

        char pszTitle[MAX_PATH]; // most applications use MAX_PATH
        size_t cchWritten = 0;
        size_t cchNeeded = 0;
        VERIFY_SUCCEEDED(_pApiRoutines->GetConsoleTitleAImpl(std::span<char>(pszTitle, ARRAYSIZE(pszTitle)), cchWritten, cchNeeded));

        VERIFY_ARE_NOT_EQUAL(0u, cchWritten);
        // NOTE: W version of API returns string length. A version of API returns buffer length (string + null).
        VERIFY_ARE_EQUAL(gci.GetTitle().length() + 1, cchWritten);
        VERIFY_ARE_EQUAL(gci.GetTitle().length(), cchNeeded);
        VERIFY_ARE_EQUAL(std::string_view{ pszExpected }, std::string_view{ pszTitle });
    }

    TEST_METHOD(ApiGetConsoleTitleW)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.SetTitle(L"Test window title.");

        wchar_t pwszTitle[MAX_PATH]; // most applications use MAX_PATH
        size_t cchWritten = 0;
        size_t cchNeeded = 0;
        VERIFY_SUCCEEDED(_pApiRoutines->GetConsoleTitleWImpl(std::span<wchar_t>(pwszTitle, ARRAYSIZE(pwszTitle)), cchWritten, cchNeeded));

        VERIFY_ARE_NOT_EQUAL(0u, cchWritten);

        const auto title = gci.GetTitle();

        // NOTE: W version of API returns string length. A version of API returns buffer length (string + null).
        VERIFY_ARE_EQUAL(title.length(), cchWritten);
        VERIFY_ARE_EQUAL(title.length(), cchNeeded);
        VERIFY_ARE_EQUAL(WEX::Common::String(title.data(), gsl::narrow_cast<int>(title.size())), WEX::Common::String(pwszTitle));
    }

    TEST_METHOD(ApiGetConsoleOriginalTitleA)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.SetOriginalTitle(L"Test original window title.");

        const auto originalTitle = gci.GetOriginalTitle();

        const auto iBytesNeeded = WideCharToMultiByte(gci.OutputCP,
                                                      0,
                                                      originalTitle.data(),
                                                      gsl::narrow_cast<int>(originalTitle.size()),
                                                      nullptr,
                                                      0,
                                                      nullptr,
                                                      nullptr);

        auto pszExpected = wil::make_unique_nothrow<char[]>(iBytesNeeded + 1);
        VERIFY_IS_NOT_NULL(pszExpected);

        VERIFY_WIN32_BOOL_SUCCEEDED(WideCharToMultiByte(gci.OutputCP,
                                                        0,
                                                        originalTitle.data(),
                                                        gsl::narrow_cast<int>(originalTitle.size()),
                                                        pszExpected.get(),
                                                        iBytesNeeded,
                                                        nullptr,
                                                        nullptr));

        // Make sure we terminate the expected title -- WC2MB does not add the \0 if we use the size variant
        pszExpected[iBytesNeeded] = '\0';

        char pszTitle[MAX_PATH]; // most applications use MAX_PATH
        size_t cchWritten = 0;
        size_t cchNeeded = 0;
        VERIFY_SUCCEEDED(_pApiRoutines->GetConsoleOriginalTitleAImpl(std::span<char>(pszTitle, ARRAYSIZE(pszTitle)), cchWritten, cchNeeded));

        VERIFY_ARE_NOT_EQUAL(0u, cchWritten);
        // NOTE: W version of API returns string length. A version of API returns buffer length (string + null).
        VERIFY_ARE_EQUAL(gci.GetOriginalTitle().length() + 1, cchWritten);
        VERIFY_ARE_EQUAL(gci.GetOriginalTitle().length(), cchNeeded);
        VERIFY_ARE_EQUAL(WEX::Common::String(pszExpected.get()), WEX::Common::String(pszTitle));
    }

    TEST_METHOD(ApiGetConsoleOriginalTitleW)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.SetOriginalTitle(L"Test original window title.");

        wchar_t pwszTitle[MAX_PATH]; // most applications use MAX_PATH
        size_t cchWritten = 0;
        size_t cchNeeded = 0;
        VERIFY_SUCCEEDED(_pApiRoutines->GetConsoleOriginalTitleWImpl(std::span<wchar_t>(pwszTitle, ARRAYSIZE(pwszTitle)), cchWritten, cchNeeded));

        VERIFY_ARE_NOT_EQUAL(0u, cchWritten);

        const auto originalTitle = gci.GetOriginalTitle();
        // NOTE: W version of API returns string length. A version of API returns buffer length (string + null).
        VERIFY_ARE_EQUAL(originalTitle.length(), cchWritten);
        VERIFY_ARE_EQUAL(originalTitle.length(), cchNeeded);
        VERIFY_ARE_EQUAL(WEX::Common::String(originalTitle.data(), gsl::narrow_cast<int>(originalTitle.size())), WEX::Common::String(pwszTitle));
    }

    static void s_AdjustOutputWait(const bool fShouldBlock)
    {
        WI_SetFlagIf(ServiceLocator::LocateGlobals().getConsoleInformation().Flags, CONSOLE_SELECTING, fShouldBlock);
        WI_ClearFlagIf(ServiceLocator::LocateGlobals().getConsoleInformation().Flags, CONSOLE_SELECTING, !fShouldBlock);
    }

    TEST_METHOD(ApiWriteConsoleA)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:fInduceWait", L"{false, true}")
            TEST_METHOD_PROPERTY(L"Data:dwCodePage", L"{437, 932, 65001}")
            TEST_METHOD_PROPERTY(L"Data:dwIncrement", L"{0, 1, 2}")
        END_TEST_METHOD_PROPERTIES();

        bool fInduceWait;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"fInduceWait", fInduceWait), L"Get whether or not we should exercise this function off a wait state.");

        DWORD dwCodePage;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"dwCodePage", dwCodePage), L"Get the codepage for the test. Check a single byte, a double byte, and UTF-8.");

        DWORD dwIncrement;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"dwIncrement", dwIncrement),
                         L"Get how many chars we should feed in at a time. This validates lead bytes and bytes held across calls.");

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& si = gci.GetActiveOutputBuffer();

        gci.LockConsole();
        auto Unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

        // Ensure global state is updated for our codepage.
        gci.OutputCP = dwCodePage;
        SetConsoleCPInfo(TRUE);

        PCSTR pszTestText;
        switch (dwCodePage)
        {
        case CP_USA: // US English ANSI
            pszTestText = "Test Text";
            break;
        case CP_JAPANESE: // Japanese Shift-JIS
            pszTestText = "J\x82\xa0\x82\xa2";
            break;
        case CP_UTF8:
            pszTestText = "Test \xe3\x82\xab Text";
            break;
        default:
            VERIFY_FAIL(L"Test is not ready for this codepage.");
            return;
        }
        auto cchTestText = strlen(pszTestText);

        // Set our increment value for the loop.
        // 0 represents the special case of feeding the whole string in at at time.
        // Otherwise, we try different segment sizes to ensure preservation across calls
        // for appropriate handling of DBCS and UTF-8 sequences.
        const size_t cchIncrement = dwIncrement == 0 ? cchTestText : dwIncrement;

        for (size_t i = 0; i < cchTestText; i += cchIncrement)
        {
            Log::Comment(WEX::Common::String().Format(L"Iteration %d of loop with increment %d", i, cchIncrement));
            s_AdjustOutputWait(fInduceWait);

            size_t cchRead = 0;

            // The increment is either the specified length or the remaining text in the string (if that is smaller).
            const auto cchWriteLength = std::min(cchIncrement, cchTestText - i);

            // Run the test method
            const auto hr = _pApiRoutines->WriteConsoleAImpl(si, { pszTestText + i, cchWriteLength }, cchRead, nullptr);

            if (!fInduceWait)
            {
                VERIFY_ARE_EQUAL(S_OK, hr);
                VERIFY_ARE_EQUAL(cchWriteLength, cchRead, L"We should have the same character count back as 'written' that we gave in.");
            }
            else
            {
                VERIFY_ARE_EQUAL(CONSOLE_STATUS_WAIT, hr);
            }
        }
    }

    TEST_METHOD(ApiWriteConsoleW)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:fInduceWait", L"{false, true}")
        END_TEST_METHOD_PROPERTIES();

        bool fInduceWait;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"fInduceWait", fInduceWait), L"Get whether or not we should exercise this function off a wait state.");

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& si = gci.GetActiveOutputBuffer();

        gci.LockConsole();
        auto Unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

        const std::wstring_view testText(L"Test text");

        s_AdjustOutputWait(fInduceWait);

        size_t cchRead = 0;
        const auto hr = _pApiRoutines->WriteConsoleWImpl(si, testText, cchRead, nullptr);

        if (!fInduceWait)
        {
            VERIFY_ARE_EQUAL(S_OK, hr);
            VERIFY_ARE_EQUAL(testText.size(), cchRead, L"We should have the same character count back as 'written' that we gave in.");
        }
        else
        {
            VERIFY_ARE_EQUAL(CONSOLE_STATUS_WAIT, hr);
        }
    }

    void ValidateScreen(SCREEN_INFORMATION& si,
                        const CHAR_INFO background,
                        const CHAR_INFO fill,
                        const til::point delta,
                        const std::optional<Viewport> clip)
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& activeSi = si.GetActiveBuffer();
        auto bufferSize = activeSi.GetBufferSize();

        // Find the background area viewport by taking the size, translating it by the delta, then cropping it back to the buffer size.
        auto backgroundArea = Viewport::Offset(bufferSize, delta);
        bufferSize.Clamp(backgroundArea);

        auto it = activeSi.GetCellDataAt({ 0, 0 }); // We're going to walk the whole thing. Start in the top left corner.

        while (it)
        {
            if (backgroundArea.IsInBounds(it._pos) ||
                (clip.has_value() && !clip.value().IsInBounds(it._pos)))
            {
                auto cellInfo = gci.AsCharInfo(*it);
                VERIFY_ARE_EQUAL(background, cellInfo);
            }
            else
            {
                VERIFY_ARE_EQUAL(fill, gci.AsCharInfo(*it));
            }
            it++;
        }
    }

    void ValidateComplexScreen(SCREEN_INFORMATION& si,
                               const CHAR_INFO background,
                               const CHAR_INFO fill,
                               const CHAR_INFO scroll,
                               const Viewport scrollArea,
                               const til::point destPoint,
                               const std::optional<Viewport> clip)
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& activeSi = si.GetActiveBuffer();
        auto bufferSize = activeSi.GetBufferSize();

        // Find the delta by comparing the scroll area to the destination point
        til::point delta;
        delta.x = destPoint.x - scrollArea.Left();
        delta.y = destPoint.y - scrollArea.Top();

        // Find the area where the scroll text should have gone by taking the scrolled area by the delta
        auto scrolledDestination = Viewport::Offset(scrollArea, delta);
        bufferSize.Clamp(scrolledDestination);

        auto it = activeSi.GetCellDataAt({ 0, 0 }); // We're going to walk the whole thing. Start in the top left corner.

        while (it)
        {
            // If there's no clip rectangle...
            if (!clip.has_value())
            {
                // Three states.
                // 1. We filled the background with something (background CHAR_INFO)
                // 2. We filled another smaller area with a different something (scroll CHAR_INFO)
                // 3. We moved #2 by delta and the uncovered area was filled with a third something (fill CHAR_INFO)

                // If it's in the scrolled destination, it's the value that just got moved.
                if (scrolledDestination.IsInBounds(it._pos))
                {
                    VERIFY_ARE_EQUAL(scroll, gci.AsCharInfo(*it));
                }
                // Otherwise, if it's not in the destination but it was in the source, assume it got filled in.
                else if (scrollArea.IsInBounds(it._pos))
                {
                    VERIFY_ARE_EQUAL(fill, gci.AsCharInfo(*it));
                }
                // Lastly if it's not in either spot, it should have our background CHAR_INFO
                else
                {
                    VERIFY_ARE_EQUAL(background, gci.AsCharInfo(*it));
                }
            }
            // If there is a clip rectangle...
            else
            {
                const auto unboxedClip = clip.value();

                if (unboxedClip.IsInBounds(it._pos))
                {
                    if (scrolledDestination.IsInBounds(it._pos))
                    {
                        VERIFY_ARE_EQUAL(scroll, gci.AsCharInfo(*it));
                    }
                    else if (scrollArea.IsInBounds(it._pos))
                    {
                        VERIFY_ARE_EQUAL(fill, gci.AsCharInfo(*it));
                    }
                    else
                    {
                        VERIFY_ARE_EQUAL(background, gci.AsCharInfo(*it));
                    }
                }
                else
                {
                    if (scrollArea.IsInBounds(it._pos))
                    {
                        VERIFY_ARE_EQUAL(scroll, gci.AsCharInfo(*it));
                    }
                    else
                    {
                        VERIFY_ARE_EQUAL(background, gci.AsCharInfo(*it));
                    }
                }
            }

            // Move to next iterator position and check.
            it++;
        }
    }

    TEST_METHOD(ApiScrollConsoleScreenBufferW)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"data:setMargins", L"{false, true}")
            TEST_METHOD_PROPERTY(L"data:checkClipped", L"{false, true}")
        END_TEST_METHOD_PROPERTIES();

        bool setMargins;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"setMargins", setMargins), L"Get whether or not we should set the DECSTBM margins.");
        bool checkClipped;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"checkClipped", checkClipped), L"Get whether or not we should check all the options using a clipping rectangle.");

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& si = gci.GetActiveOutputBuffer();

        si.GetTextBuffer().ResizeTraditional({ 5, 5 });

        // Tests are run both with and without the DECSTBM margins set. This should not alter
        // the results, since ScrollConsoleScreenBuffer should not be affected by VT margins.
        auto& stateMachine = si.GetStateMachine();
        stateMachine.ProcessString(setMargins ? L"\x1b[2;4r" : L"\x1b[r");
        // Make sure we clear the margins on exit so they can't break other tests.
        auto clearMargins = wil::scope_exit([&] { stateMachine.ProcessString(L"\x1b[r"); });

        gci.LockConsole();
        auto Unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

        CHAR_INFO fill;
        fill.Char.UnicodeChar = L'A';
        fill.Attributes = FOREGROUND_RED;

        // By default, we're going to use a nullopt clip rectangle.
        // If this instance of the test is checking clipping, we'll assign a clip value
        // prior to each call variation.
        std::optional<til::inclusive_rect> clipRectangle = std::nullopt;
        std::optional<Viewport> clipViewport = std::nullopt;
        const auto bufferSize = si.GetBufferSize();

        auto scroll = bufferSize.ToInclusive();
        til::point destination{ 0, -2 }; // scroll up.

        Log::Comment(L"Fill screen with green Zs. Scroll all up by two, backfilling with red As. Confirm every cell.");
        si.GetActiveBuffer().ClearTextData(); // Clean out screen

        CHAR_INFO background;
        background.Char.UnicodeChar = L'Z';
        background.Attributes = FOREGROUND_GREEN;

        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.

        if (checkClipped)
        {
            // for scrolling up and down, we're going to clip to only modify the left half of the buffer
            auto clipRectDimensions = bufferSize.Dimensions();
            clipRectDimensions.width /= 2;

            clipViewport = Viewport::FromDimensions({ 0, 0 }, clipRectDimensions);
            clipRectangle = clipViewport.value().ToInclusive();
        }

        // Scroll everything up and backfill with red As.
        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, fill.Char.UnicodeChar, fill.Attributes));
        ValidateScreen(si, background, fill, destination, clipViewport);

        Log::Comment(L"Fill screen with green Zs. Scroll all down by two, backfilling with red As. Confirm every cell.");

        si.GetActiveBuffer().ClearTextData(); // Clean out screen
        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.

        // Scroll everything down and backfill with red As.
        destination = { 0, 2 };
        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, fill.Char.UnicodeChar, fill.Attributes));
        ValidateScreen(si, background, fill, destination, clipViewport);

        if (checkClipped)
        {
            // for scrolling left and right, we're going to clip to only modify the top half of the buffer
            auto clipRectDimensions = bufferSize.Dimensions();
            clipRectDimensions.height /= 2;

            clipViewport = Viewport::FromDimensions({ 0, 0 }, clipRectDimensions);
            clipRectangle = clipViewport.value().ToInclusive();
        }

        Log::Comment(L"Fill screen with green Zs. Scroll all left by two, backfilling with red As. Confirm every cell.");

        si.GetActiveBuffer().ClearTextData(); // Clean out screen
        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.

        // Scroll everything left and backfill with red As.
        destination = { -2, 0 };
        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, fill.Char.UnicodeChar, fill.Attributes));
        ValidateScreen(si, background, fill, destination, clipViewport);

        Log::Comment(L"Fill screen with green Zs. Scroll all right by two, backfilling with red As. Confirm every cell.");

        si.GetActiveBuffer().ClearTextData(); // Clean out screen
        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.

        // Scroll everything right and backfill with red As.
        destination = { 2, 0 };
        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, fill.Char.UnicodeChar, fill.Attributes));
        ValidateScreen(si, background, fill, destination, clipViewport);

        Log::Comment(L"Fill screen with green Zs. Move everything down and right by two, backfilling with red As. Confirm every cell.");

        si.GetActiveBuffer().ClearTextData(); // Clean out screen
        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.

        // Scroll everything down and right and backfill with red As.
        destination = { 2, 2 };
        if (checkClipped)
        {
            // Clip out the left most and top most column.
            clipViewport = Viewport::FromDimensions({ 1, 1 }, { 4, 4 });
            clipRectangle = clipViewport.value().ToInclusive();
        }
        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, fill.Char.UnicodeChar, fill.Attributes));
        ValidateScreen(si, background, fill, destination, clipViewport);

        Log::Comment(L"Fill screen with green Zs. Move everything up and left by two, backfilling with red As. Confirm every cell.");

        si.GetActiveBuffer().ClearTextData(); // Clean out screen
        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.

        // Scroll everything up and left and backfill with red As.
        destination = { -2, -2 };
        if (checkClipped)
        {
            // Clip out the bottom most and right most column
            clipViewport = Viewport::FromDimensions({ 0, 0 }, { 4, 4 });
            clipRectangle = clipViewport.value().ToInclusive();
        }
        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, fill.Char.UnicodeChar, fill.Attributes));
        ValidateScreen(si, background, fill, destination, clipViewport);

        Log::Comment(L"Scroll everything completely off the screen.");

        si.GetActiveBuffer().ClearTextData(); // Clean out screen
        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.

        // Scroll everything way off the screen.
        destination = { 0, -10 };
        if (checkClipped)
        {
            // for scrolling up and down, we're going to clip to only modify the left half of the buffer
            auto clipRectDimensions = bufferSize.Dimensions();
            clipRectDimensions.width /= 2;

            clipViewport = Viewport::FromDimensions({ 0, 0 }, clipRectDimensions);
            clipRectangle = clipViewport.value().ToInclusive();
        }
        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, fill.Char.UnicodeChar, fill.Attributes));
        ValidateScreen(si, background, fill, destination, clipViewport);

        Log::Comment(L"Scroll everything completely off the screen but use a null fill and confirm it is replaced with default attribute spaces.");

        si.GetActiveBuffer().ClearTextData(); // Clean out screen
        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.
        // Scroll everything way off the screen.
        destination = { -10, -10 };

        CHAR_INFO nullFill = { 0 };

        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, nullFill.Char.UnicodeChar, nullFill.Attributes));

        CHAR_INFO fillExpected;
        fillExpected.Char.UnicodeChar = UNICODE_SPACE;
        fillExpected.Attributes = si.GetAttributes().GetLegacyAttributes();
        ValidateScreen(si, background, fillExpected, destination, clipViewport);

        if (checkClipped)
        {
            // If we're doing clipping here, we're going to clip the scrolled area (after Bs are filled onto field of Zs)
            // to only the 3rd and 4th columns of the pattern.
            clipViewport = Viewport::FromDimensions({ 2, 0 }, { 2, 5 });
            clipRectangle = clipViewport.value().ToInclusive();
        }

        Log::Comment(L"Scroll a small portion of the screen in an overlapping fashion.");
        scroll.top = 1;
        scroll.bottom = 2;
        scroll.left = 1;
        scroll.right = 2;

        si.GetActiveBuffer().ClearTextData(); // Clean out screen
        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.

        // Screen now looks like:
        // ZZZZZ
        // ZZZZZ
        // ZZZZZ
        // ZZZZZ
        // ZZZZZ

        // Fill the scroll rectangle with Blue Bs.
        CHAR_INFO scrollRect;
        scrollRect.Char.UnicodeChar = L'B';
        scrollRect.Attributes = FOREGROUND_BLUE;
        si.GetActiveBuffer().WriteRect(OutputCellIterator(scrollRect), Viewport::FromInclusive(scroll));

        // Screen now looks like:
        // ZZZZZ
        // ZBBZZ
        // ZBBZZ
        // ZZZZZ
        // ZZZZZ

        // We're going to move our little embedded rectangle of Blue Bs inside the field of Green Zs down and to the right just one.
        destination = { scroll.left + 1, scroll.top + 1 };

        // Move rectangle and backfill with red As.
        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, fill.Char.UnicodeChar, fill.Attributes));

        // Screen should now look like either:
        // (with no clip rectangle):
        // ZZZZZ
        // ZAAZZ
        // ZABBZ
        // ZZBBZ
        // ZZZZZ
        // or with clip rectangle (of 3rd and 4th columns only, defined above)
        // ZZZZZ
        // ZBAZZ
        // ZBBBZ
        // ZZBBZ
        // ZZZZZ

        ValidateComplexScreen(si, background, fill, scrollRect, Viewport::FromInclusive(scroll), destination, clipViewport);

        Log::Comment(L"Scroll a small portion of the screen in a non-overlapping fashion.");

        si.GetActiveBuffer().ClearTextData(); // Clean out screen
        si.GetActiveBuffer().Write(OutputCellIterator(background), { 0, 0 }); // Fill entire screen with green Zs.

        // Screen now looks like:
        // ZZZZZ
        // ZZZZZ
        // ZZZZZ
        // ZZZZZ
        // ZZZZZ

        // Fill the scroll rectangle with Blue Bs.
        si.GetActiveBuffer().WriteRect(OutputCellIterator(scrollRect), Viewport::FromInclusive(scroll));

        // Screen now looks like:
        // ZZZZZ
        // ZBBZZ
        // ZBBZZ
        // ZZZZZ
        // ZZZZZ

        // We're going to move our little embedded rectangle of Blue Bs inside the field of Green Zs down and to the right by two.
        destination = { scroll.left + 2, scroll.top + 2 };

        // Move rectangle and backfill with red As.
        VERIFY_SUCCEEDED(_pApiRoutines->ScrollConsoleScreenBufferWImpl(si, scroll, destination, clipRectangle, fill.Char.UnicodeChar, fill.Attributes));

        // Screen should now look like either:
        // (with no clip rectangle):
        // ZZZZZ
        // ZAAZZ
        // ZAAZZ
        // ZZZBB
        // ZZZBB
        // or with clip rectangle (of 3rd and 4th columns only, defined above)
        // ZZZZZ
        // ZBAZZ
        // ZBAZZ
        // ZZZBZ
        // ZZZBZ

        ValidateComplexScreen(si, background, fill, scrollRect, Viewport::FromInclusive(scroll), destination, clipViewport);
    }
};
