// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace winrt::TerminalApp::implementation
{
    template<typename T, winrt::TerminalApp::PaletteItemType Ty>
    struct BasePaletteItem
    {
    public:
        winrt::TerminalApp::PaletteItemType Type() { return Ty; }

        Windows::UI::Xaml::Controls::IconElement ResolvedIcon()
        {
            const auto icon = Microsoft::Terminal::UI::IconPathConverter::IconWUX(static_cast<T*>(this)->Icon());
            icon.Width(16);
            icon.Height(16);
            return icon;
        }

        til::property_changed_event PropertyChanged;

    protected:
        template<typename... Ts>
        void BaseRaisePropertyChanged(Ts&&... args)
        {
            PropertyChanged.raise(std::forward<Ts>(args)...);
        }
    };
}
