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
#include "ActionAndArgs.h"
#include "SettingsTypes.h"

// fwdecl unittest classes
namespace SettingsModelUnitTests
{
    class DeserializationTests;
    class CommandTests;
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct Command : CommandT<Command>
    {
        Command();
        com_ptr<Command> Copy() const;

        static winrt::com_ptr<Command> FromJson(const Json::Value& json,
                                                std::vector<SettingsLoadWarnings>& warnings,
                                                const bool parseKeys = true);

        static void ExpandCommands(Windows::Foundation::Collections::IMap<winrt::hstring, Model::Command>& commands,
                                   Windows::Foundation::Collections::IVectorView<Model::Profile> profiles,
                                   Windows::Foundation::Collections::IVectorView<Model::ColorScheme> schemes);

        static std::vector<SettingsLoadWarnings> LayerJson(Windows::Foundation::Collections::IMap<winrt::hstring, Model::Command>& commands,
                                                           const Json::Value& json);
        Json::Value ToJson() const;

        bool HasNestedCommands() const;
        bool IsNestedCommand() const noexcept;
        Windows::Foundation::Collections::IMapView<winrt::hstring, Model::Command> NestedCommands() const;
        void NestedCommands(const Windows::Foundation::Collections::IVectorView<Model::Command>& nested);

        bool HasName() const noexcept;
        hstring Name() const noexcept;
        void Name(const hstring& name);

        Control::KeyChord Keys() const noexcept;
        hstring KeyChordText() const noexcept;
        std::vector<Control::KeyChord> KeyMappings() const noexcept;
        void RegisterKey(const Control::KeyChord& keys);
        void EraseKey(const Control::KeyChord& keys);

        hstring IconPath() const noexcept;
        void IconPath(const hstring& val);

        static Windows::Foundation::Collections::IVector<Model::Command> ParsePowerShellMenuComplete(winrt::hstring json, int32_t replaceLength);
        static Windows::Foundation::Collections::IVector<Model::Command> HistoryToCommands(Windows::Foundation::Collections::IVector<winrt::hstring> history,
                                                                                           winrt::hstring currentCommandline,
                                                                                           bool directories);

        WINRT_PROPERTY(ExpandCommandType, IterateOn, ExpandCommandType::None);
        WINRT_PROPERTY(Model::ActionAndArgs, ActionAndArgs);

    private:
        Json::Value _originalJson;
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::Command> _subcommands{ nullptr };
        std::vector<Control::KeyChord> _keyMappings;
        std::optional<std::wstring> _name;
        std::optional<std::wstring> _iconPath;
        bool _nestedCommand{ false };

        static std::vector<Model::Command> _expandCommand(Command* const expandable,
                                                          Windows::Foundation::Collections::IVectorView<Model::Profile> profiles,
                                                          Windows::Foundation::Collections::IVectorView<Model::ColorScheme> schemes);
        friend class SettingsModelUnitTests::DeserializationTests;
        friend class SettingsModelUnitTests::CommandTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(Command);
}
