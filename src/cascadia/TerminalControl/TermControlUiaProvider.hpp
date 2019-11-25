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

namespace Microsoft::Terminal
{
    class TermControlUiaProvider : public Microsoft::Console::Types::ScreenInfoUiaProviderBase
    {
    public:
        TermControlUiaProvider() = default;
        HRESULT RuntimeClassInitialize(_In_ winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl* termControl,
                                       _In_ std::function<RECT()> GetBoundingRect);

        // IRawElementProviderFragment methods
        IFACEMETHODIMP Navigate(_In_ NavigateDirection direction,
                                _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) override;
        IFACEMETHODIMP get_BoundingRectangle(_Out_ UiaRect* pRect) override;
        IFACEMETHODIMP get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider) override;

        const COORD GetFontSize() const;
        const winrt::Windows::UI::Xaml::Thickness GetPadding() const;

    protected:
        HRESULT GetSelectionRanges(_In_ IRawElementProviderSimple* pProvider, _Out_ std::deque<WRL::ComPtr<Microsoft::Console::Types::UiaTextRangeBase>>& selectionRanges) override;

        // degenerate range
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider, _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // degenerate range at cursor position
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                const Cursor& cursor,
                                _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // specific endpoint range
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                const Endpoint start,
                                const Endpoint end,
                                const bool degenerate,
                                _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // range from a UiaPoint
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                const UiaPoint point,
                                _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

    private:
        std::function<RECT(void)> _getBoundingRect;
        winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl* _termControl;
    };
}
