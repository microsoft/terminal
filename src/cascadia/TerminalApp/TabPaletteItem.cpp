// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TabPaletteItem.h"
#include "TerminalTab.h"
#include <LibraryResources.h>

#include "TabPaletteItem.g.cpp"

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
    TabPaletteItem::TabPaletteItem(const winrt::TerminalApp::TabBase& tab) :
        _tab(tab)
    {
        Name(tab.Title());
        Icon(tab.Icon());

        _tabChangedRevoker = tab.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& sender, auto& e) {
            auto item{ weakThis.get() };
            auto senderTab{ sender.try_as<winrt::TerminalApp::TabBase>() };

            if (item && senderTab)
            {
                auto changedProperty = e.PropertyName();
                if (changedProperty == L"Title")
                {
                    item->Name(senderTab.Title());
                }
                else if (changedProperty == L"Icon")
                {
                    item->Icon(senderTab.Icon());
                }
            }
        });

        if (const auto terminalTab{ tab.try_as<winrt::TerminalApp::TerminalTab>() })
        {
            const auto status = terminalTab.TabStatus();
            TabStatus(status);

            _tabStatusChangedRevoker = status.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& /*sender*/, auto& /*e*/) {
                // Sometimes nested bindings do not get updated,
                // thus let's notify property changed on TabStatus when one of its properties changes
                auto item{ weakThis.get() };
                item->_PropertyChangedHandlers(*item, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"TabStatus" });
            });
        }
    }
}
