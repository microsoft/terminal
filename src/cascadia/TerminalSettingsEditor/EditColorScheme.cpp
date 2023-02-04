// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "EditColorScheme.h"
#include "EditColorScheme.g.cpp"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace WUX;
using namespace WUX::Navigation;
using namespace WUXC;
using namespace WUXMedia;
using namespace WF;
using namespace WFC;
using namespace MUXC;

namespace winrt
{
    namespace MUX = MUX;
    namespace WUX = WUX;
}

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    EditColorScheme::EditColorScheme()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(NameBox(), RS_(L"ColorScheme_Name/Header"));
        Automation::AutomationProperties::SetFullDescription(NameBox(), RS_(L"ColorScheme_Name/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        ToolTipService::SetToolTip(NameBox(), box_value(RS_(L"ColorScheme_Name/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip")));
        Automation::AutomationProperties::SetName(RenameAcceptButton(), RS_(L"RenameAccept/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetName(RenameCancelButton(), RS_(L"RenameCancel/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetName(SetAsDefaultButton(), RS_(L"ColorScheme_SetAsDefault/Header"));
    }

    void EditColorScheme::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::ColorSchemeViewModel>();

        // Set the text disclaimer for the text box
        hstring disclaimer{};
        if (_ViewModel.IsInBoxScheme())
        {
            // load disclaimer for in-box profiles
            disclaimer = RS_(L"ColorScheme_DeleteButtonDisclaimerInBox/Text");
        }
        RenameContainer().HelpText(disclaimer);

        NameBox().Text(_ViewModel.Name());
    }

    void EditColorScheme::ColorPickerChanged(const IInspectable& sender,
                                             const MUX::Controls::ColorChangedEventArgs& args)
    {
        const til::color newColor{ args.NewColor() };
        if (const auto& picker{ sender.try_as<MUX::Controls::ColorPicker>() })
        {
            if (const auto& tag{ picker.Tag() })
            {
                if (const auto index{ tag.try_as<uint8_t>() })
                {
                    if (index < ColorTableDivider)
                    {
                        _ViewModel.NonBrightColorTable().GetAt(*index).Color(newColor);
                    }
                    else
                    {
                        _ViewModel.BrightColorTable().GetAt(*index - ColorTableDivider).Color(newColor);
                    }
                }
                else if (const auto stringTag{ tag.try_as<hstring>() })
                {
                    if (stringTag == ForegroundColorTag)
                    {
                        _ViewModel.ForegroundColor().Color(newColor);
                    }
                    else if (stringTag == BackgroundColorTag)
                    {
                        _ViewModel.BackgroundColor().Color(newColor);
                    }
                    else if (stringTag == CursorColorTag)
                    {
                        _ViewModel.CursorColor().Color(newColor);
                    }
                    else if (stringTag == SelectionBackgroundColorTag)
                    {
                        _ViewModel.SelectionBackgroundColor().Color(newColor);
                    }
                }
            }
        }
    }

    void EditColorScheme::RenameAccept_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _RenameCurrentScheme(NameBox().Text());
    }

    void EditColorScheme::RenameCancel_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        RenameErrorTip().IsOpen(false);
        NameBox().Text(_ViewModel.Name());
    }

    void EditColorScheme::NameBox_PreviewKeyDown(const IInspectable& /*sender*/, const WUX::Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == WS::VirtualKey::Enter)
        {
            _RenameCurrentScheme(NameBox().Text());
            e.Handled(true);
        }
        else if (e.OriginalKey() == WS::VirtualKey::Escape)
        {
            RenameErrorTip().IsOpen(false);
            NameBox().Text(_ViewModel.Name());
            e.Handled(true);
        }
    }

    void EditColorScheme::_RenameCurrentScheme(hstring newName)
    {
        if (_ViewModel.RequestRename(newName))
        {
            // update the UI
            RenameErrorTip().IsOpen(false);

            NameBox().Focus(FocusState::Programmatic);
        }
        else
        {
            RenameErrorTip().Target(NameBox());
            RenameErrorTip().IsOpen(true);

            // focus the name box
            NameBox().Focus(FocusState::Programmatic);
            NameBox().SelectAll();
        }
    }
}
