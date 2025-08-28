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

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view IDKey{ "id" };
static constexpr std::string_view IconKey{ "icon" };
static constexpr std::string_view ActionKey{ "command" };
static constexpr std::string_view IterateOnKey{ "iterateOn" };
static constexpr std::string_view CommandsKey{ "commands" };
static constexpr std::string_view KeysKey{ "keys" };
static constexpr std::string_view DescriptionKey{ "description" };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct Command : CommandT<Command>
    {
        struct CommandNameOrResource
        {
            std::wstring name;
            std::wstring resource;
        };

        Command();
        static Model::Command NewUserCommand();
        static Model::Command CopyAsUserCommand(const Model::Command& originalCmd);
        com_ptr<Command> Copy() const;

        static winrt::com_ptr<Command> FromJson(const Json::Value& json,
                                                std::vector<SettingsLoadWarnings>& warnings,
                                                const OriginTag origin);

        static winrt::com_ptr<Command> FromSnippetJson(const Json::Value& json);

        static void ExpandCommands(Windows::Foundation::Collections::IMap<winrt::hstring, Model::Command>& commands,
                                   Windows::Foundation::Collections::IVectorView<Model::Profile> profiles,
                                   Windows::Foundation::Collections::IVectorView<Model::ColorScheme> schemes);

        static std::vector<SettingsLoadWarnings> LayerJson(Windows::Foundation::Collections::IMap<winrt::hstring, Model::Command>& commands,
                                                           const Json::Value& json,
                                                           const OriginTag origin);
        Json::Value ToJson() const;
        void LogSettingChanges(std::set<std::string>& changes);

        bool HasNestedCommands() const;
        bool IsNestedCommand() const noexcept;
        Windows::Foundation::Collections::IMapView<winrt::hstring, Model::Command> NestedCommands() const;
        void NestedCommands(const Windows::Foundation::Collections::IVectorView<Model::Command>& nested);

        bool HasName() const noexcept;
        hstring Name() const noexcept;
        void Name(const hstring& name);
        hstring LanguageNeutralName() const noexcept;

        hstring ID() const noexcept;
        void ID(const hstring& ID) noexcept;
        void GenerateID();
        bool IDWasGenerated();

        IMediaResource Icon() const noexcept;
        void Icon(const IMediaResource& val);

        void ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const Model::MediaResourceResolver& resolver);

        static Windows::Foundation::Collections::IVector<Model::Command> ParsePowerShellMenuComplete(winrt::hstring json, int32_t replaceLength);
        static Windows::Foundation::Collections::IVector<Model::Command> HistoryToCommands(Windows::Foundation::Collections::IVector<winrt::hstring> history,
                                                                                           winrt::hstring currentCommandline,
                                                                                           bool directories,
                                                                                           hstring iconPath);

        WINRT_PROPERTY(Model::ActionAndArgs, ActionAndArgs, Model::ActionAndArgs{});
        WINRT_PROPERTY(ExpandCommandType, IterateOn, ExpandCommandType::None);
        WINRT_PROPERTY(OriginTag, Origin);
        WINRT_PROPERTY(winrt::hstring, Description, L"");

    private:
        Json::Value _originalJson;
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::Command> _subcommands{ nullptr };
        std::optional<CommandNameOrResource> _name;
        std::wstring _ID;
        bool _IDWasGenerated{ false };
        std::optional<IMediaResource> _icon;
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
