/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TermControlUiaProvider.hpp

Abstract:
- This module provides UI Automation access to the screen buffer to
  support both automation tests and accessibility (screen reading)
  applications.
- ConHost and Windows Terminal must use IRenderData to have access to the proper information
- Based on examples, sample code, and guidance from
  https://msdn.microsoft.com/en-us/library/windows/desktop/ee671596(v=vs.85).aspx

Author(s):
- Carlos Zamora   (CaZamor)   2019
--*/

#pragma once

#include "..\types\ScreenInfoUiaProviderBase.h"
#include "..\types\UiaTextRangeBase.hpp"
#include "UiaTextRange.hpp"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct TermControl;
}

    class TermControlUiaProvider : public Microsoft::Console::Types::ScreenInfoUiaProviderBase
    {
    public:
        TermControlUiaProvider(_In_ winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl const& termControl,
                               _In_ std::function<RECT()> GetBoundingRect);

        // IRawElementProviderFragment methods
        IFACEMETHODIMP Navigate(_In_ NavigateDirection direction,
                                _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) override;
        IFACEMETHODIMP get_BoundingRectangle(_Out_ UiaRect* pRect) override;
        IFACEMETHODIMP get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider) override;

        const COORD GetFontSize() const;
        const winrt::Windows::UI::Xaml::Thickness GetPadding() const;

    protected:
        std::deque<Microsoft::Console::Types::UiaTextRangeBase*> GetSelectionRanges(_In_ IRawElementProviderSimple* pProvider) override;

        // degenerate range
        Microsoft::Console::Types::UiaTextRangeBase* CreateTextRange(_In_ IRawElementProviderSimple* const pProvider) override;

        // degenerate range at cursor position
        Microsoft::Console::Types::UiaTextRangeBase* CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                                     const Cursor& cursor) override;

        // specific endpoint range
        Microsoft::Console::Types::UiaTextRangeBase* CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                                     const Endpoint start,
                                                                     const Endpoint end,
                                                                     const bool degenerate) override;

        // range from a UiaPoint
        Microsoft::Console::Types::UiaTextRangeBase* CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                                     const UiaPoint point) override;

    private:
        std::function<RECT(void)> _getBoundingRect;
        winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl const& _termControl;
    };
}
