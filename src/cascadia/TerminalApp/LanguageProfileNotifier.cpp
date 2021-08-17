// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "LanguageProfileNotifier.h"

using namespace winrt::TerminalApp::implementation;

LanguageProfileNotifier::LanguageProfileNotifier(std::function<void()>&& callback) :
    _callback{ std::move(callback) },
    _currentKeyboardLayout{ GetKeyboardLayout(0) }
{
    const auto manager = wil::CoCreateInstance<ITfThreadMgr>(CLSID_TF_ThreadMgr);
    _source = manager.query<ITfSource>();
    if (FAILED(_source->AdviseSink(IID_ITfInputProcessorProfileActivationSink, static_cast<ITfInputProcessorProfileActivationSink*>(this), &_cookie)))
    {
        _cookie = TF_INVALID_COOKIE;
        THROW_LAST_ERROR();
    }
}

LanguageProfileNotifier::~LanguageProfileNotifier()
{
    if (_cookie != TF_INVALID_COOKIE)
    {
        _source->UnadviseSink(_cookie);
    }
}

STDMETHODIMP LanguageProfileNotifier::OnActivated(DWORD /*dwProfileType*/, LANGID /*langid*/, REFCLSID /*clsid*/, REFGUID /*catid*/, REFGUID /*guidProfile*/, HKL hkl, DWORD /*dwFlags*/)
{
    if (hkl && hkl != _currentKeyboardLayout)
    {
        _currentKeyboardLayout = hkl;
        try
        {
            _callback();
        }
        CATCH_RETURN();
    }
    return S_OK;
}
