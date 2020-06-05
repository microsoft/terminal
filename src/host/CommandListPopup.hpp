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
    CommandListPopup(SCREEN_INFORMATION& screenInfo, const CommandHistory& history);

    [[nodiscard]] NTSTATUS Process(COOKED_READ_DATA& cookedReadData) noexcept override;

protected:
    void _DrawContent() override;

private:
    void _drawList();
    void _update(const SHORT delta, const bool wrap = false);
    void _updateHighlight(const SHORT oldCommand, const SHORT newCommand);

    void _handleReturn(COOKED_READ_DATA& cookedReadData);
    void _cycleSelectionToMatchingCommands(COOKED_READ_DATA& cookedReadData, const wchar_t wch);
    void _setBottomIndex();
    [[nodiscard]] NTSTATUS _handlePopupKeys(COOKED_READ_DATA& cookedReadData, const wchar_t wch, const DWORD modifiers) noexcept;
    [[nodiscard]] NTSTATUS _deleteSelection(COOKED_READ_DATA& cookedReadData) noexcept;
    [[nodiscard]] NTSTATUS _swapUp(COOKED_READ_DATA& cookedReadData) noexcept;
    [[nodiscard]] NTSTATUS _swapDown(COOKED_READ_DATA& cookedReadData) noexcept;

    SHORT _currentCommand;
    SHORT _bottomIndex; // number of command displayed on last line of popup
    const CommandHistory& _history;

#ifdef UNIT_TESTING
    friend class CommandListPopupTests;
#endif
};
