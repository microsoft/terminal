// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - Jumplist.h
//
// Abstract:
// - The Jumplist is the menu that pops up when right clicking a pinned
// item in the taskbar. This class handles updating the Terminal's jumplist
// using the Terminal's settings.
//

#pragma once

#include "CascadiaSettings.h"
#include "Profile.h"

#include <ShObjIdl.h>

class Jumplist
{
public:
    Jumplist() = default;
    void UpdateJumplist(std::shared_ptr<TerminalApp::CascadiaSettings> settings);
private:
    HRESULT _updateProfiles(winrt::com_ptr<IObjectCollection>& jumplistItems, gsl::span<const winrt::TerminalApp::Profile> profiles);
    HRESULT _createShellLink(const std::wstring_view& name, const std::wstring_view& path, const std::wstring_view& args, winrt::com_ptr<IShellLink>& shLink);
};
