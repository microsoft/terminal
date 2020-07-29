#include "pch.h"
#include <winrt/Windows.UI.Xaml.Media.h>
#include "ColorSchemes.h"
#if __has_include("ColorSchemes.g.cpp")
#include "ColorSchemes.g.cpp"
#endif
#include <ObjectModel\ColorScheme.h>

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;

namespace winrt::SettingsControl::implementation
{
    ColorSchemes::ColorSchemes()
    {
        m_colorSchemeModel = winrt::make<ObjectModel::implementation::ColorSchemeModel>();
        InitializeComponent();
    }

    ObjectModel::ColorSchemeModel ColorSchemes::ColorSchemeModel()
    {
        return m_colorSchemeModel;
    }

    void ColorSchemes::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }

    void ColorSchemes::Background_ColorChanged(ColorPicker const& , ColorChangedEventArgs const& event)
    {
        m_colorSchemeModel.ColorScheme().Background(event.NewColor());
    }
}
