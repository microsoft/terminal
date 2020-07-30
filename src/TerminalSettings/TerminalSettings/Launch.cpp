#include "pch.h"
#include "Launch.h"
#include "Launch.g.cpp"
#include <ObjectModel\GlobalSettings.h>

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Launch::Launch()
    {
        m_globalSettingsModel = winrt::make<ObjectModel::implementation::GlobalSettingsModel>();
        InitializeComponent();
    }

    ObjectModel::GlobalSettingsModel Launch::GlobalSettingsModel()
    {
        return m_globalSettingsModel;
    }

    void Launch::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }
}
