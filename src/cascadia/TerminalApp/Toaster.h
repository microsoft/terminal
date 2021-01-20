// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Toaster.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct Toaster : ToasterT<Toaster>
    {
        Toaster(Windows::UI::Xaml::Controls::Panel root);
        void MakeToast(const hstring& title,
                       const hstring& subtitle,
                       const Windows::UI::Xaml::Controls::Panel& target);

    private:
        Windows::Foundation::Collections::IVector<TerminalApp::Toast> _toasts;

        winrt::Windows::UI::Xaml::Controls::IPanel _root;

        void _onToastClosed(const IInspectable& sender, const IInspectable& eventArgs);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(Toaster);
}
