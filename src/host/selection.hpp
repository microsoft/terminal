/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- selection.hpp

Abstract:
- This module is used for managing the selection region

Author(s):
- Michael Niksa (MiNiksa) 4-Jun-2014
- Paul Campbell (PaulCam) 4-Jun-2014

Revision History:
- From components of clipbrd.h/.c and input.h/.c of v1 console.
--*/

#pragma once

#include "input.h"

#include "..\interactivity\inc\IAccessibilityNotifier.hpp"
#include "..\types\IConsoleWindow.hpp"

class Selection
{
public:
    ~Selection() = default;

    std::vector<SMALL_RECT> GetSelectionRects() const;

    void ShowSelection();
    void HideSelection();

    static Selection& Instance();

    // Key selection generally refers to "mark mode" selection where
    // the cursor is present and used to navigate 100% with the
    // keyboard.
    //
    // Mouse selection means either the block or line mode selection
    // usually initiated by the mouse.
    //
    // However, Mouse mode can also mean initiated with our
    // shift+directional commands as no block cursor is required for
    // navigation.

    void InitializeMarkSelection();
    void InitializeMouseSelection(const COORD coordBufferPos);

    void SelectNewRegion(const COORD coordStart, const COORD coordEnd);
    void SelectAll();

    void ExtendSelection(_In_ COORD coordBufferPos);
    void AdjustSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd);

    void ClearSelection();
    void ClearSelection(const bool fStartingNewSelection);
    void ColorSelection(const SMALL_RECT& srRect, const TextAttribute attr);
    void ColorSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd, const TextAttribute attr);

    // delete these or we can accidentally get copies of the singleton
    Selection(Selection const&) = delete;
    void operator=(Selection const&) = delete;

protected:
    Selection();

private:
    void _SetSelectionVisibility(const bool fMakeVisible);

    void _PaintSelection() const;

    static SMALL_RECT s_BisectSelection(const short sStringLength,
                                        const COORD coordTargetPoint,
                                        const SCREEN_INFORMATION& screenInfo,
                                        const SMALL_RECT rect);

    static std::vector<SMALL_RECT> s_GetSelectionRects(const SMALL_RECT& selectionRect,
                                                       const COORD selectionAnchor,
                                                       const bool lineSelection);

    void _CancelMarkSelection();
    void _CancelMouseSelection();

    // -------------------------------------------------------------------------------------------------------
    // input handling (selectionInput.cpp)
public:
    // key handling

    // N.B.: This enumeration helps push up calling clipboard functions into
    //       the caller. This way, all of the selection code is independent of
    //       the clipboard and thus more easily shareable with Windows editions
    //       that do not have a clipboard (i.e. OneCore).
    enum class KeySelectionEventResult
    {
        EventHandled,
        EventNotHandled,
        CopyToClipboard
    };

    KeySelectionEventResult HandleKeySelectionEvent(const INPUT_KEY_INFO* const pInputKeyInfo);
    static bool s_IsValidKeyboardLineSelection(const INPUT_KEY_INFO* const pInputKeyInfo);
    bool HandleKeyboardLineSelectionEvent(const INPUT_KEY_INFO* const pInputKeyInfo);

    void CheckAndSetAlternateSelection();

    // calculation functions
    [[nodiscard]] static bool s_GetInputLineBoundaries(_Out_opt_ COORD* const pcoordInputStart, _Out_opt_ COORD* const pcoordInputEnd);
    void GetValidAreaBoundaries(_Out_opt_ COORD* const pcoordValidStart, _Out_opt_ COORD* const pcoordValidEnd) const;
    static bool s_IsWithinBoundaries(const COORD coordPosition, const COORD coordStart, const COORD coordEnd);

private:
    // key handling
    bool _HandleColorSelection(const INPUT_KEY_INFO* const pInputKeyInfo);
    bool _HandleMarkModeSelectionNav(const INPUT_KEY_INFO* const pInputKeyInfo);
    COORD WordByWordSelection(const bool fPrevious,
                              const Microsoft::Console::Types::Viewport& bufferSize,
                              const COORD coordAnchor,
                              const COORD coordSelPoint) const;

    // -------------------------------------------------------------------------------------------------------
    // selection state (selectionState.cpp)
public:
    bool IsKeyboardMarkSelection() const;
    bool IsMouseInitiatedSelection() const;

    bool IsLineSelection() const;

    bool IsInSelectingState() const;
    bool IsInQuickEditMode() const;

    bool IsAreaSelected() const;
    bool IsMouseButtonDown() const;

    DWORD GetPublicSelectionFlags() const noexcept;
    COORD GetSelectionAnchor() const noexcept;
    SMALL_RECT GetSelectionRectangle() const noexcept;

    void SetLineSelection(const bool fLineSelectionOn);

    bool ShouldAllowMouseDragSelection(const COORD mousePosition) const noexcept;

    // TODO: these states likely belong somewhere else
    void MouseDown();
    void MouseUp();

private:
    void _SaveCursorData(const Cursor& cursor) noexcept;
    void _RestoreDataToCursor(Cursor& cursor) noexcept;

    void _AlignAlternateSelection(const bool fAlignToLineSelect);

    void _SetSelectingState(const bool fSelectingOn);

    // TODO: extended edit key should probably be in here (remaining code is in cmdline.cpp)
    // TODO: trim leading zeros should probably be in here (pending move of reactive code from input.cpp to selectionInput.cpp)
    // TODO: enable color selection should be in here
    // TODO: quick edit mode should be in here
    // TODO: console selection mode should be in here
    // TODO: consider putting word delims in here

    // -- State/Flags --
    // This replaces/deprecates CONSOLE_SELECTION_INVERTED on gci->SelectionFlags
    bool _fSelectionVisible;

    bool _fLineSelection; // whether to use line selection or block selection
    bool _fUseAlternateSelection; // whether the user has triggered the alternate selection method
    bool _allowMouseDragSelection; // true if the dragging the mouse should change the selection

    // Flags for this DWORD are defined in wincon.h. Search for def:CONSOLE_SELECTION_IN_PROGRESS, etc.
    DWORD _dwSelectionFlags;

    // -- Current Selection Data --
    // Anchor is the point the selection was started from (and will be one of the corners of the rectangle).
    COORD _coordSelectionAnchor;
    // Rectangle is the area inscribing the selection. It is extended to screen edges in a particular way for line selection.
    SMALL_RECT _srSelectionRect;

    // -- Saved Cursor Data --
    // Saved when a selection is started for restoration later. Position is in character coordinates, not pixels.
    COORD _coordSavedCursorPosition;
    ULONG _ulSavedCursorSize;
    bool _fSavedCursorVisible;
    COLORREF _savedCursorColor;
    CursorType _savedCursorType;

#ifdef UNIT_TESTING
    friend class SelectionTests;
    friend class SelectionInputTests;
    friend class ClipboardTests;
#endif
};
