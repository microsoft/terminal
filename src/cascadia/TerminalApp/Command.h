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

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class CommandTests;
};

namespace winrt::TerminalApp::implementation
{
    enum class ExpandCommandType : uint32_t
    {
        None = 0,
        Profiles
    };

    struct Command : CommandT<Command>
    {
        Command();

        static winrt::com_ptr<Command> FromJson(const Json::Value& json,
                                                std::vector<::TerminalApp::SettingsLoadWarnings>& warnings);

        static void ExpandCommands(std::unordered_map<winrt::hstring, winrt::TerminalApp::Command>& commands,
                                   gsl::span<const ::TerminalApp::Profile> profiles,
                                   std::vector<::TerminalApp::SettingsLoadWarnings>& warnings);

        static std::vector<::TerminalApp::SettingsLoadWarnings> LayerJson(std::unordered_map<winrt::hstring, winrt::TerminalApp::Command>& commands,
                                                                          const Json::Value& json);

        Windows::Foundation::Collections::IVector<TerminalApp::Command> NestedCommands();

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::TerminalApp::ActionAndArgs, Action, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, KeyChordText, _PropertyChangedHandlers);
        GETSET_PROPERTY(ExpandCommandType, IterateOn, ExpandCommandType::None);

    private:
        Json::Value _originalJson;
        std::unordered_map<winrt::hstring, winrt::TerminalApp::Command> _subcommands;
        Windows::Foundation::Collections::IVector<TerminalApp::Command> _nestedCommandsView{ nullptr };

        static std::vector<winrt::TerminalApp::Command> _expandCommand(winrt::com_ptr<Command> expandable,
                                                                       gsl::span<const ::TerminalApp::Profile> profiles,
                                                                       std::vector<::TerminalApp::SettingsLoadWarnings>& warnings);
        void _createView();

        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::CommandTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(Command);
}
