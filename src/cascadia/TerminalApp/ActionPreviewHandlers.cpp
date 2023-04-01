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
    // - We use _lastPreviewedAction to determine what type of action to clean up.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_EndPreview()
    {
        if (_lastPreviewedAction == nullptr)
        {
            return;
        }
        switch (_lastPreviewedAction.Action())
        {
        case ShortcutAction::SetColorScheme:
        case ShortcutAction::AdjustOpacity:
        {
            _RunRestorePreviews();
            break;
        }
        }
        _lastPreviewedAction = nullptr;
    }

    // Method Description:
    // - Revert any changes from the preview action. This will run the restore
    //   function that the preview added to _restorePreviewFuncs
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_RunRestorePreviews()
    {
        // Apply the reverts in reverse order - If we had multiple previews
        // stacked on top of each other, then this will ensure the first one in
        // is the last one out.
        const auto cleanup = wil::scope_exit([this]() {
            _restorePreviewFuncs.clear();
        });

        for (const auto& f : _restorePreviewFuncs)
        {
            f();
        }
    }

    // Method Description:
    // - Preview handler for the SetColorScheme action.
    // - This method will stash functions to reset the settings of the selected controls in
    //   _restorePreviewFuncs. Then it will create a new TerminalSettings object
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
        if (const auto& scheme{ _settings.GlobalSettings().ColorSchemes().TryLookup(args.SchemeName()) })
        {
            const auto backup = _restorePreviewFuncs.empty();

            _ApplyToActiveControls([&](const auto& control) {
                // Stash a copy of the current scheme.
                auto originalScheme{ control.ColorScheme() };

                // Apply the new scheme.
                control.ColorScheme(scheme.ToCoreScheme());

                if (backup)
                {
                    // Each control will emplace a revert into the
                    // _restorePreviewFuncs for itself.
                    _restorePreviewFuncs.emplace_back([=]() {
                        // On dismiss, restore the original scheme.
                        control.ColorScheme(originalScheme);
                    });
                }
            });
        }
    }

    void TerminalPage::_PreviewAdjustOpacity(const Settings::Model::AdjustOpacityArgs& args)
    {
        const auto backup = _restorePreviewFuncs.empty();

        _ApplyToActiveControls([&](const auto& control) {
            // Stash a copy of the original opacity.
            auto originalOpacity{ control.BackgroundOpacity() };

            // Apply the new opacity
            control.AdjustOpacity(args.Opacity() / 100.0, args.Relative());

            if (backup)
            {
                _restorePreviewFuncs.emplace_back([=]() {
                    // On dismiss:
                    // Don't adjust relatively, just set outright.
                    control.AdjustOpacity(originalOpacity, false);
                });
            }
        });
    }

    void TerminalPage::_PreviewAction(const Settings::Model::ActionAndArgs& args)
    {
        switch (args.Action())
        {
        case ShortcutAction::SetColorScheme:
            _PreviewColorScheme(args.Args().try_as<SetColorSchemeArgs>());
            break;
        case ShortcutAction::AdjustOpacity:
            _PreviewAdjustOpacity(args.Args().try_as<AdjustOpacityArgs>());
            break;
        }

        // GH#9818 Other ideas for actions that could be preview-able:
        // * Set Font size
        // * Set acrylic true/false/opacity?
        // * SetPixelShaderPath?
        // * SetWindowTheme (light/dark/system/<some theme from #3327>)?

        // Stash this action, so we know what to do when we're done
        // previewing.
        _lastPreviewedAction = args;
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
            _PreviewAction(args.ActionAndArgs());
        }
    }
}
