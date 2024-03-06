// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace winrt::TerminalApp::implementation
{
    class LanguageProfileNotifier : public winrt::implements<LanguageProfileNotifier, ITfInputProcessorProfileActivationSink>
    {
    public:
        explicit LanguageProfileNotifier(std::function<void()>&& callback);
        ~LanguageProfileNotifier();
        STDMETHODIMP OnActivated(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID catid, REFGUID guidProfile, HKL hkl, DWORD dwFlags);

    private:
        std::function<void()> _callback;
        wil::com_ptr<ITfSource> _source;
        DWORD _cookie = TF_INVALID_COOKIE;
        HKL _currentKeyboardLayout;
    };
}
