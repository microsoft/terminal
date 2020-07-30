#include "pch.h"
#include "HomeGridItem.h"
#include "HomeGridItem.g.cpp"

namespace winrt::SettingsControl::implementation
{
    HomeGridItem::HomeGridItem(hstring const& title, hstring const& pageTag) : m_title { title }, m_pagetag { pageTag }
    {
    }
    hstring HomeGridItem::Title()
    {
        return m_title;
    }
    hstring HomeGridItem::PageTag()
    {
        return m_pagetag;
    }
    void HomeGridItem::Title(hstring const& value)
    {
        if (m_title != value)
        {
            m_title = value;
            m_propertyChanged(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"Title" });
        }
    }
    void HomeGridItem::PageTag(hstring const& value)
    {
        if (m_pagetag != value)
        {
            m_pagetag = value;
            m_propertyChanged(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"Page Tag" });
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
