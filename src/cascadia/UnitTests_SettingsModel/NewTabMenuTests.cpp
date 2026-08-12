// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/NewTabMenuEntry.h"
#include "../TerminalSettingsModel/FolderEntry.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/resource.h"
#include "../types/inc/colorTable.hpp"
#include "JsonTestClass.h"

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace Model = winrt::Microsoft::Terminal::Settings::Model;

namespace SettingsModelUnitTests
{
    class NewTabMenuTests : public JsonTestClass
    {
        TEST_CLASS(NewTabMenuTests);

        TEST_METHOD(DefaultsToRemainingProfiles);
        TEST_METHOD(ParseEmptyFolder);
        TEST_METHOD(InsertEntryAtRootSyncsJson);
        TEST_METHOD(RemoveEntryAtRootSyncsJson);
        TEST_METHOD(SetEntryAtRootSyncsJson);
        TEST_METHOD(InsertEntryInFolderSyncsJson);
        TEST_METHOD(RemoveEntryFromFolderSyncsJson);
        TEST_METHOD(UpdateFolderNameSyncsJson);
        TEST_METHOD(UpdateFolderIconSyncsJson);
        TEST_METHOD(UpdateFolderAllowEmptySyncsJson);
        TEST_METHOD(UpdateFolderInliningSyncsJson);
        TEST_METHOD(ClearNewTabMenuClearsJson);
    };

    void NewTabMenuTests::DefaultsToRemainingProfiles()
    {
        Log::Comment(L"If the user doesn't customize the menu, put one entry for each profile");

        static constexpr std::string_view settingsString{ R"json({
        })json" };

        try
        {
            const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };

            VERIFY_ARE_EQUAL(0u, settings->Warnings().Size());

            const auto& entries = settings->GlobalSettings().NewTabMenu();
            VERIFY_ARE_EQUAL(1u, entries.Size());
            VERIFY_ARE_EQUAL(winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntryType::RemainingProfiles, entries.GetAt(0).Type());
        }
        catch (const SettingsException& ex)
        {
            auto loadError = ex.Error();
            loadError;
            throw ex;
        }
        catch (const SettingsTypedDeserializationException& e)
        {
            auto deserializationErrorMessage = til::u8u16(e.what());
            Log::Comment(NoThrowString().Format(deserializationErrorMessage.c_str()));
            throw e;
        }
    }

    void NewTabMenuTests::ParseEmptyFolder()
    {
        Log::Comment(L"GH #14557 - An empty folder entry shouldn't crash");

        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "folder" }
            ]
        })json" };

        try
        {
            const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };

            VERIFY_ARE_EQUAL(0u, settings->Warnings().Size());

            const auto& entries = settings->GlobalSettings().NewTabMenu();
            VERIFY_ARE_EQUAL(1u, entries.Size());
        }
        catch (const SettingsException& ex)
        {
            auto loadError = ex.Error();
            loadError;
            throw ex;
        }
        catch (const SettingsTypedDeserializationException& e)
        {
            auto deserializationErrorMessage = til::u8u16(e.what());
            Log::Comment(NoThrowString().Format(deserializationErrorMessage.c_str()));
            throw e;
        }
    }

    void NewTabMenuTests::InsertEntryAtRootSyncsJson()
    {
        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "remainingProfiles" }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();

        Model::SeparatorEntry separator;
        globals.NewTabMenu().InsertEntryAt(0, separator);

        // In-memory NewTabMenu reflects the new entry.
        VERIFY_ARE_EQUAL(2u, globals.NewTabMenu().Size());
        VERIFY_ARE_EQUAL(Model::NewTabMenuEntryType::Separator, globals.NewTabMenu().GetAt(0).Type());

        // JSON reflects the new entry.
        const auto json = winrt::get_self<Model::implementation::GlobalAppSettings>(globals)->ToJson();
        VERIFY_IS_TRUE(json.isMember("newTabMenu"));
        VERIFY_ARE_EQUAL(2u, json["newTabMenu"].size());
        VERIFY_ARE_EQUAL(std::string{ "separator" }, json["newTabMenu"][0]["type"].asString());
    }

    void NewTabMenuTests::RemoveEntryAtRootSyncsJson()
    {
        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "separator" },
                { "type": "remainingProfiles" }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();

        globals.NewTabMenu().RemoveEntryAt(0);

        VERIFY_ARE_EQUAL(1u, globals.NewTabMenu().Size());
        VERIFY_ARE_EQUAL(Model::NewTabMenuEntryType::RemainingProfiles, globals.NewTabMenu().GetAt(0).Type());

        const auto json = winrt::get_self<Model::implementation::GlobalAppSettings>(globals)->ToJson();
        VERIFY_ARE_EQUAL(1u, json["newTabMenu"].size());
        VERIFY_ARE_EQUAL(std::string{ "remainingProfiles" }, json["newTabMenu"][0]["type"].asString());
    }

    void NewTabMenuTests::SetEntryAtRootSyncsJson()
    {
        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "separator" }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();

        Model::RemainingProfilesEntry remaining;
        globals.NewTabMenu().SetEntryAt(0, remaining);

        VERIFY_ARE_EQUAL(Model::NewTabMenuEntryType::RemainingProfiles, globals.NewTabMenu().GetAt(0).Type());

        const auto json = winrt::get_self<Model::implementation::GlobalAppSettings>(globals)->ToJson();
        VERIFY_ARE_EQUAL(std::string{ "remainingProfiles" }, json["newTabMenu"][0]["type"].asString());
    }

    void NewTabMenuTests::InsertEntryInFolderSyncsJson()
    {
        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "folder", "name": "MyFolder", "entries": [] }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();
        const auto folder = globals.NewTabMenu().GetAt(0).as<Model::FolderEntry>();

        Model::SeparatorEntry separator;
        globals.NewTabMenu().InsertEntryInFolder(folder, 0, separator);

        VERIFY_ARE_EQUAL(1u, folder.RawEntries().Size());
        VERIFY_ARE_EQUAL(Model::NewTabMenuEntryType::Separator, folder.RawEntries().GetAt(0).Type());

        const auto json = winrt::get_self<Model::implementation::GlobalAppSettings>(globals)->ToJson();
        VERIFY_ARE_EQUAL(1u, json["newTabMenu"][0]["entries"].size());
        VERIFY_ARE_EQUAL(std::string{ "separator" }, json["newTabMenu"][0]["entries"][0]["type"].asString());
    }

    void NewTabMenuTests::RemoveEntryFromFolderSyncsJson()
    {
        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "folder", "name": "MyFolder", "entries": [
                    { "type": "separator" }
                ] }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();
        const auto folder = globals.NewTabMenu().GetAt(0).as<Model::FolderEntry>();

        globals.NewTabMenu().RemoveEntryFromFolder(folder, 0);

        VERIFY_ARE_EQUAL(0u, folder.RawEntries().Size());

        const auto json = winrt::get_self<Model::implementation::GlobalAppSettings>(globals)->ToJson();
        VERIFY_ARE_EQUAL(0u, json["newTabMenu"][0]["entries"].size());
    }

    void NewTabMenuTests::UpdateFolderNameSyncsJson()
    {
        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "folder", "name": "OldName", "entries": [] }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();
        const auto folder = globals.NewTabMenu().GetAt(0).as<Model::FolderEntry>();

        globals.NewTabMenu().UpdateFolderName(folder, L"NewName");

        VERIFY_ARE_EQUAL(winrt::hstring{ L"NewName" }, folder.Name());

        const auto json = winrt::get_self<Model::implementation::GlobalAppSettings>(globals)->ToJson();
        VERIFY_ARE_EQUAL(std::string{ "NewName" }, json["newTabMenu"][0]["name"].asString());
    }

    void NewTabMenuTests::UpdateFolderIconSyncsJson()
    {
        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "folder", "name": "F", "entries": [] }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();
        const auto folder = globals.NewTabMenu().GetAt(0).as<Model::FolderEntry>();

        globals.NewTabMenu().UpdateFolderIcon(folder, MediaResource::FromString(L"\uE756"));

        const auto json = winrt::get_self<Model::implementation::GlobalAppSettings>(globals)->ToJson();
        VERIFY_IS_TRUE(json["newTabMenu"][0].isMember("icon"));
    }

    void NewTabMenuTests::UpdateFolderAllowEmptySyncsJson()
    {
        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "folder", "name": "F", "entries": [] }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();
        const auto folder = globals.NewTabMenu().GetAt(0).as<Model::FolderEntry>();

        globals.NewTabMenu().UpdateFolderAllowEmpty(folder, true);

        VERIFY_IS_TRUE(folder.AllowEmpty());

        const auto json = winrt::get_self<Model::implementation::GlobalAppSettings>(globals)->ToJson();
        VERIFY_IS_TRUE(json["newTabMenu"][0]["allowEmpty"].asBool());
    }

    void NewTabMenuTests::UpdateFolderInliningSyncsJson()
    {
        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "folder", "name": "F", "entries": [] }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();
        const auto folder = globals.NewTabMenu().GetAt(0).as<Model::FolderEntry>();

        globals.NewTabMenu().UpdateFolderInlining(folder, Model::FolderEntryInlining::Auto);

        VERIFY_ARE_EQUAL(Model::FolderEntryInlining::Auto, folder.Inlining());

        const auto json = winrt::get_self<Model::implementation::GlobalAppSettings>(globals)->ToJson();
        VERIFY_ARE_EQUAL(std::string{ "auto" }, json["newTabMenu"][0]["inline"].asString());
    }

    void NewTabMenuTests::ClearNewTabMenuClearsJson()
    {
        Log::Comment(L"ClearNewTabMenu must clear BOTH the local entries and the JSON key");

        static constexpr std::string_view settingsString{ R"json({
            "newTabMenu": [
                { "type": "separator" }
            ]
        })json" };
        const auto settings{ winrt::make_self<CascadiaSettings>(settingsString, LoadStringResource(IDR_DEFAULTS)) };
        const auto globals = settings->GlobalSettings();
        const auto globalsImpl = winrt::get_self<Model::implementation::GlobalAppSettings>(globals);

        VERIFY_IS_TRUE(globals.HasNewTabMenu());

        globals.ClearNewTabMenu();

        VERIFY_IS_FALSE(globals.HasNewTabMenu());

        const auto json = globalsImpl->ToJson();
        // After Clear, the key should be absent from the global JSON so
        // auto-save doesn't re-persist the cleared value.
        VERIFY_IS_FALSE(json.isMember("newTabMenu"));
    }
}
