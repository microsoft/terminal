/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- screenInfoUiaProvider.hpp

Abstract:
- This module provides UI Automation access to the screen buffer to
  support both automation tests and accessibility (screen reading)
  applications.
- This is the ConHost extension of ScreenInfoUiaProviderBase.hpp
- Based on examples, sample code, and guidance from
  https://msdn.microsoft.com/en-us/library/windows/desktop/ee671596(v=vs.85).aspx

Author(s):
- Carlos Zamora   (CaZamor)   2019
--*/

#pragma once

#include "precomp.h"
#include "..\types\ScreenInfoUiaProviderBase.h"
#include "..\types\UiaTextRangeBase.hpp"
#include "uiaTextRange.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class ScreenInfoUiaProvider final : public Microsoft::Console::Types::ScreenInfoUiaProviderBase
    {
    public:
        ScreenInfoUiaProvider() = default;
        HRESULT RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                       _In_ Microsoft::Console::Types::WindowUiaProviderBase* const pUiaParent);

        // IRawElementProviderFragment methods
        IFACEMETHODIMP Navigate(_In_ NavigateDirection direction,
                                _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) override;
        IFACEMETHODIMP get_BoundingRectangle(_Out_ UiaRect* pRect) override;
        IFACEMETHODIMP get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider) override;

        HWND GetWindowHandle() const;
        void ChangeViewport(const SMALL_RECT NewWindow);

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
        // weak reference to uia parent
        Microsoft::Console::Types::WindowUiaProviderBase* _pUiaParent;
    };
}
