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

#include "../interactivity/inc/IConsoleWindow.hpp"
#include "til/generational.h"

class Selection
{
public:
    ~Selection() = default;

    std::span<const til::point_span> GetSelectionSpans() const;

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
    void InitializeMouseSelection(const til::point coordBufferPos);

    void SelectNewRegion(const til::point coordStart, const til::point coordEnd);
    void SelectAll();

    void ExtendSelection(_In_ til::point coordBufferPos);
    void AdjustSelection(const til::point coordSelectionStart, const til::point coordSelectionEnd);

    void ClearSelection();
    void ClearSelection(const bool fStartingNewSelection);
    void ColorSelection(const til::rect& srRect, const TextAttribute attr);
    void ColorSelection(const til::point coordSelectionStart, const til::point coordSelectionEnd, const TextAttribute attr);

    // delete these or we can accidentally get copies of the singleton
    Selection(const Selection&) = delete;
    void operator=(const Selection&) = delete;

protected:
    Selection();

private:
    void _SetSelectionVisibility(const bool fMakeVisible);

    void _PaintSelection() const;

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
    [[nodiscard]] static bool s_GetInputLineBoundaries(_Out_opt_ til::point* const pcoordInputStart, _Out_opt_ til::point* const pcoordInputEnd);
    void GetValidAreaBoundaries(_Out_opt_ til::point* const pcoordValidStart, _Out_opt_ til::point* const pcoordValidEnd) const;
    static bool s_IsWithinBoundaries(const til::point coordPosition, const til::point coordStart, const til::point coordEnd);

private:
    // key handling
    bool _HandleColorSelection(const INPUT_KEY_INFO* const pInputKeyInfo);
    bool _HandleMarkModeSelectionNav(const INPUT_KEY_INFO* const pInputKeyInfo);
    til::point WordByWordSelection(const bool fPrevious,
                                   const Microsoft::Console::Types::Viewport& bufferSize,
                                   const til::point coordAnchor,
                                   const til::point coordSelPoint) const;

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
    til::point GetSelectionAnchor() const noexcept;
    std::pair<til::point, til::point> GetSelectionAnchors() const noexcept;
    til::inclusive_rect GetSelectionRectangle() const noexcept;

    void SetLineSelection(const bool fLineSelectionOn);

    bool ShouldAllowMouseDragSelection(const til::point mousePosition) const noexcept;

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

    struct SelectionData
    {
        // -- State/Flags --
        // This replaces/deprecates CONSOLE_SELECTION_INVERTED on gci->SelectionFlags
        bool fSelectionVisible{ false };

        bool fLineSelection{ true }; // whether to use line selection or block selection
        bool fUseAlternateSelection{ false }; // whether the user has triggered the alternate selection method
        bool allowMouseDragSelection{ true }; // true if the dragging the mouse should change the selection

        // Flags for this DWORD are defined in wincon.h. Search for def:CONSOLE_SELECTION_IN_PROGRESS, etc.
        DWORD dwSelectionFlags{ 0 };

        // -- Current Selection Data --
        // Anchor is the point the selection was started from (and will be one of the corners of the rectangle).
        til::point coordSelectionAnchor{};
        // Rectangle is the area inscribing the selection. It is extended to screen edges in a particular way for line selection.
        til::inclusive_rect srSelectionRect{};

        // -- Saved Cursor Data --
        // Saved when a selection is started for restoration later. Position is in character coordinates, not pixels.
        til::point coordSavedCursorPosition{};
        ULONG ulSavedCursorSize{ 0 };
        bool fSavedCursorVisible{ false };
        CursorType savedCursorType{ CursorType::Legacy };
    };
    til::generational<SelectionData> _d{};

    mutable std::vector<til::point_span> _lastSelectionSpans;
    mutable til::generation_t _lastSelectionGeneration;

    void _ExtendSelection(SelectionData* d, _In_ til::point coordBufferPos);
    void _RegenerateSelectionSpans() const;

#ifdef UNIT_TESTING
    friend class SelectionTests;
    friend class SelectionInputTests;
    friend class ClipboardTests;
#endif
};
