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
            const auto icon{ static_cast<T*>(this)->Icon() };
            if (!_resolvedIcon && !icon.empty())
            {
                const auto resolvedIcon{ Microsoft::Terminal::UI::IconPathConverter::IconWUX(icon) };
                resolvedIcon.Width(16);
                resolvedIcon.Height(16);
                _resolvedIcon = resolvedIcon;
            }
            return _resolvedIcon;
        }

        til::property_changed_event PropertyChanged;

    protected:
        void BaseRaisePropertyChanged(wil::zwstring_view property)
        {
            PropertyChanged.raise(*static_cast<T*>(this), winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs{ property });
        }

        void InvalidateResolvedIcon()
        {
            _resolvedIcon = nullptr;
            BaseRaisePropertyChanged(L"ResolvedIcon");
        }

    private:
        Windows::UI::Xaml::Controls::IconElement _resolvedIcon{ nullptr };
    };
}
