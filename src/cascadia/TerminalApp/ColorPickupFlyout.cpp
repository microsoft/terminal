#include "pch.h"
#include "ColorPickupFlyout.h"
#include "ColorPickupFlyout.g.cpp"
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

        OkButton().Content(winrt::box_value(RS_(L"Ok")));
        CustomColorButton().Content(winrt::box_value(RS_(L"TabColorCustomButton/Content")));
        ClearColorButton().Content(winrt::box_value(RS_(L"TabColorClearButton/Content")));
    }

    // Method Description:
    // - Handler of the click event for the preset color swatches.
    // Reads the color from the clicked rectangle and fires an event
    // with the selected color. After that hides the flyout
    // Arguments:
    // - sender: the rectangle that got clicked
    // Return Value:
    // - <none>
    void ColorPickupFlyout::ColorButton_Click(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs&)
    {
        auto button{ sender.as<Windows::UI::Xaml::Controls::Button>() };
        auto rectClr{ button.Background().as<Windows::UI::Xaml::Media::SolidColorBrush>() };
        _ColorSelectedHandlers(rectClr.Color());
        Hide();
    }

    // Method Description:
    // - Handler of the clear color button. Clears the current
    // color of the tab, if any. Hides the flyout after that
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ColorPickupFlyout::ClearColorButton_Click(const IInspectable&, const Windows::UI::Xaml::RoutedEventArgs&)
    {
        _ColorClearedHandlers();
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
    void ColorPickupFlyout::ShowColorPickerButton_Click(const Windows::Foundation::IInspectable&, const Windows::UI::Xaml::RoutedEventArgs&)
    {
        auto visibility = customColorPanel().Visibility();
        if (visibility == winrt::Windows::UI::Xaml::Visibility::Collapsed)
        {
            customColorPanel().Visibility(winrt::Windows::UI::Xaml::Visibility::Visible);
        }
        else
        {
            customColorPanel().Visibility(winrt::Windows::UI::Xaml::Visibility::Collapsed);
        }
    }

    // Method Description:
    // - Handles the color selection of the color pickup. Gets
    // the currently selected color and fires an event with it
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ColorPickupFlyout::CustomColorButton_Click(const Windows::Foundation::IInspectable&, const Windows::UI::Xaml::RoutedEventArgs&)
    {
        auto color = customColorPicker().Color();
        _ColorSelectedHandlers(color);
        Hide();
    }

    void ColorPickupFlyout::ColorPicker_ColorChanged(const Microsoft::UI::Xaml::Controls::ColorPicker&, const Microsoft::UI::Xaml::Controls::ColorChangedEventArgs& args)
    {
        _ColorSelectedHandlers(args.NewColor());
    }
}
