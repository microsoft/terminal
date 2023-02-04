// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TabPaletteItem.h"
#include "TerminalTab.h"
#include <LibraryResources.h>

#include "TabPaletteItem.g.cpp"

using namespace winrt;
using namespace MTApp;
using namespace WUC;
using namespace WUX;
using namespace WS;
using namespace WF;
using namespace WFC;
using namespace MTSM;

namespace winrt::TerminalApp::implementation
{
    TabPaletteItem::TabPaletteItem(const MTApp::TabBase& tab) :
        _tab(tab)
    {
        Name(tab.Title());
        Icon(tab.Icon());

        _tabChangedRevoker = tab.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& sender, auto& e) {
            auto item{ weakThis.get() };
            auto senderTab{ sender.try_as<MTApp::TabBase>() };

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

        if (const auto terminalTab{ tab.try_as<MTApp::TerminalTab>() })
        {
            const auto status = terminalTab.TabStatus();
            TabStatus(status);

            _tabStatusChangedRevoker = status.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& /*sender*/, auto& /*e*/) {
                // Sometimes nested bindings do not get updated,
                // thus let's notify property changed on TabStatus when one of its properties changes
                auto item{ weakThis.get() };
                item->_PropertyChangedHandlers(*item, WUX::Data::PropertyChangedEventArgs{ L"TabStatus" });
            });
        }
    }
}
