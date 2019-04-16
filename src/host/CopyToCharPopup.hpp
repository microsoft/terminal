/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CopyToCharPopup.hpp

Abstract:
- Popup used for use copying to char input
- contains code pulled from popup.cpp and cmdline.cpp

Author:
- Austin Diviness (AustDi) 18-Aug-2018
--*/

#pragma once

#include "popup.h"

class CopyToCharPopup final : public Popup
{
public:
    CopyToCharPopup(SCREEN_INFORMATION& screenInfo);

    [[nodiscard]]
    NTSTATUS Process(CookedRead& cookedReadData) noexcept override;
protected:
    void _DrawContent() override;

private:
    void _copyToChar(CookedRead& cookedReadData, const std::wstring_view LastCommand, const wchar_t wch);
};
