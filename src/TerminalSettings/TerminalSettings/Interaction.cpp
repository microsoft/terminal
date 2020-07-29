#include "pch.h"
#include "Interaction.h"
#include "Interaction.g.cpp"
#include <ObjectModel\GlobalSettings.h>

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Interaction::Interaction()
    {
        m_globalSettingsModel = winrt::make<ObjectModel::implementation::GlobalSettingsModel>();
        InitializeComponent();

    }

    ObjectModel::GlobalSettingsModel Interaction::GlobalSettingsModel()
    {
        return m_globalSettingsModel;
    }

    void Interaction::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }
}
