/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CommandNumberPopup.hpp

Abstract:
- Popup used for use command number input
- contains code pulled from popup.cpp and cmdline.cpp

Author:
- Austin Diviness (AustDi) 18-Aug-2018
--*/

#pragma once

#include "popup.h"


class CommandNumberPopup final : public Popup
{
public:
    CommandNumberPopup(SCREEN_INFORMATION& screenInfo);

    [[nodiscard]]
    NTSTATUS Process(CookedRead& cookedReadData) noexcept override;

protected:
    void _DrawContent() override;

private:
    std::wstring _userInput;

    void _handleNumber(CookedRead& cookedReadData, const wchar_t wch) noexcept;
    void _handleBackspace(CookedRead& cookedReadData) noexcept;
    void _handleEscape(CookedRead& cookedReadData) noexcept;
    void _handleReturn(CookedRead& cookedReadData) noexcept;

    void _push(const wchar_t wch);
    void _pop() noexcept;
    int _parse() const noexcept;

#ifdef UNIT_TESTING
    friend class CommandNumberPopupTests;
#endif
};
