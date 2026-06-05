// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/ActionMap.h"
#include "../TerminalSettingsModel/FileUtils.h"
#include "JsonTestClass.h"

#include <til/io.h>

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelUnitTests
{
    class JsonManagerTests : public JsonTestClass
    {
        TEST_CLASS(JsonManagerTests);

        TEST_METHOD(HashIsDeterministicAndSensitive);
        TEST_METHOD(WriteSettingsFileWritesContent);
        TEST_METHOD(WriteSettingsFileCreatesBackup);
        TEST_METHOD(WriteHandlerFiresOnSetterAndClear);
        TEST_METHOD(ClonedSettingsDoNotFireWriteHandler);
        TEST_METHOD(CollectionEditsFireWriteHandler);
        TEST_METHOD(ActionMapEditsFireWriteHandler);

    private:
        static std::filesystem::path _tempFile(const wchar_t* name)
        {
            return std::filesystem::temp_directory_path() / name;
        }
    };

    void JsonManagerTests::HashIsDeterministicAndSensitive()
    {
        const FILETIME timeA{ 0x11111111, 0x22222222 };
        const FILETIME timeB{ 0x33333333, 0x44444444 };

        const auto hashA1 = CalculateSettingsHash("hello world", timeA);
        const auto hashA2 = CalculateSettingsHash("hello world", timeA);
        const auto hashDifferentContent = CalculateSettingsHash("hello worlated", timeA);
        const auto hashDifferentTime = CalculateSettingsHash("hello world", timeB);

        // Same content + time -> identical hash.
        VERIFY_ARE_EQUAL(hashA1, hashA2);
        // Different content -> different hash.
        VERIFY_ARE_NOT_EQUAL(hashA1, hashDifferentContent);
        // Different write time -> different hash.
        VERIFY_ARE_NOT_EQUAL(hashA1, hashDifferentTime);
    }

    void JsonManagerTests::WriteSettingsFileWritesContent()
    {
        const auto path = _tempFile(L"wt_jsonmgr_write.json");
        std::error_code ec;
        std::filesystem::remove(path, ec);

        FILETIME writeTime{};
        WriteSettingsFile(path, "{ \"a\": 1 }", /*makeBackup*/ false, &writeTime);

        const auto readBack = til::io::read_file_as_utf8_string_if_exists(path, false, nullptr);
        VERIFY_ARE_EQUAL(std::string{ "{ \"a\": 1 }" }, readBack);

        std::filesystem::remove(path, ec);
    }

    void JsonManagerTests::WriteSettingsFileCreatesBackup()
    {
        const auto path = _tempFile(L"wt_jsonmgr_backup.json");
        auto backupPath = path;
        backupPath += L".bak";

        std::error_code ec;
        std::filesystem::remove(path, ec);
        std::filesystem::remove(backupPath, ec);

        // First write (no prior file): a backup of a non-existent file is a no-op.
        WriteSettingsFile(path, "first", /*makeBackup*/ true, nullptr);
        VERIFY_IS_TRUE(std::filesystem::exists(path));

        // Second write: the prior contents must be preserved in the .bak file.
        WriteSettingsFile(path, "second", /*makeBackup*/ true, nullptr);
        VERIFY_IS_TRUE(std::filesystem::exists(backupPath));

        VERIFY_ARE_EQUAL(std::string{ "second" }, til::io::read_file_as_utf8_string_if_exists(path, false, nullptr));
        VERIFY_ARE_EQUAL(std::string{ "first" }, til::io::read_file_as_utf8_string_if_exists(backupPath, false, nullptr));

        std::filesystem::remove(path, ec);
        std::filesystem::remove(backupPath, ec);
    }

    void JsonManagerTests::WriteHandlerFiresOnSetterAndClear()
    {
        static constexpr std::string_view settingsJson{ R"({
            "profiles": {
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "historySize": 1000
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);

        auto writeRequests = 0;
        settings->SetWriteHandler([&]() { ++writeRequests; });

        const auto profile = settings->AllProfiles().GetAt(0);

        // A typed setter on the live tree should request a save.
        profile.HistorySize(5000);
        VERIFY_ARE_EQUAL(1, writeRequests);

        // Clearing a setting should also request a save.
        profile.ClearHistorySize();
        VERIFY_ARE_EQUAL(2, writeRequests);

        // A global setting should request a save too.
        settings->GlobalSettings().AlwaysShowTabs(false);
        VERIFY_ARE_EQUAL(3, writeRequests);
    }

    void JsonManagerTests::ClonedSettingsDoNotFireWriteHandler()
    {
        static constexpr std::string_view settingsJson{ R"({
            "profiles": {
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "historySize": 1000
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);

        auto writeRequests = 0;
        settings->SetWriteHandler([&]() { ++writeRequests; });

        // The editor works on a deep clone. Clones must never auto-save the live
        // settings file, so mutating the clone must NOT invoke the handler.
        const auto clone = settings->Copy();
        clone.AllProfiles().GetAt(0).HistorySize(7000);

        VERIFY_ARE_EQUAL(0, writeRequests);

        // Sanity: the original tree's handler still fires.
        settings->AllProfiles().GetAt(0).HistorySize(7000);
        VERIFY_ARE_EQUAL(1, writeRequests);
    }

    void JsonManagerTests::CollectionEditsFireWriteHandler()
    {
        // Color schemes / themes aren't IInheritable, so per-property edits aren't
        // covered, but add/remove is wired at the GlobalAppSettings collection
        // level. Verify that coarse-grained coverage requests a save.
        static constexpr std::string_view settingsJson{ R"({
            "profiles": {
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);

        auto writeRequests = 0;
        settings->SetWriteHandler([&]() { ++writeRequests; });

        const auto scheme = winrt::make<implementation::ColorScheme>(winrt::hstring{ L"My Scheme" });
        settings->GlobalSettings().AddColorScheme(scheme);
        VERIFY_ARE_EQUAL(1, writeRequests);

        settings->GlobalSettings().RemoveColorScheme(L"My Scheme");
        VERIFY_ARE_EQUAL(2, writeRequests);
    }

    void JsonManagerTests::ActionMapEditsFireWriteHandler()
    {
        // ActionMap isn't JSON-backed, but its mutators request a save directly and
        // it's registered with the write sink, so user action/keybinding edits must
        // request an auto-save.
        static constexpr std::string_view settingsJson{ R"({
            "profiles": {
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                    }
                ]
            },
            "actions": [
                { "command": "newTab", "id": "Test.NewTab" }
            ],
            "keybindings": [
                { "keys": "ctrl+t", "id": "Test.NewTab" }
            ]
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);

        auto writeRequests = 0;
        settings->SetWriteHandler([&]() { ++writeRequests; });

        auto actionMap = settings->GlobalSettings().ActionMap();

        // { ctrl, alt, shift, win, vkey, scanCode }
        const KeyChord ctrlT{ true, false, false, false, static_cast<int32_t>('T'), 0 };
        const KeyChord ctrlShiftT{ true, false, true, false, static_cast<int32_t>('T'), 0 };
        const KeyChord ctrlY{ true, false, false, false, static_cast<int32_t>('Y'), 0 };

        // Binding a key chord to an existing action requests a save.
        actionMap.AddKeyBinding(ctrlY, L"Test.NewTab");
        VERIFY_ARE_EQUAL(1, writeRequests);

        // Rebinding a key chord requests a save.
        actionMap.RebindKeys(ctrlY, ctrlShiftT);
        VERIFY_ARE_EQUAL(2, writeRequests);

        // Unbinding a key chord requests a save.
        actionMap.DeleteKeyBinding(ctrlShiftT);
        VERIFY_ARE_EQUAL(3, writeRequests);

        // Adding a command (this funnels through AddAction, the path the settings
        // editor's "add action" also uses) requests a save.
        actionMap.AddSendInputAction(L"My Snippet", L"echo hi\r", nullptr);
        VERIFY_ARE_EQUAL(4, writeRequests);

        // Deleting a user command requests a save.
        actionMap.DeleteUserCommand(L"Test.NewTab");
        VERIFY_ARE_EQUAL(5, writeRequests);

        // The editor works on a deep clone, which must never auto-save the live
        // file: a clone's ActionMap edit must NOT invoke the handler.
        const auto clone = settings->Copy();
        clone.GlobalSettings().ActionMap().AddKeyBinding(ctrlT, L"Test.NewTab");
        VERIFY_ARE_EQUAL(5, writeRequests);
    }
}
