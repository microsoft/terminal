/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ScreenInfoUiaProviderBase.hpp

Abstract:
- This module provides UI Automation access to the screen buffer to
  support both automation tests and accessibility (screen reading)
  applications.
- ConHost and Windows Terminal must use IRenderData to have access to the proper information
- Based on examples, sample code, and guidance from
  https://msdn.microsoft.com/en-us/library/windows/desktop/ee671596(v=vs.85).aspx

Author(s):
- Michael Niksa   (MiNiksa)   2017
- Austin Diviness (AustDi)    2017
- Carlos Zamora   (CaZamor)   2019
--*/

#pragma once

#include "../buffer/out/textBuffer.hpp"
#include "UiaTextRangeBase.hpp"
#include "IUiaData.h"
#include "IUiaTraceable.h"

#include <UIAutomationCore.h>

#include <wrl/implements.h>

namespace Microsoft::Console::Types
{
    class Viewport;

    class ScreenInfoUiaProviderBase :
        public WRL::RuntimeClass<WRL::RuntimeClassFlags<WRL::ClassicCom | WRL::InhibitFtmBase>, IRawElementProviderSimple, IRawElementProviderFragment, ITextProvider>,
        public IUiaTraceable
    {
    public:
        virtual HRESULT RuntimeClassInitialize(_In_ IUiaData* pData, _In_ std::wstring_view wordDelimiters = UiaTextRangeBase::DefaultWordDelimiter) noexcept;

        ScreenInfoUiaProviderBase(const ScreenInfoUiaProviderBase&) = delete;
        ScreenInfoUiaProviderBase(ScreenInfoUiaProviderBase&&) = delete;
        ScreenInfoUiaProviderBase& operator=(const ScreenInfoUiaProviderBase&) = delete;
        ScreenInfoUiaProviderBase& operator=(ScreenInfoUiaProviderBase&&) = delete;
        ~ScreenInfoUiaProviderBase() = default;

        [[nodiscard]] HRESULT Signal(_In_ EVENTID id);
        virtual void ChangeViewport(const SMALL_RECT NewWindow) = 0;

        // IRawElementProviderSimple methods
        IFACEMETHODIMP get_ProviderOptions(_Out_ ProviderOptions* pOptions) noexcept override;
        IFACEMETHODIMP GetPatternProvider(_In_ PATTERNID iid,
                                          _COM_Outptr_result_maybenull_ IUnknown** ppInterface) override;
        IFACEMETHODIMP GetPropertyValue(_In_ PROPERTYID idProp,
                                        _Out_ VARIANT* pVariant) noexcept override;
        IFACEMETHODIMP get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider) noexcept override;

        // IRawElementProviderFragment methods
        virtual IFACEMETHODIMP Navigate(_In_ NavigateDirection direction,
                                        _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) = 0;
        IFACEMETHODIMP GetRuntimeId(_Outptr_result_maybenull_ SAFEARRAY** ppRuntimeId) override;
        virtual IFACEMETHODIMP get_BoundingRectangle(_Out_ UiaRect* pRect) = 0;
        IFACEMETHODIMP GetEmbeddedFragmentRoots(_Outptr_result_maybenull_ SAFEARRAY** ppRoots) noexcept override;
        IFACEMETHODIMP SetFocus() override;
        virtual IFACEMETHODIMP get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider) = 0;

        // ITextProvider
        IFACEMETHODIMP GetSelection(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal) override;
        IFACEMETHODIMP GetVisibleRanges(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal) override;
        IFACEMETHODIMP RangeFromChild(_In_ IRawElementProviderSimple* childElement,
                                      _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) override;
        IFACEMETHODIMP RangeFromPoint(_In_ UiaPoint point,
                                      _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) override;
        IFACEMETHODIMP get_DocumentRange(_COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) override;
        IFACEMETHODIMP get_SupportedTextSelection(_Out_ SupportedTextSelection* pRetVal) noexcept override;

    protected:
        ScreenInfoUiaProviderBase() = default;

        virtual HRESULT GetSelectionRange(_In_ IRawElementProviderSimple* pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr) = 0;

        // degenerate range
        virtual HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr) = 0;

        // degenerate range at cursor position
        virtual HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                        const Cursor& cursor,
                                        const std::wstring_view wordDelimiters,
                                        _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr) = 0;

        // specific endpoint range
        virtual HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                        const COORD start,
                                        const COORD end,
                                        const std::wstring_view wordDelimiters,
                                        _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr) = 0;

        // range from a UiaPoint
        virtual HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                        const UiaPoint point,
                                        const std::wstring_view wordDelimiters,
                                        _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr) = 0;

        // weak reference to IUiaData
        IUiaData* _pData{ nullptr };

        std::wstring _wordDelimiters{};

    private:
        // this is used to prevent the object from
        // signaling an event while it is already in the
        // process of signalling another event.
        // This fixes a problem with JAWS where it would
        // call a public method that calls
        // UiaRaiseAutomationEvent to signal something
        // happened, which JAWS then detects the signal
        // and calls the same method in response,
        // eventually overflowing the stack.
        // We aren't using this as a cheap locking
        // mechanism for multi-threaded code.
        std::map<EVENTID, bool> _signalFiringMapping{};

        const COORD _getScreenBufferCoords() const;
        const TextBuffer& _getTextBuffer() const noexcept;
        const Viewport _getViewport() const noexcept;
        void _LockConsole() noexcept;
        void _UnlockConsole() noexcept;
    };
}
