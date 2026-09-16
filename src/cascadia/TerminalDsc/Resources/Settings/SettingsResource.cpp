// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsResource.h"

using namespace Microsoft::Terminal::Dsc;

namespace Model = winrt::Microsoft::Terminal::Settings::Model;

namespace
{
    // One managed setting. To manage another global setting, add a row to
    // settingProperties below and refresh the checked-in resource manifest
    // (`TerminalDsc.exe manifest --save`).
    struct SettingProperty
    {
        std::string_view jsonName;
        Json::Value (*read)(const Model::GlobalAppSettings& globals);
        void (*write)(const Model::GlobalAppSettings& globals, const Json::Value& value);
        Json::Value (*schema)();
    };

    // "theme" round-trips the same two shapes settings.json stores: a single
    // theme name, or a { "dark": ..., "light": ... } pair.
    Json::Value readTheme(const Model::GlobalAppSettings& globals)
    {
        const auto theme{ globals.Theme() };
        if (!theme)
        {
            return {};
        }
        const auto dark{ til::u16u8(theme.DarkName()) };
        const auto light{ til::u16u8(theme.LightName()) };
        if (dark == light)
        {
            return Json::Value{ dark };
        }
        Json::Value pair{ Json::objectValue };
        pair["dark"] = dark;
        pair["light"] = light;
        return pair;
    }

    void writeTheme(const Model::GlobalAppSettings& globals, const Json::Value& value)
    {
        if (value.isString())
        {
            globals.Theme(Model::ThemePair{ winrt::hstring{ til::u8u16(value.asString()) } });
            return;
        }
        if (value.isObject())
        {
            for (const auto& member : value.getMemberNames())
            {
                if (member != "dark" && member != "light")
                {
                    throw DscInputError{ fmt::format(FMT_COMPILE("unknown 'theme' property '{}'; expected only 'dark' and 'light'"), member) };
                }
            }
            const auto& dark{ value["dark"] };
            const auto& light{ value["light"] };
            if (!dark.isString() || !light.isString())
            {
                throw DscInputError{ "'theme' object requires 'dark' and 'light' strings" };
            }
            globals.Theme(Model::ThemePair{ winrt::hstring{ til::u8u16(dark.asString()) }, winrt::hstring{ til::u8u16(light.asString()) } });
            return;
        }
        throw DscInputError{ "'theme' must be a theme name or an object with 'dark' and 'light' theme names" };
    }

    Json::Value themeSchema()
    {
        Json::Value stringForm{ Json::objectValue };
        stringForm["type"] = "string";

        Json::Value pairProperties{ Json::objectValue };
        Json::Value themeName{ Json::objectValue };
        themeName["type"] = "string";
        pairProperties["dark"] = themeName;
        pairProperties["light"] = themeName;

        Json::Value pairForm{ Json::objectValue };
        pairForm["type"] = "object";
        pairForm["additionalProperties"] = false;
        Json::Value required{ Json::arrayValue };
        required.append("dark");
        required.append("light");
        pairForm["required"] = required;
        pairForm["properties"] = pairProperties;

        Json::Value schema{ Json::objectValue };
        schema["description"] = "The theme of the application. Either the name of a built-in ('dark', 'light', 'system') or custom theme, or a pair of theme names applied when the OS is in dark or light mode.";
        Json::Value oneOf{ Json::arrayValue };
        oneOf.append(stringForm);
        oneOf.append(pairForm);
        schema["oneOf"] = oneOf;
        return schema;
    }

    Json::Value readCopyOnSelect(const Model::GlobalAppSettings& globals)
    {
        return Json::Value{ globals.CopyOnSelect() };
    }

    void writeCopyOnSelect(const Model::GlobalAppSettings& globals, const Json::Value& value)
    {
        if (!value.isBool())
        {
            throw DscInputError{ "'copyOnSelect' must be a boolean" };
        }
        globals.CopyOnSelect(value.asBool());
    }

    Json::Value copyOnSelectSchema()
    {
        Json::Value schema{ Json::objectValue };
        schema["description"] = "When true, a selection is immediately copied to the clipboard upon creation.";
        schema["type"] = "boolean";
        return schema;
    }

    Json::Value readInitialCols(const Model::GlobalAppSettings& globals)
    {
        return Json::Value{ globals.InitialCols() };
    }

    void writeInitialCols(const Model::GlobalAppSettings& globals, const Json::Value& value)
    {
        if (!value.isInt() || value.asInt() < 1 || value.asInt() > 999)
        {
            throw DscInputError{ "'initialCols' must be an integer between 1 and 999" };
        }
        globals.InitialCols(value.asInt());
    }

    Json::Value initialColsSchema()
    {
        Json::Value schema{ Json::objectValue };
        schema["description"] = "The number of columns displayed in the window upon first load.";
        schema["type"] = "integer";
        schema["minimum"] = 1;
        schema["maximum"] = 999;
        return schema;
    }

    constexpr std::array settingProperties{
        SettingProperty{ "theme", readTheme, writeTheme, themeSchema },
        SettingProperty{ "copyOnSelect", readCopyOnSelect, writeCopyOnSelect, copyOnSelectSchema },
        SettingProperty{ "initialCols", readInitialCols, writeInitialCols, initialColsSchema },
    };

    Model::CascadiaSettings loadSettings()
    {
        const auto settings{ Model::CascadiaSettings::LoadAll() };
        // Refuse to touch a broken settings.json: applying state on top of the
        // fallback defaults would overwrite whatever the user still has on disk.
        if (settings.GetLoadingError())
        {
            throw DscResourceError{ "settings.json failed to load; launch Windows Terminal to see the error details" };
        }
        if (const auto errorMessage{ settings.GetSerializationErrorMessage() }; !errorMessage.empty())
        {
            throw DscResourceError{ fmt::format(FMT_COMPILE("settings.json failed to load: {}"), til::u16u8(errorMessage)) };
        }
        return settings;
    }
}

const ResourceMetadata& SettingsResource::Metadata() const noexcept
{
    static const ResourceMetadata metadata{
        .type = "Microsoft.WindowsTerminal/Settings",
        .version = "0.1.0",
        .description = "Manage Windows Terminal global settings.",
        .tags = { "Windows", "WindowsTerminal", "Terminal" },
        .setReturn = SetReturn::State,
        .getRequiresInput = false,
        .exitCodes = {
            { ExitSuccess, "Success" },
            { ExitError, "Error" },
            { ExitInvalidInput, "Invalid desired state input" },
            { ExitResourceError, "Failed to load or save the Windows Terminal settings file" },
        },
    };
    return metadata;
}

Json::Value SettingsResource::ReadState(const Model::GlobalAppSettings& globals)
{
    Json::Value state{ Json::objectValue };
    for (const auto& property : settingProperties)
    {
        auto value{ property.read(globals) };
        if (!value.isNull())
        {
            state[std::string{ property.jsonName }] = std::move(value);
        }
    }
    return state;
}

bool SettingsResource::ApplyState(const Model::GlobalAppSettings& globals, const Json::Value& desiredState)
{
    if (!desiredState.isObject())
    {
        throw DscInputError{ "the desired state must be a JSON object" };
    }

    for (const auto& member : desiredState.getMemberNames())
    {
        const auto known{ std::ranges::any_of(settingProperties, [&](const auto& property) { return property.jsonName == member; }) };
        if (!known)
        {
            throw DscInputError{ fmt::format(FMT_COMPILE("unknown property '{}'; this resource manages: theme, copyOnSelect, initialCols"), member) };
        }
    }

    auto changed{ false };
    for (const auto& property : settingProperties)
    {
        const auto name{ std::string{ property.jsonName } };
        if (desiredState.isMember(name))
        {
            property.write(globals, desiredState[name]);
            changed = true;
        }
    }
    return changed;
}

Json::Value SettingsResource::StateSchema()
{
    Json::Value schema{ Json::objectValue };
    schema["$schema"] = "https://json-schema.org/draft/2020-12/schema";
    schema["type"] = "object";
    schema["additionalProperties"] = false;
    Json::Value properties{ Json::objectValue };
    for (const auto& property : settingProperties)
    {
        properties[std::string{ property.jsonName }] = property.schema();
    }
    schema["properties"] = properties;
    return schema;
}

Json::Value SettingsResource::Schema() const
{
    return StateSchema();
}

Json::Value SettingsResource::Get(const std::optional<Json::Value>& /*instance*/)
{
    // This resource has no key properties; any provided instance is ignored.
    return ReadState(loadSettings().GlobalSettings());
}

Json::Value SettingsResource::Set(const Json::Value& instance)
{
    const auto settings{ loadSettings() };
    const auto globals{ settings.GlobalSettings() };
    if (ApplyState(globals, instance))
    {
        if (!settings.WriteSettingsToDisk())
        {
            throw DscResourceError{ "failed to write settings.json" };
        }
    }
    return ReadState(globals);
}

std::vector<Json::Value> SettingsResource::Export(const std::optional<Json::Value>& /*filter*/)
{
    std::vector<Json::Value> instances;
    instances.emplace_back(Get(std::nullopt));
    return instances;
}
