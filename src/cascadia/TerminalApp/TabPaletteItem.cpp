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
    TabPaletteItem::TabPaletteItem(winrt::TerminalApp::TabBase const& tab) :
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
            if (const auto tabImpl{ winrt::get_self<winrt::TerminalApp::implementation::TerminalTab>(terminalTab) })
            {
                const auto headerControl{ tabImpl->HeaderControl() };
                IsProgressRingActive(headerControl.IsProgressRingActive());
                IsProgressRingIndeterminate(headerControl.IsProgressRingIndeterminate());
                ProgressValue(headerControl.ProgressValue());
                BellIndicator(headerControl.BellIndicator());
                IsPaneZoomed(headerControl.IsPaneZoomed());

                _tabHeaderChangedRevoker = headerControl.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& sender, auto& e) {
                    auto item{ weakThis.get() };
                    auto senderHeaderControl{ sender.try_as<winrt::TerminalApp::TabHeaderControl>() };

                    if (item && senderHeaderControl)
                    {
                        auto changedProperty = e.PropertyName();
                        if (changedProperty == L"IsProgressRingActive")
                        {
                            item->IsProgressRingActive(senderHeaderControl.IsProgressRingActive());
                        }
                        else if (changedProperty == L"IsProgressRingIndeterminate")
                        {
                            item->IsProgressRingIndeterminate(senderHeaderControl.IsProgressRingIndeterminate());
                        }
                        else if (changedProperty == L"ProgressValue")
                        {
                            item->ProgressValue(senderHeaderControl.ProgressValue());
                        }
                        else if (changedProperty == L"IsPaneZoomed")
                        {
                            item->IsPaneZoomed(senderHeaderControl.IsPaneZoomed());
                        }
                        else if (changedProperty == L"BellIndicator")
                        {
                            item->BellIndicator(senderHeaderControl.BellIndicator());
                        }
                    }
                });
            }
        }
    }
}
