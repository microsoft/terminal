// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "settings.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/colorTable.hpp"

#pragma hdrstop

constexpr unsigned int DEFAULT_NUMBER_OF_COMMANDS = 25;
constexpr unsigned int DEFAULT_NUMBER_OF_BUFFERS = 4;

using Microsoft::Console::Interactivity::ServiceLocator;

Settings::Settings() :
    _dwHotKey(0),
    _dwStartupFlags(0),
    _wFillAttribute(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE), // White (not bright) on black by default
    _wPopupFillAttribute(FOREGROUND_RED | FOREGROUND_BLUE | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY), // Purple on white (bright) by default
    _wShowWindow(SW_SHOWNORMAL),
    _wReserved(0),
    // dwScreenBufferSize initialized below
    // dwWindowSize initialized below
    // dwWindowOrigin initialized below
    _nFont(0),
    // dwFontSize initialized below
    _uFontFamily(0),
    _uFontWeight(0),
    // FaceName initialized below
    _uCursorSize(Cursor::CURSOR_SMALL_SIZE),
    _bFullScreen(false),
    _bQuickEdit(true),
    _bInsertMode(true),
    _bAutoPosition(true),
    _uHistoryBufferSize(DEFAULT_NUMBER_OF_COMMANDS),
    _uNumberOfHistoryBuffers(DEFAULT_NUMBER_OF_BUFFERS),
    _bHistoryNoDup(false),
    // ColorTable initialized below
    _uCodePage(ServiceLocator::LocateGlobals().uiOEMCP),
    _uScrollScale(1),
    _bLineSelection(true),
    _bWrapText(true),
    _fCtrlKeyShortcutsDisabled(false),
    _bWindowAlpha(BYTE_MAX), // 255 alpha = opaque. 0 = transparent.
    _fFilterOnPaste(false),
    _LaunchFaceName{},
    _fTrimLeadingZeros(FALSE),
    _fEnableColorSelection(FALSE),
    _fAllowAltF4Close(true),
    _dwVirtTermLevel(0),
    _fUseWindowSizePixels(false),
    // window size pixels initialized below
    _fInterceptCopyPaste(0),
    _fUseDx(UseDx::Disabled),
    _fCopyColor(false)
{
    _dwScreenBufferSize.X = 80;
    _dwScreenBufferSize.Y = 25;

    _dwWindowSize.X = _dwScreenBufferSize.X;
    _dwWindowSize.Y = _dwScreenBufferSize.Y;

    _dwWindowSizePixels = { 0 };

    _dwWindowOrigin.X = 0;
    _dwWindowOrigin.Y = 0;

    _dwFontSize.X = 0;
    _dwFontSize.Y = 16;

    ZeroMemory((void*)&_FaceName, sizeof(_FaceName));
    wcscpy_s(_FaceName, DEFAULT_TT_FONT_FACENAME);

    _CursorType = CursorType::Legacy;
}

// Routine Description:
// - Applies hard-coded default settings that are in line with what is defined
//   in our Windows edition manifest (living in win32k-settings.man).
// - NOTE: This exists in case we cannot access the registry on desktop platforms.
//   We will use this to provide better defaults than the constructor values which
//   are optimized for OneCore.
// Arguments:
// - <none>
// Return Value:
// - <none> - Adjusts internal state only.
void Settings::ApplyDesktopSpecificDefaults()
{
    _dwFontSize.X = 0;
    _dwFontSize.Y = 16;
    _uFontFamily = 0;
    _dwScreenBufferSize.X = 120;
    _dwScreenBufferSize.Y = 9001;
    _uCursorSize = 25;
    _dwWindowSize.X = 120;
    _dwWindowSize.Y = 30;
    _wFillAttribute = 0x7;
    _wPopupFillAttribute = 0xf5;
    wcscpy_s(_FaceName, L"__DefaultTTFont__");
    _uFontWeight = 0;
    _bInsertMode = TRUE;
    _bFullScreen = FALSE;
    _fCtrlKeyShortcutsDisabled = false;
    _bWrapText = true;
    _bLineSelection = TRUE;
    _bWindowAlpha = 255;
    _fFilterOnPaste = TRUE;
    _bQuickEdit = TRUE;
    _uHistoryBufferSize = 50;
    _uNumberOfHistoryBuffers = 4;
    _bHistoryNoDup = FALSE;

    _renderSettings.ResetColorTable();

    _fTrimLeadingZeros = false;
    _fEnableColorSelection = false;
    _uScrollScale = 1;
}

void Settings::ApplyStartupInfo(const Settings* const pStartupSettings)
{
    const auto dwFlags = pStartupSettings->_dwStartupFlags;

    // See: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686331(v=vs.85).aspx

    // Note: These attributes do not get sent to us if we started conhost
    //      directly.  See minkernel/console/client/dllinit for the
    //      initialization of these values for cmdline applications.

    if (WI_IsFlagSet(dwFlags, STARTF_USECOUNTCHARS))
    {
        _dwScreenBufferSize = pStartupSettings->_dwScreenBufferSize;
    }

    if (WI_IsFlagSet(dwFlags, STARTF_USESIZE))
    {
        // WARNING: This size is in pixels when passed in the create process call.
        // It will need to be divided by the font size before use.
        // All other Window Size values (from registry/shortcuts) are stored in characters.
        _dwWindowSizePixels = pStartupSettings->_dwWindowSize;
        _fUseWindowSizePixels = true;
    }

    if (WI_IsFlagSet(dwFlags, STARTF_USEPOSITION))
    {
        _dwWindowOrigin = pStartupSettings->_dwWindowOrigin;
        _bAutoPosition = FALSE;
    }

    if (WI_IsFlagSet(dwFlags, STARTF_USEFILLATTRIBUTE))
    {
        _wFillAttribute = pStartupSettings->_wFillAttribute;
    }

    if (WI_IsFlagSet(dwFlags, STARTF_USESHOWWINDOW))
    {
        _wShowWindow = pStartupSettings->_wShowWindow;
    }
}

// Method Description:
// - Applies settings passed on the commandline to this settings structure.
//      Currently, the only settings that can be passed on the commandline are
//      the initial width and height of the screenbuffer/viewport.
// Arguments:
// - consoleArgs: A reference to a parsed command-line args object.
// Return Value:
// - <none>
void Settings::ApplyCommandlineArguments(const ConsoleArguments& consoleArgs)
{
    const auto width = consoleArgs.GetWidth();
    const auto height = consoleArgs.GetHeight();

    if (width > 0 && height > 0)
    {
        _dwScreenBufferSize.X = width;
        _dwWindowSize.X = width;

        _dwScreenBufferSize.Y = height;
        _dwWindowSize.Y = height;
    }
    else if (ServiceLocator::LocateGlobals().getConsoleInformation().IsInVtIoMode())
    {
        // If we're a PTY but we weren't explicitly told a size, use the window size as the buffer size.
        _dwScreenBufferSize = _dwWindowSize;
    }
}

// WARNING: this function doesn't perform any validation or conversion
void Settings::InitFromStateInfo(_In_ PCONSOLE_STATE_INFO pStateInfo)
{
    _wFillAttribute = pStateInfo->ScreenAttributes;
    _wPopupFillAttribute = pStateInfo->PopupAttributes;
    _dwScreenBufferSize = pStateInfo->ScreenBufferSize;
    _dwWindowSize = pStateInfo->WindowSize;
    _dwWindowOrigin.X = (SHORT)pStateInfo->WindowPosX;
    _dwWindowOrigin.Y = (SHORT)pStateInfo->WindowPosY;
    _dwFontSize = pStateInfo->FontSize;
    _uFontFamily = pStateInfo->FontFamily;
    _uFontWeight = pStateInfo->FontWeight;
    StringCchCopyW(_FaceName, ARRAYSIZE(_FaceName), pStateInfo->FaceName);
    _uCursorSize = pStateInfo->CursorSize;
    _bFullScreen = pStateInfo->FullScreen;
    _bQuickEdit = pStateInfo->QuickEdit;
    _bAutoPosition = pStateInfo->AutoPosition;
    _bInsertMode = pStateInfo->InsertMode;
    _bHistoryNoDup = pStateInfo->HistoryNoDup;
    _uHistoryBufferSize = pStateInfo->HistoryBufferSize;
    _uNumberOfHistoryBuffers = pStateInfo->NumberOfHistoryBuffers;
    for (size_t i = 0; i < std::size(pStateInfo->ColorTable); i++)
    {
        SetLegacyColorTableEntry(i, pStateInfo->ColorTable[i]);
    }
    _uCodePage = pStateInfo->CodePage;
    _bWrapText = !!pStateInfo->fWrapText;
    _fFilterOnPaste = pStateInfo->fFilterOnPaste;
    _fCtrlKeyShortcutsDisabled = pStateInfo->fCtrlKeyShortcutsDisabled;
    _bLineSelection = pStateInfo->fLineSelection;
    _bWindowAlpha = pStateInfo->bWindowTransparency;
    _CursorType = static_cast<CursorType>(pStateInfo->CursorType);
    _fInterceptCopyPaste = pStateInfo->InterceptCopyPaste;
    SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, pStateInfo->DefaultForeground);
    SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, pStateInfo->DefaultBackground);
    SetColorTableEntry(TextColor::CURSOR_COLOR, pStateInfo->CursorColor);
    _TerminalScrolling = pStateInfo->TerminalScrolling;
}

// Method Description:
// - Create a CONSOLE_STATE_INFO with the current state of this settings structure.
// Arguments:
// - <none>
// Return Value:
// - a CONSOLE_STATE_INFO with the current state of this settings structure.
CONSOLE_STATE_INFO Settings::CreateConsoleStateInfo() const
{
    CONSOLE_STATE_INFO csi = { 0 };
    csi.ScreenAttributes = _wFillAttribute;
    csi.PopupAttributes = _wPopupFillAttribute;
    csi.ScreenBufferSize = _dwScreenBufferSize;
    csi.WindowSize = _dwWindowSize;
    csi.WindowPosX = (SHORT)_dwWindowOrigin.X;
    csi.WindowPosY = (SHORT)_dwWindowOrigin.Y;
    csi.FontSize = _dwFontSize;
    csi.FontFamily = _uFontFamily;
    csi.FontWeight = _uFontWeight;
    StringCchCopyW(csi.FaceName, ARRAYSIZE(_FaceName), _FaceName);
    csi.CursorSize = _uCursorSize;
    csi.FullScreen = _bFullScreen;
    csi.QuickEdit = _bQuickEdit;
    csi.AutoPosition = _bAutoPosition;
    csi.InsertMode = _bInsertMode;
    csi.HistoryNoDup = _bHistoryNoDup;
    csi.HistoryBufferSize = _uHistoryBufferSize;
    csi.NumberOfHistoryBuffers = _uNumberOfHistoryBuffers;
    for (size_t i = 0; i < std::size(csi.ColorTable); i++)
    {
        csi.ColorTable[i] = GetLegacyColorTableEntry(i);
    }
    csi.CodePage = _uCodePage;
    csi.fWrapText = !!_bWrapText;
    csi.fFilterOnPaste = _fFilterOnPaste;
    csi.fCtrlKeyShortcutsDisabled = _fCtrlKeyShortcutsDisabled;
    csi.fLineSelection = _bLineSelection;
    csi.bWindowTransparency = _bWindowAlpha;
    csi.CursorType = static_cast<unsigned int>(_CursorType);
    csi.InterceptCopyPaste = _fInterceptCopyPaste;
    csi.DefaultForeground = GetColorTableEntry(TextColor::DEFAULT_FOREGROUND);
    csi.DefaultBackground = GetColorTableEntry(TextColor::DEFAULT_BACKGROUND);
    csi.CursorColor = GetColorTableEntry(TextColor::CURSOR_COLOR);
    csi.TerminalScrolling = _TerminalScrolling;
    return csi;
}

void Settings::Validate()
{
    // If we were explicitly given a size in pixels from the startup info, divide by the font to turn it into characters.
    // See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms686331%28v=vs.85%29.aspx
    if (WI_IsFlagSet(_dwStartupFlags, STARTF_USESIZE))
    {
        // TODO: FIX
        //// Get the font that we're going to use to convert pixels to characters.
        // const auto dwFontIndexWant = FindCreateFont(_uFontFamily,
        //                                             _FaceName,
        //                                             _dwFontSize,
        //                                             _uFontWeight,
        //                                             _uCodePage);

        //_dwWindowSize.X /= g_pfiFontInfo[dwFontIndexWant].Size.X;
        //_dwWindowSize.Y /= g_pfiFontInfo[dwFontIndexWant].Size.Y;
    }

    // minimum screen buffer size 1x1
    _dwScreenBufferSize.X = std::max(_dwScreenBufferSize.X, 1i16);
    _dwScreenBufferSize.Y = std::max(_dwScreenBufferSize.Y, 1i16);

    // minimum window size 1x1
    _dwWindowSize.X = std::max(_dwWindowSize.X, 1i16);
    _dwWindowSize.Y = std::max(_dwWindowSize.Y, 1i16);

    // if buffer size is less than window size, increase buffer size to meet window size
    _dwScreenBufferSize.X = std::max(_dwWindowSize.X, _dwScreenBufferSize.X);
    _dwScreenBufferSize.Y = std::max(_dwWindowSize.Y, _dwScreenBufferSize.Y);

    // ensure that the window alpha value is not below the minimum. (no invisible windows)
    // if it's below minimum, just set it to the opaque value
    if (_bWindowAlpha < MIN_WINDOW_OPACITY)
    {
        _bWindowAlpha = BYTE_MAX;
    }

    // If text wrapping is on, ensure that the window width is the same as the buffer width.
    if (_bWrapText)
    {
        _dwWindowSize.X = _dwScreenBufferSize.X;
    }

    // Ensure that our fill attributes only contain colors and not any box drawing or invert attributes.
    WI_ClearAllFlags(_wFillAttribute, ~(FG_ATTRS | BG_ATTRS));
    WI_ClearAllFlags(_wPopupFillAttribute, ~(FG_ATTRS | BG_ATTRS));

    const auto defaultForeground = GetColorTableEntry(TextColor::DEFAULT_FOREGROUND);
    const auto defaultBackground = GetColorTableEntry(TextColor::DEFAULT_BACKGROUND);
    const auto cursorColor = GetColorTableEntry(TextColor::CURSOR_COLOR);

    // If the extended color options are set to invalid values (all the same color), reset them.
    if (cursorColor != INVALID_COLOR && cursorColor == defaultBackground)
    {
        // INVALID_COLOR is used to represent "Invert Colors"
        SetColorTableEntry(TextColor::CURSOR_COLOR, INVALID_COLOR);
    }

    if (defaultForeground != INVALID_COLOR && defaultForeground == defaultBackground)
    {
        // INVALID_COLOR is used as an "unset" sentinel in future attribute functions.
        SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, INVALID_COLOR);
        SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, INVALID_COLOR);
        // If the damaged settings _further_ propagated to the default fill attribute, fix it.
        if (_wFillAttribute == 0)
        {
            // These attributes were taken from the Settings ctor and equal "gray on black"
            _wFillAttribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        }
    }

    // At this point the default fill attributes are fully initialized
    // so we can pass on the final colors to the TextAttribute class.
    TextAttribute::SetLegacyDefaultAttributes(_wFillAttribute);
    // And calculate the position of the default colors in the color table.
    CalculateDefaultColorIndices();

    FAIL_FAST_IF(!(_dwWindowSize.X > 0));
    FAIL_FAST_IF(!(_dwWindowSize.Y > 0));
    FAIL_FAST_IF(!(_dwScreenBufferSize.X > 0));
    FAIL_FAST_IF(!(_dwScreenBufferSize.Y > 0));
}

DWORD Settings::GetDefaultVirtTermLevel() const
{
    return _dwVirtTermLevel;
}
void Settings::SetDefaultVirtTermLevel(const DWORD dwVirtTermLevel)
{
    _dwVirtTermLevel = dwVirtTermLevel;
}

bool Settings::IsAltF4CloseAllowed() const
{
    return _fAllowAltF4Close;
}
void Settings::SetAltF4CloseAllowed(const bool fAllowAltF4Close)
{
    _fAllowAltF4Close = fAllowAltF4Close;
}

bool Settings::GetFilterOnPaste() const
{
    return _fFilterOnPaste;
}
void Settings::SetFilterOnPaste(const bool fFilterOnPaste)
{
    _fFilterOnPaste = fFilterOnPaste;
}

const std::wstring_view Settings::GetLaunchFaceName() const
{
    return _LaunchFaceName;
}
void Settings::SetLaunchFaceName(const std::wstring_view launchFaceName)
{
    _LaunchFaceName = launchFaceName;
}

UINT Settings::GetCodePage() const
{
    return _uCodePage;
}
void Settings::SetCodePage(const UINT uCodePage)
{
    _uCodePage = uCodePage;
}

UINT Settings::GetScrollScale() const
{
    return _uScrollScale;
}
void Settings::SetScrollScale(const UINT uScrollScale)
{
    _uScrollScale = uScrollScale;
}

bool Settings::GetTrimLeadingZeros() const
{
    return _fTrimLeadingZeros;
}
void Settings::SetTrimLeadingZeros(const bool fTrimLeadingZeros)
{
    _fTrimLeadingZeros = fTrimLeadingZeros;
}

bool Settings::GetEnableColorSelection() const
{
    return _fEnableColorSelection;
}
void Settings::SetEnableColorSelection(const bool fEnableColorSelection)
{
    _fEnableColorSelection = fEnableColorSelection;
}

bool Settings::GetLineSelection() const
{
    return _bLineSelection;
}
void Settings::SetLineSelection(const bool bLineSelection)
{
    _bLineSelection = bLineSelection;
}

bool Settings::GetWrapText() const
{
    return _bWrapText;
}
void Settings::SetWrapText(const bool bWrapText)
{
    _bWrapText = bWrapText;
}

bool Settings::GetCtrlKeyShortcutsDisabled() const
{
    return _fCtrlKeyShortcutsDisabled;
}
void Settings::SetCtrlKeyShortcutsDisabled(const bool fCtrlKeyShortcutsDisabled)
{
    _fCtrlKeyShortcutsDisabled = fCtrlKeyShortcutsDisabled;
}

BYTE Settings::GetWindowAlpha() const
{
    return _bWindowAlpha;
}
void Settings::SetWindowAlpha(const BYTE bWindowAlpha)
{
    // if we're out of bounds, set it to 100% opacity so it appears as if nothing happened.
    _bWindowAlpha = (bWindowAlpha < MIN_WINDOW_OPACITY) ? BYTE_MAX : bWindowAlpha;
}

DWORD Settings::GetHotKey() const
{
    return _dwHotKey;
}
void Settings::SetHotKey(const DWORD dwHotKey)
{
    _dwHotKey = dwHotKey;
}

DWORD Settings::GetStartupFlags() const
{
    return _dwStartupFlags;
}
void Settings::SetStartupFlags(const DWORD dwStartupFlags)
{
    _dwStartupFlags = dwStartupFlags;
}

WORD Settings::GetFillAttribute() const
{
    return _wFillAttribute;
}
void Settings::SetFillAttribute(const WORD wFillAttribute)
{
    _wFillAttribute = wFillAttribute;

    // Do not allow the default fill attribute to use any attrs other than fg/bg colors.
    // This prevents us from accidentally inverting everything or suddenly drawing lines
    // everywhere by default.
    WI_ClearAllFlags(_wFillAttribute, ~(FG_ATTRS | BG_ATTRS));
}

WORD Settings::GetPopupFillAttribute() const
{
    return _wPopupFillAttribute;
}
void Settings::SetPopupFillAttribute(const WORD wPopupFillAttribute)
{
    _wPopupFillAttribute = wPopupFillAttribute;

    // Do not allow the default popup fill attribute to use any attrs other than fg/bg colors.
    // This prevents us from accidentally inverting everything or suddenly drawing lines
    // everywhere by default.
    WI_ClearAllFlags(_wPopupFillAttribute, ~(FG_ATTRS | BG_ATTRS));
}

WORD Settings::GetShowWindow() const
{
    return _wShowWindow;
}
void Settings::SetShowWindow(const WORD wShowWindow)
{
    _wShowWindow = wShowWindow;
}

WORD Settings::GetReserved() const
{
    return _wReserved;
}
void Settings::SetReserved(const WORD wReserved)
{
    _wReserved = wReserved;
}

til::size Settings::GetScreenBufferSize() const
{
    return til::wrap_coord_size(_dwScreenBufferSize);
}
void Settings::SetScreenBufferSize(const til::size dwScreenBufferSize)
{
    LOG_IF_FAILED(til::unwrap_coord_size_hr(dwScreenBufferSize, _dwScreenBufferSize));
}

til::size Settings::GetWindowSize() const
{
    return til::wrap_coord_size(_dwWindowSize);
}
void Settings::SetWindowSize(const til::size dwWindowSize)
{
    LOG_IF_FAILED(til::unwrap_coord_size_hr(dwWindowSize, _dwWindowSize));
}

bool Settings::IsWindowSizePixelsValid() const
{
    return _fUseWindowSizePixels;
}
til::size Settings::GetWindowSizePixels() const
{
    return til::wrap_coord_size(_dwWindowSizePixels);
}
void Settings::SetWindowSizePixels(const til::size dwWindowSizePixels)
{
    LOG_IF_FAILED(til::unwrap_coord_size_hr(dwWindowSizePixels, _dwWindowSizePixels));
}

til::size Settings::GetWindowOrigin() const
{
    return til::wrap_coord_size(_dwWindowOrigin);
}
void Settings::SetWindowOrigin(const til::size dwWindowOrigin)
{
    LOG_IF_FAILED(til::unwrap_coord_size_hr(dwWindowOrigin, _dwWindowOrigin));
}

DWORD Settings::GetFont() const
{
    return _nFont;
}
void Settings::SetFont(const DWORD nFont)
{
    _nFont = nFont;
}

til::size Settings::GetFontSize() const
{
    return til::wrap_coord_size(_dwFontSize);
}
void Settings::SetFontSize(const til::size dwFontSize)
{
    LOG_IF_FAILED(til::unwrap_coord_size_hr(dwFontSize, _dwFontSize));
}

UINT Settings::GetFontFamily() const
{
    return _uFontFamily;
}
void Settings::SetFontFamily(const UINT uFontFamily)
{
    _uFontFamily = uFontFamily;
}

UINT Settings::GetFontWeight() const
{
    return _uFontWeight;
}
void Settings::SetFontWeight(const UINT uFontWeight)
{
    _uFontWeight = uFontWeight;
}

const WCHAR* const Settings::GetFaceName() const
{
    return _FaceName;
}
void Settings::SetFaceName(const std::wstring_view faceName)
{
    auto extent = std::min<size_t>(faceName.size(), ARRAYSIZE(_FaceName));
    StringCchCopyW(_FaceName, extent, faceName.data());
}

UINT Settings::GetCursorSize() const
{
    return _uCursorSize;
}
void Settings::SetCursorSize(const UINT uCursorSize)
{
    _uCursorSize = uCursorSize;
}

bool Settings::GetFullScreen() const
{
    return _bFullScreen;
}
void Settings::SetFullScreen(const bool bFullScreen)
{
    _bFullScreen = bFullScreen;
}

bool Settings::GetQuickEdit() const
{
    return _bQuickEdit;
}
void Settings::SetQuickEdit(const bool bQuickEdit)
{
    _bQuickEdit = bQuickEdit;
}

bool Settings::GetInsertMode() const
{
    return _bInsertMode;
}
void Settings::SetInsertMode(const bool bInsertMode)
{
    _bInsertMode = bInsertMode;
}

bool Settings::GetAutoPosition() const
{
    return _bAutoPosition;
}
void Settings::SetAutoPosition(const bool bAutoPosition)
{
    _bAutoPosition = bAutoPosition;
}

UINT Settings::GetHistoryBufferSize() const
{
    return _uHistoryBufferSize;
}
void Settings::SetHistoryBufferSize(const UINT uHistoryBufferSize)
{
    _uHistoryBufferSize = uHistoryBufferSize;
}

UINT Settings::GetNumberOfHistoryBuffers() const
{
    return _uNumberOfHistoryBuffers;
}
void Settings::SetNumberOfHistoryBuffers(const UINT uNumberOfHistoryBuffers)
{
    _uNumberOfHistoryBuffers = uNumberOfHistoryBuffers;
}

bool Settings::GetHistoryNoDup() const
{
    return _bHistoryNoDup;
}
void Settings::SetHistoryNoDup(const bool bHistoryNoDup)
{
    _bHistoryNoDup = bHistoryNoDup;
}

bool Settings::IsStartupTitleIsLinkNameSet() const
{
    return WI_IsFlagSet(_dwStartupFlags, STARTF_TITLEISLINKNAME);
}

bool Settings::IsFaceNameSet() const
{
    return GetFaceName()[0] != '\0';
}

void Settings::UnsetStartupFlag(const DWORD dwFlagToUnset)
{
    _dwStartupFlags &= ~dwFlagToUnset;
}

void Settings::SetColorTableEntry(const size_t index, const COLORREF color)
{
    _renderSettings.SetColorTableEntry(index, color);
}

COLORREF Settings::GetColorTableEntry(const size_t index) const
{
    return _renderSettings.GetColorTableEntry(index);
}

void Settings::SetLegacyColorTableEntry(const size_t index, const COLORREF color)
{
    SetColorTableEntry(TextColor::TransposeLegacyIndex(index), color);
}

COLORREF Settings::GetLegacyColorTableEntry(const size_t index) const
{
    return GetColorTableEntry(TextColor::TransposeLegacyIndex(index));
}

CursorType Settings::GetCursorType() const noexcept
{
    return _CursorType;
}

void Settings::SetCursorType(const CursorType cursorType) noexcept
{
    _CursorType = cursorType;
}

bool Settings::GetInterceptCopyPaste() const noexcept
{
    return _fInterceptCopyPaste;
}

void Settings::SetInterceptCopyPaste(const bool interceptCopyPaste) noexcept
{
    _fInterceptCopyPaste = interceptCopyPaste;
}

void Settings::CalculateDefaultColorIndices() noexcept
{
    const auto foregroundColor = GetColorTableEntry(TextColor::DEFAULT_FOREGROUND);
    const auto foregroundIndex = TextColor::TransposeLegacyIndex(_wFillAttribute & FG_ATTRS);
    const auto foregroundAlias = foregroundColor != INVALID_COLOR ? TextColor::DEFAULT_FOREGROUND : foregroundIndex;
    _renderSettings.SetColorAliasIndex(ColorAlias::DefaultForeground, foregroundAlias);

    const auto backgroundColor = GetColorTableEntry(TextColor::DEFAULT_BACKGROUND);
    const auto backgroundIndex = TextColor::TransposeLegacyIndex((_wFillAttribute & BG_ATTRS) >> 4);
    const auto backgroundAlias = backgroundColor != INVALID_COLOR ? TextColor::DEFAULT_BACKGROUND : backgroundIndex;
    _renderSettings.SetColorAliasIndex(ColorAlias::DefaultBackground, backgroundAlias);
}

bool Settings::IsTerminalScrolling() const noexcept
{
    return _TerminalScrolling;
}

void Settings::SetTerminalScrolling(const bool terminalScrollingEnabled) noexcept
{
    _TerminalScrolling = terminalScrollingEnabled;
}

// Determines whether our primary renderer should be DirectX or GDI.
// This is based on user preference and velocity hold back state.
UseDx Settings::GetUseDx() const noexcept
{
    return _fUseDx;
}

bool Settings::GetCopyColor() const noexcept
{
    return _fCopyColor;
}
