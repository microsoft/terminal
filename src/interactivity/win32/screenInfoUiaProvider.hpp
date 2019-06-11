/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- screenInfoUiaProvider.hpp

Abstract:
- This module provides UI Automation access to the screen buffer to
  support both automation tests and accessibility (screen reading)
  applications.
- Based on examples, sample code, and guidance from
  https://msdn.microsoft.com/en-us/library/windows/desktop/ee671596(v=vs.85).aspx

Author(s):
- Michael Niksa (MiNiksa)     2017
- Austin Diviness (AustDi)    2017
--*/

#pragma once

#include "precomp.h"

// Forward declare, prevent circular ref.
class SCREEN_INFORMATION;

namespace Microsoft::Console::Interactivity::Win32
{
    class Window;

    class WindowUiaProvider;

    class ScreenInfoUiaProvider final :
        public IRawElementProviderSimple,
        public IRawElementProviderFragment,
        public ITextProvider
    {
    public:
        ScreenInfoUiaProvider(_In_ WindowUiaProvider* const pUiaParent);
        virtual ~ScreenInfoUiaProvider();

        [[nodiscard]] HRESULT Signal(_In_ EVENTID id);

        // IUnknown methods
        IFACEMETHODIMP_(ULONG)
        AddRef();
        IFACEMETHODIMP_(ULONG)
        Release();
        IFACEMETHODIMP QueryInterface(_In_ REFIID riid,
                                      _COM_Outptr_result_maybenull_ void** ppInterface);

        // IRawElementProviderSimple methods
        IFACEMETHODIMP get_ProviderOptions(_Out_ ProviderOptions* pOptions);
        IFACEMETHODIMP GetPatternProvider(_In_ PATTERNID iid,
                                          _COM_Outptr_result_maybenull_ IUnknown** ppInterface);
        IFACEMETHODIMP GetPropertyValue(_In_ PROPERTYID idProp,
                                        _Out_ VARIANT* pVariant);
        IFACEMETHODIMP get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider);

        // IRawElementProviderFragment methods
        IFACEMETHODIMP Navigate(_In_ NavigateDirection direction,
                                _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider);
        IFACEMETHODIMP GetRuntimeId(_Outptr_result_maybenull_ SAFEARRAY** ppRuntimeId);
        IFACEMETHODIMP get_BoundingRectangle(_Out_ UiaRect* pRect);
        IFACEMETHODIMP GetEmbeddedFragmentRoots(_Outptr_result_maybenull_ SAFEARRAY** ppRoots);
        IFACEMETHODIMP SetFocus();
        IFACEMETHODIMP get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider);

        // ITextProvider
        IFACEMETHODIMP GetSelection(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal);
        IFACEMETHODIMP GetVisibleRanges(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal);
        IFACEMETHODIMP RangeFromChild(_In_ IRawElementProviderSimple* childElement,
                                      _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal);
        IFACEMETHODIMP RangeFromPoint(_In_ UiaPoint point,
                                      _COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal);
        IFACEMETHODIMP get_DocumentRange(_COM_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal);
        IFACEMETHODIMP get_SupportedTextSelection(_Out_ SupportedTextSelection* pRetVal);

    private:
        // Ref counter for COM object
        ULONG _cRefs;

        // weak reference to uia parent
        WindowUiaProvider* const _pUiaParent;

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
        static SCREEN_INFORMATION& _getScreenInfo();
        static IConsoleWindow* const _getIConsoleWindow();
    };

    namespace ScreenInfoUiaProviderTracing
    {
        enum class ApiCall
        {
            Constructor,
            Signal,
            AddRef,
            Release,
            QueryInterface,
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
