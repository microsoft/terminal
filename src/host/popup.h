/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- popup.h

Abstract:
- This file contains the internal structures and definitions used by command line input and editing.

Author:
- Therese Stowell (ThereseS) 15-Nov-1991

Revision History:
- Mike Griese (migrie) Jan 2018:
    Refactored the history and alias functionality into their own files.
- Michael Niksa (miniksa) May 2018:
    Separated out popups from the rest of command line functionality.
--*/

#pragma once

#include "readDataCooked.hpp"
#include "screenInfo.hpp"
#include "readDataCooked.hpp"

class CommandHistory;

class Popup
{
public:
    static constexpr unsigned int MINIMUM_COMMAND_PROMPT_SIZE = 5;

    using UserInputFunction = std::function<NTSTATUS(COOKED_READ_DATA&, bool&, DWORD&, wchar_t&)>;

    Popup(SCREEN_INFORMATION& screenInfo, const COORD proposedSize);
    virtual ~Popup();
    [[nodiscard]] virtual NTSTATUS Process(COOKED_READ_DATA& cookedReadData) noexcept = 0;

    void Draw();

    void UpdateStoredColors(const TextAttribute& newAttr,
                            const TextAttribute& newPopupAttr,
                            const TextAttribute& oldAttr,
                            const TextAttribute& oldPopupAttr);

    void End();

    SHORT Width() const noexcept;
    SHORT Height() const noexcept;

    COORD GetCursorPosition() const noexcept;

protected:
    // used in test code to alter how the popup fetches use input
    void SetUserInputFunction(UserInputFunction function) noexcept;

#ifdef UNIT_TESTING
    friend class CopyFromCharPopupTests;
    friend class CopyToCharPopupTests;
    friend class CommandNumberPopupTests;
    friend class CommandListPopupTests;
#endif

    [[nodiscard]] NTSTATUS _getUserInput(COOKED_READ_DATA& cookedReadData, bool& popupKey, DWORD& modifiers, wchar_t& wch) noexcept;
    void _DrawPrompt(const UINT id);
    virtual void _DrawContent() = 0;

    SMALL_RECT _region; // region popup occupies
    SCREEN_INFORMATION& _screenInfo;
    TextAttribute _attributes; // text attributes

private:
    COORD _CalculateSize(const SCREEN_INFORMATION& screenInfo, const COORD proposedSize);
    COORD _CalculateOrigin(const SCREEN_INFORMATION& screenInfo, const COORD size);

    void _DrawBorder();

    [[nodiscard]] static NTSTATUS _getUserInputInternal(COOKED_READ_DATA& cookedReadData,
                                                        bool& popupKey,
                                                        DWORD& modifiers,
                                                        wchar_t& wch) noexcept;

    OutputCellRect _oldContents; // contains data under popup
    COORD _oldScreenSize;
    UserInputFunction _userInputFunction;
};
