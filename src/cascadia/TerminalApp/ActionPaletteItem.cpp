// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionPaletteItem.h"
#include <LibraryResources.h>

#include "ActionPaletteItem.g.cpp"

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
    ActionPaletteItem::ActionPaletteItem(const MTSM::Command& command) :
        _Command(command)
    {
        Name(command.Name());
        KeyChordText(command.KeyChordText());
        Icon(command.IconPath());

        _commandChangedRevoker = command.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& sender, auto& e) {
            auto item{ weakThis.get() };
            auto senderCommand{ sender.try_as<MTSM::Command>() };

            if (item && senderCommand)
            {
                auto changedProperty = e.PropertyName();
                if (changedProperty == L"Name")
                {
                    item->Name(senderCommand.Name());
                }
                else if (changedProperty == L"KeyChordText")
                {
                    item->KeyChordText(senderCommand.KeyChordText());
                }
                else if (changedProperty == L"IconPath")
                {
                    item->Icon(senderCommand.IconPath());
                }
            }
        });
    }
}
