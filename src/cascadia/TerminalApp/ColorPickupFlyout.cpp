#include "pch.h"
#include "ColorPickupFlyout.h"
#include "ColorPickupFlyout.g.cpp"
#include "winrt/Windows.UI.Xaml.Shapes.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include <LibraryResources.h>

using namespace winrt::Windows::UI::Xaml;

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

        OkButton().Content(box_value(RS_(L"Ok")));
    }

    void ColorPickupFlyout::Flyout_Opened(const IInspectable&, const IInspectable&)
    {
        // Pivot retains the selected index.
        // We want to reset it every time you open the flyout.
        FlyoutPivot().SelectedIndex(0);
    }

    // Method Description:
    // - Handler of the click event for the preset color swatches.
    // Reads the color from the clicked rectangle and fires an event
    // with the selected color. After that hides the flyout
    // Arguments:
    // - sender: the rectangle that got clicked
    // Return Value:
    // - <none>
    void ColorPickupFlyout::ColorButton_Click(const IInspectable& sender, const RoutedEventArgs&)
    {
        const auto button{ sender.as<Windows::UI::Xaml::Controls::Button>() };
        const auto rectClr{ button.Background().as<Windows::UI::Xaml::Media::SolidColorBrush>() };
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
    void ColorPickupFlyout::ClearColorButton_Click(const IInspectable&, const RoutedEventArgs&)
    {
        _ColorClearedHandlers();
        Hide();
    }

    // Method Description:
    // - Handles the color selection of the color pickup. Gets
    // the currently selected color and fires an event with it
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ColorPickupFlyout::CustomColorButton_Click(const IInspectable&, const RoutedEventArgs&)
    {
        const auto color = CustomColorPicker().Color();
        _ColorSelectedHandlers(color);
        Hide();
    }

    void ColorPickupFlyout::ColorPicker_ColorChanged(const Microsoft::UI::Xaml::Controls::ColorPicker&, const Microsoft::UI::Xaml::Controls::ColorChangedEventArgs& args)
    {
        _ColorSelectedHandlers(args.NewColor());
    }

    void ColorPickupFlyout::Pivot_SelectionChanged(const IInspectable&, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& args)
    {
        // Pivot likes to take up as much width as possible (as opposed to just what it needs from its children).
        // Using trial-and-error, we've determined a reasonable width for each pivot item.
        const auto selectedItem = args.AddedItems().GetAt(0).as<Windows::UI::Xaml::Controls::PivotItem>();
        const auto tag = unbox_value<hstring>(selectedItem.Tag());
        if (tag == L"Standard")
        {
            FlyoutPivot().Width(170);
        }
        else if (tag == L"Custom")
        {
            FlyoutPivot().Width(340);
        }
    }
}
