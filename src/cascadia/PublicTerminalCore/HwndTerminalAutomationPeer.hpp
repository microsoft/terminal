/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- HwndTerminalAutomationPeer.hpp

Abstract:
- This module provides UI Automation access to the TermControl
  to support both automation tests and accessibility (screen
  reading) applications. This mainly interacts with ScreenInfoUiaProvider
  to allow for shared code between ConHost and Windows Terminal
  accessibility providers.
- Based on the Custom Automation Peers guide on msdn
  (https://docs.microsoft.com/en-us/windows/uwp/design/accessibility/custom-automation-peers)
- Wraps the UIAutomationCore ITextProvider
  (https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcore/nn-uiautomationcore-itextprovider)
  with a XAML ITextProvider
  (https://docs.microsoft.com/en-us/uwp/api/windows.ui.xaml.automation.provider.itextprovider)

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

#pragma region IUiaEventDispatcher
    void SignalSelectionChanged() override;
    void SignalTextChanged() override;
    void SignalCursorChanged() override;
    void NotifyNewOutput(std::wstring_view newOutput) override;
#pragma endregion
private:
    std::deque<wchar_t> _keyEvents;
};
