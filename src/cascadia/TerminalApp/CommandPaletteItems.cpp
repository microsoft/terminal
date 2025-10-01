// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Tab.h"

#include <LibraryResources.h>

#include "CommandPaletteItems.h"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    TabPaletteItem::TabPaletteItem(const winrt::TerminalApp::Tab& tab) :
        _tab{ tab }
    {
        _tabChangedRevoker = tab.PropertyChanged(winrt::auto_revoke, [=](auto& sender, auto& e) {
            if (auto senderTab{ sender.try_as<winrt::TerminalApp::Tab>() })
            {
                auto changedProperty = e.PropertyName();
                if (changedProperty == L"Title")
                {
                    BaseRaisePropertyChanged(L"Name");
                }
                else if (changedProperty == L"Icon")
                {
                    BaseRaisePropertyChanged(L"Icon");
                    InvalidateResolvedIcon();
                }
            }
        });

        if (const auto status = tab.TabStatus())
        {
            _tabStatusChangedRevoker = status.PropertyChanged(winrt::auto_revoke, [=](auto& /*sender*/, auto& /*e*/) {
                // Sometimes nested bindings do not get updated,
                // thus let's notify property changed on TabStatus when one of its properties changes
                BaseRaisePropertyChanged(L"TabStatus");
            });
        }
    }
}
