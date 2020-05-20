#include "pch.h"
#include "Profiles.h"
#include "Profiles.g.cpp"

using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Control::implementation
{
    Profiles::Profiles()
    {
        InitializeComponent();
    }

    Profiles::Profiles(winrt::hstring const& name) : m_name{ name }
    {

    }

    winrt::hstring Profiles::Name()
    {
        return m_name;
    }

    void Profiles::Name(winrt::hstring const& value)
    {
        if (m_name != value)
        {

        }
    }

    int32_t Profiles::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void Profiles::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void Profiles::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {

    }
}
