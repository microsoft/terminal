#pragma once
#include "HomeGridItem.g.h"

namespace winrt::SettingsControl::implementation
{
    struct HomeGridItem : HomeGridItemT<HomeGridItem>
    {
        HomeGridItem() = delete;
        HomeGridItem(hstring const& title, hstring const& pageTag);

        hstring Title();
        hstring PageTag();
        void Title(hstring const& value);
        void PageTag(hstring const& value);
        winrt::event_token PropertyChanged(Windows::UI::Xaml::Data::PropertyChangedEventHandler const& value);
        void PropertyChanged(winrt::event_token const& token);

        private:
        hstring m_title;
        hstring m_pagetag;
        event<Windows::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged; 
    };
}
