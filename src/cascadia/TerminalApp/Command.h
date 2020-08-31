/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Command.h

Abstract:
- A command represents a single entry in the Command Palette. This is an object
  that has a user facing "name" to display to the user, and an associated action
  which can be dispatched.

- For more information, see GH#2046, #5400, #5674, and #6635

Author(s):
- Mike Griese - June 2020

--*/
#pragma once

#include "Command.g.h"
#include "TerminalWarnings.h"
#include "Profile.h"
#include "..\inc\cppwinrt_utils.h"
#include "SettingsTypes.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class CommandTests;
};

namespace winrt::TerminalApp::implementation
{
    struct Command : CommandT<Command>
    {
        Command();

        static winrt::com_ptr<Command> FromJson(const Json::Value& json,
                                                std::vector<::TerminalApp::SettingsLoadWarnings>& warnings);

        static void ExpandCommands(Windows::Foundation::Collections::IMap<winrt::hstring, winrt::TerminalApp::Command>& commands,
                                   gsl::span<const winrt::TerminalApp::Profile> profiles,
                                   gsl::span<winrt::TerminalApp::ColorScheme> schemes,
                                   std::vector<::TerminalApp::SettingsLoadWarnings>& warnings);

        static std::vector<::TerminalApp::SettingsLoadWarnings> LayerJson(Windows::Foundation::Collections::IMap<winrt::hstring, winrt::TerminalApp::Command>& commands,
                                                                          const Json::Value& json);
        bool HasNestedCommands();
        Windows::Foundation::Collections::IMapView<winrt::hstring, TerminalApp::Command> NestedCommands();

        void RefreshIcon();

        winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker propertyChangedRevoker;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::TerminalApp::ActionAndArgs, Action, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, KeyChordText, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::Windows::UI::Xaml::Controls::IconSource, IconSource, _PropertyChangedHandlers, nullptr);

        GETSET_PROPERTY(ExpandCommandType, IterateOn, ExpandCommandType::None);

    private:
        Json::Value _originalJson;
        Windows::Foundation::Collections::IMap<winrt::hstring, winrt::TerminalApp::Command> _subcommands{ nullptr };

        winrt::hstring _lastIconPath{};

        static std::vector<winrt::TerminalApp::Command> _expandCommand(Command* const expandable,
                                                                       gsl::span<const winrt::TerminalApp::Profile> profiles,
                                                                       gsl::span<winrt::TerminalApp::ColorScheme> schemes,
                                                                       std::vector<::TerminalApp::SettingsLoadWarnings>& warnings);
        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::CommandTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(Command);
}
