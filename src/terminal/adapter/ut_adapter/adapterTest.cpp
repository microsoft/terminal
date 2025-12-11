// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <wextestclass.h>
#include "../../inc/consoletaeftemplates.hpp"
#include "../../parser/OutputStateMachineEngine.hpp"
#include "../../../renderer/inc/DummyRenderer.hpp"

#include "adaptDispatch.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class AdapterTest;
            class ConAdapterTestGetSet;
        };
    };
};

enum class CursorY
{
    TOP,
    BOTTOM,
    YCENTER
};

enum class CursorX
{
    LEFT,
    RIGHT,
    XCENTER
};

enum class CursorDirection : Microsoft::Console::VirtualTerminal::VTInt
{
    UP = 0,
    DOWN = 1,
    RIGHT = 2,
    LEFT = 3,
    NEXTLINE = 4,
    PREVLINE = 5
};

enum class AbsolutePosition : Microsoft::Console::VirtualTerminal::VTInt
{
    CursorHorizontal = 0,
    VerticalLine = 1,
};

using namespace Microsoft::Console::VirtualTerminal;

class TestGetSet final : public ITerminalApi
{
public:
    void ReturnResponse(const std::wstring_view response) override
    {
        Log::Comment(L"ReturnResponse MOCK called...");

        THROW_HR_IF(E_FAIL, !_returnResponseResult);

        if (_retainResponse)
        {
            _response += response;
        }
        else
        {
            _response = response;
        }
    }

    StateMachine& GetStateMachine() override
    {
        return *_stateMachine;
    }

    BufferState GetBufferAndViewport() override
    {
        const auto viewport = til::rect{ _viewport.left, _viewport.top, _viewport.right, _viewport.bottom };
        return { *_textBuffer.get(), viewport, true };
    }

    void SetViewportPosition(const til::point /*position*/) override
    {
        Log::Comment(L"SetViewportPosition MOCK called...");
    }

    bool IsVtInputEnabled() const override
    {
        return false;
    }

    void SetSystemMode(const Mode mode, const bool enabled)
    {
        Log::Comment(L"SetSystemMode MOCK called...");
        _systemMode.set(mode, enabled);
    }

    bool GetSystemMode(const Mode mode) const
    {
        Log::Comment(L"GetSystemMode MOCK called...");
        return _systemMode.test(mode);
    }

    void ReturnAnswerback()
    {
        Log::Comment(L"ReturnAnswerback MOCK called...");
    }

    void WarningBell() override
    {
        Log::Comment(L"WarningBell MOCK called...");
    }

    void SetWindowTitle(const std::wstring_view title)
    {
        Log::Comment(L"SetWindowTitle MOCK called...");

        if (_setWindowTitleResult)
        {
            // Put into WEX strings for rich logging when they don't compare.
            VERIFY_ARE_EQUAL(String(_expectedWindowTitle.data(), gsl::narrow<int>(_expectedWindowTitle.size())),
                             String(title.data(), gsl::narrow<int>(title.size())));
        }
    }

    void UseAlternateScreenBuffer(const TextAttribute& /*attr*/) override
    {
        Log::Comment(L"UseAlternateScreenBuffer MOCK called...");
    }

    void UseMainScreenBuffer() override
    {
        Log::Comment(L"UseMainScreenBuffer MOCK called...");
    }

    CursorType GetUserDefaultCursorStyle() const override
    {
        return CursorType::Legacy;
    }

    void ShowWindow(bool showOrHide) override
    {
        Log::Comment(L"ShowWindow MOCK called...");
        VERIFY_ARE_EQUAL(_expectedShowWindow, showOrHide);
    }

    bool ResizeWindow(const til::CoordType /*width*/, const til::CoordType /*height*/) override
    {
        Log::Comment(L"ResizeWindow MOCK called...");
        return true;
    }

    void SetCodePage(const unsigned int codepage) override
    {
        Log::Comment(L"SetCodePage MOCK called...");
        THROW_HR_IF(E_FAIL, !_setCodePageResult);
        VERIFY_ARE_EQUAL(_expectedCodePage, codepage);
    }

    void ResetCodePage() override
    {
        Log::Comment(L"ResetCodePage MOCK called...");
    }

    unsigned int GetOutputCodePage() const override
    {
        Log::Comment(L"GetOutputCodePage MOCK called...");
        return _expectedCodePage;
    }

    unsigned int GetInputCodePage() const override
    {
        Log::Comment(L"GetInputCodePage MOCK called...");
        return _expectedCodePage;
    }

    void CopyToClipboard(const wil::zwstring_view /*content*/)
    {
        Log::Comment(L"CopyToClipboard MOCK called...");
    }

    void SetTaskbarProgress(const DispatchTypes::TaskbarState /*state*/, const size_t /*progress*/)
    {
        Log::Comment(L"SetTaskbarProgress MOCK called...");
    }

    void SetWorkingDirectory(const std::wstring_view /*uri*/)
    {
        Log::Comment(L"SetWorkingDirectory MOCK called...");
    }

    void PlayMidiNote(const int /*noteNumber*/, const int /*velocity*/, const std::chrono::microseconds /*duration*/) override
    {
        Log::Comment(L"PlayMidiNote MOCK called...");
    }

    void NotifyBufferRotation(const int /*delta*/) override
    {
        Log::Comment(L"NotifyBufferRotation MOCK called...");
    }

    void NotifyShellIntegrationMark() override
    {
        Log::Comment(L"NotifyShellIntegrationMark MOCK called...");
    }

    void InvokeCompletions(std::wstring_view menuJson, unsigned int replaceLength) override
    {
        Log::Comment(L"InvokeCompletions MOCK called...");
        VERIFY_ARE_EQUAL(_expectedMenuJson, menuJson);
        VERIFY_ARE_EQUAL(_expectedReplaceLength, replaceLength);
    }

    void SearchMissingCommand(const std::wstring_view /*command*/) override
    {
        Log::Comment(L"SearchMissingCommand MOCK called...");
    }

    void PrepData()
    {
        PrepData(CursorDirection::UP); // if called like this, the cursor direction doesn't matter.
    }

    void PrepData(CursorDirection dir)
    {
        switch (dir)
        {
        case CursorDirection::UP:
            return PrepData(CursorX::LEFT, CursorY::TOP);
        case CursorDirection::DOWN:
            return PrepData(CursorX::LEFT, CursorY::BOTTOM);
        case CursorDirection::LEFT:
            return PrepData(CursorX::LEFT, CursorY::TOP);
        case CursorDirection::RIGHT:
            return PrepData(CursorX::RIGHT, CursorY::TOP);
        case CursorDirection::NEXTLINE:
            return PrepData(CursorX::LEFT, CursorY::BOTTOM);
        case CursorDirection::PREVLINE:
            return PrepData(CursorX::LEFT, CursorY::TOP);
        }
    }

    void PrepData(CursorX xact, CursorY yact)
    {
        Log::Comment(L"Resetting mock data state.");

        // APIs succeed by default
        _returnResponseResult = TRUE;

        _textBuffer = std::make_unique<TextBuffer>(til::size{ 100, 600 }, TextAttribute{}, 0, false, &_renderer);

        // Viewport sitting in the "middle" of the buffer somewhere (so all sides have excess buffer around them)
        _viewport.top = 20;
        _viewport.bottom = 49;
        _viewport.left = 30;
        _viewport.right = 59;

        // Call cursor positions separately
        PrepCursor(xact, yact);

        // Attribute default is gray on black.
        _textBuffer->SetCurrentAttributes(TextAttribute{ FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED });
        _expectedAttribute = _textBuffer->GetCurrentAttributes();

        _response.clear();
        _retainResponse = false;
    }

    void PrepCursor(CursorX xact, CursorY yact)
    {
        Log::Comment(L"Adjusting cursor within viewport... Expected will match actual when done.");

        til::point cursorPos;
        const auto bufferSize = _textBuffer->GetSize().Dimensions();

        switch (xact)
        {
        case CursorX::LEFT:
            Log::Comment(L"Cursor set to left edge of buffer.");
            cursorPos.x = 0;
            break;
        case CursorX::RIGHT:
            Log::Comment(L"Cursor set to right edge of buffer.");
            cursorPos.x = bufferSize.width - 1;
            break;
        case CursorX::XCENTER:
            Log::Comment(L"Cursor set to centered X of buffer.");
            cursorPos.x = bufferSize.width / 2;
            break;
        }

        switch (yact)
        {
        case CursorY::TOP:
            Log::Comment(L"Cursor set to top edge of viewport.");
            cursorPos.y = _viewport.top;
            break;
        case CursorY::BOTTOM:
            Log::Comment(L"Cursor set to bottom edge of viewport.");
            cursorPos.y = _viewport.bottom - 1;
            break;
        case CursorY::YCENTER:
            Log::Comment(L"Cursor set to centered Y of viewport.");
            cursorPos.y = _viewport.top + ((_viewport.bottom - _viewport.top) / 2);
            break;
        }

        _textBuffer->GetCursor().SetPosition(cursorPos);
        _expectedCursorPos = cursorPos;
    }

    static void MakeSubParamsAndRanges(std::initializer_list<std::initializer_list<const VTParameter>> subParamList, _Out_ std::vector<VTParameter>& subParams, _Out_ std::vector<std::pair<BYTE, BYTE>>& subParamRanges)
    {
        // Args are a list of lists of VTParameters:
        //    { {P1S1, P1S2, P1S3, ... }, { P2S1, P2S2, P2S3, ... } ... }
        //
        // P1 and P2 denotes the parameters, while S1, S2, S3 denotes the
        // subparameters of the corresponding parameter.
        size_t totalSubParams = 0;
        subParams.clear();
        subParamRanges.clear();
        for (const auto& it : subParamList)
        {
            subParams.insert(subParams.end(), it.begin(), it.end());
            subParamRanges.push_back({ gsl::narrow_cast<BYTE>(totalSubParams), gsl::narrow_cast<BYTE>(it.size() + totalSubParams) });
            totalSubParams += it.size();
        }
    }

    void ValidateExpectedCursorPos()
    {
        VERIFY_ARE_EQUAL(_expectedCursorPos, _textBuffer->GetCursor().GetPosition());
    }

    void ValidateExpectedAttributes()
    {
        VERIFY_ARE_EQUAL(_expectedAttribute, _textBuffer->GetCurrentAttributes());
    }

    void ValidateInputEvent(_In_ PCWSTR pwszExpectedResponse)
    {
        VERIFY_ARE_EQUAL(pwszExpectedResponse, _response);
    }

    ~TestGetSet() = default;

    static const WCHAR s_wchErase = (WCHAR)0x20;
    static const WCHAR s_wchDefault = L'Z';
    static const WORD s_wAttrErase = FOREGROUND_BLUE | FOREGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY;
    static const WORD s_wDefaultAttribute = 0;
    static const WORD s_defaultFill = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED; // dark gray on black.

    std::wstring _response;
    bool _retainResponse{ false };

    auto EnableInputRetentionInScope()
    {
        auto oldRetainValue{ _retainResponse };
        _retainResponse = true;
        return wil::scope_exit([oldRetainValue, this] {
            _retainResponse = oldRetainValue;
        });
    }

    StateMachine* _stateMachine;
    DummyRenderer _renderer;
    std::unique_ptr<TextBuffer> _textBuffer;
    til::inclusive_rect _viewport;

    til::point _expectedCursorPos;

    TextAttribute _expectedAttribute = {};
    unsigned int _expectedCodePage = 0;
    bool _isPty = false;

    bool _returnResponseResult = false;

    til::enumset<Mode> _systemMode{ Mode::AutoWrap };

    bool _setWindowTitleResult = false;
    std::wstring_view _expectedWindowTitle{};
    bool _setCodePageResult = false;
    bool _expectedShowWindow = false;

    std::wstring _expectedMenuJson{};
    unsigned int _expectedReplaceLength = 0;

private:
    HANDLE _hCon;
};

class AdapterTest
{
public:
    TEST_CLASS(AdapterTest);

    TEST_METHOD_SETUP(SetupMethods)
    {
        auto fSuccess = true;

        auto api = std::make_unique<TestGetSet>();
        fSuccess = api.get() != nullptr;
        if (fSuccess)
        {
            _testGetSet = std::move(api);
            _terminalInput = TerminalInput{};
            auto& renderer = _testGetSet->_renderer;
            auto& renderSettings = renderer._renderSettings;
            auto adapter = std::make_unique<AdaptDispatch>(*_testGetSet, &renderer, renderSettings, _terminalInput);

            fSuccess = adapter.get() != nullptr;
            if (fSuccess)
            {
                _pDispatch = adapter.get();
                auto engine = std::make_unique<OutputStateMachineEngine>(std::move(adapter));
                _stateMachine = std::make_unique<StateMachine>(std::move(engine));
                _testGetSet->_stateMachine = _stateMachine.get();
            }
        }
        return fSuccess;
    }

    TEST_METHOD_CLEANUP(CleanupMethods)
    {
        _stateMachine.reset();
        _pDispatch = nullptr;
        _testGetSet = nullptr;
        return true;
    }

    TEST_METHOD(CursorMovementTest)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiDirection", L"{0, 1, 2, 3, 4, 5}") // These values align with the CursorDirection enum class to try all the directions.
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        // Used to switch between the various function options.
        typedef void (AdaptDispatch::*CursorMoveFunc)(VTInt);
        CursorMoveFunc moveFunc = nullptr;

        // Modify variables based on directionality of this test
        CursorDirection direction;
        size_t dir;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiDirection", dir));
        direction = (CursorDirection)dir;

        switch (direction)
        {
        case CursorDirection::UP:
            Log::Comment(L"Testing up direction.");
            moveFunc = &AdaptDispatch::CursorUp;
            break;
        case CursorDirection::DOWN:
            Log::Comment(L"Testing down direction.");
            moveFunc = &AdaptDispatch::CursorDown;
            break;
        case CursorDirection::RIGHT:
            Log::Comment(L"Testing right direction.");
            moveFunc = &AdaptDispatch::CursorForward;
            break;
        case CursorDirection::LEFT:
            Log::Comment(L"Testing left direction.");
            moveFunc = &AdaptDispatch::CursorBackward;
            break;
        case CursorDirection::NEXTLINE:
            Log::Comment(L"Testing next line direction.");
            moveFunc = &AdaptDispatch::CursorNextLine;
            break;
        case CursorDirection::PREVLINE:
            Log::Comment(L"Testing prev line direction.");
            moveFunc = &AdaptDispatch::CursorPrevLine;
            break;
        }

        if (moveFunc == nullptr)
        {
            VERIFY_FAIL();
            return;
        }

        // success cases
        // place cursor in top left. moving up is expected to go nowhere (it should get bounded by the viewport)
        Log::Comment(L"Test 1: Cursor doesn't move when placed in corner of viewport.");
        _testGetSet->PrepData(direction);

        (_pDispatch->*moveFunc)(1);
        _testGetSet->ValidateExpectedCursorPos();

        Log::Comment(L"Test 1b: Cursor moves to left of line with next/prev line command when cursor can't move higher/lower.");

        auto fDoTest1b = false;

        switch (direction)
        {
        case CursorDirection::NEXTLINE:
            _testGetSet->PrepData(CursorX::RIGHT, CursorY::BOTTOM);
            fDoTest1b = true;
            break;
        case CursorDirection::PREVLINE:
            _testGetSet->PrepData(CursorX::RIGHT, CursorY::TOP);
            fDoTest1b = true;
            break;
        }

        if (fDoTest1b)
        {
            _testGetSet->_expectedCursorPos.x = 0;
            (_pDispatch->*moveFunc)(1);
            _testGetSet->ValidateExpectedCursorPos();
        }
        else
        {
            Log::Comment(L"Test not applicable to direction selected. Skipping.");
        }

        // place cursor lower, move up 1.
        Log::Comment(L"Test 2: Cursor moves 1 in the correct direction from viewport.");
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

        switch (direction)
        {
        case CursorDirection::UP:
            _testGetSet->_expectedCursorPos.y--;
            break;
        case CursorDirection::DOWN:
            _testGetSet->_expectedCursorPos.y++;
            break;
        case CursorDirection::RIGHT:
            _testGetSet->_expectedCursorPos.x++;
            break;
        case CursorDirection::LEFT:
            _testGetSet->_expectedCursorPos.x--;
            break;
        case CursorDirection::NEXTLINE:
            _testGetSet->_expectedCursorPos.y++;
            _testGetSet->_expectedCursorPos.x = 0;
            break;
        case CursorDirection::PREVLINE:
            _testGetSet->_expectedCursorPos.y--;
            _testGetSet->_expectedCursorPos.x = 0;
            break;
        }

        (_pDispatch->*moveFunc)(1);
        _testGetSet->ValidateExpectedCursorPos();

        // place cursor and move it up too far. It should get bounded by the viewport.
        Log::Comment(L"Test 3: Cursor moves and gets stuck at viewport when started away from edges and moved beyond edges.");
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

        // Bottom and right viewports are -1 because those two sides are specified to be 1 outside the viewable area.

        switch (direction)
        {
        case CursorDirection::UP:
            _testGetSet->_expectedCursorPos.y = _testGetSet->_viewport.top;
            break;
        case CursorDirection::DOWN:
            _testGetSet->_expectedCursorPos.y = _testGetSet->_viewport.bottom - 1;
            break;
        case CursorDirection::RIGHT:
            _testGetSet->_expectedCursorPos.x = _testGetSet->_textBuffer->GetSize().Dimensions().width - 1;
            break;
        case CursorDirection::LEFT:
            _testGetSet->_expectedCursorPos.x = 0;
            break;
        case CursorDirection::NEXTLINE:
            _testGetSet->_expectedCursorPos.x = 0;
            _testGetSet->_expectedCursorPos.y = _testGetSet->_viewport.bottom - 1;
            break;
        case CursorDirection::PREVLINE:
            _testGetSet->_expectedCursorPos.x = 0;
            _testGetSet->_expectedCursorPos.y = _testGetSet->_viewport.top;
            break;
        }

        (_pDispatch->*moveFunc)(100);
        _testGetSet->ValidateExpectedCursorPos();
    }

    TEST_METHOD(CursorPositionTest)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Place cursor within the viewport. Start from top left, move to middle.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        auto sCol = (_testGetSet->_viewport.right - _testGetSet->_viewport.left) / 2;
        auto sRow = (_testGetSet->_viewport.bottom - _testGetSet->_viewport.top) / 2;

        // The X coordinate is unaffected by the viewport.
        _testGetSet->_expectedCursorPos.x = sCol - 1;
        _testGetSet->_expectedCursorPos.y = _testGetSet->_viewport.top + (sRow - 1);

        _pDispatch->CursorPosition(sRow, sCol);
        _testGetSet->ValidateExpectedCursorPos();

        Log::Comment(L"Test 2: Move to 0, 0 (which is 1,1 in VT speak)");
        _testGetSet->PrepData(CursorX::RIGHT, CursorY::BOTTOM);

        // The X coordinate is unaffected by the viewport.
        _testGetSet->_expectedCursorPos.x = 0;
        _testGetSet->_expectedCursorPos.y = _testGetSet->_viewport.top;

        _pDispatch->CursorPosition(1, 1);
        _testGetSet->ValidateExpectedCursorPos();

        Log::Comment(L"Test 3: Move beyond rectangle (down/right too far). Should be bounded back in.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        sCol = (_testGetSet->_textBuffer->GetSize().Dimensions().width) * 2;
        sRow = (_testGetSet->_viewport.bottom - _testGetSet->_viewport.top) * 2;

        _testGetSet->_expectedCursorPos.x = _testGetSet->_textBuffer->GetSize().Dimensions().width - 1;
        _testGetSet->_expectedCursorPos.y = _testGetSet->_viewport.bottom - 1;

        _pDispatch->CursorPosition(sRow, sCol);
        _testGetSet->ValidateExpectedCursorPos();
    }

    TEST_METHOD(CursorSingleDimensionMoveTest)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiDirection", L"{0, 1}") // These values align with the CursorDirection enum class to try all the directions.
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        //// Used to switch between the various function options.
        typedef void (AdaptDispatch::*CursorMoveFunc)(VTInt);
        CursorMoveFunc moveFunc = nullptr;
        auto sRangeEnd = 0;
        auto sRangeStart = 0;
        til::CoordType* psCursorExpected = nullptr;

        // Modify variables based on directionality of this test
        AbsolutePosition direction;
        size_t dir;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiDirection", dir));
        direction = (AbsolutePosition)dir;
        _testGetSet->PrepData();

        switch (direction)
        {
        case AbsolutePosition::CursorHorizontal:
            Log::Comment(L"Testing cursor horizontal movement.");
            sRangeEnd = _testGetSet->_textBuffer->GetSize().Dimensions().width;
            sRangeStart = 0;
            psCursorExpected = &_testGetSet->_expectedCursorPos.x;
            moveFunc = &AdaptDispatch::CursorHorizontalPositionAbsolute;
            break;
        case AbsolutePosition::VerticalLine:
            Log::Comment(L"Testing vertical line movement.");
            sRangeEnd = _testGetSet->_viewport.bottom;
            sRangeStart = _testGetSet->_viewport.top;
            psCursorExpected = &_testGetSet->_expectedCursorPos.y;
            moveFunc = &AdaptDispatch::VerticalLinePositionAbsolute;
            break;
        }

        if (moveFunc == nullptr || psCursorExpected == nullptr)
        {
            VERIFY_FAIL();
            return;
        }

        Log::Comment(L"Test 1: Place cursor within the viewport. Start from top left, move to middle.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        auto sVal = (sRangeEnd - sRangeStart) / 2;

        *psCursorExpected = sRangeStart + (sVal - 1);

        (_pDispatch->*moveFunc)(sVal);
        _testGetSet->ValidateExpectedCursorPos();

        Log::Comment(L"Test 2: Move to 0 (which is 1 in VT speak)");
        _testGetSet->PrepData(CursorX::RIGHT, CursorY::BOTTOM);

        *psCursorExpected = sRangeStart;
        sVal = 1;

        (_pDispatch->*moveFunc)(sVal);
        _testGetSet->ValidateExpectedCursorPos();

        Log::Comment(L"Test 3: Move beyond rectangle (down/right too far). Should be bounded back in.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        sVal = (sRangeEnd - sRangeStart) * 2;

        *psCursorExpected = sRangeEnd - 1;

        (_pDispatch->*moveFunc)(sVal);
        _testGetSet->ValidateExpectedCursorPos();
    }

    TEST_METHOD(CursorSaveRestoreTest)
    {
        Log::Comment(L"Starting test...");

        til::point coordExpected;

        Log::Comment(L"Test 1: Restore with no saved data should move to top-left corner, the null/default position.");

        // Move cursor to top left and save off expected position.
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);
        coordExpected = _testGetSet->_expectedCursorPos;

        // Then move cursor to the middle and reset the expected to the top left.
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);
        _testGetSet->_expectedCursorPos = coordExpected;

        // Attributes are restored to defaults.
        _testGetSet->_expectedAttribute = {};

        _pDispatch->CursorRestoreState();
        _testGetSet->ValidateExpectedCursorPos();
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 2: Place cursor in center. Save. Move cursor to corner. Restore. Should come back to center.");
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);
        _pDispatch->CursorSaveState();
        _testGetSet->ValidateExpectedCursorPos();

        Log::Comment(L"Backup expected cursor (in the middle). Move cursor to corner. Then re-set expected cursor to middle.");
        // save expected cursor position
        coordExpected = _testGetSet->_expectedCursorPos;

        // adjust cursor to corner
        _testGetSet->PrepData(CursorX::LEFT, CursorY::BOTTOM);

        // restore expected cursor position to center.
        _testGetSet->_expectedCursorPos = coordExpected;

        _pDispatch->CursorRestoreState();
        _testGetSet->ValidateExpectedCursorPos();
        _testGetSet->ValidateExpectedAttributes();
    }

    TEST_METHOD(CursorHideShowTest)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:fStartingVis", L"{TRUE, FALSE}")
            TEST_METHOD_PROPERTY(L"Data:fEndingVis", L"{TRUE, FALSE}")
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        // Modify variables based on permutations of this test.
        bool fStart;
        bool fEnd;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"fStartingVis", fStart));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"fEndingVis", fEnd));

        Log::Comment(L"Verify successful API call modifies visibility state.");
        _testGetSet->PrepData();
        _testGetSet->_textBuffer->GetCursor().SetIsVisible(fStart);
        if (fEnd)
        {
            _pDispatch->SetMode(DispatchTypes::DECTCEM_TextCursorEnableMode);
        }
        else
        {
            _pDispatch->ResetMode(DispatchTypes::DECTCEM_TextCursorEnableMode);
        }
        VERIFY_ARE_EQUAL(fEnd, _testGetSet->_textBuffer->GetCursor().IsVisible());
    }

    TEST_METHOD(GraphicsBaseTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Send no options.");

        _testGetSet->PrepData();

        VTParameter rgOptions[16];
        size_t cOptions = 0;

        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();
    }

    TEST_METHOD(GraphicsSingleTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiGraphicsOptions", L"{0, 1, 2, 4, 7, 8, 9, 21, 22, 24, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 53, 55, 90, 91, 92, 93, 94, 95, 96, 97, 100, 101, 102, 103, 104, 105, 106, 107}") // corresponds to options in DispatchTypes::GraphicsOptions
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");
        _testGetSet->PrepData();

        // Modify variables based on type of this test
        DispatchTypes::GraphicsOptions graphicsOption;
        size_t uiGraphicsOption;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiGraphicsOptions", uiGraphicsOption));
        graphicsOption = (DispatchTypes::GraphicsOptions)uiGraphicsOption;

        VTParameter rgOptions[16];
        size_t cOptions = 1;
        rgOptions[0] = graphicsOption;

        TextAttribute startingAttribute;
        switch (graphicsOption)
        {
        case DispatchTypes::GraphicsOptions::Off:
            Log::Comment(L"Testing graphics 'Off/Reset'");
            startingAttribute = TextAttribute{ (WORD)~_testGetSet->s_defaultFill };
            _testGetSet->_expectedAttribute = TextAttribute{};
            break;
        case DispatchTypes::GraphicsOptions::Intense:
            Log::Comment(L"Testing graphics 'Intense'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetIntense(true);
            break;
        case DispatchTypes::GraphicsOptions::RGBColorOrFaint:
            Log::Comment(L"Testing graphics 'Faint'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetFaint(true);
            break;
        case DispatchTypes::GraphicsOptions::Underline:
            Log::Comment(L"Testing graphics 'Underline'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::SinglyUnderlined);
            break;
        case DispatchTypes::GraphicsOptions::DoublyUnderlined:
            Log::Comment(L"Testing graphics 'Doubly Underlined'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
            break;
        case DispatchTypes::GraphicsOptions::Overline:
            Log::Comment(L"Testing graphics 'Overline'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ COMMON_LVB_GRID_HORIZONTAL };
            break;
        case DispatchTypes::GraphicsOptions::Negative:
            Log::Comment(L"Testing graphics 'Negative'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ COMMON_LVB_REVERSE_VIDEO };
            break;
        case DispatchTypes::GraphicsOptions::Invisible:
            Log::Comment(L"Testing graphics 'Invisible'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetInvisible(true);
            break;
        case DispatchTypes::GraphicsOptions::CrossedOut:
            Log::Comment(L"Testing graphics 'Crossed Out'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetCrossedOut(true);
            break;
        case DispatchTypes::GraphicsOptions::NotIntenseOrFaint:
            Log::Comment(L"Testing graphics 'No Intense or Faint'");
            startingAttribute = TextAttribute{ 0 };
            startingAttribute.SetIntense(true);
            startingAttribute.SetFaint(true);
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            break;
        case DispatchTypes::GraphicsOptions::NoUnderline:
            Log::Comment(L"Testing graphics 'No Underline'");
            startingAttribute = TextAttribute{ 0 };
            startingAttribute.SetUnderlineStyle(UnderlineStyle::CurlyUnderlined);
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            break;
        case DispatchTypes::GraphicsOptions::NoOverline:
            Log::Comment(L"Testing graphics 'No Overline'");
            startingAttribute = TextAttribute{ COMMON_LVB_GRID_HORIZONTAL };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            break;
        case DispatchTypes::GraphicsOptions::Positive:
            Log::Comment(L"Testing graphics 'Positive'");
            startingAttribute = TextAttribute{ COMMON_LVB_REVERSE_VIDEO };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            break;
        case DispatchTypes::GraphicsOptions::Visible:
            Log::Comment(L"Testing graphics 'Visible'");
            startingAttribute = TextAttribute{ 0 };
            startingAttribute.SetInvisible(true);
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            break;
        case DispatchTypes::GraphicsOptions::NotCrossedOut:
            Log::Comment(L"Testing graphics 'Not Crossed Out'");
            startingAttribute = TextAttribute{ 0 };
            startingAttribute.SetCrossedOut(true);
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            break;
        case DispatchTypes::GraphicsOptions::ForegroundBlack:
            Log::Comment(L"Testing graphics 'Foreground Color Black'");
            startingAttribute = TextAttribute{ FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_BLACK);
            break;
        case DispatchTypes::GraphicsOptions::ForegroundBlue:
            Log::Comment(L"Testing graphics 'Foreground Color Blue'");
            startingAttribute = TextAttribute{ FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_BLUE);
            break;
        case DispatchTypes::GraphicsOptions::ForegroundGreen:
            Log::Comment(L"Testing graphics 'Foreground Color Green'");
            startingAttribute = TextAttribute{ FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_GREEN);
            break;
        case DispatchTypes::GraphicsOptions::ForegroundCyan:
            Log::Comment(L"Testing graphics 'Foreground Color Cyan'");
            startingAttribute = TextAttribute{ FOREGROUND_RED | FOREGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_CYAN);
            break;
        case DispatchTypes::GraphicsOptions::ForegroundRed:
            Log::Comment(L"Testing graphics 'Foreground Color Red'");
            startingAttribute = TextAttribute{ FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_RED);
            break;
        case DispatchTypes::GraphicsOptions::ForegroundMagenta:
            Log::Comment(L"Testing graphics 'Foreground Color Magenta'");
            startingAttribute = TextAttribute{ FOREGROUND_GREEN | FOREGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_MAGENTA);
            break;
        case DispatchTypes::GraphicsOptions::ForegroundYellow:
            Log::Comment(L"Testing graphics 'Foreground Color Yellow'");
            startingAttribute = TextAttribute{ FOREGROUND_BLUE | FOREGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_YELLOW);
            break;
        case DispatchTypes::GraphicsOptions::ForegroundWhite:
            Log::Comment(L"Testing graphics 'Foreground Color White'");
            startingAttribute = TextAttribute{ FOREGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_WHITE);
            break;
        case DispatchTypes::GraphicsOptions::ForegroundDefault:
            Log::Comment(L"Testing graphics 'Foreground Color Default'");
            startingAttribute = TextAttribute{ (WORD)~_testGetSet->s_wDefaultAttribute }; // set the current attribute to the opposite of default so we can ensure all relevant bits flip.
            // To get expected value, take what we started with and change ONLY the background series of bits to what the Default says.
            _testGetSet->_expectedAttribute = startingAttribute; // expect = starting
            _testGetSet->_expectedAttribute.SetDefaultForeground(); // set the foreground as default
            break;
        case DispatchTypes::GraphicsOptions::BackgroundBlack:
            Log::Comment(L"Testing graphics 'Background Color Black'");
            startingAttribute = TextAttribute{ BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_BLACK);
            break;
        case DispatchTypes::GraphicsOptions::BackgroundBlue:
            Log::Comment(L"Testing graphics 'Background Color Blue'");
            startingAttribute = TextAttribute{ BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_BLUE);
            break;
        case DispatchTypes::GraphicsOptions::BackgroundGreen:
            Log::Comment(L"Testing graphics 'Background Color Green'");
            startingAttribute = TextAttribute{ BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_GREEN);
            break;
        case DispatchTypes::GraphicsOptions::BackgroundCyan:
            Log::Comment(L"Testing graphics 'Background Color Cyan'");
            startingAttribute = TextAttribute{ BACKGROUND_RED | BACKGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_CYAN);
            break;
        case DispatchTypes::GraphicsOptions::BackgroundRed:
            Log::Comment(L"Testing graphics 'Background Color Red'");
            startingAttribute = TextAttribute{ BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_RED);
            break;
        case DispatchTypes::GraphicsOptions::BackgroundMagenta:
            Log::Comment(L"Testing graphics 'Background Color Magenta'");
            startingAttribute = TextAttribute{ BACKGROUND_GREEN | BACKGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_MAGENTA);
            break;
        case DispatchTypes::GraphicsOptions::BackgroundYellow:
            Log::Comment(L"Testing graphics 'Background Color Yellow'");
            startingAttribute = TextAttribute{ BACKGROUND_BLUE | BACKGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_YELLOW);
            break;
        case DispatchTypes::GraphicsOptions::BackgroundWhite:
            Log::Comment(L"Testing graphics 'Background Color White'");
            startingAttribute = TextAttribute{ BACKGROUND_INTENSITY };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_WHITE);
            break;
        case DispatchTypes::GraphicsOptions::BackgroundDefault:
            Log::Comment(L"Testing graphics 'Background Color Default'");
            startingAttribute = TextAttribute{ (WORD)~_testGetSet->s_wDefaultAttribute }; // set the current attribute to the opposite of default so we can ensure all relevant bits flip.
            // To get expected value, take what we started with and change ONLY the background series of bits to what the Default says.
            _testGetSet->_expectedAttribute = startingAttribute; // expect = starting
            _testGetSet->_expectedAttribute.SetDefaultBackground(); // set the background as default
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundBlack:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Black'");
            startingAttribute = TextAttribute{ FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_BLACK);
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundBlue:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Blue'");
            startingAttribute = TextAttribute{ FOREGROUND_RED | FOREGROUND_GREEN };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_BLUE);
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundGreen:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Green'");
            startingAttribute = TextAttribute{ FOREGROUND_RED | FOREGROUND_BLUE };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_GREEN);
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundCyan:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Cyan'");
            startingAttribute = TextAttribute{ FOREGROUND_RED };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_CYAN);
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundRed:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Red'");
            startingAttribute = TextAttribute{ FOREGROUND_BLUE | FOREGROUND_GREEN };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_RED);
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundMagenta:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Magenta'");
            startingAttribute = TextAttribute{ FOREGROUND_GREEN };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_MAGENTA);
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundYellow:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Yellow'");
            startingAttribute = TextAttribute{ FOREGROUND_BLUE };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_YELLOW);
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundWhite:
            Log::Comment(L"Testing graphics 'Bright Foreground Color White'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_WHITE);
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundBlack:
            Log::Comment(L"Testing graphics 'Bright Background Color Black'");
            startingAttribute = TextAttribute{ BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::BRIGHT_BLACK);
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundBlue:
            Log::Comment(L"Testing graphics 'Bright Background Color Blue'");
            startingAttribute = TextAttribute{ BACKGROUND_RED | BACKGROUND_GREEN };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::BRIGHT_BLUE);
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundGreen:
            Log::Comment(L"Testing graphics 'Bright Background Color Green'");
            startingAttribute = TextAttribute{ BACKGROUND_RED | BACKGROUND_BLUE };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::BRIGHT_GREEN);
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundCyan:
            Log::Comment(L"Testing graphics 'Bright Background Color Cyan'");
            startingAttribute = TextAttribute{ BACKGROUND_RED };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::BRIGHT_CYAN);
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundRed:
            Log::Comment(L"Testing graphics 'Bright Background Color Red'");
            startingAttribute = TextAttribute{ BACKGROUND_BLUE | BACKGROUND_GREEN };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::BRIGHT_RED);
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundMagenta:
            Log::Comment(L"Testing graphics 'Bright Background Color Magenta'");
            startingAttribute = TextAttribute{ BACKGROUND_GREEN };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::BRIGHT_MAGENTA);
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundYellow:
            Log::Comment(L"Testing graphics 'Bright Background Color Yellow'");
            startingAttribute = TextAttribute{ BACKGROUND_BLUE };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::BRIGHT_YELLOW);
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundWhite:
            Log::Comment(L"Testing graphics 'Bright Background Color White'");
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = startingAttribute;
            _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::BRIGHT_WHITE);
            break;
        default:
            VERIFY_FAIL(L"Test not implemented yet!");
            break;
        }

        _testGetSet->_textBuffer->SetCurrentAttributes(startingAttribute);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();
    }

    TEST_METHOD(GraphicsSingleWithSubParamTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiGraphicsOptions", L"{4, 38, 48, 58}") // corresponds to options in DispatchTypes::GraphicsOptions
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");
        _testGetSet->PrepData();

        // Modify variables based on type of this test
        DispatchTypes::GraphicsOptions graphicsOption;
        std::vector<VTParameter> subParams;
        std::vector<std::pair<BYTE, BYTE>> subParamRanges;
        size_t uiGraphicsOption;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiGraphicsOptions", uiGraphicsOption));
        graphicsOption = (DispatchTypes::GraphicsOptions)uiGraphicsOption;

        VTParameter rgOptions[16];
        size_t cOptions = 1;
        rgOptions[0] = graphicsOption;

        TextAttribute startingAttribute;
        switch (graphicsOption)
        {
        case DispatchTypes::GraphicsOptions::Underline:
            Log::Comment(L"Testing graphics 'Underline'");
            _testGetSet->MakeSubParamsAndRanges({ { 3 } }, subParams, subParamRanges);
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::CurlyUnderlined);
            break;
        case DispatchTypes::GraphicsOptions::ForegroundExtended:
            Log::Comment(L"Testing graphics 'ForegroundExtended'");
            _testGetSet->MakeSubParamsAndRanges({ { DispatchTypes::GraphicsOptions::BlinkOrXterm256Index, TextColor::DARK_RED } }, subParams, subParamRanges);
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetIndexedForeground256(TextColor::DARK_RED);
            break;
        case DispatchTypes::GraphicsOptions::BackgroundExtended:
            Log::Comment(L"Testing graphics 'BackgroundExtended'");
            _testGetSet->MakeSubParamsAndRanges({ { DispatchTypes::GraphicsOptions::BlinkOrXterm256Index, TextColor::BRIGHT_WHITE } }, subParams, subParamRanges);
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetIndexedBackground256(TextColor::BRIGHT_WHITE);
            break;
        case DispatchTypes::GraphicsOptions::UnderlineColor:
            Log::Comment(L"Testing graphics 'UnderlineColor'");
            _testGetSet->MakeSubParamsAndRanges({ { DispatchTypes::GraphicsOptions::BlinkOrXterm256Index, TextColor::DARK_RED } }, subParams, subParamRanges);
            startingAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute = TextAttribute{ 0 };
            _testGetSet->_expectedAttribute.SetUnderlineColor({ TextColor::DARK_RED, true });
            break;
        default:
            VERIFY_FAIL(L"Test not implemented yet!");
            break;
        }
        _testGetSet->_textBuffer->SetCurrentAttributes(startingAttribute);
        _pDispatch->SetGraphicsRendition({ std::span{ rgOptions, cOptions }, subParams, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();
    }

    TEST_METHOD(GraphicsPushPopTests)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED

        VTParameter rgOptions[16];
        std::vector<VTParameter> subParams;
        std::vector<std::pair<BYTE, BYTE>> subParamRanges;
        VTParameter rgStackOptions[16];
        size_t cOptions = 1;

        Log::Comment(L"Test 1: Basic push and pop");

        rgOptions[0] = DispatchTypes::GraphicsOptions::Off;
        _testGetSet->_expectedAttribute = {};
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        cOptions = 0;
        _pDispatch->PushGraphicsRendition({ rgStackOptions, cOptions });

        _pDispatch->PopGraphicsRendition();
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 2: Push, change color, pop");

        _pDispatch->PushGraphicsRendition({ rgStackOptions, cOptions });

        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundCyan;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_CYAN);
        _testGetSet->_expectedAttribute.SetDefaultBackground();
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        cOptions = 0;
        _testGetSet->_expectedAttribute = {};
        _pDispatch->PopGraphicsRendition();
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 3: two pushes (nested) and pops");

        // First push:
        _pDispatch->PushGraphicsRendition({ rgStackOptions, cOptions });

        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundRed;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_RED);
        _testGetSet->_expectedAttribute.SetDefaultBackground();
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        // Second push:
        cOptions = 0;
        _pDispatch->PushGraphicsRendition({ rgStackOptions, cOptions });

        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundGreen;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_GREEN);
        _testGetSet->_expectedAttribute.SetDefaultBackground();
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        // First pop:
        cOptions = 0;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_RED);
        _testGetSet->_expectedAttribute.SetDefaultBackground();
        _pDispatch->PopGraphicsRendition();
        _testGetSet->ValidateExpectedAttributes();

        // Second pop:
        cOptions = 0;
        _testGetSet->_expectedAttribute = {};
        _pDispatch->PopGraphicsRendition();
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 4: Save and restore partial attributes");

        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundGreen;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_GREEN);
        _testGetSet->_expectedAttribute.SetDefaultBackground();
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::Intense;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_GREEN);
        _testGetSet->_expectedAttribute.SetIntense(true);
        _testGetSet->_expectedAttribute.SetDefaultBackground();
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundBlue;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_GREEN);
        _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_BLUE);
        _testGetSet->_expectedAttribute.SetIntense(true);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        // Push, specifying that we only want to save the background, the intensity, and double-underline-ness:
        cOptions = 3;
        rgStackOptions[0] = (size_t)DispatchTypes::SgrSaveRestoreStackOptions::Intense;
        rgStackOptions[1] = (size_t)DispatchTypes::SgrSaveRestoreStackOptions::SaveBackgroundColor;
        rgStackOptions[2] = (size_t)DispatchTypes::SgrSaveRestoreStackOptions::DoublyUnderlined;
        _pDispatch->PushGraphicsRendition({ rgStackOptions, cOptions });

        // Now change everything...
        cOptions = 2;
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundGreen;
        rgOptions[1] = DispatchTypes::GraphicsOptions::DoublyUnderlined;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_GREEN);
        _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_GREEN);
        _testGetSet->_expectedAttribute.SetIntense(true);
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundRed;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_RED);
        _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_GREEN);
        _testGetSet->_expectedAttribute.SetIntense(true);
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        rgOptions[0] = DispatchTypes::GraphicsOptions::NotIntenseOrFaint;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_RED);
        _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_GREEN);
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        // And then restore...
        cOptions = 0;
        _testGetSet->_expectedAttribute = {};
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_RED);
        _testGetSet->_expectedAttribute.SetIndexedBackground(TextColor::DARK_BLUE);
        _testGetSet->_expectedAttribute.SetIntense(true);
        _pDispatch->PopGraphicsRendition();
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 5: Save 'no singly underline' state, set singly underlined, and pop. "
                     L"Singly underlined is off after the pop.");

        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::NoUnderline;
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::NoUnderline);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        // save 'no underlined' state
        cOptions = 1;
        rgStackOptions[0] = (size_t)DispatchTypes::SgrSaveRestoreStackOptions::Underline;
        _pDispatch->PushGraphicsRendition({ rgStackOptions, cOptions });

        // set underlined
        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::Underline;
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::SinglyUnderlined);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        // restore, expect no underline
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::NoUnderline);
        _pDispatch->PopGraphicsRendition();
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 6: Save 'no singly underlined' state, set doubly underlined, and pop. "
                     L"Doubly underlined is retained after the pop.");

        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::NoUnderline;
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::NoUnderline);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        // save no underline state
        cOptions = 1;
        rgStackOptions[0] = (size_t)DispatchTypes::SgrSaveRestoreStackOptions::Underline;
        _pDispatch->PushGraphicsRendition({ rgStackOptions, cOptions });

        // set doubly underlined
        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::DoublyUnderlined;
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        // restore, expect doubly underlined
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        _pDispatch->PopGraphicsRendition();
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 7: Save 'curly underlined' state, set doubly underlined, and pop. "
                     L"Curly underlined is restored after the pop.");

        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::Underline;
        _testGetSet->MakeSubParamsAndRanges({ { 3 } }, subParams, subParamRanges);
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::CurlyUnderlined);
        _pDispatch->SetGraphicsRendition({ std::span{ rgOptions, cOptions }, subParams, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();

        // save curly underlined state
        cOptions = 1;
        rgStackOptions[0] = (size_t)DispatchTypes::SgrSaveRestoreStackOptions::Underline;
        _pDispatch->PushGraphicsRendition({ rgStackOptions, cOptions });

        // set doubly underlined
        cOptions = 1;
        rgOptions[0] = DispatchTypes::GraphicsOptions::DoublyUnderlined;
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        // restore, expect curly underlined
        _testGetSet->_expectedAttribute.SetUnderlineStyle(UnderlineStyle::CurlyUnderlined);
        _pDispatch->PopGraphicsRendition();
        _testGetSet->ValidateExpectedAttributes();
    }

    TEST_METHOD(GraphicsPersistBrightnessTests)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED

        VTParameter rgOptions[16];
        size_t cOptions = 1;

        Log::Comment(L"Test 1: Basic brightness test");
        Log::Comment(L"Resetting graphics options");
        rgOptions[0] = DispatchTypes::GraphicsOptions::Off;
        _testGetSet->_expectedAttribute = {};
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Testing graphics 'Foreground Color Blue'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundBlue;
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_BLUE);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Enabling brightness");
        rgOptions[0] = DispatchTypes::GraphicsOptions::Intense;
        _testGetSet->_expectedAttribute.SetIntense(true);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_TRUE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Testing graphics 'Foreground Color Green, with brightness'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundGreen;
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_GREEN);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_TRUE(WI_IsFlagSet(_testGetSet->_textBuffer->GetCurrentAttributes().GetLegacyAttributes(), FOREGROUND_GREEN));
        VERIFY_IS_TRUE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 2: Disable brightness, use a bright color, next normal call remains not bright");
        Log::Comment(L"Resetting graphics options");
        rgOptions[0] = DispatchTypes::GraphicsOptions::Off;
        _testGetSet->_expectedAttribute = {};
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_TRUE(WI_IsFlagClear(_testGetSet->_textBuffer->GetCurrentAttributes().GetLegacyAttributes(), FOREGROUND_INTENSITY));
        VERIFY_IS_FALSE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Testing graphics 'Foreground Color Bright Blue'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BrightForegroundBlue;
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_BLUE);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_FALSE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Testing graphics 'Foreground Color Blue', brightness of 9x series doesn't persist");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundBlue;
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_BLUE);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_FALSE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 3: Enable brightness, use a bright color, brightness persists to next normal call");
        Log::Comment(L"Resetting graphics options");
        rgOptions[0] = DispatchTypes::GraphicsOptions::Off;
        _testGetSet->_expectedAttribute = {};
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_FALSE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Testing graphics 'Foreground Color Blue'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundBlue;
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_BLUE);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_FALSE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Enabling brightness");
        rgOptions[0] = DispatchTypes::GraphicsOptions::Intense;
        _testGetSet->_expectedAttribute.SetIntense(true);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_TRUE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Testing graphics 'Foreground Color Bright Blue'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BrightForegroundBlue;
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::BRIGHT_BLUE);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_TRUE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Testing graphics 'Foreground Color Blue, with brightness', brightness of 9x series doesn't affect brightness");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundBlue;
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_BLUE);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_TRUE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Testing graphics 'Foreground Color Green, with brightness'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundGreen;
        _testGetSet->_expectedAttribute.SetIndexedForeground(TextColor::DARK_GREEN);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        VERIFY_IS_TRUE(_testGetSet->_textBuffer->GetCurrentAttributes().IsIntense());
        _testGetSet->ValidateExpectedAttributes();
    }

    TEST_METHOD(DeviceStatus_OperatingStatusTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify good operating condition.");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::OperatingStatus, {});

        _testGetSet->ValidateInputEvent(L"\x1b[0n");
    }

    TEST_METHOD(DeviceStatus_CursorPositionReportTests)
    {
        Log::Comment(L"Starting test...");

        {
            Log::Comment(L"Test 1: Verify normal cursor response position.");
            _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

            // start with the cursor position in the buffer.
            til::point coordCursorExpected{ _testGetSet->_textBuffer->GetCursor().GetPosition() };

            // to get to VT, we have to adjust it to its position relative to the viewport top.
            coordCursorExpected.y -= _testGetSet->_viewport.top;

            // Then note that VT is 1,1 based for the top left, so add 1. (The rest of the console uses 0,0 for array index bases.)
            coordCursorExpected.x++;
            coordCursorExpected.y++;

            _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::CursorPositionReport, {});

            wchar_t pwszBuffer[50];

            swprintf_s(pwszBuffer, ARRAYSIZE(pwszBuffer), L"\x1b[%d;%dR", coordCursorExpected.y, coordCursorExpected.x);
            _testGetSet->ValidateInputEvent(pwszBuffer);
        }

        {
            Log::Comment(L"Test 2: Verify multiple CPRs with a cursor move between them");
            _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

            // enable retention so that the two DSR responses don't delete each other
            auto retentionScope{ _testGetSet->EnableInputRetentionInScope() };

            // start with the cursor position in the buffer.
            til::point coordCursorExpectedFirst{ _testGetSet->_textBuffer->GetCursor().GetPosition() };

            // to get to VT, we have to adjust it to its position relative to the viewport top.
            coordCursorExpectedFirst -= til::point{ 0, _testGetSet->_viewport.top };

            // Then note that VT is 1,1 based for the top left, so add 1. (The rest of the console uses 0,0 for array index bases.)
            coordCursorExpectedFirst += til::point{ 1, 1 };

            _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::CursorPositionReport, {});

            auto cursorPos = _testGetSet->_textBuffer->GetCursor().GetPosition();
            cursorPos.x++;
            cursorPos.y++;
            _testGetSet->_textBuffer->GetCursor().SetPosition(cursorPos);

            auto coordCursorExpectedSecond{ coordCursorExpectedFirst };
            coordCursorExpectedSecond += til::point{ 1, 1 };

            _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::CursorPositionReport, {});

            wchar_t pwszBuffer[50];

            swprintf_s(pwszBuffer, ARRAYSIZE(pwszBuffer), L"\x1b[%d;%dR\x1b[%d;%dR", coordCursorExpectedFirst.y, coordCursorExpectedFirst.x, coordCursorExpectedSecond.y, coordCursorExpectedSecond.x);
            _testGetSet->ValidateInputEvent(pwszBuffer);
        }
    }

    TEST_METHOD(DeviceStatus_ExtendedCursorPositionReportTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify extended cursor position report.");
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

        // start with the cursor position in the buffer.
        til::point coordCursorExpected{ _testGetSet->_textBuffer->GetCursor().GetPosition() };

        // to get to VT, we have to adjust it to its position relative to the viewport top.
        coordCursorExpected.y -= _testGetSet->_viewport.top;

        // Then note that VT is 1,1 based for the top left, so add 1. (The rest of the console uses 0,0 for array index bases.)
        coordCursorExpected.x++;
        coordCursorExpected.y++;

        // By default, the initial page number should be 1.
        auto pageExpected = 1;

        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::ExtendedCursorPositionReport, {});

        wchar_t pwszBuffer[50];
        swprintf_s(pwszBuffer, ARRAYSIZE(pwszBuffer), L"\x1b[?%d;%d;%dR", coordCursorExpected.y, coordCursorExpected.x, pageExpected);
        _testGetSet->ValidateInputEvent(pwszBuffer);

        // Now test with the page number set to 3.
        pageExpected = 3;
        _pDispatch->PagePositionAbsolute(pageExpected);

        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::ExtendedCursorPositionReport, {});

        swprintf_s(pwszBuffer, ARRAYSIZE(pwszBuffer), L"\x1b[?%d;%d;%dR", coordCursorExpected.y, coordCursorExpected.x, pageExpected);
        _testGetSet->ValidateInputEvent(pwszBuffer);
    }

    TEST_METHOD(DeviceStatus_MacroSpaceReportTest)
    {
        Log::Comment(L"Starting test...");

        // Space is measured in blocks of 16 bytes.
        const auto availableSpace = MacroBuffer::MAX_SPACE / 16;

        Log::Comment(L"Test 1: Verify maximum space available");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::MacroSpaceReport, {});

        wchar_t pwszBuffer[50];
        swprintf_s(pwszBuffer, ARRAYSIZE(pwszBuffer), L"\x1b[%zu*{", availableSpace);
        _testGetSet->ValidateInputEvent(pwszBuffer);

        Log::Comment(L"Test 2: Verify space decrease");
        _testGetSet->PrepData();
        // Define four 8-byte macros, i.e. 32 byes (2 macro blocks).
        _stateMachine->ProcessString(L"\033P1;0;0!z12345678\033\\");
        _stateMachine->ProcessString(L"\033P2;0;0!z12345678\033\\");
        _stateMachine->ProcessString(L"\033P3;0;0!z12345678\033\\");
        _stateMachine->ProcessString(L"\033P4;0;0!z12345678\033\\");
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::MacroSpaceReport, {});

        swprintf_s(pwszBuffer, ARRAYSIZE(pwszBuffer), L"\x1b[%zu*{", availableSpace - 2);
        _testGetSet->ValidateInputEvent(pwszBuffer);

        Log::Comment(L"Test 3: Verify space reset");
        _testGetSet->PrepData();
        _pDispatch->HardReset();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::MacroSpaceReport, {});

        swprintf_s(pwszBuffer, ARRAYSIZE(pwszBuffer), L"\x1b[%zu*{", availableSpace);
        _testGetSet->ValidateInputEvent(pwszBuffer);
    }

    TEST_METHOD(DeviceStatus_MemoryChecksumReportTest)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify initial checksum is 0");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::MemoryChecksum, 12);

        _testGetSet->ValidateInputEvent(L"\033P12!~0000\033\\");

        Log::Comment(L"Test 2: Verify checksum after macros defined");
        _testGetSet->PrepData();
        // Define a couple of text macros
        _stateMachine->ProcessString(L"\033P1;0;0!zABCD\033\\");
        _stateMachine->ProcessString(L"\033P2;0;0!zabcd\033\\");
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::MemoryChecksum, 34);

        // Checksum is a 16-bit negated sum of the macro buffer characters.
        const auto checksum = gsl::narrow_cast<uint16_t>(-('A' + 'B' + 'C' + 'D' + 'a' + 'b' + 'c' + 'd'));
        wchar_t pwszBuffer[50];
        swprintf_s(pwszBuffer, ARRAYSIZE(pwszBuffer), L"\033P34!~%04X\033\\", checksum);
        _testGetSet->ValidateInputEvent(pwszBuffer);

        Log::Comment(L"Test 3: Verify checksum resets to 0");
        _testGetSet->PrepData();
        _pDispatch->HardReset();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::MemoryChecksum, 56);

        _testGetSet->ValidateInputEvent(L"\033P56!~0000\033\\");
    }

    TEST_METHOD(DeviceStatus_PrivateStatusTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify printer is not connected.");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::PrinterStatus, {});
        _testGetSet->ValidateInputEvent(L"\x1b[?13n");

        Log::Comment(L"Test 2: Verify UDKs are not supported.");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::UserDefinedKeys, {});
        _testGetSet->ValidateInputEvent(L"\x1b[?23n");

        Log::Comment(L"Test 3: Verify PC keyboard with unknown dialect.");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::KeyboardStatus, {});
        _testGetSet->ValidateInputEvent(L"\x1b[?27;0;0;5n");

        Log::Comment(L"Test 4: Verify locator is not connected.");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::LocatorStatus, {});
        _testGetSet->ValidateInputEvent(L"\x1b[?53n");

        Log::Comment(L"Test 5: Verify locator type is unknown.");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::LocatorIdentity, {});
        _testGetSet->ValidateInputEvent(L"\x1b[?57;0n");

        Log::Comment(L"Test 6: Verify terminal is ready.");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::DataIntegrity, {});
        _testGetSet->ValidateInputEvent(L"\x1b[?70n");

        Log::Comment(L"Test 7: Verify multiple sessions are not supported.");
        _testGetSet->PrepData();
        _pDispatch->DeviceStatusReport(DispatchTypes::StatusType::MultipleSessionStatus, {});
        _testGetSet->ValidateInputEvent(L"\x1b[?83n");
    }

    TEST_METHOD(DeviceAttributesTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify normal response.");
        _testGetSet->PrepData();
        _pDispatch->DeviceAttributes();
        _testGetSet->ValidateInputEvent(L"\x1b[?61;4;6;7;14;21;22;23;24;28;32;42;52c");

        Log::Comment(L"Test 2: Verify response with clipboard disabled.");
        _testGetSet->PrepData();
        _pDispatch->SetOptionalFeatures({});
        _pDispatch->DeviceAttributes();
        _testGetSet->ValidateInputEvent(L"\x1b[?61;4;6;7;14;21;22;23;24;28;32;42c");

        Log::Comment(L"Test 3: Verify failure when ReturnResponse doesn't work.");
        _testGetSet->PrepData();
        _testGetSet->_returnResponseResult = FALSE;

        VERIFY_THROWS(_pDispatch->DeviceAttributes(), std::exception);
    }

    TEST_METHOD(SecondaryDeviceAttributesTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify normal response.");
        _testGetSet->PrepData();
        _pDispatch->SecondaryDeviceAttributes();

        auto pwszExpectedResponse = L"\x1b[>0;10;1c";
        _testGetSet->ValidateInputEvent(pwszExpectedResponse);

        Log::Comment(L"Test 2: Verify failure when ReturnResponse doesn't work.");
        _testGetSet->PrepData();
        _testGetSet->_returnResponseResult = FALSE;

        VERIFY_THROWS(_pDispatch->SecondaryDeviceAttributes(), std::exception);
    }

    TEST_METHOD(TertiaryDeviceAttributesTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify normal response.");
        _testGetSet->PrepData();
        _pDispatch->TertiaryDeviceAttributes();

        auto pwszExpectedResponse = L"\x1bP!|00000000\x1b\\";
        _testGetSet->ValidateInputEvent(pwszExpectedResponse);

        Log::Comment(L"Test 2: Verify failure when ReturnResponse doesn't work.");
        _testGetSet->PrepData();
        _testGetSet->_returnResponseResult = FALSE;

        VERIFY_THROWS(_pDispatch->TertiaryDeviceAttributes(), std::exception);
    }

    TEST_METHOD(RequestDisplayedExtentTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify DECRQDE response in home position");
        _testGetSet->PrepData();
        _testGetSet->_viewport.left = 0;
        _testGetSet->_viewport.right = 80;
        _testGetSet->_viewport.top = 0;
        _testGetSet->_viewport.bottom = 24;
        _pDispatch->RequestDisplayedExtent();
        _testGetSet->ValidateInputEvent(L"\x1b[24;80;1;1;1\"w");

        Log::Comment(L"Test 2: Verify DECRQDE response when panned horizontally");
        _testGetSet->_viewport.left += 5;
        _testGetSet->_viewport.right += 5;
        _pDispatch->RequestDisplayedExtent();
        _testGetSet->ValidateInputEvent(L"\x1b[24;80;6;1;1\"w");

        Log::Comment(L"Test 3: Verify DECRQDE response on page 3");
        _pDispatch->PagePositionAbsolute(3);
        _pDispatch->RequestDisplayedExtent();
        _testGetSet->ValidateInputEvent(L"\x1b[24;80;6;1;3\"w");

        Log::Comment(L"Test 3: Verify DECRQDE response when active page not visible");
        _pDispatch->ResetMode(DispatchTypes::ModeParams::DECPCCM_PageCursorCouplingMode);
        _pDispatch->PagePositionAbsolute(1);
        _pDispatch->RequestDisplayedExtent();
        _testGetSet->ValidateInputEvent(L"\x1b[24;80;6;1;3\"w");

        Log::Comment(L"Test 4: Verify DECRQDE response when page 1 visible again");
        _pDispatch->SetMode(DispatchTypes::ModeParams::DECPCCM_PageCursorCouplingMode);
        _pDispatch->RequestDisplayedExtent();
        _testGetSet->ValidateInputEvent(L"\x1b[24;80;6;1;1\"w");
    }

    TEST_METHOD(RequestTerminalParametersTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify response for unsolicited permission.");
        _testGetSet->PrepData();
        _pDispatch->RequestTerminalParameters(DispatchTypes::ReportingPermission::Unsolicited);
        _testGetSet->ValidateInputEvent(L"\x1b[2;1;1;128;128;1;0x");

        Log::Comment(L"Test 2: Verify response for solicited permission.");
        _testGetSet->PrepData();
        _pDispatch->RequestTerminalParameters(DispatchTypes::ReportingPermission::Solicited);
        _testGetSet->ValidateInputEvent(L"\x1b[3;1;1;128;128;1;0x");

        Log::Comment(L"Test 3: Verify failure when ReturnResponse doesn't work.");
        _testGetSet->PrepData();
        _testGetSet->_returnResponseResult = FALSE;
        VERIFY_THROWS(_pDispatch->RequestTerminalParameters(DispatchTypes::ReportingPermission::Unsolicited), std::exception);
    }

    TEST_METHOD(RequestSettingsTests)
    {
        const auto requestSetting = [=](const std::wstring_view settingId = {}) {
            const auto stringHandler = _pDispatch->RequestSetting();
            for (auto ch : settingId)
            {
                stringHandler(ch);
            }
            stringHandler(L'\033'); // String terminator
        };

        Log::Comment(L"Requesting DECSTBM margins (5 to 10).");
        _testGetSet->PrepData();
        _pDispatch->SetTopBottomScrollingMargins(5, 10);
        requestSetting(L"r");
        _testGetSet->ValidateInputEvent(L"\033P1$r5;10r\033\\");

        Log::Comment(L"Requesting DECSTBM margins (full screen).");
        _testGetSet->PrepData();
        // Set screen height to 25 - this will be the expected margin range.
        _testGetSet->_viewport.bottom = _testGetSet->_viewport.top + 25;
        _pDispatch->SetTopBottomScrollingMargins(0, 0);
        requestSetting(L"r");
        _testGetSet->ValidateInputEvent(L"\033P1$r1;25r\033\\");

        Log::Comment(L"Requesting DECSLRM margins (5 to 10).");
        _testGetSet->PrepData();
        // We need to enable DECLRMM for horizontal margins to work.
        _pDispatch->SetMode(DispatchTypes::DECLRMM_LeftRightMarginMode);
        _pDispatch->SetLeftRightScrollingMargins(5, 10);
        requestSetting(L"s");
        _testGetSet->ValidateInputEvent(L"\033P1$r5;10s\033\\");

        Log::Comment(L"Requesting DECSLRM margins (full width).");
        _testGetSet->PrepData();
        _pDispatch->SetLeftRightScrollingMargins(0, 0);
        requestSetting(L"s");
        _testGetSet->ValidateInputEvent(L"\033P1$r1;100s\033\\");
        // Reset DECLRMM once we're done with horizontal margin testing.
        _pDispatch->ResetMode(DispatchTypes::DECLRMM_LeftRightMarginMode);

        Log::Comment(L"Requesting SGR attributes (default).");
        _testGetSet->PrepData();
        TextAttribute attribute = {};
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0m\033\\");

        Log::Comment(L"Requesting SGR attributes (intense, underlined, reversed).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetIntense(true);
        attribute.SetUnderlineStyle(UnderlineStyle::SinglyUnderlined);
        attribute.SetReverseVideo(true);
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0;1;4;7m\033\\");

        Log::Comment(L"Requesting SGR attributes (extended underline style).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetUnderlineStyle(UnderlineStyle::CurlyUnderlined);
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0;4:3m\033\\");

        Log::Comment(L"Requesting SGR attributes (faint, blinking, invisible).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetFaint(true);
        attribute.SetBlinking(true);
        attribute.SetInvisible(true);
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0;2;5;8m\033\\");

        Log::Comment(L"Requesting SGR attributes (italic, crossed-out).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetItalic(true);
        attribute.SetCrossedOut(true);
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0;3;9m\033\\");

        Log::Comment(L"Requesting SGR attributes (doubly underlined, overlined).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
        attribute.SetOverlined(true);
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0;21;53m\033\\");

        Log::Comment(L"Requesting SGR attributes (standard colors).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetIndexedForeground(TextColor::DARK_YELLOW);
        attribute.SetIndexedBackground(TextColor::DARK_CYAN);
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0;33;46m\033\\");

        Log::Comment(L"Requesting SGR attributes (AIX colors).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetIndexedForeground(TextColor::BRIGHT_CYAN);
        attribute.SetIndexedBackground(TextColor::BRIGHT_YELLOW);
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0;96;103m\033\\");

        Log::Comment(L"Requesting SGR attributes (ITU indexed colors).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetIndexedForeground256(123);
        attribute.SetIndexedBackground256(45);
        attribute.SetUnderlineColor(TextColor{ 128, true });
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0;38:5:123;48:5:45;58:5:128m\033\\");

        Log::Comment(L"Requesting SGR attributes (ITU RGB colors).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetForeground(RGB(12, 34, 56));
        attribute.SetBackground(RGB(65, 43, 21));
        attribute.SetUnderlineColor(RGB(128, 222, 45));
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"m");
        _testGetSet->ValidateInputEvent(L"\033P1$r0;38:2::12:34:56;48:2::65:43:21;58:2::128:222:45m\033\\");

        Log::Comment(L"Requesting DECSCUSR style (blinking block).");
        _testGetSet->PrepData();
        _testGetSet->_textBuffer->GetCursor().SetIsBlinking(true);
        _testGetSet->_textBuffer->GetCursor().SetType(CursorType::FullBox);
        requestSetting(L" q");
        _testGetSet->ValidateInputEvent(L"\033P1$r1 q\033\\");

        Log::Comment(L"Requesting DECSCUSR style (steady block).");
        _testGetSet->PrepData();
        _testGetSet->_textBuffer->GetCursor().SetIsBlinking(false);
        _testGetSet->_textBuffer->GetCursor().SetType(CursorType::FullBox);
        requestSetting(L" q");
        _testGetSet->ValidateInputEvent(L"\033P1$r2 q\033\\");

        Log::Comment(L"Requesting DECSCUSR style (blinking underline).");
        _testGetSet->PrepData();
        _testGetSet->_textBuffer->GetCursor().SetIsBlinking(true);
        _testGetSet->_textBuffer->GetCursor().SetType(CursorType::Underscore);
        requestSetting(L" q");
        _testGetSet->ValidateInputEvent(L"\033P1$r3 q\033\\");

        Log::Comment(L"Requesting DECSCUSR style (steady underline).");
        _testGetSet->PrepData();
        _testGetSet->_textBuffer->GetCursor().SetIsBlinking(false);
        _testGetSet->_textBuffer->GetCursor().SetType(CursorType::Underscore);
        requestSetting(L" q");
        _testGetSet->ValidateInputEvent(L"\033P1$r4 q\033\\");

        Log::Comment(L"Requesting DECSCUSR style (blinking bar).");
        _testGetSet->PrepData();
        _testGetSet->_textBuffer->GetCursor().SetIsBlinking(true);
        _testGetSet->_textBuffer->GetCursor().SetType(CursorType::VerticalBar);
        requestSetting(L" q");
        _testGetSet->ValidateInputEvent(L"\033P1$r5 q\033\\");

        Log::Comment(L"Requesting DECSCUSR style (steady bar).");
        _testGetSet->PrepData();
        _testGetSet->_textBuffer->GetCursor().SetIsBlinking(false);
        _testGetSet->_textBuffer->GetCursor().SetType(CursorType::VerticalBar);
        requestSetting(L" q");
        _testGetSet->ValidateInputEvent(L"\033P1$r6 q\033\\");

        Log::Comment(L"Requesting DECSCUSR style (non-standard).");
        _testGetSet->PrepData();
        _testGetSet->_textBuffer->GetCursor().SetIsBlinking(true);
        _testGetSet->_textBuffer->GetCursor().SetType(CursorType::Legacy);
        requestSetting(L" q");
        _testGetSet->ValidateInputEvent(L"\033P1$r0 q\033\\");

        Log::Comment(L"Requesting DECSCA attributes (unprotected).");
        _testGetSet->PrepData();
        attribute = {};
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"\"q");
        _testGetSet->ValidateInputEvent(L"\033P1$r0\"q\033\\");

        Log::Comment(L"Requesting DECSCA attributes (protected).");
        _testGetSet->PrepData();
        attribute = {};
        attribute.SetProtected(true);
        _testGetSet->_textBuffer->SetCurrentAttributes(attribute);
        requestSetting(L"\"q");
        _testGetSet->ValidateInputEvent(L"\033P1$r1\"q\033\\");

        // Initialize the color alias indices for the DECAC tests below.
        _testGetSet->PrepData();
        auto& renderSettings = _testGetSet->_renderer._renderSettings;
        renderSettings.SetColorAliasIndex(ColorAlias::DefaultForeground, 3);
        renderSettings.SetColorAliasIndex(ColorAlias::DefaultBackground, 5);
        renderSettings.SetColorAliasIndex(ColorAlias::FrameForeground, 4);
        renderSettings.SetColorAliasIndex(ColorAlias::FrameBackground, 6);

        Log::Comment(L"Requesting DECAC colors (default).");
        requestSetting(L",|");
        _testGetSet->ValidateInputEvent(L"\033P1$r1;3;5,|\033\\");

        Log::Comment(L"Requesting DECAC colors (normal text).");
        requestSetting(L"1,|");
        _testGetSet->ValidateInputEvent(L"\033P1$r1;3;5,|\033\\");

        Log::Comment(L"Requesting DECAC colors (window frame).");
        requestSetting(L"2,|");
        _testGetSet->ValidateInputEvent(L"\033P1$r2;4;6,|\033\\");

        Log::Comment(L"Requesting DECAC colors (invalid item).");
        requestSetting(L"3,|");
        _testGetSet->ValidateInputEvent(L"\033P0$r\033\\");

        Log::Comment(L"Requesting an unsupported setting.");
        _testGetSet->PrepData();
        requestSetting(L"x");
        _testGetSet->ValidateInputEvent(L"\033P0$r\033\\");
    }

    TEST_METHOD(RequestStandardModeTests)
    {
        // The mode numbers below correspond to the ANSIStandardMode values
        // in the ModeParams enum in DispatchTypes.hpp.

        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:modeNumber", L"{4, 20}")
        END_TEST_METHOD_PROPERTIES()

        VTInt modeNumber;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"modeNumber", modeNumber));
        const auto mode = DispatchTypes::ANSIStandardMode(modeNumber);

        // DISABLE_
        Log::Comment(NoThrowString().Format(L"Setting standard mode %d", modeNumber));
        _testGetSet->PrepData();
        _pDispatch->SetMode(mode);
        _pDispatch->RequestMode(mode);

        wchar_t expectedResponse[20];
        swprintf_s(expectedResponse, ARRAYSIZE(expectedResponse), L"\x1b[%d;1$y", modeNumber);
        _testGetSet->ValidateInputEvent(expectedResponse);

        Log::Comment(NoThrowString().Format(L"Resetting standard mode %d", modeNumber));
        _testGetSet->PrepData();
        _pDispatch->ResetMode(mode);
        _pDispatch->RequestMode(mode);

        swprintf_s(expectedResponse, ARRAYSIZE(expectedResponse), L"\x1b[%d;2$y", modeNumber);
        _testGetSet->ValidateInputEvent(expectedResponse);
    }

    TEST_METHOD(RequestPrivateModeTests)
    {
        // The mode numbers below correspond to the DECPrivateMode values
        // in the ModeParams enum in DispatchTypes.hpp. We don't include
        // AnsiMode (2), because once that's disabled we'd be in VT52 mode,
        // and DECRQM would not then be applicable.

        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:modeNumber", L"{1, 3, 5, 6, 7, 8, 12, 25, 40, 66, 67, 69, 117, 1000, 1002, 1003, 1004, 1005, 1006, 1007, 1049, 2004, 9001}")
        END_TEST_METHOD_PROPERTIES()

        VTInt modeNumber;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"modeNumber", modeNumber));
        const auto mode = DispatchTypes::DECPrivateMode(modeNumber);

        if (mode == DispatchTypes::DECCOLM_SetNumberOfColumns)
        {
            Log::Comment(L"Make sure DECCOLM is allowed");
            _pDispatch->SetMode(DispatchTypes::XTERM_EnableDECCOLMSupport);
        }

        Log::Comment(NoThrowString().Format(L"Setting private mode %d", modeNumber));
        _testGetSet->PrepData();
        _pDispatch->SetMode(mode);
        _pDispatch->RequestMode(mode);

        wchar_t expectedResponse[20];
        swprintf_s(expectedResponse, ARRAYSIZE(expectedResponse), L"\x1b[?%d;1$y", modeNumber);
        _testGetSet->ValidateInputEvent(expectedResponse);

        Log::Comment(NoThrowString().Format(L"Resetting private mode %d", modeNumber));
        _testGetSet->PrepData();
        _pDispatch->ResetMode(mode);
        _pDispatch->RequestMode(mode);

        swprintf_s(expectedResponse, ARRAYSIZE(expectedResponse), L"\x1b[?%d;2$y", modeNumber);
        _testGetSet->ValidateInputEvent(expectedResponse);
    }

    TEST_METHOD(RequestPermanentModeTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:modeNumber", L"{2027}")
        END_TEST_METHOD_PROPERTIES()

        VTInt modeNumber;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"modeNumber", modeNumber));
        const auto mode = DispatchTypes::DECPrivateMode(modeNumber);

        _testGetSet->PrepData();
        _pDispatch->ResetMode(mode); // as a test to ensure that it stays permanently enabled (= 3)
        _pDispatch->RequestMode(mode);

        wchar_t expectedResponse[20];
        swprintf_s(expectedResponse, ARRAYSIZE(expectedResponse), L"\x1b[?%d;3$y", modeNumber);
        _testGetSet->ValidateInputEvent(expectedResponse);
    }

    TEST_METHOD(RequestChecksumReportTests)
    {
        const auto requestChecksumReport = [this](const auto length) {
            wchar_t checksumQuery[30];
            swprintf_s(checksumQuery, ARRAYSIZE(checksumQuery), L"\033[99;1;1;1;1;%zu*y", length);
            _stateMachine->ProcessString(checksumQuery);
        };

        const auto verifyChecksumReport = [this](const auto checksum) {
            wchar_t expectedResponse[20];
            swprintf_s(expectedResponse, ARRAYSIZE(expectedResponse), L"\x1bP99!~%s\033\\", checksum);
            _testGetSet->ValidateInputEvent(expectedResponse);
        };

        const auto outputText = [&](const auto text) {
            _testGetSet->PrepData();
            _pDispatch->PrintString(text);
            requestChecksumReport(text.length());
        };

        const auto outputTextWithAttributes = [&](const auto text, const auto& attrCallback) {
            _testGetSet->PrepData();
            auto attr = TextAttribute{};
            attrCallback(attr);
            _testGetSet->_textBuffer->SetCurrentAttributes(attr);
            _pDispatch->PrintString(text);
            requestChecksumReport(text.length());
        };

        using namespace std::string_view_literals;

        _pDispatch->SetOptionalFeatures(ITermDispatch::OptionalFeature::ChecksumReport);

        Log::Comment(L"Test 1: ASCII characters");
        outputText(L"A"sv);
        verifyChecksumReport(L"FF4F");
        outputText(L" "sv);
        verifyChecksumReport(L"FF70");
        outputText(L"~"sv);
        verifyChecksumReport(L"FF12");
        outputText(L"ABC"sv);
        verifyChecksumReport(L"FDEA");

        Log::Comment(L"Test 2: Latin-1 characters");
        outputText(L"Á"sv);
        verifyChecksumReport(L"FECF");
        outputText(L"¡"sv);
        verifyChecksumReport(L"FEEF");
        outputText(L"ÿ"sv);
        verifyChecksumReport(L"FE91");
        outputText(L"ÁÂÃ"sv);
        verifyChecksumReport(L"FC6A");

        Log::Comment(L"Test 3: Rendition attributes");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetIntense(true);
        });
        verifyChecksumReport(L"FECF");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetUnderlineStyle(UnderlineStyle::SinglyUnderlined);
        });
        verifyChecksumReport(L"FF3F");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetBlinking(true);
        });
        verifyChecksumReport(L"FF0F");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetReverseVideo(true);
        });
        verifyChecksumReport(L"FF2F");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetInvisible(true);
        });
        verifyChecksumReport(L"FF47");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetIntense(true);
            attr.SetUnderlineStyle(UnderlineStyle::SinglyUnderlined);
            attr.SetReverseVideo(true);
        });
        verifyChecksumReport(L"FE9F");

        Log::Comment(L"Test 4: Selective erase");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetProtected(true);
        });
        verifyChecksumReport(L"FF4B");
        outputTextWithAttributes(L"B"sv, [](auto& attr) {
            attr.SetProtected(true);
        });
        verifyChecksumReport(L"FF4A");

        Log::Comment(L"Test 5: Color attributes");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetIndexedForeground(TextColor::DARK_RED);
        });
        verifyChecksumReport(L"FFAF");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetIndexedBackground(TextColor::DARK_GREEN);
        });
        verifyChecksumReport(L"FF4D");
        outputTextWithAttributes(L"A"sv, [](auto& attr) {
            attr.SetIndexedForeground(TextColor::DARK_YELLOW);
            attr.SetIndexedBackground(TextColor::DARK_BLUE);
        });
        verifyChecksumReport(L"FF8B");
    }

    TEST_METHOD(ColorTableReportTests)
    {
        _testGetSet->PrepData();

        // The test cases below are copied from the VT525 default color table,
        // but our color table holds many more values, so we're just filling the
        // remaining slots with black for the purposes of this test.
        auto& renderSettings = _testGetSet->_renderer._renderSettings;
        renderSettings.SetColorTableEntry(0, RGB(0, 0, 0));
        renderSettings.SetColorTableEntry(1, RGB(204, 36, 36));
        renderSettings.SetColorTableEntry(2, RGB(51, 204, 51));
        renderSettings.SetColorTableEntry(3, RGB(204, 204, 51));
        renderSettings.SetColorTableEntry(4, RGB(51, 51, 204));
        renderSettings.SetColorTableEntry(5, RGB(204, 51, 204));
        renderSettings.SetColorTableEntry(6, RGB(51, 204, 204));
        renderSettings.SetColorTableEntry(7, RGB(120, 120, 120));
        renderSettings.SetColorTableEntry(8, RGB(69, 69, 69));
        renderSettings.SetColorTableEntry(9, RGB(255, 0, 0));
        renderSettings.SetColorTableEntry(10, RGB(0, 255, 0));
        renderSettings.SetColorTableEntry(11, RGB(255, 255, 0));
        renderSettings.SetColorTableEntry(12, RGB(0, 0, 255));
        renderSettings.SetColorTableEntry(13, RGB(255, 0, 255));
        renderSettings.SetColorTableEntry(14, RGB(0, 255, 255));
        renderSettings.SetColorTableEntry(15, RGB(255, 255, 255));
        for (size_t i = 16; i < TextColor::TABLE_SIZE; i++)
        {
            renderSettings.SetColorTableEntry(i, RGB(0, 0, 0));
        }

        // Color table reports start with a DCS $s introducer with a parameter
        // value of 2, and end with an ST terminator.
        const auto DECCTR = L"\033P2$s";
        const auto ST = L"\033\\";

        Log::Comment(L"HLS color model");

        const auto hlsColorModel = static_cast<int>(DispatchTypes::ColorModel::HLS);
        _pDispatch->RequestTerminalStateReport(DispatchTypes::ReportFormat::ColorTableReport, hlsColorModel);

        std::wstring expectedResponse = DECCTR;
        // RGB(0,0,0) -> HLS(0°,0%,0%)
        expectedResponse += L"0;1;0;0;0/";
        // RGB(204,36,36) -> HLS(120°,47%,70%)
        expectedResponse += L"1;1;120;47;70/";
        // RGB(51,204,51) -> HLS(240°,50%,60%)
        expectedResponse += L"2;1;240;50;60/";
        // RGB(204,204,51) -> HLS(180°,50%,60%)
        expectedResponse += L"3;1;180;50;60/";
        // RGB(51,51,204) -> HLS(0°,50%,60%)
        expectedResponse += L"4;1;0;50;60/";
        // RGB(204,51,204) -> HLS(60°,50%,60%)
        expectedResponse += L"5;1;60;50;60/";
        // RGB(51,204,204) -> HLS(300°,50%,60%)
        expectedResponse += L"6;1;300;50;60/";
        // RGB(120,120,120) -> HLS(0°,47%,0%)
        expectedResponse += L"7;1;0;47;0/";
        // RGB(69,69,69) -> HLS(0°,27%,0%)
        expectedResponse += L"8;1;0;27;0/";
        // RGB(255,0,0) -> HLS(120°,50%,100%)
        expectedResponse += L"9;1;120;50;100/";
        // RGB(0,255,0) -> HLS(240°,50%,100%)
        expectedResponse += L"10;1;240;50;100/";
        // RGB(255,255,0) -> HLS(180°,50%,100%)
        expectedResponse += L"11;1;180;50;100/";
        // RGB(0,0,255) -> HLS(0°,50%,100%)
        expectedResponse += L"12;1;0;50;100/";
        // RGB(255,0,255) -> HLS(60°,50%,100%)
        expectedResponse += L"13;1;60;50;100/";
        // RGB(0,255,255) -> HLS(300°,50%,100%)
        expectedResponse += L"14;1;300;50;100/";
        // RGB(255,255,255) -> HLS(0°,100%,0%)
        expectedResponse += L"15;1;0;100;0/";
        // Remaining slots are black, i.e. HLS(0°,0%,0%)
        for (size_t i = 16; i < TextColor::TABLE_SIZE; i++)
        {
            expectedResponse += std::to_wstring(i) + L";1;0;0;0";
            if (i + 1 < TextColor::TABLE_SIZE)
            {
                expectedResponse += L'/';
            }
        }
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        Log::Comment(L"RGB color model");

        const auto rgbColorModel = static_cast<int>(DispatchTypes::ColorModel::RGB);
        _pDispatch->RequestTerminalStateReport(DispatchTypes::ReportFormat::ColorTableReport, rgbColorModel);

        expectedResponse = DECCTR;
        // RGB(0,0,0) -> RGB(0%,0%,0%)
        expectedResponse += L"0;2;0;0;0/";
        // RGB(204,36,36) -> RGB(80%,14%,14%)
        expectedResponse += L"1;2;80;14;14/";
        // RGB(51,204,51) -> RGB(20%,80%,20%)
        expectedResponse += L"2;2;20;80;20/";
        // RGB(204,204,51) -> RGB(80%,80%,20%)
        expectedResponse += L"3;2;80;80;20/";
        // RGB(51,51,204) -> RGB(20%,20%,80%)
        expectedResponse += L"4;2;20;20;80/";
        // RGB(204,51,204) -> RGB(80%,20%,80%)
        expectedResponse += L"5;2;80;20;80/";
        // RGB(51,204,204) -> RGB(20%,80%,80%)
        expectedResponse += L"6;2;20;80;80/";
        // RGB(120,120,120) -> RGB(47%,47%,47%)
        expectedResponse += L"7;2;47;47;47/";
        // RGB(69,69,69) -> RGB(27%,27%,27%)
        expectedResponse += L"8;2;27;27;27/";
        // RGB(255,0,0) -> RGB(100%,0%,0%)
        expectedResponse += L"9;2;100;0;0/";
        // RGB(0,255,0) -> RGB(0%,100%,0%)
        expectedResponse += L"10;2;0;100;0/";
        // RGB(255,255,0) -> RGB(100%,100%,0%)
        expectedResponse += L"11;2;100;100;0/";
        // RGB(0,0,255) -> RGB(0%,0%,100%)
        expectedResponse += L"12;2;0;0;100/";
        // RGB(255,0,255) -> RGB(100%,0%,100%)
        expectedResponse += L"13;2;100;0;100/";
        // RGB(0,255,255) -> RGB(0%,100%,100%)
        expectedResponse += L"14;2;0;100;100/";
        // RGB(255,255,255) -> RGB(100%,100%,100%)
        expectedResponse += L"15;2;100;100;100/";
        // Remaining slots are black, i.e. RGB(0%,0%,0%)
        for (size_t i = 16; i < TextColor::TABLE_SIZE; i++)
        {
            expectedResponse += std::to_wstring(i) + L";2;0;0;0";
            if (i + 1 < TextColor::TABLE_SIZE)
            {
                expectedResponse += L'/';
            }
        }
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());
    }

    TEST_METHOD(Osc4ColorPaletteReportTests)
    {
        _testGetSet->PrepData();

        // The colors below use the same VT525 colors as the other color table report tests.
        auto& renderSettings = _testGetSet->_renderer._renderSettings;
        renderSettings.SetColorTableEntry(0, RGB(0, 0, 0));
        renderSettings.SetColorTableEntry(1, RGB(204, 36, 36));
        renderSettings.SetColorTableEntry(2, RGB(51, 204, 51));
        renderSettings.SetColorTableEntry(3, RGB(204, 204, 51));
        renderSettings.SetColorTableEntry(4, RGB(51, 51, 204));
        renderSettings.SetColorTableEntry(5, RGB(204, 51, 204));
        renderSettings.SetColorTableEntry(6, RGB(51, 204, 204));
        renderSettings.SetColorTableEntry(7, RGB(120, 120, 120));
        renderSettings.SetColorTableEntry(8, RGB(69, 69, 69));
        renderSettings.SetColorTableEntry(9, RGB(255, 0, 0));
        renderSettings.SetColorTableEntry(10, RGB(0, 255, 0));
        renderSettings.SetColorTableEntry(11, RGB(255, 255, 0));
        renderSettings.SetColorTableEntry(12, RGB(0, 0, 255));
        renderSettings.SetColorTableEntry(13, RGB(255, 0, 255));
        renderSettings.SetColorTableEntry(14, RGB(0, 255, 255));
        renderSettings.SetColorTableEntry(15, RGB(255, 255, 255));
        for (size_t i = 16; i < TextColor::TABLE_SIZE; i++)
        {
            renderSettings.SetColorTableEntry(i, RGB(0, 0, 0));
        }

        // Dynamic color reports begin with an OSC, parameter 4, another parameter for the index, and end with ST.
        const auto OSC = L"\033]";
        const auto ST = L"\033\\";

        _pDispatch->RequestColorTableEntry(0);
        std::wstring expectedResponse = OSC;
        expectedResponse += L"4;0;rgb:0000/0000/0000";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(1);
        expectedResponse = OSC;
        expectedResponse += L"4;1;rgb:cccc/2424/2424";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(2);
        expectedResponse = OSC;
        expectedResponse += L"4;2;rgb:3333/cccc/3333";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(3);
        expectedResponse = OSC;
        expectedResponse += L"4;3;rgb:cccc/cccc/3333";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(4);
        expectedResponse = OSC;
        expectedResponse += L"4;4;rgb:3333/3333/cccc";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(5);
        expectedResponse = OSC;
        expectedResponse += L"4;5;rgb:cccc/3333/cccc";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(6);
        expectedResponse = OSC;
        expectedResponse += L"4;6;rgb:3333/cccc/cccc";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(7);
        expectedResponse = OSC;
        expectedResponse += L"4;7;rgb:7878/7878/7878";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(8);
        expectedResponse = OSC;
        expectedResponse += L"4;8;rgb:4545/4545/4545";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(9);
        expectedResponse = OSC;
        expectedResponse += L"4;9;rgb:ffff/0000/0000";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(10);
        expectedResponse = OSC;
        expectedResponse += L"4;10;rgb:0000/ffff/0000";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(11);
        expectedResponse = OSC;
        expectedResponse += L"4;11;rgb:ffff/ffff/0000";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(12);
        expectedResponse = OSC;
        expectedResponse += L"4;12;rgb:0000/0000/ffff";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(13);
        expectedResponse = OSC;
        expectedResponse += L"4;13;rgb:ffff/0000/ffff";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(14);
        expectedResponse = OSC;
        expectedResponse += L"4;14;rgb:0000/ffff/ffff";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->RequestColorTableEntry(15);
        expectedResponse = OSC;
        expectedResponse += L"4;15;rgb:ffff/ffff/ffff";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());
    }

    TEST_METHOD(XtermColorResourceReportTests)
    {
        _testGetSet->PrepData();

        // The colors below use the same VT525 colors as the other color table report tests.
        auto& renderSettings = _testGetSet->_renderer._renderSettings;
        renderSettings.SetColorTableEntry(0, RGB(0, 0, 0));
        renderSettings.SetColorTableEntry(1, RGB(204, 36, 36));
        renderSettings.SetColorTableEntry(2, RGB(51, 204, 51));
        renderSettings.SetColorTableEntry(3, RGB(204, 204, 51));
        renderSettings.SetColorTableEntry(4, RGB(51, 51, 204));
        renderSettings.SetColorTableEntry(5, RGB(204, 51, 204));
        renderSettings.SetColorTableEntry(6, RGB(51, 204, 204));
        renderSettings.SetColorTableEntry(7, RGB(120, 120, 120));
        renderSettings.SetColorTableEntry(8, RGB(69, 69, 69));
        renderSettings.SetColorTableEntry(9, RGB(255, 0, 0));
        renderSettings.SetColorTableEntry(10, RGB(0, 255, 0));
        renderSettings.SetColorTableEntry(11, RGB(255, 255, 0));
        renderSettings.SetColorTableEntry(12, RGB(0, 0, 255));
        renderSettings.SetColorTableEntry(13, RGB(255, 0, 255));
        renderSettings.SetColorTableEntry(14, RGB(0, 255, 255));
        renderSettings.SetColorTableEntry(15, RGB(255, 255, 255));

        renderSettings.SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, RGB(190, 190, 190));
        renderSettings.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, RGB(12, 12, 12));
        renderSettings.SetColorTableEntry(TextColor::CURSOR_COLOR, RGB(255, 0, 0));

        // Dynamic color reports begin with an OSC, a parameter matching the requested value, and end with ST.
        const auto OSC = L"\033]";
        const auto ST = L"\033\\";

        // Foreground mapped to DARK_WHITE
        _pDispatch->RequestXtermColorResource(10);
        std::wstring expectedResponse = OSC;
        expectedResponse += L"10;rgb:7878/7878/7878";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        // Foreground mapped to independent foreground color
        renderSettings.SetColorAliasIndex(ColorAlias::DefaultForeground, TextColor::DEFAULT_FOREGROUND);
        _pDispatch->RequestXtermColorResource(10);
        expectedResponse = OSC;
        expectedResponse += L"10;rgb:bebe/bebe/bebe";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        // Background mapped to DARK_BLACK
        _pDispatch->RequestXtermColorResource(11);
        expectedResponse = OSC;
        expectedResponse += L"11;rgb:0000/0000/0000";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        // Background mapped to independent background color
        renderSettings.SetColorAliasIndex(ColorAlias::DefaultBackground, TextColor::DEFAULT_BACKGROUND);
        _pDispatch->RequestXtermColorResource(11);
        expectedResponse = OSC;
        expectedResponse += L"11;rgb:0c0c/0c0c/0c0c";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        // Foreground and Background mapped to different indices (e.g. via DECAC)
        {
            _testGetSet->_response.clear(); // manually clear (since we aren't issuing a call that will empty it)
            auto retentionScope = _testGetSet->EnableInputRetentionInScope();
            renderSettings.SetColorAliasIndex(ColorAlias::DefaultForeground, TextColor::DARK_RED);
            renderSettings.SetColorAliasIndex(ColorAlias::DefaultBackground, TextColor::BRIGHT_GREEN);
            _pDispatch->RequestXtermColorResource(10);
            _pDispatch->RequestXtermColorResource(11);
            expectedResponse = OSC;
            expectedResponse += L"10;rgb:cccc/2424/2424";
            expectedResponse += ST;
            expectedResponse += OSC;
            expectedResponse += L"11;rgb:0000/ffff/0000";
            expectedResponse += ST;
            _testGetSet->ValidateInputEvent(expectedResponse.c_str());
        }

        _pDispatch->RequestXtermColorResource(12);
        expectedResponse = OSC;
        expectedResponse += L"12;rgb:ffff/0000/0000";
        expectedResponse += ST;
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        // Resource set to unrepresentable color
        _testGetSet->_response.clear(); // manually clear (since we aren't issuing a call that will empty it)
        renderSettings.SetColorTableEntry(TextColor::CURSOR_COLOR, INVALID_COLOR);
        _pDispatch->RequestXtermColorResource(12);
        expectedResponse = L"";
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        // Unsupported resource - no mapped color
        _testGetSet->_response.clear();
        _pDispatch->RequestXtermColorResource(13);
        expectedResponse = L"";
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());
    }

    TEST_METHOD(TabulationStopReportTests)
    {
        _testGetSet->PrepData();
        auto& textBuffer = *_testGetSet->_textBuffer;
        auto& stateMachine = *_testGetSet->_stateMachine;

        Log::Comment(L"Default tabs stops in 80-column mode");
        textBuffer.ResizeTraditional({ 80, 600 });
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::TabulationStopReport);
        _testGetSet->ValidateInputEvent(L"\033P2$u9/17/25/33/41/49/57/65/73\033\\");

        Log::Comment(L"Default tabs stops in 132-column mode");
        textBuffer.ResizeTraditional({ 132, 600 });
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::TabulationStopReport);
        _testGetSet->ValidateInputEvent(L"\033P2$u9/17/25/33/41/49/57/65/73/81/89/97/105/113/121/129\033\\");

        Log::Comment(L"Custom tab stops in 80 columns");
        textBuffer.ResizeTraditional({ 80, 600 });
        _testGetSet->_stateMachine->ProcessString(L"\033P2$t30/60/120/240\033\\");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::TabulationStopReport);
        _testGetSet->ValidateInputEvent(L"\033P2$u30/60\033\\");

        Log::Comment(L"After expanding width to 132 columns");
        textBuffer.ResizeTraditional({ 132, 600 });
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::TabulationStopReport);
        _testGetSet->ValidateInputEvent(L"\033P2$u30/60/120\033\\");
        textBuffer.ResizeTraditional({ 80, 600 });

        Log::Comment(L"Out of order tab stops");
        stateMachine.ProcessString(L"\033P2$t44/22/66\033\\");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::TabulationStopReport);
        _testGetSet->ValidateInputEvent(L"\033P2$u22/44/66\033\\");

        Log::Comment(L"Empty tab stop are ignored");
        stateMachine.ProcessString(L"\033P2$t3//7\033\\");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::TabulationStopReport);
        _testGetSet->ValidateInputEvent(L"\033P2$u3/7\033\\");

        Log::Comment(L"'0' tab stops are ignored");
        stateMachine.ProcessString(L"\033P2$t0/5/10\033\\");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::TabulationStopReport);
        _testGetSet->ValidateInputEvent(L"\033P2$u5/10\033\\");

        Log::Comment(L"'1' tab stops are ignored");
        stateMachine.ProcessString(L"\033P2$t1/8/18\033\\");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::TabulationStopReport);
        _testGetSet->ValidateInputEvent(L"\033P2$u8/18\033\\");

        Log::Comment(L"Clear tab stops");
        _pDispatch->TabClear(DispatchTypes::TabClearType::ClearAllColumns);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::TabulationStopReport);
        _testGetSet->ValidateInputEvent(L"\033P2$u\033\\");
    }

    TEST_METHOD(CursorInformationReportTests)
    {
        _testGetSet->PrepData();
        auto& textBuffer = *_testGetSet->_textBuffer;
        auto& stateMachine = *_testGetSet->_stateMachine;
        auto& termOutput = _pDispatch->_termOutput;
        const auto viewportTop = _testGetSet->_viewport.top;
        auto attributes = TextAttribute{};

        Log::Comment(L"Initial state");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;1;1;@;@;@;0;2;@;BBBB\033\\");

        Log::Comment(L"Cursor position");
        textBuffer.GetCursor().SetPosition({ 3, viewportTop + 2 });
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u3;4;1;@;@;@;0;2;@;BBBB\033\\");

        Log::Comment(L"Intense rendition");
        attributes.SetIntense(true);
        textBuffer.SetCurrentAttributes(attributes);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u3;4;1;A;@;@;0;2;@;BBBB\033\\");

        Log::Comment(L"Underlined rendition");
        attributes.SetUnderlineStyle(UnderlineStyle::SinglyUnderlined);
        textBuffer.SetCurrentAttributes(attributes);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u3;4;1;C;@;@;0;2;@;BBBB\033\\");

        Log::Comment(L"Blinking rendition");
        attributes.SetBlinking(true);
        textBuffer.SetCurrentAttributes(attributes);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u3;4;1;G;@;@;0;2;@;BBBB\033\\");

        Log::Comment(L"Reverse video rendition");
        attributes.SetReverseVideo(true);
        textBuffer.SetCurrentAttributes(attributes);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u3;4;1;O;@;@;0;2;@;BBBB\033\\");

        Log::Comment(L"Invisible rendition");
        attributes.SetInvisible(true);
        textBuffer.SetCurrentAttributes(attributes);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u3;4;1;_;@;@;0;2;@;BBBB\033\\");

        Log::Comment(L"Protected attribute");
        attributes.SetProtected(true);
        textBuffer.SetCurrentAttributes(attributes);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u3;4;1;_;A;@;0;2;@;BBBB\033\\");

        Log::Comment(L"Origin mode");
        _pDispatch->SetMode(DispatchTypes::DECOM_OriginMode);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;1;1;_;A;A;0;2;@;BBBB\033\\");

        Log::Comment(L"Single shift 2");
        _pDispatch->SingleShift(2);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;1;1;_;A;C;0;2;@;BBBB\033\\");

        Log::Comment(L"Single shift 3");
        _pDispatch->SingleShift(3);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;1;1;_;A;E;0;2;@;BBBB\033\\");

        Log::Comment(L"Delayed EOL wrap");
        _pDispatch->CursorForward(999);
        _pDispatch->Print(L'*');
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;100;1;_;A;I;0;2;@;BBBB\033\\");

        Log::Comment(L"Locking shifts");
        _pDispatch->LockingShift(1);
        _pDispatch->LockingShiftRight(3);
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;100;1;_;A;I;1;3;@;BBBB\033\\");

        Log::Comment(L"94 charset designations");
        termOutput.AssignUserPreferenceCharset(VTID("%5"), false);
        _pDispatch->Designate94Charset(0, "%5");
        _pDispatch->Designate94Charset(1, "<");
        _pDispatch->Designate94Charset(2, "0");
        _pDispatch->Designate94Charset(3, "K");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;100;1;_;A;I;1;3;@;%5<0K\033\\");

        Log::Comment(L"96 charset designation (G1)");
        _pDispatch->Designate96Charset(1, "H");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;100;1;_;A;I;1;3;B;%5H0K\033\\");

        Log::Comment(L"96 charset designation (G2)");
        _pDispatch->Designate96Charset(2, "M");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;100;1;_;A;I;1;3;F;%5HMK\033\\");

        Log::Comment(L"96 charset designation (G3)");
        _pDispatch->Designate96Charset(3, "B");
        _pDispatch->RequestPresentationStateReport(DispatchTypes::PresentationReportFormat::CursorInformationReport);
        _testGetSet->ValidateInputEvent(L"\033P1$u1;100;1;_;A;I;1;3;N;%5HMB\033\\");

        Log::Comment(L"Restore cursor position");
        stateMachine.ProcessString(L"\033P1$t3;4;1;@;@;@;0;2;@;BBBB\033\\");
        auto expectedPosition = til::point{ 3, viewportTop + 2 };
        VERIFY_ARE_EQUAL(expectedPosition, textBuffer.GetCursor().GetPosition());

        Log::Comment(L"Restore rendition attributes");

        // In the following sequence, U (0101 0101) represents the bold, blinking, invisible attributes to be active.
        stateMachine.ProcessString(L"\033P1$t1;1;1;U;@;@;0;2;@;BBBB\033\\");
        attributes = {};
        attributes.SetIntense(true); // bold
        attributes.SetBlinking(true);
        attributes.SetInvisible(true);
        VERIFY_ARE_EQUAL(attributes, textBuffer.GetCurrentAttributes());

        // In the following sequence, J (0100 1010) represents the underline, reverse video attributes to be active.
        stateMachine.ProcessString(L"\033P1$t1;1;1;J;A;@;0;2;@;BBBB\033\\");
        attributes = {};
        attributes.SetUnderlineStyle(UnderlineStyle::SinglyUnderlined);
        attributes.SetReverseVideo(true);
        attributes.SetProtected(true);
        VERIFY_ARE_EQUAL(attributes, textBuffer.GetCurrentAttributes());

        Log::Comment(L"Restore flags");
        stateMachine.ProcessString(L"\033P1$t1;1;1;@;@;E;0;2;@;BBBB\033\\");
        VERIFY_IS_TRUE(_pDispatch->_modes.test(AdaptDispatch::Mode::Origin));
        VERIFY_IS_FALSE(termOutput.IsSingleShiftPending(2));
        VERIFY_IS_TRUE(termOutput.IsSingleShiftPending(3));
        VERIFY_IS_FALSE(textBuffer.GetCursor().GetDelayEOLWrap().has_value());
        stateMachine.ProcessString(L"\033P1$t1;1;1;@;@;J;0;2;@;BBBB\033\\");
        VERIFY_IS_FALSE(_pDispatch->_modes.test(AdaptDispatch::Mode::Origin));
        VERIFY_IS_TRUE(termOutput.IsSingleShiftPending(2));
        VERIFY_IS_FALSE(termOutput.IsSingleShiftPending(3));
        VERIFY_IS_TRUE(textBuffer.GetCursor().GetDelayEOLWrap().has_value());

        Log::Comment(L"Restore charset configuration");
        stateMachine.ProcessString(L"\033P1$t1;1;1;@;@;@;3;1;H;ABCF\033\\");
        VERIFY_ARE_EQUAL(3u, termOutput.GetLeftSetNumber());
        VERIFY_ARE_EQUAL(1u, termOutput.GetRightSetNumber());
        VERIFY_ARE_EQUAL(94u, termOutput.GetCharsetSize(0));
        VERIFY_ARE_EQUAL(94u, termOutput.GetCharsetSize(1));
        VERIFY_ARE_EQUAL(94u, termOutput.GetCharsetSize(2));
        VERIFY_ARE_EQUAL(96u, termOutput.GetCharsetSize(3));
        VERIFY_ARE_EQUAL(VTID("A"), termOutput.GetCharsetId(0));
        VERIFY_ARE_EQUAL(VTID("B"), termOutput.GetCharsetId(1));
        VERIFY_ARE_EQUAL(VTID("C"), termOutput.GetCharsetId(2));
        VERIFY_ARE_EQUAL(VTID("F"), termOutput.GetCharsetId(3));
    }

    TEST_METHOD(CursorKeysModeTest)
    {
        Log::Comment(L"Starting test...");
        _terminalInput.SetInputMode(TerminalInput::Mode::CursorKey, true);

        // success cases
        // set numeric mode = true
        Log::Comment(L"Test 1: application mode = false");
        _pDispatch->ResetMode(DispatchTypes::DECCKM_CursorKeysMode);
        VERIFY_IS_FALSE(_terminalInput.GetInputMode(TerminalInput::Mode::CursorKey));

        // set numeric mode = false
        Log::Comment(L"Test 2: application mode = true");
        _pDispatch->SetMode(DispatchTypes::DECCKM_CursorKeysMode);
        VERIFY_IS_TRUE(_terminalInput.GetInputMode(TerminalInput::Mode::CursorKey));
    }

    TEST_METHOD(KeypadModeTest)
    {
        Log::Comment(L"Starting test...");
        _terminalInput.SetInputMode(TerminalInput::Mode::Keypad, true);

        // success cases
        // set numeric mode = true
        Log::Comment(L"Test 1: application mode = false");
        _pDispatch->SetKeypadMode(false);
        VERIFY_IS_FALSE(_terminalInput.GetInputMode(TerminalInput::Mode::Keypad));

        // set numeric mode = false
        Log::Comment(L"Test 2: application mode = true");
        _pDispatch->SetKeypadMode(true);
        VERIFY_IS_TRUE(_terminalInput.GetInputMode(TerminalInput::Mode::Keypad));
    }

    TEST_METHOD(AnsiModeTest)
    {
        Log::Comment(L"Starting test...");
        _stateMachine->SetParserMode(StateMachine::Mode::Ansi, false);

        // success cases
        // set ansi mode = true
        Log::Comment(L"Test 1: ansi mode = true");
        _pDispatch->SetAnsiMode(true);
        VERIFY_IS_TRUE(_stateMachine->GetParserMode(StateMachine::Mode::Ansi));

        // set ansi mode = false
        Log::Comment(L"Test 2: ansi mode = false.");
        _pDispatch->SetAnsiMode(false);
        VERIFY_IS_FALSE(_stateMachine->GetParserMode(StateMachine::Mode::Ansi));
    }

    TEST_METHOD(AllowBlinkingTest)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->PrepData();

        // success cases
        // set blinking mode = true
        Log::Comment(L"Test 1: enable blinking = true");
        _testGetSet->_textBuffer->GetCursor().SetIsBlinking(false);
        _pDispatch->SetMode(DispatchTypes::ATT610_StartCursorBlink);
        VERIFY_IS_TRUE(_testGetSet->_textBuffer->GetCursor().IsBlinking());

        // set blinking mode = false
        Log::Comment(L"Test 2: enable blinking = false");
        _testGetSet->_textBuffer->GetCursor().SetIsBlinking(true);
        _pDispatch->ResetMode(DispatchTypes::ATT610_StartCursorBlink);
        VERIFY_IS_FALSE(_testGetSet->_textBuffer->GetCursor().IsBlinking());
    }

    TEST_METHOD(ScrollMarginsTest)
    {
        Log::Comment(L"Starting test...");

        til::inclusive_rect srTestMargins;
        _testGetSet->_textBuffer = std::make_unique<TextBuffer>(til::size{ 100, 600 }, TextAttribute{}, 0, false, &_testGetSet->_renderer);
        _testGetSet->_viewport.right = 8;
        _testGetSet->_viewport.bottom = 8;
        auto sScreenHeight = _testGetSet->_viewport.bottom - _testGetSet->_viewport.top;

        Log::Comment(L"Test 1: Verify having both values is valid.");
        _pDispatch->SetTopBottomScrollingMargins(2, 6);
        VERIFY_ARE_EQUAL(2, _pDispatch->_scrollMargins.top + 1);
        VERIFY_ARE_EQUAL(6, _pDispatch->_scrollMargins.bottom + 1);

        Log::Comment(L"Test 2: Verify having only top is valid.");
        _pDispatch->SetTopBottomScrollingMargins(7, 0);
        VERIFY_ARE_EQUAL(7, _pDispatch->_scrollMargins.top + 1);
        VERIFY_ARE_EQUAL(sScreenHeight, _pDispatch->_scrollMargins.bottom + 1);

        Log::Comment(L"Test 3: Verify having only bottom is valid.");
        _pDispatch->SetTopBottomScrollingMargins(0, 7);
        VERIFY_ARE_EQUAL(1, _pDispatch->_scrollMargins.top + 1);
        VERIFY_ARE_EQUAL(7, _pDispatch->_scrollMargins.bottom + 1);

        Log::Comment(L"Test 4: Verify having no values is valid.");
        _pDispatch->SetTopBottomScrollingMargins(0, 0);
        VERIFY_ARE_EQUAL(til::inclusive_rect{}, _pDispatch->_scrollMargins);

        Log::Comment(L"Test 5: Verify having both values, but bad bounds has no effect.");
        _pDispatch->_scrollMargins = {};
        _pDispatch->SetTopBottomScrollingMargins(7, 3);
        VERIFY_ARE_EQUAL(til::inclusive_rect{}, _pDispatch->_scrollMargins);

        Log::Comment(L"Test 6: Verify setting margins to (0, height) clears them");
        // First set,
        _pDispatch->SetTopBottomScrollingMargins(2, 6);
        // Then clear
        _pDispatch->SetTopBottomScrollingMargins(0, sScreenHeight);
        VERIFY_ARE_EQUAL(til::inclusive_rect{}, _pDispatch->_scrollMargins);

        Log::Comment(L"Test 7: Verify setting margins to (1, height) clears them");
        // First set,
        _pDispatch->SetTopBottomScrollingMargins(2, 6);
        // Then clear
        _pDispatch->SetTopBottomScrollingMargins(1, sScreenHeight);
        VERIFY_ARE_EQUAL(til::inclusive_rect{}, _pDispatch->_scrollMargins);

        Log::Comment(L"Test 8: Verify setting margins to (1, 0) clears them");
        // First set,
        _pDispatch->SetTopBottomScrollingMargins(2, 6);
        // Then clear
        _pDispatch->SetTopBottomScrollingMargins(1, 0);
        VERIFY_ARE_EQUAL(til::inclusive_rect{}, _pDispatch->_scrollMargins);

        Log::Comment(L"Test 9: Verify having top and bottom margin the same has no effect.");
        _pDispatch->_scrollMargins = {};
        _pDispatch->SetTopBottomScrollingMargins(4, 4);
        VERIFY_ARE_EQUAL(til::inclusive_rect{}, _pDispatch->_scrollMargins);

        Log::Comment(L"Test 10: Verify having top margin out of bounds has no effect.");
        _pDispatch->_scrollMargins = {};
        _pDispatch->SetTopBottomScrollingMargins(sScreenHeight + 1, sScreenHeight + 10);
        VERIFY_ARE_EQUAL(til::inclusive_rect{}, _pDispatch->_scrollMargins);

        Log::Comment(L"Test 11: Verify having bottom margin out of bounds has no effect.");
        _pDispatch->_scrollMargins = {};
        _pDispatch->SetTopBottomScrollingMargins(1, sScreenHeight + 1);
        VERIFY_ARE_EQUAL(til::inclusive_rect{}, _pDispatch->_scrollMargins);
    }

    TEST_METHOD(LineFeedTest)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->PrepData();
        auto& cursor = _testGetSet->_textBuffer->GetCursor();

        Log::Comment(L"Test 1: Line feed without carriage return.");
        cursor.SetPosition({ 10, 0 });
        _pDispatch->LineFeed(DispatchTypes::LineFeedType::WithoutReturn);
        VERIFY_ARE_EQUAL(til::point(10, 1), cursor.GetPosition());

        Log::Comment(L"Test 2: Line feed with carriage return.");
        cursor.SetPosition({ 10, 0 });
        _pDispatch->LineFeed(DispatchTypes::LineFeedType::WithReturn);
        VERIFY_ARE_EQUAL(til::point(0, 1), cursor.GetPosition());

        Log::Comment(L"Test 3: Line feed depends on mode, and mode reset.");
        _testGetSet->_systemMode.reset(ITerminalApi::Mode::LineFeed);
        cursor.SetPosition({ 10, 0 });
        _pDispatch->LineFeed(DispatchTypes::LineFeedType::DependsOnMode);
        VERIFY_ARE_EQUAL(til::point(10, 1), cursor.GetPosition());

        Log::Comment(L"Test 4: Line feed depends on mode, and mode set.");
        _testGetSet->_systemMode.set(ITerminalApi::Mode::LineFeed);
        cursor.SetPosition({ 10, 0 });
        _pDispatch->LineFeed(DispatchTypes::LineFeedType::DependsOnMode);
        VERIFY_ARE_EQUAL(til::point(0, 1), cursor.GetPosition());
    }

    TEST_METHOD(SetConsoleTitleTest)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: set title to be non-null");
        _testGetSet->_setWindowTitleResult = TRUE;
        _testGetSet->_expectedWindowTitle = L"Foo bar";

        _pDispatch->SetWindowTitle(_testGetSet->_expectedWindowTitle);

        Log::Comment(L"Test 2: set title to be null");
        _testGetSet->_setWindowTitleResult = FALSE;
        _testGetSet->_expectedWindowTitle = {};

        _pDispatch->SetWindowTitle({});
    }

    TEST_METHOD(TestMouseModes)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Test Default Mouse Mode");
        _terminalInput.SetInputMode(TerminalInput::Mode::DefaultMouseTracking, false);
        _pDispatch->SetMode(DispatchTypes::VT200_MOUSE_MODE);
        VERIFY_IS_TRUE(_terminalInput.GetInputMode(TerminalInput::Mode::DefaultMouseTracking));
        _pDispatch->ResetMode(DispatchTypes::VT200_MOUSE_MODE);
        VERIFY_IS_FALSE(_terminalInput.GetInputMode(TerminalInput::Mode::DefaultMouseTracking));

        Log::Comment(L"Test 2: Test UTF-8 Extended Mouse Mode");
        _terminalInput.SetInputMode(TerminalInput::Mode::Utf8MouseEncoding, false);
        _pDispatch->SetMode(DispatchTypes::UTF8_EXTENDED_MODE);
        VERIFY_IS_TRUE(_terminalInput.GetInputMode(TerminalInput::Mode::Utf8MouseEncoding));
        _pDispatch->ResetMode(DispatchTypes::UTF8_EXTENDED_MODE);
        VERIFY_IS_FALSE(_terminalInput.GetInputMode(TerminalInput::Mode::Utf8MouseEncoding));

        Log::Comment(L"Test 3: Test SGR Extended Mouse Mode");
        _terminalInput.SetInputMode(TerminalInput::Mode::SgrMouseEncoding, false);
        _pDispatch->SetMode(DispatchTypes::SGR_EXTENDED_MODE);
        VERIFY_IS_TRUE(_terminalInput.GetInputMode(TerminalInput::Mode::SgrMouseEncoding));
        _pDispatch->ResetMode(DispatchTypes::SGR_EXTENDED_MODE);
        VERIFY_IS_FALSE(_terminalInput.GetInputMode(TerminalInput::Mode::SgrMouseEncoding));

        Log::Comment(L"Test 4: Test Button-Event Mouse Mode");
        _terminalInput.SetInputMode(TerminalInput::Mode::ButtonEventMouseTracking, false);
        _pDispatch->SetMode(DispatchTypes::BUTTON_EVENT_MOUSE_MODE);
        VERIFY_IS_TRUE(_terminalInput.GetInputMode(TerminalInput::Mode::ButtonEventMouseTracking));
        _pDispatch->ResetMode(DispatchTypes::BUTTON_EVENT_MOUSE_MODE);
        VERIFY_IS_FALSE(_terminalInput.GetInputMode(TerminalInput::Mode::ButtonEventMouseTracking));

        Log::Comment(L"Test 5: Test Any-Event Mouse Mode");
        _terminalInput.SetInputMode(TerminalInput::Mode::AnyEventMouseTracking, false);
        _pDispatch->SetMode(DispatchTypes::ANY_EVENT_MOUSE_MODE);
        VERIFY_IS_TRUE(_terminalInput.GetInputMode(TerminalInput::Mode::AnyEventMouseTracking));
        _pDispatch->ResetMode(DispatchTypes::ANY_EVENT_MOUSE_MODE);
        VERIFY_IS_FALSE(_terminalInput.GetInputMode(TerminalInput::Mode::AnyEventMouseTracking));

        Log::Comment(L"Test 6: Test Alt Scroll Mouse Mode");
        _terminalInput.SetInputMode(TerminalInput::Mode::AlternateScroll, false);
        _pDispatch->SetMode(DispatchTypes::ALTERNATE_SCROLL);
        VERIFY_IS_TRUE(_terminalInput.GetInputMode(TerminalInput::Mode::AlternateScroll));
        _pDispatch->ResetMode(DispatchTypes::ALTERNATE_SCROLL);
        VERIFY_IS_FALSE(_terminalInput.GetInputMode(TerminalInput::Mode::AlternateScroll));
    }

    TEST_METHOD(Xterm256ColorTest)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED

        VTParameter rgOptions[16];
        size_t cOptions = 3;

        _testGetSet->_expectedAttribute = _testGetSet->_textBuffer->GetCurrentAttributes();

        Log::Comment(L"Test 1: Change Foreground");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)2; // Green
        _testGetSet->_expectedAttribute.SetIndexedForeground256(TextColor::DARK_GREEN);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 2: Change Background");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)9; // Bright Red
        _testGetSet->_expectedAttribute.SetIndexedBackground256(TextColor::BRIGHT_RED);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 3: Change Foreground to RGB color");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)42; // Arbitrary Color
        _testGetSet->_expectedAttribute.SetIndexedForeground256(42);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 4: Change Background to RGB color");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)142; // Arbitrary Color
        _testGetSet->_expectedAttribute.SetIndexedBackground256(142);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 5: Change Foreground to Legacy Attr while BG is RGB color");
        // Unfortunately this test isn't all that good, because the adapterTest adapter isn't smart enough
        //   to have its own color table and translate the preexisting RGB BG into a legacy BG.
        // Fortunately, the ft_api:RgbColorTests IS smart enough to test that.
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)9; // Bright Red
        _testGetSet->_expectedAttribute.SetIndexedForeground256(TextColor::BRIGHT_RED);
        _pDispatch->SetGraphicsRendition({ rgOptions, cOptions });
        _testGetSet->ValidateExpectedAttributes();
    }

    TEST_METHOD(XtermExtendedColorDefaultParameterTest)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED

        VTParameter rgOptions[16];

        _testGetSet->_expectedAttribute = _testGetSet->_textBuffer->GetCurrentAttributes();

        Log::Comment(L"Test 1: Change Indexed Foreground with missing index parameter");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        _testGetSet->_expectedAttribute.SetIndexedForeground256(TextColor::DARK_BLACK);
        _pDispatch->SetGraphicsRendition({ rgOptions, 2 });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 2: Change Indexed Background with default index parameter");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = {};
        _testGetSet->_expectedAttribute.SetIndexedBackground256(TextColor::DARK_BLACK);
        _pDispatch->SetGraphicsRendition({ rgOptions, 3 });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 3: Change RGB Foreground with all RGB parameters missing");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::RGBColorOrFaint;
        _testGetSet->_expectedAttribute.SetForeground(RGB(0, 0, 0));
        _pDispatch->SetGraphicsRendition({ rgOptions, 2 });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 4: Change RGB Background with some missing RGB parameters");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::RGBColorOrFaint;
        rgOptions[2] = 123;
        _testGetSet->_expectedAttribute.SetBackground(RGB(123, 0, 0));
        _pDispatch->SetGraphicsRendition({ rgOptions, 3 });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 5: Change RGB Foreground with some default RGB parameters");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::RGBColorOrFaint;
        rgOptions[2] = {};
        rgOptions[3] = {};
        rgOptions[4] = 123;
        _testGetSet->_expectedAttribute.SetForeground(RGB(0, 0, 123));
        _pDispatch->SetGraphicsRendition({ rgOptions, 5 });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 6: Ignore Rgb color when R, G or B is out of range (>255)");
        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::RGBColorOrFaint;
        rgOptions[2] = 283;
        rgOptions[3] = 182;
        rgOptions[4] = 123;
        // expect no change
        _testGetSet->_expectedAttribute = TextAttribute{ FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED };
        _pDispatch->SetGraphicsRendition({ rgOptions, 5 });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 7: Ignore indexed color when index is out of range (>255)");
        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = 283;
        // expect no change
        _testGetSet->_expectedAttribute = TextAttribute{ FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED };
        _pDispatch->SetGraphicsRendition({ rgOptions, 3 });
        _testGetSet->ValidateExpectedAttributes();
    }

    TEST_METHOD(XtermExtendedSubParameterColorTest)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED

        VTParameter rgOptions[1];
        std::vector<VTParameter> rgSubParamOpts;
        std::vector<std::pair<BYTE, BYTE>> subParamRanges;

        _testGetSet->_expectedAttribute = _testGetSet->_textBuffer->GetCurrentAttributes();

        Log::Comment(L"Test 1: Change Indexed Foreground with missing index sub parameter");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        _testGetSet->MakeSubParamsAndRanges({ { 5 } }, rgSubParamOpts, subParamRanges);
        _testGetSet->_expectedAttribute.SetIndexedForeground256(TextColor::DARK_BLACK);
        _pDispatch->SetGraphicsRendition({ rgOptions, rgSubParamOpts, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 2: Change Indexed Background with default index sub parameter");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundExtended;
        _testGetSet->MakeSubParamsAndRanges({ { 5, {} } }, rgSubParamOpts, subParamRanges);
        _testGetSet->_expectedAttribute.SetIndexedBackground256(TextColor::DARK_BLACK);
        _pDispatch->SetGraphicsRendition({ rgOptions, rgSubParamOpts, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 3: Change RGB Foreground with all RGB sub parameters missing");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        _testGetSet->MakeSubParamsAndRanges({ { 2 } }, rgSubParamOpts, subParamRanges);
        _testGetSet->_expectedAttribute.SetForeground(RGB(0, 0, 0));
        _pDispatch->SetGraphicsRendition({ rgOptions, rgSubParamOpts, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 4: Change RGB Background with some missing RGB sub parameters");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundExtended;
        _testGetSet->MakeSubParamsAndRanges({ { 2, {}, 123 } }, rgSubParamOpts, subParamRanges);
        _testGetSet->_expectedAttribute.SetBackground(RGB(123, 0, 0));
        _pDispatch->SetGraphicsRendition({ rgOptions, rgSubParamOpts, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 5: Change RGB Foreground with some default RGB sub parameters");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        _testGetSet->MakeSubParamsAndRanges({ { 2, {}, {}, {}, 123 } }, rgSubParamOpts, subParamRanges);
        _testGetSet->_expectedAttribute.SetForeground(RGB(0, 0, 123));
        _pDispatch->SetGraphicsRendition({ rgOptions, rgSubParamOpts, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 6: Ignore color when ColorSpaceID is not empty");
        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        _testGetSet->MakeSubParamsAndRanges({ { 2, 7, 182, 182, 123 } }, rgSubParamOpts, subParamRanges);
        // expect no change
        _testGetSet->_expectedAttribute = TextAttribute{ FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED };
        _pDispatch->SetGraphicsRendition({ rgOptions, rgSubParamOpts, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 7: Ignore Rgb color when R, G or B is out of range (>255)");
        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundExtended;
        // Ensure r, g and b set a color that is different from current color.
        // Otherwise, the test will pass even if the color is not ignored.
        _testGetSet->MakeSubParamsAndRanges({ { 2, {}, 128, 283, 155 } }, rgSubParamOpts, subParamRanges);
        // expect no change
        _testGetSet->_expectedAttribute = TextAttribute{ FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED };
        _pDispatch->SetGraphicsRendition({ rgOptions, rgSubParamOpts, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();

        Log::Comment(L"Test 8: Ignore indexed color when index is out of range (>255)");
        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        _testGetSet->MakeSubParamsAndRanges({ { 5, 283 } }, rgSubParamOpts, subParamRanges);
        // expect no change
        _testGetSet->_expectedAttribute = TextAttribute{ FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED };
        _pDispatch->SetGraphicsRendition({ rgOptions, rgSubParamOpts, subParamRanges });
        _testGetSet->ValidateExpectedAttributes();
    }

    TEST_METHOD(SetColorTableValue)
    {
        _testGetSet->PrepData();

        const auto testColor = RGB(1, 2, 3);
        const auto& renderSettings = _testGetSet->_renderer._renderSettings;

        for (size_t i = 0; i < 256; i++)
        {
            _pDispatch->SetColorTableEntry(i, testColor);
            VERIFY_ARE_EQUAL(testColor, renderSettings.GetColorTableEntry(i));
        }
    }

    TEST_METHOD(SoftFontSizeDetection)
    {
        using CellMatrix = DispatchTypes::DrcsCellMatrix;
        using FontSet = DispatchTypes::DrcsFontSet;
        using FontUsage = DispatchTypes::DrcsFontUsage;

        FontBuffer fontBuffer;
        til::size expectedCellSize;

        const auto decdld = [&](const auto cmw, const auto cmh, const auto ss, const auto u, const std::wstring_view data = {}) {
            const auto cellMatrix = static_cast<DispatchTypes::DrcsCellMatrix>(cmw);
            if (!fontBuffer.SetEraseControl(DispatchTypes::DrcsEraseControl::AllChars))
            {
                return false;
            }
            if (!fontBuffer.SetAttributes(cellMatrix, cmh, ss, u))
            {
                return false;
            }
            if (!fontBuffer.SetStartChar(0, DispatchTypes::CharsetSize::Size94))
            {
                return false;
            }

            fontBuffer.AddSixelData(L'B'); // Charset identifier
            for (auto ch : data)
            {
                fontBuffer.AddSixelData(ch);
            }
            if (!fontBuffer.FinalizeSixelData())
            {
                return false;
            }

            const auto cellSize = fontBuffer.GetCellSize();
            Log::Comment(NoThrowString().Format(L"Cell size: %dx%d", cellSize.width, cellSize.height));
            VERIFY_ARE_EQUAL(expectedCellSize.width, cellSize.width);
            VERIFY_ARE_EQUAL(expectedCellSize.height, cellSize.height);
            return true;
        };

        // Matrix sizes at 80x24 should always use a 10x10 cell size (VT2xx).
        Log::Comment(L"Matrix 5x10 for 80x24 font set with text usage");
        expectedCellSize = { 10, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Size5x10, 0, FontSet::Size80x24, FontUsage::Text));
        Log::Comment(L"Matrix 6x10 for 80x24 font set with text usage");
        expectedCellSize = { 10, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Size6x10, 0, FontSet::Size80x24, FontUsage::Text));
        Log::Comment(L"Matrix 7x10 for 80x24 font set with text usage");
        expectedCellSize = { 10, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Size7x10, 0, FontSet::Size80x24, FontUsage::Text));

        // At 132x24 the cell size is typically 6x10 (VT240), but could be 10x10 (VT220)
        Log::Comment(L"Matrix 5x10 for 132x24 font set with text usage");
        expectedCellSize = { 6, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Size5x10, 0, FontSet::Size132x24, FontUsage::Text));
        Log::Comment(L"Matrix 6x10 for 132x24 font set with text usage");
        expectedCellSize = { 6, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Size6x10, 0, FontSet::Size132x24, FontUsage::Text));
        Log::Comment(L"Matrix 7x10 for 132x24 font set with text usage (VT220 only)");
        expectedCellSize = { 10, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Size7x10, 0, FontSet::Size132x24, FontUsage::Text));

        // Full cell usage is invalid for all matrix sizes except 6x10 at 132x24.
        Log::Comment(L"Matrix 5x10 for 80x24 font set with full cell usage (invalid)");
        VERIFY_IS_FALSE(decdld(CellMatrix::Size5x10, 0, FontSet::Size80x24, FontUsage::FullCell));
        Log::Comment(L"Matrix 6x10 for 80x24 font set with full cell usage (invalid)");
        VERIFY_IS_FALSE(decdld(CellMatrix::Size6x10, 0, FontSet::Size80x24, FontUsage::FullCell));
        Log::Comment(L"Matrix 7x10 for 80x24 font set with full cell usage (invalid)");
        VERIFY_IS_FALSE(decdld(CellMatrix::Size7x10, 0, FontSet::Size80x24, FontUsage::FullCell));
        Log::Comment(L"Matrix 5x10 for 132x24 font set with full cell usage (invalid)");
        VERIFY_IS_FALSE(decdld(CellMatrix::Size5x10, 0, FontSet::Size132x24, FontUsage::FullCell));
        Log::Comment(L"Matrix 6x10 for 132x24 font set with full cell usage");
        expectedCellSize = { 6, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Size6x10, 0, FontSet::Size132x24, FontUsage::FullCell));
        Log::Comment(L"Matrix 7x10 for 132x24 font set with full cell usage (invalid)");
        VERIFY_IS_FALSE(decdld(CellMatrix::Size7x10, 0, FontSet::Size132x24, FontUsage::FullCell));

        // Matrix size 1 is always invalid.
        Log::Comment(L"Matrix 1 for 80x24 font set with text usage (invalid)");
        VERIFY_IS_FALSE(decdld(CellMatrix::Invalid, 0, FontSet::Size80x24, FontUsage::Text));
        Log::Comment(L"Matrix 1 for 132x24 font set with text usage (invalid)");
        VERIFY_IS_FALSE(decdld(CellMatrix::Invalid, 0, FontSet::Size132x24, FontUsage::Text));
        Log::Comment(L"Matrix 1 for 80x24 font set with full cell usage (invalid)");
        VERIFY_IS_FALSE(decdld(CellMatrix::Invalid, 0, FontSet::Size80x24, FontUsage::FullCell));
        Log::Comment(L"Matrix 1 for 132x24 font set with full cell usage (invalid)");
        VERIFY_IS_FALSE(decdld(CellMatrix::Invalid, 0, FontSet::Size132x24, FontUsage::FullCell));

        // The height parameter has no effect when a matrix size is used.
        Log::Comment(L"Matrix 7x10 with unused height parameter");
        expectedCellSize = { 10, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Size7x10, 20, FontSet::Size80x24, FontUsage::Text));

        // Full cell fonts with explicit dimensions are accepted as their given cell size.
        Log::Comment(L"Explicit 13x17 for 80x24 font set with full cell usage");
        expectedCellSize = { 13, 17 };
        VERIFY_IS_TRUE(decdld(13, 17, FontSet::Size80x24, FontUsage::FullCell));
        Log::Comment(L"Explicit 9x25 for 132x24 font set with full cell usage");
        expectedCellSize = { 9, 25 };
        VERIFY_IS_TRUE(decdld(9, 25, FontSet::Size132x24, FontUsage::FullCell));

        // Cell sizes outside the maximum supported range (16x32) are invalid.
        Log::Comment(L"Explicit 18x38 for 80x24 font set with full cell usage (invalid)");
        VERIFY_IS_FALSE(decdld(18, 38, FontSet::Size80x24, FontUsage::FullCell));

        // Text fonts with explicit dimensions are interpreted as their closest matching device.
        Log::Comment(L"Explicit 12x12 for 80x24 font set with text usage (VT320)");
        expectedCellSize = { 15, 12 };
        VERIFY_IS_TRUE(decdld(12, 12, FontSet::Size80x24, FontUsage::Text));
        Log::Comment(L"Explicit 9x20 for 80x24 font set with text usage (VT340)");
        expectedCellSize = { 10, 20 };
        VERIFY_IS_TRUE(decdld(9, 20, FontSet::Size80x24, FontUsage::Text));
        Log::Comment(L"Explicit 10x30 for 80x24 font set with text usage (VT382)");
        expectedCellSize = { 12, 30 };
        VERIFY_IS_TRUE(decdld(10, 30, FontSet::Size80x24, FontUsage::Text));
        Log::Comment(L"Explicit 8x16 for 80x24 font set with text usage (VT420/VT5xx)");
        expectedCellSize = { 10, 16 };
        VERIFY_IS_TRUE(decdld(8, 16, FontSet::Size80x24, FontUsage::Text));
        Log::Comment(L"Explicit 7x12 for 132x24 font set with text usage (VT320)");
        expectedCellSize = { 9, 12 };
        VERIFY_IS_TRUE(decdld(7, 12, FontSet::Size132x24, FontUsage::Text));
        Log::Comment(L"Explicit 5x20 for 132x24 font set with text usage (VT340)");
        expectedCellSize = { 6, 20 };
        VERIFY_IS_TRUE(decdld(5, 20, FontSet::Size132x24, FontUsage::Text));
        Log::Comment(L"Explicit 6x30 for 132x24 font set with text usage (VT382)");
        expectedCellSize = { 7, 30 };
        VERIFY_IS_TRUE(decdld(6, 30, FontSet::Size132x24, FontUsage::Text));
        Log::Comment(L"Explicit 5x16 for 132x24 font set with text usage (VT420/VT5xx)");
        expectedCellSize = { 6, 16 };
        VERIFY_IS_TRUE(decdld(5, 16, FontSet::Size132x24, FontUsage::Text));

        // Font sets with more than 24 lines must be VT420/VT5xx.
        Log::Comment(L"80x36 font set with text usage (VT420/VT5xx)");
        expectedCellSize = { 10, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x36, FontUsage::Text));
        Log::Comment(L"80x48 font set with text usage (VT420/VT5xx)");
        expectedCellSize = { 10, 8 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x48, FontUsage::Text));
        Log::Comment(L"132x36 font set with text usage (VT420/VT5xx)");
        expectedCellSize = { 6, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x36, FontUsage::Text));
        Log::Comment(L"132x48 font set with text usage (VT420/VT5xx)");
        expectedCellSize = { 6, 8 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x48, FontUsage::Text));
        Log::Comment(L"80x36 font set with full cell usage (VT420/VT5xx)");
        expectedCellSize = { 10, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x36, FontUsage::FullCell));
        Log::Comment(L"80x48 font set with full cell usage (VT420/VT5xx)");
        expectedCellSize = { 10, 8 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x48, FontUsage::FullCell));
        Log::Comment(L"132x36 font set with full cell usage (VT420/VT5xx)");
        expectedCellSize = { 6, 10 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x36, FontUsage::FullCell));
        Log::Comment(L"132x48 font set with full cell usage (VT420/VT5xx)");
        expectedCellSize = { 6, 8 };
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x48, FontUsage::FullCell));

        // Without an explicit size, the cell size is estimated from the number of sixels
        // used in the character bitmaps. But note that sixel heights are always a multiple
        // of 6, so will often be larger than the cell size for which they were intended.
        Log::Comment(L"8x12 bitmap for 80x24 font set with text usage (VT2xx)");
        expectedCellSize = { 10, 10 };
        const auto bitmapOf8x12 = L"????????/????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x24, FontUsage::Text, bitmapOf8x12));
        Log::Comment(L"12x12 bitmap for 80x24 font set with text usage (VT320)");
        expectedCellSize = { 15, 12 };
        const auto bitmapOf12x12 = L"????????????/????????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x24, FontUsage::Text, bitmapOf12x12));
        Log::Comment(L"9x24 bitmap for 80x24 font set with text usage (VT340)");
        expectedCellSize = { 10, 20 };
        const auto bitmapOf9x24 = L"?????????/?????????/?????????/?????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x24, FontUsage::Text, bitmapOf9x24));
        Log::Comment(L"10x30 bitmap for 80x24 font set with text usage (VT382)");
        expectedCellSize = { 12, 30 };
        const auto bitmapOf10x30 = L"??????????/??????????/??????????/??????????/??????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x24, FontUsage::Text, bitmapOf10x30));
        Log::Comment(L"8x18 bitmap for 80x24 font set with text usage (VT420/VT5xx)");
        expectedCellSize = { 10, 16 };
        const auto bitmapOf8x18 = L"????????/????????/????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x24, FontUsage::Text, bitmapOf8x18));

        Log::Comment(L"5x12 bitmap for 132x24 font set with text usage (VT240)");
        expectedCellSize = { 6, 10 };
        const auto bitmapOf5x12 = L"?????/?????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::Text, bitmapOf5x12));
        Log::Comment(L"7x12 bitmap for 132x24 font set with text usage (VT320)");
        expectedCellSize = { 9, 12 };
        const auto bitmapOf7x12 = L"???????/???????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::Text, bitmapOf7x12));
        Log::Comment(L"5x24 bitmap for 132x24 font set with text usage (VT340)");
        expectedCellSize = { 6, 20 };
        const auto bitmapOf5x24 = L"?????/?????/?????/?????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::Text, bitmapOf5x24));
        Log::Comment(L"6x30 bitmap for 132x24 font set with text usage (VT382)");
        expectedCellSize = { 7, 30 };
        const auto bitmapOf6x30 = L"??????/??????/??????/??????/??????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::Text, bitmapOf6x30));
        Log::Comment(L"5x18 bitmap for 132x24 font set with text usage (VT420/VT5xx)");
        expectedCellSize = { 6, 16 };
        const auto bitmapOf5x18 = L"?????/?????/?????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::Text, bitmapOf5x18));

        Log::Comment(L"15x12 bitmap for 80x24 font set with full cell usage (VT320)");
        expectedCellSize = { 15, 12 };
        const auto bitmapOf15x12 = L"???????????????/???????????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x24, FontUsage::FullCell, bitmapOf15x12));
        Log::Comment(L"10x24 bitmap for 80x24 font set with full cell usage (VT340)");
        expectedCellSize = { 10, 20 };
        const auto bitmapOf10x24 = L"??????????/??????????/??????????/??????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x24, FontUsage::FullCell, bitmapOf10x24));
        Log::Comment(L"12x30 bitmap for 80x24 font set with full cell usage (VT382)");
        expectedCellSize = { 12, 30 };
        const auto bitmapOf12x30 = L"????????????/????????????/????????????/????????????/????????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x24, FontUsage::FullCell, bitmapOf12x30));
        Log::Comment(L"10x18 bitmap for 80x24 font set with full cell usage (VT420/VT5xx)");
        expectedCellSize = { 10, 16 };
        const auto bitmapOf10x18 = L"??????????/??????????/??????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size80x24, FontUsage::FullCell, bitmapOf10x18));

        Log::Comment(L"6x12 bitmap for 132x24 font set with full cell usage (VT240)");
        expectedCellSize = { 6, 10 };
        const auto bitmapOf6x12 = L"??????/??????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::FullCell, bitmapOf6x12));
        Log::Comment(L"9x12 bitmap for 132x24 font set with full cell usage (VT320)");
        expectedCellSize = { 9, 12 };
        const auto bitmapOf9x12 = L"?????????/?????????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::FullCell, bitmapOf9x12));
        Log::Comment(L"6x24 bitmap for 132x24 font set with full cell usage (VT340)");
        expectedCellSize = { 6, 20 };
        const auto bitmapOf6x24 = L"??????/??????/??????/??????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::FullCell, bitmapOf6x24));
        Log::Comment(L"7x30 bitmap for 132x24 font set with full cell usage (VT382)");
        expectedCellSize = { 7, 30 };
        const auto bitmapOf7x30 = L"???????/???????/???????/???????/???????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::FullCell, bitmapOf7x30));
        Log::Comment(L"6x18 bitmap for 132x24 font set with full cell usage (VT420/VT5xx)");
        expectedCellSize = { 6, 16 };
        const auto bitmapOf6x18 = L"??????/??????/??????";
        VERIFY_IS_TRUE(decdld(CellMatrix::Default, 0, FontSet::Size132x24, FontUsage::FullCell, bitmapOf6x18));
    }

    TEST_METHOD(TogglingC1ParserMode)
    {
        _stateMachine->SetParserMode(StateMachine::Mode::AcceptC1, false);

        Log::Comment(L"1. Accept C1 controls");
        _pDispatch->AcceptC1Controls(true);
        VERIFY_IS_TRUE(_stateMachine->GetParserMode(StateMachine::Mode::AcceptC1));

        Log::Comment(L"2. Don't accept C1 controls");
        _pDispatch->AcceptC1Controls(false);
        VERIFY_IS_FALSE(_stateMachine->GetParserMode(StateMachine::Mode::AcceptC1));

        Log::Comment(L"3. Designate ISO-2022 coding system");
        // Code page should be set to ISO-8859-1 and C1 parsing enabled
        _testGetSet->_setCodePageResult = true;
        _testGetSet->_expectedCodePage = 28591;
        _pDispatch->DesignateCodingSystem(DispatchTypes::CodingSystem::ISO2022);
        VERIFY_IS_TRUE(_stateMachine->GetParserMode(StateMachine::Mode::AcceptC1));

        Log::Comment(L"4. Designate UTF-8 coding system");
        // Code page should be set to UTF-8 and C1 parsing disabled
        _testGetSet->_setCodePageResult = true;
        _testGetSet->_expectedCodePage = CP_UTF8;
        _pDispatch->DesignateCodingSystem(DispatchTypes::CodingSystem::UTF8);
        VERIFY_IS_FALSE(_stateMachine->GetParserMode(StateMachine::Mode::AcceptC1));
    }

    TEST_METHOD(AssignUserPreferenceCharsets)
    {
        const auto assignCharset = [=](const auto charsetSize, const std::wstring_view charsetId = {}) {
            const auto stringHandler = _pDispatch->AssignUserPreferenceCharset(charsetSize);
            for (auto ch : charsetId)
            {
                stringHandler(ch);
            }
            stringHandler(L'\033'); // String terminator
        };
        auto& termOutput = _pDispatch->_termOutput;
        termOutput.SoftReset();

        Log::Comment(L"DECAUPSS: DEC Supplemental");
        assignCharset(DispatchTypes::CharsetSize::Size94, L"%5");
        VERIFY_ARE_EQUAL(VTID("%5"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(94u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: DEC Greek");
        assignCharset(DispatchTypes::CharsetSize::Size94, L"\"?");
        VERIFY_ARE_EQUAL(VTID("\"?"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(94u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: DEC Hebrew");
        assignCharset(DispatchTypes::CharsetSize::Size94, L"\"4");
        VERIFY_ARE_EQUAL(VTID("\"4"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(94u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: DEC Turkish");
        assignCharset(DispatchTypes::CharsetSize::Size94, L"%0");
        VERIFY_ARE_EQUAL(VTID("%0"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(94u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: DEC Cyrillic");
        assignCharset(DispatchTypes::CharsetSize::Size94, L"&4");
        VERIFY_ARE_EQUAL(VTID("&4"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(94u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: ISO Latin-1");
        assignCharset(DispatchTypes::CharsetSize::Size96, L"A");
        VERIFY_ARE_EQUAL(VTID("A"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(96u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: ISO Latin-2");
        assignCharset(DispatchTypes::CharsetSize::Size96, L"B");
        VERIFY_ARE_EQUAL(VTID("B"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(96u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: ISO Latin-Greek");
        assignCharset(DispatchTypes::CharsetSize::Size96, L"F");
        VERIFY_ARE_EQUAL(VTID("F"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(96u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: ISO Latin-Hebrew");
        assignCharset(DispatchTypes::CharsetSize::Size96, L"H");
        VERIFY_ARE_EQUAL(VTID("H"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(96u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: ISO Latin-Cyrillic");
        assignCharset(DispatchTypes::CharsetSize::Size96, L"L");
        VERIFY_ARE_EQUAL(VTID("L"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(96u, termOutput.GetUserPreferenceCharsetSize());

        Log::Comment(L"DECAUPSS: ISO Latin-5");
        assignCharset(DispatchTypes::CharsetSize::Size96, L"M");
        VERIFY_ARE_EQUAL(VTID("M"), termOutput.GetUserPreferenceCharsetId());
        VERIFY_ARE_EQUAL(96u, termOutput.GetUserPreferenceCharsetSize());
    }

    TEST_METHOD(RequestUserPreferenceCharsets)
    {
        auto& termOutput = _pDispatch->_termOutput;

        Log::Comment(L"DECRQUPSS: DEC Supplemental");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("%5"), false);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P0!u%5\033\\");

        Log::Comment(L"DECRQUPSS: DEC Greek");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("\"?"), false);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P0!u\"?\033\\");

        Log::Comment(L"DECRQUPSS: DEC Hebrew");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("\"4"), false);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P0!u\"4\033\\");

        Log::Comment(L"DECRQUPSS: DEC Turkish");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("%0"), false);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P0!u%0\033\\");

        Log::Comment(L"DECRQUPSS: DEC Cyrillic");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("&4"), false);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P0!u&4\033\\");

        Log::Comment(L"DECRQUPSS: ISO Latin-1");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("A"), true);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P1!uA\033\\");

        Log::Comment(L"DECRQUPSS: ISO Latin-2");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("B"), true);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P1!uB\033\\");

        Log::Comment(L"DECRQUPSS: ISO Latin-Greek");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("F"), true);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P1!uF\033\\");

        Log::Comment(L"DECRQUPSS: ISO Latin-Hebrew");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("H"), true);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P1!uH\033\\");

        Log::Comment(L"DECRQUPSS: ISO Latin-Cyrillic");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("L"), true);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P1!uL\033\\");

        Log::Comment(L"DECRQUPSS: ISO Latin-5");
        _testGetSet->PrepData();
        termOutput.AssignUserPreferenceCharset(VTID("M"), true);
        _pDispatch->RequestUserPreferenceCharset();
        _testGetSet->ValidateInputEvent(L"\033P1!uM\033\\");
    }

    TEST_METHOD(MacroDefinitions)
    {
        const auto getMacroText = [&](const auto id) {
            return _pDispatch->_macroBuffer->_macros.at(id);
        };

        Log::Comment(L"Text encoding");
        _stateMachine->ProcessString(L"\033P1;0;0!zText Encoding\033\\");
        VERIFY_ARE_EQUAL(L"Text Encoding", getMacroText(1));

        Log::Comment(L"Hex encoding (uppercase)");
        _stateMachine->ProcessString(L"\033P2;0;1!z486578204A4B4C4D4E4F\033\\");
        VERIFY_ARE_EQUAL(L"Hex JKLMNO", getMacroText(2));

        Log::Comment(L"Hex encoding (lowercase)");
        _stateMachine->ProcessString(L"\033P3;0;1!z486578206a6b6c6d6e6f\033\\");
        VERIFY_ARE_EQUAL(L"Hex jklmno", getMacroText(3));

        Log::Comment(L"Default encoding is text");
        _stateMachine->ProcessString(L"\033P4;0;!zDefault Encoding\033\\");
        VERIFY_ARE_EQUAL(L"Default Encoding", getMacroText(4));

        Log::Comment(L"Default ID is 0");
        _stateMachine->ProcessString(L"\033P;0;0!zDefault ID\033\\");
        VERIFY_ARE_EQUAL(L"Default ID", getMacroText(0));

        Log::Comment(L"Replacing a single macro");
        _stateMachine->ProcessString(L"\033P1;0;0!zRetained\033\\");
        _stateMachine->ProcessString(L"\033P2;0;0!zReplaced\033\\");
        _stateMachine->ProcessString(L"\033P2;0;0!zNew\033\\");
        VERIFY_ARE_EQUAL(L"Retained", getMacroText(1));
        VERIFY_ARE_EQUAL(L"New", getMacroText(2));

        Log::Comment(L"Replacing all macros");
        _stateMachine->ProcessString(L"\033P1;0;0!zErased\033\\");
        _stateMachine->ProcessString(L"\033P2;0;0!zReplaced\033\\");
        _stateMachine->ProcessString(L"\033P2;1;0!zNew\033\\");
        VERIFY_ARE_EQUAL(L"", getMacroText(1));
        VERIFY_ARE_EQUAL(L"New", getMacroText(2));

        Log::Comment(L"Default replacement is a single macro");
        _stateMachine->ProcessString(L"\033P1;0;0!zRetained\033\\");
        _stateMachine->ProcessString(L"\033P2;0;0!zReplaced\033\\");
        _stateMachine->ProcessString(L"\033P2;;0!zNew\033\\");
        VERIFY_ARE_EQUAL(L"Retained", getMacroText(1));
        VERIFY_ARE_EQUAL(L"New", getMacroText(2));

        Log::Comment(L"Repeating three times");
        _stateMachine->ProcessString(L"\033P5;0;1!z526570656174!3;206563686F;207468726565\033\\");
        VERIFY_ARE_EQUAL(L"Repeat echo echo echo three", getMacroText(5));

        Log::Comment(L"Zero repeats once");
        _stateMachine->ProcessString(L"\033P6;0;1!z526570656174!0;206563686F;207A65726F\033\\");
        VERIFY_ARE_EQUAL(L"Repeat echo zero", getMacroText(6));

        Log::Comment(L"Default repeats once");
        _stateMachine->ProcessString(L"\033P7;0;1!z526570656174!;206563686F;2064656661756C74\033\\");
        VERIFY_ARE_EQUAL(L"Repeat echo default", getMacroText(7));

        Log::Comment(L"Unterminated repeat sequence");
        _stateMachine->ProcessString(L"\033P8;0;1!z556E7465726D696E61746564!3;206563686F\033\\");
        VERIFY_ARE_EQUAL(L"Unterminated echo echo echo", getMacroText(8));

        Log::Comment(L"Unexpected semicolon cancels definition");
        _stateMachine->ProcessString(L"\033P9;0;0!zReplaced\033\\");
        _stateMachine->ProcessString(L"\033P9;0;1!z526570656174!3;206563;686F;207468726565\033\\");
        VERIFY_ARE_EQUAL(L"", getMacroText(9));

        Log::Comment(L"Control characters in a text encoding");
        _stateMachine->ProcessString(L"\033P10;0;0!zA\aB\bC\tD\nE\vF\fG\rH\033\\");
        VERIFY_ARE_EQUAL(L"ABCDEFGH", getMacroText(10));

        Log::Comment(L"Control characters in a hex encoding");
        _stateMachine->ProcessString(L"\033P11;0;1!z41\a42\b43\t44\n45\v46\f47\r48\033\\");
        VERIFY_ARE_EQUAL(L"ABCDEFGH", getMacroText(11));

        Log::Comment(L"Control characters in a repeat");
        _stateMachine->ProcessString(L"\033P12;0;1!z!\a3\b;\t4\n1\v4\f2\r4\a3\b;\033\\");
        VERIFY_ARE_EQUAL(L"ABCABCABC", getMacroText(12));

        Log::Comment(L"Encoded control characters");
        _stateMachine->ProcessString(L"\033P13;0;1!z410742084309440A450B460C470D481B49\033\\");
        VERIFY_ARE_EQUAL(L"A\aB\bC\tD\nE\vF\fG\rH\033I", getMacroText(13));

        _pDispatch->_macroBuffer = nullptr;
    }

    TEST_METHOD(MacroInvokes)
    {
        _pDispatch->_macroBuffer = std::make_shared<MacroBuffer>();

        const auto setMacroText = [&](const auto id, const auto value) {
            _pDispatch->_macroBuffer->_macros.at(id) = value;
        };

        setMacroText(0, L"Macro 0");
        setMacroText(1, L"Macro 1");
        setMacroText(2, L"Macro 2");
        setMacroText(63, L"Macro 63");

        const auto getBufferOutput = [&]() {
            const auto& textBuffer = _testGetSet->GetBufferAndViewport().buffer;
            const auto cursorPos = textBuffer.GetCursor().GetPosition();
            return textBuffer.GetRowByOffset(cursorPos.y).GetText().substr(0, cursorPos.x);
        };

        Log::Comment(L"Simple macro invoke");
        _testGetSet->PrepData();
        _stateMachine->ProcessString(L"\033[2*z");
        VERIFY_ARE_EQUAL(L"Macro 2", getBufferOutput());

        Log::Comment(L"Default macro invoke");
        _testGetSet->PrepData();
        _stateMachine->ProcessString(L"\033[*z");
        VERIFY_ARE_EQUAL(L"Macro 0", getBufferOutput());

        Log::Comment(L"Maximum ID number");
        _testGetSet->PrepData();
        _stateMachine->ProcessString(L"\033[63*z");
        VERIFY_ARE_EQUAL(L"Macro 63", getBufferOutput());

        Log::Comment(L"Out of range ID number");
        _testGetSet->PrepData();
        _stateMachine->ProcessString(L"\033[64*z");
        VERIFY_ARE_EQUAL(L"", getBufferOutput());

        Log::Comment(L"Only one ID parameter allowed");
        _testGetSet->PrepData();
        _stateMachine->ProcessString(L"\033[2;0;1*z");
        VERIFY_ARE_EQUAL(L"Macro 2", getBufferOutput());

        Log::Comment(L"DECDMAC ignored when inside a macro");
        setMacroText(10, L"[\033P1;0;0!zReplace Macro 1\033\\]");
        _testGetSet->PrepData();
        _stateMachine->ProcessString(L"\033[10*z");
        _stateMachine->ProcessString(L"\033[1*z");
        VERIFY_ARE_EQUAL(L"[]Macro 1", getBufferOutput());

        Log::Comment(L"Maximum recursive depth is 16");
        setMacroText(0, L"<\033[1*z>");
        setMacroText(1, L"[\033[0*z]");
        _testGetSet->PrepData();
        _stateMachine->ProcessString(L"\033[0*z");
        VERIFY_ARE_EQUAL(L"<[<[<[<[<[<[<[<[]>]>]>]>]>]>]>]>", getBufferOutput());

        _pDispatch->_macroBuffer = nullptr;
    }

    TEST_METHOD(WindowManipulationTypeTests)
    {
        // Our pixel size reports are based on a virtual cell size of 10x20 pixels
        // for compatibility with the VT240 and VT340 graphic terminals.
        const auto cellSize = til::size{ 10, 20 };

        _testGetSet->PrepData();
        const auto [textBuffer, viewport, _] = _testGetSet->GetBufferAndViewport();

        _pDispatch->WindowManipulation(DispatchTypes::WindowManipulationType::ReportTextSizeInCharacters, NULL, NULL);
        std::wstring expectedResponse = fmt::format(L"\033[8;{};{}t", viewport.height(), textBuffer.GetSize().Width());
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->WindowManipulation(DispatchTypes::WindowManipulationType::ReportTextSizeInPixels, NULL, NULL);
        expectedResponse = fmt::format(L"\033[4;{};{}t", viewport.height() * cellSize.height, textBuffer.GetSize().Width() * cellSize.width);
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());

        _pDispatch->WindowManipulation(DispatchTypes::WindowManipulationType::ReportCharacterCellSize, NULL, NULL);
        expectedResponse = fmt::format(L"\033[6;{};{}t", cellSize.height, cellSize.width);
        _testGetSet->ValidateInputEvent(expectedResponse.c_str());
    }

    TEST_METHOD(MenuCompletionsTests)
    {
        _testGetSet->PrepData();

        Log::Comment(L"Not enough parameters");
        _pDispatch->DoVsCodeAction(LR"(garbage)");

        Log::Comment(L"Not enough parameters");
        _pDispatch->DoVsCodeAction(LR"(Completions)");
        _pDispatch->DoVsCodeAction(LR"(Completions;)");
        _pDispatch->DoVsCodeAction(LR"(Completions;10;)");
        _pDispatch->DoVsCodeAction(LR"(Completions;10;20)");
        _pDispatch->DoVsCodeAction(LR"(Completions;10;20;)");
        Log::Comment(L"No trailing semicolon");
        _pDispatch->DoVsCodeAction(LR"(Completions;10;20;3)");

        Log::Comment(L"Normal, good case");
        _testGetSet->_expectedMenuJson = LR"({ "foo": 1, "bar": 2 })";
        _testGetSet->_expectedReplaceLength = 2;
        _pDispatch->DoVsCodeAction(LR"(Completions;1;2;3;{ "foo": 1, "bar": 2 })");

        Log::Comment(L"JSON has a semicolon in it");
        _testGetSet->_expectedMenuJson = LR"({ "foo": "what;ever", "bar": 2 })";
        _testGetSet->_expectedReplaceLength = 20;
        _pDispatch->DoVsCodeAction(LR"(Completions;10;20;30;{ "foo": "what;ever", "bar": 2 })");
    }

    TEST_METHOD(PageMovementTests)
    {
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);
        auto& pages = _pDispatch->_pages;
        const auto startPos = pages.ActivePage().Cursor().GetPosition();
        const auto homePos = til::point{ 0, pages.ActivePage().Top() };

        // Testing PPA (page position absolute)
        VERIFY_ARE_EQUAL(1, pages.ActivePage().Number(), L"Initial page is 1");
        _pDispatch->PagePositionAbsolute(3);
        VERIFY_ARE_EQUAL(3, pages.ActivePage().Number(), L"PPA 3 moves to page 3");
        _pDispatch->PagePositionAbsolute(VTParameter{});
        VERIFY_ARE_EQUAL(1, pages.ActivePage().Number(), L"PPA with omitted page moves to 1");
        _pDispatch->PagePositionAbsolute(9999);
        VERIFY_ARE_EQUAL(6, pages.ActivePage().Number(), L"PPA is clamped at page 6");
        VERIFY_ARE_EQUAL(startPos, pages.ActivePage().Cursor().GetPosition(), L"Cursor position never changes");

        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);
        _pDispatch->PagePositionAbsolute(1); // Reset to page 1

        // Testing PPR (page position relative)
        VERIFY_ARE_EQUAL(1, pages.ActivePage().Number(), L"Initial page is 1");
        _pDispatch->PagePositionRelative(2);
        VERIFY_ARE_EQUAL(3, pages.ActivePage().Number(), L"PPR 2 moves forward 2 pages");
        _pDispatch->PagePositionRelative(VTParameter{});
        VERIFY_ARE_EQUAL(4, pages.ActivePage().Number(), L"PPR with omitted count moves forward 1");
        _pDispatch->PagePositionRelative(9999);
        VERIFY_ARE_EQUAL(6, pages.ActivePage().Number(), L"PPR is clamped at page 6");
        VERIFY_ARE_EQUAL(startPos, pages.ActivePage().Cursor().GetPosition(), L"Cursor position never changes");

        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

        // Testing PPB (page position back)
        VERIFY_ARE_EQUAL(6, pages.ActivePage().Number(), L"Initial page is 6");
        _pDispatch->PagePositionBack(2);
        VERIFY_ARE_EQUAL(4, pages.ActivePage().Number(), L"PPB 2 moves back 2 pages");
        _pDispatch->PagePositionBack(VTParameter{});
        VERIFY_ARE_EQUAL(3, pages.ActivePage().Number(), L"PPB with omitted count moves back 1");
        _pDispatch->PagePositionBack(9999);
        VERIFY_ARE_EQUAL(1, pages.ActivePage().Number(), L"PPB is clamped at page 1");
        VERIFY_ARE_EQUAL(startPos, pages.ActivePage().Cursor().GetPosition(), L"Cursor position never changes");

        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

        // Testing NP (next page)
        VERIFY_ARE_EQUAL(1, pages.ActivePage().Number(), L"Initial page is 1");
        _pDispatch->NextPage(2);
        VERIFY_ARE_EQUAL(3, pages.ActivePage().Number(), L"NP 2 moves forward 2 pages");
        _pDispatch->NextPage(VTParameter{});
        VERIFY_ARE_EQUAL(4, pages.ActivePage().Number(), L"NP with omitted count moves forward 1");
        _pDispatch->NextPage(9999);
        VERIFY_ARE_EQUAL(6, pages.ActivePage().Number(), L"NP is clamped at page 6");
        VERIFY_ARE_EQUAL(homePos, pages.ActivePage().Cursor().GetPosition(), L"Cursor position is reset to home");

        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

        // Testing PP (preceding page)
        VERIFY_ARE_EQUAL(6, pages.ActivePage().Number(), L"Initial page is 6");
        _pDispatch->PrecedingPage(2);
        VERIFY_ARE_EQUAL(4, pages.ActivePage().Number(), L"PP 2 moves back 2 pages");
        _pDispatch->PrecedingPage(VTParameter{});
        VERIFY_ARE_EQUAL(3, pages.ActivePage().Number(), L"PP with omitted count moves back 1");
        _pDispatch->PrecedingPage(9999);
        VERIFY_ARE_EQUAL(1, pages.ActivePage().Number(), L"PP is clamped at page 1");
        VERIFY_ARE_EQUAL(homePos, pages.ActivePage().Cursor().GetPosition(), L"Cursor position is reset to home");

        // Testing DECPCCM (page cursor coupling mode)
        _pDispatch->SetMode(DispatchTypes::ModeParams::DECPCCM_PageCursorCouplingMode);
        _pDispatch->PagePositionAbsolute(2);
        VERIFY_ARE_EQUAL(2, pages.ActivePage().Number());
        VERIFY_ARE_EQUAL(2, pages.VisiblePage().Number(), L"Visible page should follow active if DECPCCM set");
        _pDispatch->ResetMode(DispatchTypes::ModeParams::DECPCCM_PageCursorCouplingMode);
        _pDispatch->PagePositionAbsolute(4);
        VERIFY_ARE_EQUAL(4, pages.ActivePage().Number());
        VERIFY_ARE_EQUAL(2, pages.VisiblePage().Number(), L"Visible page should not change if DECPCCM reset");
        _pDispatch->SetMode(DispatchTypes::ModeParams::DECPCCM_PageCursorCouplingMode);
        VERIFY_ARE_EQUAL(4, pages.ActivePage().Number());
        VERIFY_ARE_EQUAL(4, pages.VisiblePage().Number(), L"Active page should become visible when DECPCCM set");

        // Reset to page 1
        _pDispatch->PagePositionAbsolute(1);
    }

    TEST_METHOD(SendC1ControlTest)
    {
        const auto S7C1T = L"\033 F";
        const auto S8C1T = L"\033 G";

        _testGetSet->PrepData();
        _testGetSet->_expectedCodePage = CP_UTF8;

        Log::Comment(L"Generating reports with C1 control sequences");
        _stateMachine->ProcessString(S8C1T);

        _pDispatch->SecondaryDeviceAttributes();
        _testGetSet->ValidateInputEvent(L"\x9b>0;10;1c");

        _pDispatch->TertiaryDeviceAttributes();
        _testGetSet->ValidateInputEvent(L"\x90!|00000000\x9c");

        _pDispatch->RequestColorTableEntry(0);
        _testGetSet->ValidateInputEvent(L"\x9d\x34;0;rgb:0c0c/0c0c/0c0c\x9c");

        Log::Comment(L"Generating reports with 7-bit escape sequence");
        _stateMachine->ProcessString(S7C1T);

        _pDispatch->SecondaryDeviceAttributes();
        _testGetSet->ValidateInputEvent(L"\x1b[>0;10;1c");

        _pDispatch->TertiaryDeviceAttributes();
        _testGetSet->ValidateInputEvent(L"\x1bP!|00000000\x1b\\");

        _pDispatch->RequestColorTableEntry(0);
        _testGetSet->ValidateInputEvent(L"\x1b]4;0;rgb:0c0c/0c0c/0c0c\x1b\\");
    }

private:
    TerminalInput _terminalInput;
    std::unique_ptr<TestGetSet> _testGetSet;
    AdaptDispatch* _pDispatch; // non-ownership pointer
    std::unique_ptr<StateMachine> _stateMachine;
};
