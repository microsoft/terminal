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

#include "precomp.h"
#include "../buffer/out/textBuffer.hpp"
#include "UiaTextRangeBase.hpp"
#include "IUiaData.h"

#include <wrl/implements.h>

namespace Microsoft::Console::Types
{
    class WindowUiaProviderBase;
    class Viewport;

    class ScreenInfoUiaProviderBase :
        public WRL::RuntimeClass<WRL::RuntimeClassFlags<WRL::ClassicCom | WRL::InhibitFtmBase>, IRawElementProviderSimple, IRawElementProviderFragment, ITextProvider>
    {
    public:
        HRESULT RuntimeClassInitialize(_In_ IUiaData* pData) noexcept;

        ScreenInfoUiaProviderBase(const ScreenInfoUiaProviderBase&) = default;
        ScreenInfoUiaProviderBase(ScreenInfoUiaProviderBase&&) = default;
        ScreenInfoUiaProviderBase& operator=(const ScreenInfoUiaProviderBase&) = default;
        ScreenInfoUiaProviderBase& operator=(ScreenInfoUiaProviderBase&&) = default;
        ~ScreenInfoUiaProviderBase() = default;

        [[nodiscard]] HRESULT Signal(_In_ EVENTID id);

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

        virtual HRESULT GetSelectionRanges(_In_ IRawElementProviderSimple* pProvider, _Out_ std::deque<WRL::ComPtr<UiaTextRangeBase>>& selectionRanges) = 0;

        // degenerate range
        virtual HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider, _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr) = 0;

        // degenerate range at cursor position
        virtual HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                        const Cursor& cursor,
                                        _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr) = 0;

        // specific endpoint range
        virtual HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                        const Endpoint start,
                                        const Endpoint end,
                                        const bool degenerate,
                                        _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr) = 0;

        // range from a UiaPoint
        virtual HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                        const UiaPoint point,
                                        _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr) = 0;

        // weak reference to IUiaData
        IUiaData* _pData;

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
        std::map<EVENTID, bool> _signalFiringMapping;

        const COORD _getScreenBufferCoords() const;
        const TextBuffer& _getTextBuffer() const noexcept;
        const Viewport _getViewport() const noexcept;
        void _LockConsole() noexcept;
        void _UnlockConsole() noexcept;
    };

    namespace ScreenInfoUiaProviderTracing
    {
        enum class ApiCall
        {
            Constructor,
            Signal,
            GetProviderOptions,
            GetPatternProvider,
            GetPropertyValue,
            GetHostRawElementProvider,
            Navigate,
            GetRuntimeId,
            GetBoundingRectangle,
            GetEmbeddedFragmentRoots,
            SetFocus,
            GetFragmentRoot,
            GetSelection,
            GetVisibleRanges,
            RangeFromChild,
            RangeFromPoint,
            GetDocumentRange,
            GetSupportedTextSelection
        };

        struct IApiMsg
        {
        };

        struct ApiMsgSignal : public IApiMsg
        {
            EVENTID Signal;
        };

        struct ApiMsgNavigate : public IApiMsg
        {
            NavigateDirection Direction;
        };

        struct ApiMsgGetSelection : public IApiMsg
        {
            bool AreaSelected;
            unsigned int SelectionRowCount;
        };
    }
}
