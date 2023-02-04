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

struct IObjectCollection;
struct IShellLinkW;

class Jumplist
{
public:
    static winrt::fire_and_forget UpdateJumplist(const MTSM::CascadiaSettings& settings) noexcept;

private:
    static void _updateProfiles(IObjectCollection* jumplistItems, WFC::IVectorView<MTSM::Profile> profiles);
    static winrt::com_ptr<IShellLinkW> _createShellLink(const std::wstring_view name, const std::wstring_view path, const std::wstring_view args);
};
