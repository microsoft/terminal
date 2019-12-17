/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WindowUiaProviderBase.hpp

Abstract:
- This module provides UI Automation access to the console window to
  support both automation tests and accessibility (screen reading)
  applications.
- Based on examples, sample code, and guidance from
  https://msdn.microsoft.com/en-us/library/windows/desktop/ee671596(v=vs.85).aspx

Author(s):
- Michael Niksa (MiNiksa)     2017
- Austin Diviness (AustDi)    2017
- Carlos Zamora (cazamor)     2019
--*/

#pragma once

#include "precomp.h"

#include <wrl/implements.h>

namespace Microsoft::Console::Types
{
    class IUiaWindow;

    class WindowUiaProviderBase :
        public WRL::RuntimeClass<WRL::RuntimeClassFlags<WRL::ClassicCom | WRL::InhibitFtmBase>, IRawElementProviderSimple, IRawElementProviderFragment, IRawElementProviderFragmentRoot>
    {
    public:
        ~WindowUiaProviderBase() = default;

    protected:
        WindowUiaProviderBase() = default;
        HRESULT RuntimeClassInitialize(IUiaWindow* baseWindow) noexcept;

        WindowUiaProviderBase(const WindowUiaProviderBase&) = default;
        WindowUiaProviderBase(WindowUiaProviderBase&&) = default;
        WindowUiaProviderBase& operator=(const WindowUiaProviderBase&) = default;
        WindowUiaProviderBase& operator=(WindowUiaProviderBase&&) = default;

    public:
        [[nodiscard]] virtual HRESULT Signal(_In_ EVENTID id) = 0;
        [[nodiscard]] virtual HRESULT SetTextAreaFocus() = 0;

        // IRawElementProviderSimple methods
        IFACEMETHODIMP get_ProviderOptions(_Out_ ProviderOptions* pOptions) override;
        IFACEMETHODIMP GetPatternProvider(_In_ PATTERNID iid,
                                          _COM_Outptr_result_maybenull_ IUnknown** ppInterface) override;
        IFACEMETHODIMP GetPropertyValue(_In_ PROPERTYID idProp,
                                        _Out_ VARIANT* pVariant) override;
        IFACEMETHODIMP get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider) override;

        // IRawElementProviderFragment methods
        virtual IFACEMETHODIMP Navigate(_In_ NavigateDirection direction,
                                        _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) = 0;
        IFACEMETHODIMP GetRuntimeId(_Outptr_result_maybenull_ SAFEARRAY** ppRuntimeId) override;
        IFACEMETHODIMP get_BoundingRectangle(_Out_ UiaRect* pRect) override;
        IFACEMETHODIMP GetEmbeddedFragmentRoots(_Outptr_result_maybenull_ SAFEARRAY** ppRoots) override;
        virtual IFACEMETHODIMP SetFocus() = 0;
        IFACEMETHODIMP get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider) override;

        // IRawElementProviderFragmentRoot methods
        virtual IFACEMETHODIMP ElementProviderFromPoint(_In_ double x,
                                                        _In_ double y,
                                                        _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) = 0;
        virtual IFACEMETHODIMP GetFocus(_COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) = 0;

        RECT GetWindowRect() const noexcept;
        HWND GetWindowHandle() const;
        void ChangeViewport(const SMALL_RECT NewWindow);

    protected:
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
        std::map<EVENTID, bool> _signalEventFiring;

        [[nodiscard]] HRESULT _EnsureValidHwnd() const;

        const OLECHAR* AutomationIdPropertyName = L"Console Window";
        const OLECHAR* ProviderDescriptionPropertyName = L"Microsoft Console Host Window";

    private:
        IUiaWindow* _baseWindow;
    };

    namespace WindowUiaProviderTracing
    {
        enum class ApiCall
        {
            Create,
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
            ElementProviderFromPoint,
            GetFocus
        };

        struct IApiMsg
        {
        };

        struct ApiMessageSignal : public IApiMsg
        {
            EVENTID Signal;
        };

        struct ApiMsgNavigate : public IApiMsg
        {
            NavigateDirection Direction;
        };
    }
}
