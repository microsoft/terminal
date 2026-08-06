/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- HwndTerminalAutomationPeer.hpp

Abstract:
- This module provides UI Automation access to the HwndTerminal
  to support both automation tests and accessibility (screen
  reading) applications. This mainly interacts with TermControlUiaProvider
  to allow for shared code with Windows Terminal accessibility providers.

Author(s):
- Carlos Zamora   (CaZamor)    2022
--*/

#pragma once

#include "../types/TermControlUiaProvider.hpp"
#include "../types/IUiaEventDispatcher.h"
#include "../types/IControlAccessibilityInfo.h"

class HwndTerminalAutomationPeer :
    public ::Microsoft::Console::Types::IUiaEventDispatcher,
    public ::Microsoft::Terminal::TermControlUiaProvider
{
public:
    void RecordKeyEvent(const WORD vkey);

    IFACEMETHODIMP GetPropertyValue(_In_ PROPERTYID idProp,
                                    _Out_ VARIANT* pVariant) noexcept override;

#pragma region IUiaEventDispatcher
    void SignalSelectionChanged() override;
    void SignalTextChanged() override;
    void SignalCursorChanged() override;
    void NotifyNewOutput(std::wstring_view newOutput) override;
#pragma endregion
private:
    void _tryNotify(BSTR string, BSTR activity);
    std::deque<wchar_t> _keyEvents;
    bool _notificationsUnavailable{};
};
