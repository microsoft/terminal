#include "pch.h"
#include "ColorPickupFlyout.h"
#include "ColorPickupFlyout.g.cpp"
#include "winrt/Windows.UI.Xaml.Media.h"
#include "winrt/Windows.UI.Xaml.Shapes.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include <LibraryResources.h>

namespace winrt::TerminalApp::implementation
{
    // Method Description:
    // - Default constructor, localizes the buttons and hooks
    // up the event fired by the custom color picker, so that
    // the tab color is set on the fly when selecting a non-preset color
    // Arguments:
    // - <none>
    ColorPickupFlyout::ColorPickupFlyout()
    {
        InitializeComponent();

        auto okText = RS_(L"Ok");
        auto customColorText = RS_(L"TabCustomColorChoose");
        auto clearColorText = RS_(L"TabColorClear");

        btnOk().Content(winrt::box_value(okText));
        btnCustomColor().Content(winrt::box_value(customColorText));
        btnClearColor().Content(winrt::box_value(clearColorText));

        customColorPicker().ColorChanged({ get_weak(), &ColorPickupFlyout::_ColorChangedHandler });
    }

    // Method Description:
    // - Handler of the click event for the preset color swatches.
    // Reads the color from the clicked rectangle and fires an event
    // with the selected color. After that hides the flyout
    // Arguments:
    // - sender: the rectangle that got clicked
    // Return Value:
    // - <none>
    void ColorPickupFlyout::ColorButton_Click(IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const&)
    {
        auto btn{ sender.as<Windows::UI::Xaml::Controls::Button>() };
        auto rectangle{ btn.Content().as<Windows::UI::Xaml::Shapes::Rectangle>() };
        auto rectClr{ rectangle.Fill().as<Windows::UI::Xaml::Media::SolidColorBrush>() };
        _colorSelected(rectClr.Color());
        Hide();
    }

    // Method Description:
    // - Handler of the clear color button. Clears the current
    // color of the tab, if any. Hides the flyout after that
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ColorPickupFlyout::ClearColorButton_Click(IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&)
    {
        _colorCleared();
        Hide();
    }

    // Method Description:
    // - Handler of the select custom color button. Expands or collapses the flyout
    // to show the color picker. In order to accomplish this a FlyoutPresenterStyle is used,
    // in which a Style is embedded, containing the desired width
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ColorPickupFlyout::ShowColorPickerButton_Click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&)
    {
        auto targetType = this->FlyoutPresenterStyle().TargetType();
        auto s = Windows::UI::Xaml::Style{};
        s.TargetType(targetType);
        auto visibility = customColorPanel().Visibility();
        if (visibility == winrt::Windows::UI::Xaml::Visibility::Collapsed)
        {
            customColorPanel().Visibility(winrt::Windows::UI::Xaml::Visibility::Visible);
            auto setter = Windows::UI::Xaml::Setter(Windows::UI::Xaml::FrameworkElement::MinWidthProperty(), winrt::box_value(540));
            s.Setters().Append(setter);
        }
        else
        {
            customColorPanel().Visibility(winrt::Windows::UI::Xaml::Visibility::Collapsed);
            auto setter = Windows::UI::Xaml::Setter(Windows::UI::Xaml::FrameworkElement::MinWidthProperty(), winrt::box_value(0));
            s.Setters().Append(setter);
        }
        this->FlyoutPresenterStyle(s);
    }

    // Method Description:
    // - Handles the color selection of the color pickup. Gets
    // the currently selected color and fires an event with it
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ColorPickupFlyout::CustomColorButton_Click(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&)
    {
        auto color = customColorPicker().Color();
        _colorSelected(color);
        Hide();
    }

    void ColorPickupFlyout::_ColorChangedHandler(const Windows::UI::Xaml::Controls::ColorPicker&, const Windows::UI::Xaml::Controls::ColorChangedEventArgs& args)
    {
        _colorSelected(args.NewColor());
    }

    DEFINE_EVENT(ColorPickupFlyout, ColorSelected, _colorSelected, TerminalApp::ColorSelectedArgs);
    DEFINE_EVENT(ColorPickupFlyout, ColorCleared, _colorCleared, TerminalApp::ColorClearedArgs);
}
