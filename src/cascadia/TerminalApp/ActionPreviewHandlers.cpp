// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"
#include "Utils.h"
#include "../../types/inc/utils.hpp"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace ::TerminalApp;
using namespace ::Microsoft::Console;
using namespace std::chrono_literals;
namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    // Method Description:
    // - Stop previewing the currently previewed action. We can use this to
    //   clean up any state from that action's preview.
    // - We use _lastPreviewedCommand to determine what type of action to clean up.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_EndPreview()
    {
        if (_lastPreviewedCommand == nullptr || _lastPreviewedCommand.ActionAndArgs() == nullptr)
        {
            return;
        }
        switch (_lastPreviewedCommand.ActionAndArgs().Action())
        {
        case ShortcutAction::SetColorScheme:
        {
            _EndPreviewColorScheme();
            break;
        }
        }
        _lastPreviewedCommand = nullptr;
    }

    // Method Description:
    // - Revert any changes from the preview on a SetColorScheme action. This
    //   will remove the preview TerminalSettings we inserted into the control's
    //   TerminalSettings graph, and update the control.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_EndPreviewColorScheme()
    {
        // Get the focused control
        if (const auto& activeControl{ _GetActiveControl() })
        {
            // Get the runtime settings of the focused control
            const auto& controlSettings{ activeControl.Settings().as<TerminalSettings>() };

            // Get the control's root settings, the ones that we actually
            // assigned to it.
            auto parentSettings{ controlSettings.GetParent() };
            while (parentSettings.GetParent() != nullptr)
            {
                parentSettings = parentSettings.GetParent();
            }

            // If the root settings are the same as the ones we stashed,
            // then reset the parent of the runtime settings to the stashed
            // settings. This condition might be false if the settings
            // hot-reloaded while the palette was open. In that case, we
            // don't want to reset the settings to what they were _before_
            // the hot-reload.
            if (_originalSettings == parentSettings)
            {
                // Set the original settings as the parent of the control's settings
                activeControl.Settings().as<TerminalSettings>().SetParent(_originalSettings);
            }

            activeControl.UpdateSettings();
        }
        _originalSettings = nullptr;
    }

    // Method Description:
    // - Preview handler for the SetColorScheme action.
    // - This method will stash the settings of the current control in
    //   _originalSettings. Then it will create a new TerminalSettings object
    //   with only the properties from the ColorScheme set. It'll _insert_ a
    //   TerminalSettings between the control's root settings (built from
    //   CascadiaSettings) and the control's runtime settings. That'll cause the
    //   control to use _that_ table as the active color scheme.
    // Arguments:
    // - args: The SetColorScheme args with the name of the scheme to use.
    // Return Value:
    // - <none>
    void TerminalPage::_PreviewColorScheme(const Settings::Model::SetColorSchemeArgs& args)
    {
        // Get the focused control
        if (const auto& activeControl{ _GetActiveControl() })
        {
            if (const auto& scheme{ _settings.GlobalSettings().ColorSchemes().TryLookup(args.SchemeName()) })
            {
                // Get the settings of the focused control and stash them
                const auto& controlSettings = activeControl.Settings().as<TerminalSettings>();
                // Make sure to recurse up to the root - if you're doing
                // this while you're currently previewing a SetColorScheme
                // action, then the parent of the control's settings is _the
                // last preview TerminalSettings we inserted! We don't want
                // to save that one!
                _originalSettings = controlSettings.GetParent();
                while (_originalSettings.GetParent() != nullptr)
                {
                    _originalSettings = _originalSettings.GetParent();
                }
                // Create a new child for those settings
                TerminalSettingsCreateResult fake{ _originalSettings };
                const auto& childStruct = TerminalSettings::CreateWithParent(fake);
                // Modify the child to have the applied color scheme
                childStruct.DefaultSettings().ApplyColorScheme(scheme);

                // Insert that new child as the parent of the control's settings
                controlSettings.SetParent(childStruct.DefaultSettings());
                activeControl.UpdateSettings();
            }
        }
    }

    // Method Description:
    // - Handler for the CommandPalette::PreviewAction event. The Command
    //   Palette will raise this even when an action is selected, but _not_
    //   committed. This gives the Terminal a chance to display a "preview" of
    //   the action.
    // - This will be called with a null args before an action is dispatched, or
    //   when the palette is dismissed.
    // - For any actions that are to be previewed here, MAKE SURE TO RESTORE THE
    //   STATE IN `TerminalPage::_EndPreview`. That method is called to revert
    //   the terminal to the state it was in at the start of the preview.
    // - Currently, only SetColorScheme actions are preview-able.
    // Arguments:
    // - args: The Command that's trying to be previewed, or nullptr if we should stop the preview.
    // Return Value:
    // - <none>
    void TerminalPage::_PreviewActionHandler(const IInspectable& /*sender*/,
                                             const Microsoft::Terminal::Settings::Model::Command& args)
    {
        if (args == nullptr || args.ActionAndArgs() == nullptr)
        {
            _EndPreview();
        }
        else
        {
            switch (args.ActionAndArgs().Action())
            {
            case ShortcutAction::SetColorScheme:
            {
                _PreviewColorScheme(args.ActionAndArgs().Args().try_as<SetColorSchemeArgs>());
                break;
            }
            }

            // GH#9818 Other ideas for actions that could be preview-able:
            // * Set Font size
            // * Set acrylic true/false/opacity?
            // * SetPixelShaderPath?
            // * SetWindowTheme (light/dark/system/<some theme from #3327>)?

            // Stash this action, so we know what to do when we're done
            // previewing.
            _lastPreviewedCommand = args;
        }
    }
}
