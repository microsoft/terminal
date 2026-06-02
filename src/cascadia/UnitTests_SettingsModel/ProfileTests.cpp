// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "../TerminalSettingsModel/resource.h"
#include "JsonTestClass.h"

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelUnitTests
{
    class ProfileTests : public JsonTestClass
    {
        TEST_CLASS(ProfileTests);

        TEST_METHOD(ProfileGeneratesGuid);
        TEST_METHOD(LayerProfileProperties);
        TEST_METHOD(LayerProfileIcon);
        TEST_METHOD(LayerProfilesOnArray);
        TEST_METHOD(ProfileWithEnvVars);
        TEST_METHOD(ProfileWithEnvVarsSameNameDifferentCases);

        TEST_METHOD(DuplicateProfileTest);
        TEST_METHOD(TestGenGuidsForProfiles);
        TEST_METHOD(TestCorrectOldDefaultShellPaths);
        TEST_METHOD(ProfileDefaultsProhibitedSettings);

        TEST_METHOD(JsonSyncOnSetAndClear);
        TEST_METHOD(SettingKeyEnumAndJsonKeyLookup);
        TEST_METHOD(GenericHasAndClearMatchTypedAPIs);
        TEST_METHOD(CurrentSettingsReturnsCorrectKeys);
        TEST_METHOD(SpecialCasedSettingsJsonBacked);
        TEST_METHOD(NullableSettingJsonBacked);
        TEST_METHOD(ColorSchemeJsonBacked);
    };

    void ProfileTests::ProfileGeneratesGuid()
    {
        // Parse some profiles without guids. We should NOT generate new guids
        // for them. If a profile doesn't have a GUID, we'll leave its _guid
        // set to nullopt. The Profile::Guid() getter will
        // ensure all profiles have a GUID that's actually set.
        // The null guid _is_ a valid guid, so we won't re-generate that
        // guid. null is _not_ a valid guid, so we'll leave that nullopt

        // See SettingsTests::ValidateProfilesGenerateGuids for a version of
        // this test that includes synthesizing GUIDS for profiles without GUIDs
        // set

        const auto parseAndVerifyProfile = [](const std::string profile, const bool hasGuid) {
            const auto profileAsJson = VerifyParseSucceeded(profile);
            const auto profileParsed = implementation::Profile::FromJson(profileAsJson);

            VERIFY_ARE_EQUAL(profileParsed->HasGuid(), hasGuid);
            return profileParsed;
        };

        // Invalid GUID Values
        const std::string profileWithoutGuid{ R"({
                                              "name" : "profile0"
                                              })" };
        const std::string secondProfileWithoutGuid{ R"({
                                              "name" : "profile1"
                                              })" };
        const std::string profileWithNullForGuid{ R"({
                                              "name" : "profile2",
                                              "guid" : null
                                              })" };
        const std::string profileWithHyphenlessGuid{ R"({
                                              "name" : "profile4",
                                              "guid" : "{6239A42C1DE449A380BDE8FDD045185C}"
                                              })" };
        const std::string profileWithRawGuid{ R"({
                                              "name" : "profile4",
                                              "guid" : "6239a42c-1de4-49a3-80bd-e8fdd045185c"
                                              })" };
        const std::string profileWithGuidFormatP{ R"({
                                              "name" : "profile4",
                                              "guid" : "(6239a42c-1de4-49a3-80bd-e8fdd045185c)\\"
                                              })" };
        // Valid GUIDs
        const std::string profileWithNullGuid{ R"({
                                              "name" : "profile3",
                                              "guid" : "{00000000-0000-0000-0000-000000000000}"
                                              })" };
        const std::string profileWithGuidFormatB{ R"({
                                              "name" : "profile4",
                                              "guid" : "{6239a42c-1de4-49a3-80bd-e8fdd045185c}"
                                              })" };
        const std::string profileWithGuidUpperCaseFormatB{ R"({
                                              "name" : "profile4",
                                              "guid" : "{6239A42C-1DE4-49A3-80BD-E8FDD045185C}"
                                              })" };

        parseAndVerifyProfile(profileWithoutGuid, false);
        parseAndVerifyProfile(secondProfileWithoutGuid, false);

        // The following crash JSON parsing
        VERIFY_THROWS(parseAndVerifyProfile(profileWithHyphenlessGuid, false), std::exception);
        VERIFY_THROWS(parseAndVerifyProfile(profileWithRawGuid, false), std::exception);
        VERIFY_THROWS(parseAndVerifyProfile(profileWithGuidFormatP, false), std::exception);

        const auto parsedNullGuidProfile = parseAndVerifyProfile(profileWithNullGuid, true);
        const auto parsedGuidProfileFormatB = parseAndVerifyProfile(profileWithGuidFormatB, true);
        const auto parsedGuidProfileUpperCaseFormatB = parseAndVerifyProfile(profileWithGuidUpperCaseFormatB, true);

        const winrt::guid nullGuid{};
        const winrt::guid cmdGuid = Utils::GuidFromString(L"{6239a42c-1de4-49a3-80bd-e8fdd045185c}");

        VERIFY_ARE_EQUAL(parsedNullGuidProfile->Guid(), nullGuid);
        VERIFY_ARE_EQUAL(parsedGuidProfileFormatB->Guid(), cmdGuid);
        VERIFY_ARE_EQUAL(parsedGuidProfileUpperCaseFormatB->Guid(), cmdGuid);
    }

    void ProfileTests::LayerProfileProperties()
    {
        static constexpr std::string_view profile0String{ R"({
            "name": "profile0",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "foreground": "#000000",
            "background": "#010101",
            "selectionBackground": "#010101"
        })" };
        static constexpr std::string_view profile1String{ R"({
            "name": "profile1",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "foreground": "#020202",
            "startingDirectory": "C:/"
        })" };
        static constexpr std::string_view profile2String{ R"({
            "name": "profile2",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "foreground": "#030303",
            "selectionBackground": "#020202"
        })" };

        const auto profile0Json = VerifyParseSucceeded(profile0String);
        const auto profile1Json = VerifyParseSucceeded(profile1String);
        const auto profile2Json = VerifyParseSucceeded(profile2String);

        auto profile0 = implementation::Profile::FromJson(profile0Json);
        VERIFY_IS_NOT_NULL(profile0->DefaultAppearance().Foreground());
        VERIFY_ARE_EQUAL(til::color(0, 0, 0), til::color{ profile0->DefaultAppearance().Foreground().Value() });

        VERIFY_IS_NOT_NULL(profile0->DefaultAppearance().Background());
        VERIFY_ARE_EQUAL(til::color(1, 1, 1), til::color{ profile0->DefaultAppearance().Background().Value() });

        VERIFY_IS_NOT_NULL(profile0->DefaultAppearance().SelectionBackground());
        VERIFY_ARE_EQUAL(til::color(1, 1, 1), til::color{ profile0->DefaultAppearance().SelectionBackground().Value() });

        VERIFY_ARE_EQUAL(L"profile0", profile0->Name());

        VERIFY_IS_TRUE(profile0->StartingDirectory().empty());

        Log::Comment(NoThrowString().Format(
            L"Layering profile1 on top of profile0"));
        auto profile1{ profile0->CreateChild() };
        profile1->LayerJson(profile1Json);

        VERIFY_IS_NOT_NULL(profile1->DefaultAppearance().Foreground());
        VERIFY_ARE_EQUAL(til::color(2, 2, 2), til::color{ profile1->DefaultAppearance().Foreground().Value() });

        VERIFY_IS_NOT_NULL(profile1->DefaultAppearance().Background());
        VERIFY_ARE_EQUAL(til::color(1, 1, 1), til::color{ profile1->DefaultAppearance().Background().Value() });

        VERIFY_IS_NOT_NULL(profile1->DefaultAppearance().Background());
        VERIFY_ARE_EQUAL(til::color(1, 1, 1), til::color{ profile1->DefaultAppearance().Background().Value() });

        VERIFY_ARE_EQUAL(L"profile1", profile1->Name());

        VERIFY_IS_FALSE(profile1->StartingDirectory().empty());
        VERIFY_ARE_EQUAL(L"C:/", profile1->StartingDirectory());

        Log::Comment(NoThrowString().Format(
            L"Layering profile2 on top of (profile0+profile1)"));
        auto profile2{ profile1->CreateChild() };
        profile2->LayerJson(profile2Json);

        VERIFY_IS_NOT_NULL(profile2->DefaultAppearance().Foreground());
        VERIFY_ARE_EQUAL(til::color(3, 3, 3), til::color{ profile2->DefaultAppearance().Foreground().Value() });

        VERIFY_IS_NOT_NULL(profile2->DefaultAppearance().Background());
        VERIFY_ARE_EQUAL(til::color(1, 1, 1), til::color{ profile2->DefaultAppearance().Background().Value() });

        VERIFY_IS_NOT_NULL(profile2->DefaultAppearance().SelectionBackground());
        VERIFY_ARE_EQUAL(til::color(2, 2, 2), til::color{ profile2->DefaultAppearance().SelectionBackground().Value() });

        VERIFY_ARE_EQUAL(L"profile2", profile2->Name());

        VERIFY_IS_FALSE(profile2->StartingDirectory().empty());
        VERIFY_ARE_EQUAL(L"C:/", profile2->StartingDirectory());
    }

    void ProfileTests::LayerProfileIcon()
    {
        static constexpr std::string_view profile0String{ R"({
            "name": "profile0",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "icon": "not-null.png"
        })" };
        static constexpr std::string_view profile1String{ R"({
            "name": "profile1",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "icon": null
        })" };
        static constexpr std::string_view profile2String{ R"({
            "name": "profile2",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
        })" };
        static constexpr std::string_view profile3String{ R"({
            "name": "profile3",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "icon": "another-real.png"
        })" };

        const auto profile0Json = VerifyParseSucceeded(profile0String);
        const auto profile1Json = VerifyParseSucceeded(profile1String);
        const auto profile2Json = VerifyParseSucceeded(profile2String);
        const auto profile3Json = VerifyParseSucceeded(profile3String);

        auto profile0 = implementation::Profile::FromJson(profile0Json);
        VERIFY_IS_FALSE(profile0->Icon().Path().empty());
        VERIFY_ARE_EQUAL(L"not-null.png", profile0->Icon().Path());

        Log::Comment(NoThrowString().Format(
            L"Verify that layering an object the key set to null will clear the key"));
        profile0->LayerJson(profile1Json);
        VERIFY_IS_TRUE(profile0->Icon().Path().empty());

        profile0->LayerJson(profile2Json);
        VERIFY_IS_TRUE(profile0->Icon().Path().empty());

        profile0->LayerJson(profile3Json);
        VERIFY_IS_FALSE(profile0->Icon().Path().empty());
        VERIFY_ARE_EQUAL(L"another-real.png", profile0->Icon().Path());

        Log::Comment(NoThrowString().Format(
            L"Verify that layering an object _without_ the key will not clear the key"));
        profile0->LayerJson(profile2Json);
        VERIFY_IS_FALSE(profile0->Icon().Path().empty());
        VERIFY_ARE_EQUAL(L"another-real.png", profile0->Icon().Path());

        auto profile1 = implementation::Profile::FromJson(profile1Json);
        VERIFY_IS_TRUE(profile1->Icon().Path().empty());
        profile1->LayerJson(profile3Json);
        VERIFY_IS_FALSE(profile1->Icon().Path().empty());
        VERIFY_ARE_EQUAL(L"another-real.png", profile1->Icon().Path());
    }

    void ProfileTests::LayerProfilesOnArray()
    {
        static constexpr std::string_view inboxProfiles{ R"({
            "profiles": [
                {
                    "name" : "profile0",
                    "guid" : "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                }, {
                    "name" : "profile1",
                    "guid" : "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                }, {
                    "name" : "profile2",
                    "guid" : "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };
        static constexpr std::string_view userProfiles{ R"({
            "profiles": [
                {
                    "name" : "profile3",
                    "guid" : "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                }, {
                    "name" : "profile4",
                    "guid" : "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(userProfiles, inboxProfiles);
        const auto allProfiles = settings->AllProfiles();
        VERIFY_ARE_EQUAL(3u, allProfiles.Size());
        VERIFY_ARE_EQUAL(L"profile3", allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"profile4", allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"profile2", allProfiles.GetAt(2).Name());
    }

    void ProfileTests::DuplicateProfileTest()
    {
        static constexpr std::string_view userProfiles{ R"({
            "profiles": {
                "defaults": {
                    "font": {
                        "size": 123
                    }
                },
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "backgroundImage": "file:///some/path",
                        "hidden": false,
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(userProfiles);
        const auto profile = settings->AllProfiles().GetAt(0);
        const auto duplicatedProfile = settings->DuplicateProfile(profile);

        // GH#11392: Ensure duplicated profiles properly inherit the base layer, even for nested objects.
        VERIFY_ARE_EQUAL(123, duplicatedProfile.FontInfo().FontSize());

        duplicatedProfile.Guid(profile.Guid());
        duplicatedProfile.Name(profile.Name());

        const auto json = winrt::get_self<implementation::Profile>(profile)->ToJson();
        const auto duplicatedJson = winrt::get_self<implementation::Profile>(duplicatedProfile)->ToJson();
        VERIFY_ARE_EQUAL(json, duplicatedJson, til::u8u16(toString(duplicatedJson)).c_str());
    }

    void ProfileTests::TestGenGuidsForProfiles()
    {
        // We'll generate GUIDs in the Profile::Guid getter. We should make sure that
        // the GUID generated for a dynamic profile (with a source) is different
        // than that of a profile without a source.

        static constexpr std::string_view inboxSettings{ R"({
            "profiles": [
                {
                    "name" : "profile0",
                    "source": "Terminal.App.UnitTest.0"
                },
                {
                    "name" : "profile1"
                }
            ]
        })" };
        static constexpr std::string_view userSettings{ R"({
            "profiles": [
                {
                    "name": "profile0",
                    "source": "Terminal.App.UnitTest.0",
                },
                {
                    "name": "profile0"
                }
            ]
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(userSettings, inboxSettings);

        VERIFY_ARE_EQUAL(3u, settings->AllProfiles().Size());

        VERIFY_ARE_EQUAL(L"profile0", settings->AllProfiles().GetAt(0).Name());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(0).HasGuid());
        VERIFY_IS_FALSE(settings->AllProfiles().GetAt(0).Source().empty());

        VERIFY_ARE_EQUAL(L"profile0", settings->AllProfiles().GetAt(1).Name());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(1).HasGuid());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(1).Source().empty());

        VERIFY_ARE_NOT_EQUAL(settings->AllProfiles().GetAt(0).Guid(), settings->AllProfiles().GetAt(1).Guid());
    }

    void ProfileTests::ProfileWithEnvVars()
    {
        const std::string profileString{ R"({
            "name": "profile0",
            "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "environment": {
                "VAR_1": "value1",
                "VAR_2": "value2",
                "VAR_3": "%VAR_3%;value3"
            }
        })" };
        const auto profile = implementation::Profile::FromJson(VerifyParseSucceeded(profileString));
        std::vector<IEnvironmentVariableMap> envVarMaps{};
        envVarMaps.emplace_back(profile->EnvironmentVariables());
        for (auto& envMap : envVarMaps)
        {
            VERIFY_ARE_EQUAL(static_cast<uint32_t>(3), envMap.Size());
            VERIFY_ARE_EQUAL(L"value1", envMap.Lookup(L"VAR_1"));
            VERIFY_ARE_EQUAL(L"value2", envMap.Lookup(L"VAR_2"));
            VERIFY_ARE_EQUAL(L"%VAR_3%;value3", envMap.Lookup(L"VAR_3"));
        }
    }

    void ProfileTests::ProfileWithEnvVarsSameNameDifferentCases()
    {
        const std::string userSettings{ R"({
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "environment": {
                        "FOO": "VALUE",
                        "Foo": "Value"
                    }
                }
            ]
        })" };
        const auto settings = winrt::make_self<implementation::CascadiaSettings>(userSettings);
        const auto warnings = settings->Warnings();
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(2), warnings.Size());
        uint32_t index;
        VERIFY_IS_TRUE(warnings.IndexOf(SettingsLoadWarnings::InvalidProfileEnvironmentVariables, index));
    }

    void ProfileTests::TestCorrectOldDefaultShellPaths()
    {
        static constexpr std::string_view inboxProfiles{ R"({
            "profiles": [
                {
                    "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                    "name": "Windows PowerShell",
                    "commandline": "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
                },
                {
                    "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                    "name": "Command Prompt",
                    "commandline": "%SystemRoot%\\System32\\cmd.exe",
                }
            ]
        })" };
        static constexpr std::string_view userProfiles{ R"({
            "profiles": {
                "defaults":
                {
                    "commandline": "pwsh.exe"
                },
                "list":
                [
                    {
                        "name" : "powershell 1",
                        "commandline": "powershell.exe",
                        "guid" : "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}"
                    },
                    {
                        "name" : "powershell 2",
                        "commandline": "powershell.exe",
                        "guid" : "{61c54bbd-0000-5271-96e7-009a87ff44bf}"
                    },
                    {
                        "name" : "cmd 1",
                        "commandline": "cmd.exe",
                        "guid" : "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}"
                    },
                    {
                        "name" : "cmd 2",
                        "commandline": "cmd.exe",
                        "guid" : "{0caa0dad-0000-5f56-a8ff-afceeeaa6101}"
                    }
                ]
            }
        })" };

        implementation::SettingsLoader loader{ userProfiles, inboxProfiles };
        loader.MergeInboxIntoUserSettings();
        loader.FinalizeLayering();
        loader.FixupUserSettings();

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));
        const auto allProfiles = settings->AllProfiles();
        VERIFY_ARE_EQUAL(4u, allProfiles.Size());
        VERIFY_ARE_EQUAL(L"powershell 1", allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"powershell 2", allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"cmd 1", allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"cmd 2", allProfiles.GetAt(3).Name());

        VERIFY_ARE_EQUAL(L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", allProfiles.GetAt(0).Commandline());
        VERIFY_ARE_EQUAL(L"powershell.exe", allProfiles.GetAt(1).Commandline());
        VERIFY_ARE_EQUAL(L"%SystemRoot%\\System32\\cmd.exe", allProfiles.GetAt(2).Commandline());
        VERIFY_ARE_EQUAL(L"cmd.exe", allProfiles.GetAt(3).Commandline());
    }

    void ProfileTests::ProfileDefaultsProhibitedSettings()
    {
        static constexpr std::string_view userProfiles{ R"({
            "profiles": {
                "defaults":
                {
                    "guid": "{00000000-0000-0000-0000-000000000000}",
                    "name": "Default Profile Name",
                    "source": "Default Profile Source",
                    "commandline": "foo.exe"
                },
                "list":
                [
                    {
                        "name" : "PowerShell",
                        "commandline": "powershell.exe",
                        "guid" : "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}"
                    },
                    {
                        "name": "Profile with just a name"
                    },
                    {
                        "guid": "{a0776706-1fa6-4439-b46c-287a65c084d5}",
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(userProfiles);

        // Profile Defaults should not have a GUID, name, source, or commandline.
        const auto profileDefaults = settings->ProfileDefaults();
        VERIFY_IS_FALSE(profileDefaults.HasGuid());
        VERIFY_IS_FALSE(profileDefaults.HasName());
        VERIFY_IS_FALSE(profileDefaults.HasSource());
        VERIFY_IS_FALSE(profileDefaults.HasCommandline());

        const auto allProfiles = settings->AllProfiles();
        VERIFY_ARE_EQUAL(3u, allProfiles.Size());

        // Profile settings should be set to the ones set at that layer
        VERIFY_ARE_EQUAL(L"PowerShell", allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", allProfiles.GetAt(0).Commandline());
        VERIFY_ARE_EQUAL(Utils::GuidFromString(L"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}"), static_cast<GUID>(allProfiles.GetAt(0).Guid()));
        VERIFY_IS_FALSE(allProfiles.GetAt(0).HasSource());

        // Profile should not inherit the values attempted to be set on the Profiles Defaults layer
        // This profile only has a name set
        VERIFY_ARE_EQUAL(L"Profile with just a name", allProfiles.GetAt(1).Name());
        VERIFY_ARE_NOT_EQUAL(Utils::GuidFromString(L"{00000000-0000-0000-0000-000000000000}"), static_cast<GUID>(allProfiles.GetAt(1).Guid()));
        VERIFY_ARE_NOT_EQUAL(L"Default Profile Source", allProfiles.GetAt(1).Source());
        VERIFY_ARE_NOT_EQUAL(L"foo.exe", allProfiles.GetAt(1).Commandline());

        // Profile should not inherit the values attempted to be set on the Profiles Defaults layer
        // This profile only has a guid set
        VERIFY_ARE_NOT_EQUAL(L"Default Profile Name", allProfiles.GetAt(2).Name());
        VERIFY_ARE_NOT_EQUAL(Utils::GuidFromString(L"{00000000-0000-0000-0000-000000000000}"), static_cast<GUID>(allProfiles.GetAt(2).Guid()));
        VERIFY_ARE_NOT_EQUAL(L"Default Profile Source", allProfiles.GetAt(2).Source());
        VERIFY_ARE_NOT_EQUAL(L"foo.exe", allProfiles.GetAt(2).Commandline());
    }

    void ProfileTests::JsonSyncOnSetAndClear()
    {
        // Verify that setting a value via the typed setter updates the internal
        // _json, and clearing it removes the key from _json.
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
        const auto profile = settings->AllProfiles().GetAt(0);
        const auto profileImpl = winrt::get_self<implementation::Profile>(profile);

        // Verify initial value
        VERIFY_ARE_EQUAL(1000, profile.HistorySize());
        VERIFY_IS_TRUE(profile.HasHistorySize());

        // Modify setting; _json should be updated
        profile.HistorySize(5000);
        VERIFY_ARE_EQUAL(5000, profile.HistorySize());

        // Verify ToJson reflects the change
        const auto json = profileImpl->ToJson();
        VERIFY_ARE_EQUAL(5000, json["historySize"].asInt());

        // Clear setting; should fall back to default
        profile.ClearHistorySize();
        VERIFY_IS_FALSE(profile.HasHistorySize());
        // Should now inherit or use default (9001 is the DEFAULT_HISTORY_SIZE)
        VERIFY_ARE_EQUAL(DEFAULT_HISTORY_SIZE, profile.HistorySize());

        // ToJson should no longer have historySize
        const auto json2 = profileImpl->ToJson();
        VERIFY_IS_FALSE(json2.isMember("historySize"));
    }

    void ProfileTests::SettingKeyEnumAndJsonKeyLookup()
    {
        // Verify that the SettingKey enums map to correct JSON keys.
        using namespace implementation;

        // Spot-check a few profile setting keys
        VERIFY_ARE_EQUAL(std::string_view{ "historySize" }, JsonKeyForSetting(ProfileSettingKey::HistorySize));
        VERIFY_ARE_EQUAL(std::string_view{ "snapOnInput" }, JsonKeyForSetting(ProfileSettingKey::SnapOnInput));
        VERIFY_ARE_EQUAL(std::string_view{ "commandline" }, JsonKeyForSetting(ProfileSettingKey::Commandline));
        VERIFY_ARE_EQUAL(std::string_view{ "tabTitle" }, JsonKeyForSetting(ProfileSettingKey::TabTitle));

        // Special-cased settings
        VERIFY_ARE_EQUAL(std::string_view{ "name" }, JsonKeyForSetting(ProfileSettingKey::_Name));
        VERIFY_ARE_EQUAL(std::string_view{ "guid" }, JsonKeyForSetting(ProfileSettingKey::_Guid));
        VERIFY_ARE_EQUAL(std::string_view{ "hidden" }, JsonKeyForSetting(ProfileSettingKey::_Hidden));
        VERIFY_ARE_EQUAL(std::string_view{ "padding" }, JsonKeyForSetting(ProfileSettingKey::_Padding));
        VERIFY_ARE_EQUAL(std::string_view{ "tabColor" }, JsonKeyForSetting(ProfileSettingKey::_TabColor));

        // Global setting keys
        VERIFY_ARE_EQUAL(std::string_view{ "initialRows" }, JsonKeyForSetting(GlobalSettingKey::InitialRows));
        VERIFY_ARE_EQUAL(std::string_view{ "alwaysOnTop" }, JsonKeyForSetting(GlobalSettingKey::AlwaysOnTop));

        // Font setting keys
        VERIFY_ARE_EQUAL(std::string_view{ "face" }, JsonKeyForSetting(FontSettingKey::FontFace));
        VERIFY_ARE_EQUAL(std::string_view{ "size" }, JsonKeyForSetting(FontSettingKey::FontSize));

        // SETTINGS_SIZE should be a valid (but large) enum value
        VERIFY_IS_TRUE(static_cast<int>(ProfileSettingKey::SETTINGS_SIZE) > 0);
        VERIFY_IS_TRUE(static_cast<int>(GlobalSettingKey::SETTINGS_SIZE) > 0);
        VERIFY_IS_TRUE(static_cast<int>(FontSettingKey::SETTINGS_SIZE) > 0);
        VERIFY_IS_TRUE(static_cast<int>(AppearanceSettingKey::SETTINGS_SIZE) > 0);
    }

    void ProfileTests::GenericHasAndClearMatchTypedAPIs()
    {
        // Verify that HasSetting(key) and ClearSetting(key) match the
        // typed HasXxx() and ClearXxx() methods.
        static constexpr std::string_view settingsJson{ R"({
            "profiles": {
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "historySize": 1000,
                        "snapOnInput": false
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);
        const auto profileImpl = winrt::get_self<implementation::Profile>(profile);

        // HasSetting should match HasXxx for set values
        VERIFY_ARE_EQUAL(profile.HasHistorySize(), profileImpl->HasSetting(implementation::ProfileSettingKey::HistorySize));
        VERIFY_ARE_EQUAL(profile.HasSnapOnInput(), profileImpl->HasSetting(implementation::ProfileSettingKey::SnapOnInput));

        // HasSetting should match HasXxx for unset values
        VERIFY_ARE_EQUAL(profile.HasTabTitle(), profileImpl->HasSetting(implementation::ProfileSettingKey::TabTitle));
        VERIFY_IS_FALSE(profileImpl->HasSetting(implementation::ProfileSettingKey::TabTitle));

        // ClearSetting should behave like ClearXxx
        profileImpl->ClearSetting(implementation::ProfileSettingKey::HistorySize);
        VERIFY_IS_FALSE(profile.HasHistorySize());
        VERIFY_IS_FALSE(profileImpl->HasSetting(implementation::ProfileSettingKey::HistorySize));
    }

    void ProfileTests::CurrentSettingsReturnsCorrectKeys()
    {
        // Verify that CurrentSettings() returns the keys that are explicitly
        // set at the current layer.
        static constexpr std::string_view settingsJson{ R"({
            "profiles": {
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "historySize": 1000,
                        "snapOnInput": false,
                        "tabTitle": "MyTab"
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);
        const auto profileImpl = winrt::get_self<implementation::Profile>(profile);

        const auto currentKeys = profileImpl->CurrentSettings();

        // historySize, snapOnInput, and tabTitle should be in the list
        auto hasHistorySize = false;
        auto hasSnapOnInput = false;
        auto hasTabTitle = false;
        for (const auto& key : currentKeys)
        {
            if (key == implementation::ProfileSettingKey::HistorySize)
            {
                hasHistorySize = true;
            }
            if (key == implementation::ProfileSettingKey::SnapOnInput)
            {
                hasSnapOnInput = true;
            }
            if (key == implementation::ProfileSettingKey::TabTitle)
            {
                hasTabTitle = true;
            }
        }
        VERIFY_IS_TRUE(hasHistorySize, L"historySize should be in CurrentSettings");
        VERIFY_IS_TRUE(hasSnapOnInput, L"snapOnInput should be in CurrentSettings");
        VERIFY_IS_TRUE(hasTabTitle, L"tabTitle should be in CurrentSettings");

        // Clear one and verify it's removed
        profileImpl->ClearSetting(implementation::ProfileSettingKey::TabTitle);
        const auto updatedKeys = profileImpl->CurrentSettings();
        auto hasTabTitleAfterClear = false;
        for (const auto& key : updatedKeys)
        {
            if (key == implementation::ProfileSettingKey::TabTitle)
            {
                hasTabTitleAfterClear = true;
            }
        }
        VERIFY_IS_FALSE(hasTabTitleAfterClear, L"tabTitle should NOT be in CurrentSettings after clear");
    }

    void ProfileTests::SpecialCasedSettingsJsonBacked()
    {
        // Verify that special-cased settings (Name, Guid, Hidden, Padding)
        // are now JSON-backed: setters write to _json, Has/Clear work correctly.
        //
        // NOTE: The "VisibleProfile" entry below exists only so CascadiaSettings
        // doesn't throw AllProfilesHidden during load.
        static constexpr std::string_view settingsJson{ R"({
            "profiles": {
                "list": [
                    {
                        "name": "TestProfile",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "hidden": true,
                        "padding": "12"
                    },
                    {
                        "name": "VisibleProfile",
                        "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                        "hidden": false
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);
        const auto profileImpl = winrt::get_self<implementation::Profile>(profile);

        // Verify initial values loaded from JSON
        VERIFY_ARE_EQUAL(L"TestProfile", profile.Name());
        VERIFY_IS_TRUE(profile.HasName());
        VERIFY_IS_TRUE(profile.Hidden());
        VERIFY_IS_TRUE(profile.HasHidden());
        VERIFY_ARE_EQUAL(L"12", profile.Padding());
        VERIFY_IS_TRUE(profile.HasPadding());

        // Modify Name via setter — should update _json
        profile.Name(L"NewName");
        VERIFY_ARE_EQUAL(L"NewName", profile.Name());
        const auto json1 = profileImpl->ToJson();
        VERIFY_ARE_EQUAL(L"NewName", til::u8u16(json1["name"].asString()));

        // Clear Name — should fall back to default
        profile.ClearName();
        VERIFY_IS_FALSE(profile.HasName());
        VERIFY_ARE_EQUAL(L"Default", profile.Name());

        // Modify Hidden via setter
        profile.Hidden(false);
        VERIFY_ARE_EQUAL(false, profile.Hidden());
        VERIFY_IS_TRUE(profile.HasHidden());

        // Clear Hidden — should fall back to default (false)
        profile.ClearHidden();
        VERIFY_IS_FALSE(profile.HasHidden());
        VERIFY_ARE_EQUAL(false, profile.Hidden());

        // Modify Padding — should write to _json
        profile.Padding(L"24");
        VERIFY_ARE_EQUAL(L"24", profile.Padding());

        // Test Guid setter
        static constexpr winrt::guid testGuid{ 0x11111111, 0x2222, 0x3333, { 0x44, 0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77 } };
        profile.Guid(testGuid);
        VERIFY_ARE_EQUAL(testGuid, profile.Guid());
    }

    void ProfileTests::NullableSettingJsonBacked()
    {
        // Verify that nullable settings (TabColor) are JSON-backed.
        // Null is a valid explicit value meaning "no color".
        static constexpr std::string_view settingsJson{ R"({
            "profiles": {
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "tabColor": "#FF0000"
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson);
        const auto profile = settings->AllProfiles().GetAt(0);
        const auto profileImpl = winrt::get_self<implementation::Profile>(profile);

        // Verify initial value
        VERIFY_IS_TRUE(profile.HasTabColor());
        VERIFY_IS_NOT_NULL(profile.TabColor());

        // Set to null explicitly (means "no color")
        profile.TabColor(nullptr);
        VERIFY_IS_TRUE(profile.HasTabColor()); // still "set" — explicitly null
        VERIFY_IS_NULL(profile.TabColor());

        // Clear — should inherit from parent (which has no tabColor)
        profile.ClearTabColor();
        VERIFY_IS_FALSE(profile.HasTabColor());

        // ToJson should not have tabColor after clearing
        const auto json = profileImpl->ToJson();
        VERIFY_IS_FALSE(json.isMember("tabColor"));
    }

    void ProfileTests::ColorSchemeJsonBacked()
    {
        // Verify that DarkColorSchemeName/LightColorSchemeName are JSON-backed,
        // including round-trip through LayerJson/ToJson, parent inheritance,
        // and the polymorphic colorScheme key (string vs object forms).
        //
        // NOTE: CascadiaSettings::_validateAllSchemesExist() runs during construction and
        // clears any DarkColorSchemeName/LightColorSchemeName that doesn't name a registered
        // scheme (falling back to the default). So every scheme the cases below reference must
        // be registered via this inbox JSON, otherwise the loaded values get wiped on load.
        static constexpr std::string_view inboxSchemes{ R"({
            "schemes": [
                { "name": "Campbell",       "black": "#0C0C0C", "red": "#C50F1F", "green": "#13A10E", "yellow": "#C19C00", "blue": "#0037DA", "purple": "#881798", "cyan": "#3A96DD", "white": "#CCCCCC", "brightBlack": "#767676", "brightRed": "#E74856", "brightGreen": "#16C60C", "brightYellow": "#F9F1A5", "brightBlue": "#3B78FF", "brightPurple": "#B4009E", "brightCyan": "#61D6D6", "brightWhite": "#F2F2F2" },
                { "name": "One Half Dark",  "black": "#0C0C0C", "red": "#C50F1F", "green": "#13A10E", "yellow": "#C19C00", "blue": "#0037DA", "purple": "#881798", "cyan": "#3A96DD", "white": "#CCCCCC", "brightBlack": "#767676", "brightRed": "#E74856", "brightGreen": "#16C60C", "brightYellow": "#F9F1A5", "brightBlue": "#3B78FF", "brightPurple": "#B4009E", "brightCyan": "#61D6D6", "brightWhite": "#F2F2F2" },
                { "name": "One Half Light", "black": "#0C0C0C", "red": "#C50F1F", "green": "#13A10E", "yellow": "#C19C00", "blue": "#0037DA", "purple": "#881798", "cyan": "#3A96DD", "white": "#CCCCCC", "brightBlack": "#767676", "brightRed": "#E74856", "brightGreen": "#16C60C", "brightYellow": "#F9F1A5", "brightBlue": "#3B78FF", "brightPurple": "#B4009E", "brightCyan": "#61D6D6", "brightWhite": "#F2F2F2" },
                { "name": "Tango Dark",     "black": "#0C0C0C", "red": "#C50F1F", "green": "#13A10E", "yellow": "#C19C00", "blue": "#0037DA", "purple": "#881798", "cyan": "#3A96DD", "white": "#CCCCCC", "brightBlack": "#767676", "brightRed": "#E74856", "brightGreen": "#16C60C", "brightYellow": "#F9F1A5", "brightBlue": "#3B78FF", "brightPurple": "#B4009E", "brightCyan": "#61D6D6", "brightWhite": "#F2F2F2" }
            ]
        })" };

        // Case 1: string form input — sets both dark and light to the same scheme
        {
            static constexpr std::string_view settingsJson{ R"({
                "profiles": {
                    "list": [
                        {
                            "name": "profile0",
                            "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                            "colorScheme": "One Half Dark"
                        }
                    ]
                }
            })" };

            const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson, inboxSchemes);
            const auto profile = settings->AllProfiles().GetAt(0);
            const auto profileImpl = winrt::get_self<implementation::Profile>(profile);
            const auto appearance = profile.DefaultAppearance();
            const auto appearanceImpl = winrt::get_self<implementation::AppearanceConfig>(appearance);

            // Both dark and light should be set to the same value
            VERIFY_ARE_EQUAL(L"One Half Dark", appearance.DarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"One Half Dark", appearance.LightColorSchemeName());
            VERIFY_IS_TRUE(appearanceImpl->HasDarkColorSchemeName());
            VERIFY_IS_TRUE(appearanceImpl->HasLightColorSchemeName());

            // ToJson should collapse to a string since dark == light
            const auto json = profileImpl->ToJson();
            VERIFY_IS_TRUE(json.isMember("colorScheme"));
            VERIFY_IS_TRUE(json["colorScheme"].isString());
            VERIFY_ARE_EQUAL("One Half Dark", json["colorScheme"].asString());
        }

        // Case 2: object form input — dark and light are different
        {
            static constexpr std::string_view settingsJson{ R"({
                "profiles": {
                    "list": [
                        {
                            "name": "profile0",
                            "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                            "colorScheme": {
                                "dark": "One Half Dark",
                                "light": "One Half Light"
                            }
                        }
                    ]
                }
            })" };

            const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson, inboxSchemes);
            const auto profile = settings->AllProfiles().GetAt(0);
            const auto profileImpl = winrt::get_self<implementation::Profile>(profile);
            const auto appearance = profile.DefaultAppearance();

            // Each should return its own value
            VERIFY_ARE_EQUAL(L"One Half Dark", appearance.DarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"One Half Light", appearance.LightColorSchemeName());

            // ToJson should produce an object since dark != light
            const auto json = profileImpl->ToJson();
            VERIFY_IS_TRUE(json.isMember("colorScheme"));
            VERIFY_IS_TRUE(json["colorScheme"].isObject());
            VERIFY_ARE_EQUAL("One Half Dark", json["colorScheme"]["dark"].asString());
            VERIFY_ARE_EQUAL("One Half Light", json["colorScheme"]["light"].asString());
        }

        // Case 3: setter updates — setting dark should preserve light, and vice versa
        {
            static constexpr std::string_view settingsJson{ R"({
                "profiles": {
                    "list": [
                        {
                            "name": "profile0",
                            "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                            "colorScheme": "Campbell"
                        }
                    ]
                }
            })" };

            const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson, inboxSchemes);
            const auto profile = settings->AllProfiles().GetAt(0);
            const auto profileImpl = winrt::get_self<implementation::Profile>(profile);
            const auto appearance = profile.DefaultAppearance();

            // Change dark only — light should stay "Campbell"
            appearance.DarkColorSchemeName(L"Tango Dark");
            VERIFY_ARE_EQUAL(L"Tango Dark", appearance.DarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell", appearance.LightColorSchemeName());

            // ToJson should produce an object since dark != light
            const auto json1 = profileImpl->ToJson();
            VERIFY_IS_TRUE(json1["colorScheme"].isObject());
            VERIFY_ARE_EQUAL("Tango Dark", json1["colorScheme"]["dark"].asString());
            VERIFY_ARE_EQUAL("Campbell", json1["colorScheme"]["light"].asString());

            // Change light to match dark — should collapse to string on ToJson
            appearance.LightColorSchemeName(L"Tango Dark");
            VERIFY_ARE_EQUAL(L"Tango Dark", appearance.LightColorSchemeName());
            const auto json2 = profileImpl->ToJson();
            VERIFY_IS_TRUE(json2["colorScheme"].isString());
            VERIFY_ARE_EQUAL("Tango Dark", json2["colorScheme"].asString());
        }

        // Case 4: clear — dark/light are independent. Clearing one side (even when it came from
        // a string input that set both) leaves the other side untouched. This matches main's
        // two-settings model: ClearDark only clears dark.
        {
            static constexpr std::string_view settingsJson{ R"({
                "profiles": {
                    "list": [
                        {
                            "name": "profile0",
                            "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                            "colorScheme": "One Half Dark"
                        }
                    ]
                }
            })" };

            const auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsJson, inboxSchemes);
            const auto profile = settings->AllProfiles().GetAt(0);
            const auto profileImpl = winrt::get_self<implementation::Profile>(profile);
            const auto appearance = profile.DefaultAppearance();
            const auto appearanceImpl = winrt::get_self<implementation::AppearanceConfig>(appearance);

            VERIFY_IS_TRUE(appearanceImpl->HasDarkColorSchemeName());
            VERIFY_IS_TRUE(appearanceImpl->HasLightColorSchemeName());

            // Clear dark only — light is independent and stays set.
            appearanceImpl->ClearDarkColorSchemeName();
            VERIFY_IS_FALSE(appearanceImpl->HasDarkColorSchemeName());
            VERIFY_IS_TRUE(appearanceImpl->HasLightColorSchemeName());

            // Dark falls back to the default "Campbell"; light retains "One Half Dark".
            VERIFY_ARE_EQUAL(L"Campbell", appearance.DarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"One Half Dark", appearance.LightColorSchemeName());

            // ToJson emits an object containing only the still-set light side.
            const auto json = profileImpl->ToJson();
            VERIFY_IS_TRUE(json.isMember("colorScheme"));
            VERIFY_IS_TRUE(json["colorScheme"].isObject());
            VERIFY_IS_FALSE(json["colorScheme"].isMember("dark"));
            VERIFY_ARE_EQUAL("One Half Dark", json["colorScheme"]["light"].asString());

            // Clearing light too removes colorScheme entirely.
            appearanceImpl->ClearLightColorSchemeName();
            VERIFY_IS_FALSE(appearanceImpl->HasLightColorSchemeName());
            const auto json2 = profileImpl->ToJson();
            VERIFY_IS_FALSE(json2.isMember("colorScheme"));
        }
    }
}
