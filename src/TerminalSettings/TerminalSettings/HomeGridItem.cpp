#include "pch.h"
#include "HomeGridItem.h"
#include "HomeGridItem.g.cpp"

namespace winrt::SettingsControl::implementation
{
    HomeGridItem::HomeGridItem(hstring const& title) : m_title { title }
    {
    }
    hstring HomeGridItem::Title()
    {
        return m_title;
    }
    void HomeGridItem::Title(hstring const& value)
    {
        if (m_title != value)
        {
            m_title = value;
            m_propertyChanged(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"Title" });
        }
    }
    winrt::event_token HomeGridItem::PropertyChanged(Windows::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }
    void HomeGridItem::PropertyChanged(winrt::event_token const& token)
    {
        m_propertyChanged.remove(token);
    }
}
