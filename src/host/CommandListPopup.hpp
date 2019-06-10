/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CommandListPopup.hpp

Abstract:
- Popup used for use command list input
- contains code pulled from popup.cpp and cmdline.cpp

Author:
- Austin Diviness (AustDi) 18-Aug-2018
--*/

#pragma once

#include "popup.h"


class CommandListPopup : public Popup
{
public:
    CommandListPopup(SCREEN_INFORMATION& screenInfo, CommandHistory& history);

    [[nodiscard]]
    NTSTATUS Process(CookedRead& cookedReadData) noexcept override;

protected:
    void _DrawContent() override;

private:
    void _drawList();
    void _update(const SHORT delta, const bool wrap = false);
    void _updateHighlight(const SHORT oldCommand, const SHORT newCommand);

    void _handleReturn(CookedRead& cookedReadData);
    void _cycleSelectionToMatchingCommands(CookedRead& cookedReadData, const wchar_t wch);
    void _setBottomIndex();
    [[nodiscard]]
    NTSTATUS _handlePopupKeys(CookedRead& cookedReadData, const wchar_t wch, const DWORD modifiers) noexcept;
    [[nodiscard]]
    NTSTATUS _deleteSelection(CookedRead& cookedReadData) noexcept;
    [[nodiscard]]
    NTSTATUS _swapUp(CookedRead& cookedReadData) noexcept;
    [[nodiscard]]
    NTSTATUS _swapDown(CookedRead& cookedReadData) noexcept;

    SHORT _currentCommand;
    SHORT _bottomIndex;  // number of command displayed on last line of popup
    const CommandHistory& _history;

#ifdef UNIT_TESTING
    friend class CommandListPopupTests;
#endif
};
