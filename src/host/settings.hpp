/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- settings.hpp

Abstract:
- This module is used for all configurable settings in the console

Author(s):
- Michael Niksa (MiNiksa) 23-Jul-2014
- Paul Campbell (PaulCam) 23-Jul-2014

Revision History:
- From components of consrv.h
- This is a reduced/de-duplicated version of settings that were stored in the registry, link files, and in the console information state.
--*/
#pragma once

// To prevent invisible windows, set a lower threshold on window alpha channel.
constexpr unsigned short MIN_WINDOW_OPACITY = 0x4D; // 0x4D is approximately 30% visible/opaque (70% transparent). Valid range is 0x00-0xff.

#include "ConsoleArguments.hpp"
#include "../renderer/inc/RenderSettings.hpp"

enum class UseDx : DWORD
{
    Disabled = 0,
    DxEngine,
    AtlasEngine,
};

class Settings
{
    using RenderSettings = Microsoft::Console::Render::RenderSettings;

public:
    Settings();

    void ApplyDesktopSpecificDefaults();

    void ApplyStartupInfo(const Settings* const pStartupSettings);
    void ApplyCommandlineArguments(const ConsoleArguments& consoleArgs);
    void InitFromStateInfo(_In_ PCONSOLE_STATE_INFO pStateInfo);
    void Validate();

    CONSOLE_STATE_INFO CreateConsoleStateInfo() const;

    RenderSettings& GetRenderSettings() noexcept { return _renderSettings; };
    const RenderSettings& GetRenderSettings() const noexcept { return _renderSettings; };

    DWORD GetDefaultVirtTermLevel() const;
    void SetDefaultVirtTermLevel(const DWORD dwVirtTermLevel);

    bool IsAltF4CloseAllowed() const;
    void SetAltF4CloseAllowed(const bool fAllowAltF4Close);

    bool GetFilterOnPaste() const;
    void SetFilterOnPaste(const bool fFilterOnPaste);

    const std::wstring_view GetLaunchFaceName() const;
    void SetLaunchFaceName(const std::wstring_view launchFaceName);

    UINT GetCodePage() const;
    void SetCodePage(const UINT uCodePage);

    UINT GetScrollScale() const;
    void SetScrollScale(const UINT uScrollScale);

    bool GetTrimLeadingZeros() const;
    void SetTrimLeadingZeros(const bool fTrimLeadingZeros);

    bool GetEnableColorSelection() const;
    void SetEnableColorSelection(const bool fEnableColorSelection);

    bool GetLineSelection() const;
    void SetLineSelection(const bool bLineSelection);

    bool GetWrapText() const;
    void SetWrapText(const bool bWrapText);

    bool GetCtrlKeyShortcutsDisabled() const;
    void SetCtrlKeyShortcutsDisabled(const bool fCtrlKeyShortcutsDisabled);

    BYTE GetWindowAlpha() const;
    void SetWindowAlpha(const BYTE bWindowAlpha);

    DWORD GetHotKey() const;
    void SetHotKey(const DWORD dwHotKey);

    bool IsStartupTitleIsLinkNameSet() const;

    DWORD GetStartupFlags() const;
    void SetStartupFlags(const DWORD dwStartupFlags);
    void UnsetStartupFlag(const DWORD dwFlagToUnset);

    WORD GetFillAttribute() const;
    void SetFillAttribute(const WORD wFillAttribute);

    WORD GetPopupFillAttribute() const;
    void SetPopupFillAttribute(const WORD wPopupFillAttribute);

    WORD GetShowWindow() const;
    void SetShowWindow(const WORD wShowWindow);

    WORD GetReserved() const;
    void SetReserved(const WORD wReserved);

    til::size GetScreenBufferSize() const;
    void SetScreenBufferSize(const til::size dwScreenBufferSize);

    til::size GetWindowSize() const;
    void SetWindowSize(const til::size dwWindowSize);

    bool IsWindowSizePixelsValid() const;
    til::size GetWindowSizePixels() const;
    void SetWindowSizePixels(const til::size dwWindowSizePixels);

    til::size GetWindowOrigin() const;
    void SetWindowOrigin(const til::size dwWindowOrigin);

    DWORD GetFont() const;
    void SetFont(const DWORD dwFont);

    til::size GetFontSize() const;
    void SetFontSize(const til::size dwFontSize);

    UINT GetFontFamily() const;
    void SetFontFamily(const UINT uFontFamily);

    UINT GetFontWeight() const;
    void SetFontWeight(const UINT uFontWeight);

    const WCHAR* const GetFaceName() const;
    bool IsFaceNameSet() const;
    void SetFaceName(const std::wstring_view faceName);

    UINT GetCursorSize() const;
    void SetCursorSize(const UINT uCursorSize);

    bool GetFullScreen() const;
    void SetFullScreen(const bool fFullScreen);

    bool GetQuickEdit() const;
    void SetQuickEdit(const bool fQuickEdit);

    bool GetInsertMode() const;
    void SetInsertMode(const bool fInsertMode);

    bool GetAutoPosition() const;
    void SetAutoPosition(const bool fAutoPosition);

    UINT GetHistoryBufferSize() const;
    void SetHistoryBufferSize(const UINT uHistoryBufferSize);

    UINT GetNumberOfHistoryBuffers() const;
    void SetNumberOfHistoryBuffers(const UINT uNumberOfHistoryBuffers);

    bool GetHistoryNoDup() const;
    void SetHistoryNoDup(const bool fHistoryNoDup);

    void SetColorTableEntry(const size_t index, const COLORREF color);
    COLORREF GetColorTableEntry(const size_t index) const;
    void SetLegacyColorTableEntry(const size_t index, const COLORREF color);
    COLORREF GetLegacyColorTableEntry(const size_t index) const;

    CursorType GetCursorType() const noexcept;
    void SetCursorType(const CursorType cursorType) noexcept;

    bool GetInterceptCopyPaste() const noexcept;
    void SetInterceptCopyPaste(const bool interceptCopyPaste) noexcept;

    void CalculateDefaultColorIndices() noexcept;

    bool IsTerminalScrolling() const noexcept;
    void SetTerminalScrolling(const bool terminalScrollingEnabled) noexcept;

    UseDx GetUseDx() const noexcept;
    bool GetCopyColor() const noexcept;

private:
    RenderSettings _renderSettings;

    DWORD _dwHotKey;
    DWORD _dwStartupFlags;
    WORD _wFillAttribute;
    WORD _wPopupFillAttribute;
    WORD _wShowWindow; // used when window is created
    WORD _wReserved;
    // START - This section filled via memcpy from shortcut properties. Do not rearrange/change.
    COORD _dwScreenBufferSize;
    COORD _dwWindowSize; // this is in characters.
    COORD _dwWindowOrigin; // used when window is created
    DWORD _nFont;
    COORD _dwFontSize;
    UINT _uFontFamily;
    UINT _uFontWeight;
    WCHAR _FaceName[LF_FACESIZE];
    UINT _uCursorSize;
    BOOL _bFullScreen; // deprecated
    BOOL _bQuickEdit;
    BOOL _bInsertMode; // used by command line editing
    BOOL _bAutoPosition;
    UINT _uHistoryBufferSize;
    UINT _uNumberOfHistoryBuffers;
    BOOL _bHistoryNoDup;
    // END - memcpy
    UINT _uCodePage;
    UINT _uScrollScale;
    bool _fTrimLeadingZeros;
    bool _fEnableColorSelection;
    bool _bLineSelection;
    bool _bWrapText; // whether to use text wrapping when resizing the window
    bool _fCtrlKeyShortcutsDisabled; // disables Ctrl+<something> key intercepts
    BYTE _bWindowAlpha; // describes the opacity of the window

    bool _fFilterOnPaste; // should we filter text when the user pastes? (e.g. remove <tab>)
    std::wstring _LaunchFaceName;
    bool _fAllowAltF4Close;
    DWORD _dwVirtTermLevel;
    UseDx _fUseDx;
    bool _fCopyColor;

    // this is used for the special STARTF_USESIZE mode.
    bool _fUseWindowSizePixels;
    COORD _dwWindowSizePixels;

    CursorType _CursorType;

    bool _fInterceptCopyPaste;

    bool _TerminalScrolling;
    friend class RegistrySerialization;
};
