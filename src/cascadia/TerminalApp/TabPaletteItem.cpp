// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TabPaletteItem.h"
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
    TabPaletteItem::TabPaletteItem(winrt::TerminalApp::TabBase const& tab) :
        _tab(tab)
    {
        Name(tab.Title());
        Icon(tab.Icon());

        _tabChangedRevoker = tab.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& sender, auto& e) {
            auto item{ weakThis.get() };
            auto senderTab{ sender.try_as<TabBase>() };

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
    }
}
