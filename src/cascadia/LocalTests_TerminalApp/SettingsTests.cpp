// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "JsonTestClass.h"
#include "TestUtils.h"
#include <defaults.h>
#include "../ut_app/TestDynamicProfileGenerator.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;

namespace TerminalAppLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class SettingsTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(SettingsTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(TryCreateWinRTType);
        TEST_METHOD(ValidateProfilesExist);
        TEST_METHOD(ValidateDefaultProfileExists);
        TEST_METHOD(ValidateDuplicateProfiles);
        TEST_METHOD(ValidateManyWarnings);
        TEST_METHOD(LayerGlobalProperties);
        TEST_METHOD(ValidateProfileOrdering);
        TEST_METHOD(ValidateHideProfiles);
        TEST_METHOD(ValidateProfilesGenerateGuids);
        TEST_METHOD(GeneratedGuidRoundtrips);
        TEST_METHOD(TestAllValidationsWithNullGuids);
        TEST_METHOD(TestReorderWithNullGuids);
        TEST_METHOD(TestReorderingWithoutGuid);
        TEST_METHOD(TestLayeringNameOnlyProfiles);
        TEST_METHOD(TestExplodingNameOnlyProfiles);
        TEST_METHOD(TestHideAllProfiles);
        TEST_METHOD(TestInvalidColorSchemeName);
        TEST_METHOD(TestHelperFunctions);

        TEST_METHOD(TestProfileIconWithEnvVar);
        TEST_METHOD(TestProfileBackgroundImageWithEnvVar);

        TEST_METHOD(TestCloseOnExitParsing);
        TEST_METHOD(TestCloseOnExitCompatibilityShim);

        TEST_METHOD(TestLayerUserDefaultsBeforeProfiles);
        TEST_METHOD(TestDontLayerGuidFromUserDefaults);
        TEST_METHOD(TestLayerUserDefaultsOnDynamics);

        TEST_METHOD(TestTerminalArgsForBinding);

        TEST_METHOD(FindMissingProfile);
        TEST_METHOD(MakeSettingsForProfileThatDoesntExist);
        TEST_METHOD(MakeSettingsForDefaultProfileThatDoesntExist);

        TEST_METHOD(TestLayerProfileOnColorScheme);

        TEST_METHOD(ValidateKeybindingsWarnings);

        TEST_METHOD(ValidateLegacyGlobalsWarning);

        TEST_METHOD(TestTrailingCommas);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }
    };

    void SettingsTests::TryCreateWinRTType()
    {
        winrt::Microsoft::Terminal::Settings::TerminalSettings settings;
        VERIFY_IS_NOT_NULL(settings);
        auto oldFontSize = settings.FontSize();
        settings.FontSize(oldFontSize + 5);
        auto newFontSize = settings.FontSize();
        VERIFY_ARE_NOT_EQUAL(oldFontSize, newFontSize);
    }

    void SettingsTests::ValidateProfilesExist()
    {
        const std::string settingsWithProfiles{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0"
                }
            ]
        })" };

        const std::string settingsWithoutProfiles{ R"(
        {
            "defaultProfile": "{6239a42c-1de4-49a3-80bd-e8fdd045185c}"
        })" };

        const std::string settingsWithEmptyProfiles{ R"(
        {
            "profiles": []
        })" };

        {
            // Case 1: Good settings
            const auto settingsObject = VerifyParseSucceeded(settingsWithProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ValidateProfilesExist();
        }
        {
            // Case 2: Bad settings
            const auto settingsObject = VerifyParseSucceeded(settingsWithoutProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            bool caughtExpectedException = false;
            try
            {
                settings->_ValidateProfilesExist();
            }
            catch (const ::TerminalApp::SettingsException& ex)
            {
                VERIFY_IS_TRUE(ex.Error() == ::TerminalApp::SettingsLoadErrors::NoProfiles);
                caughtExpectedException = true;
            }
            VERIFY_IS_TRUE(caughtExpectedException);
        }
        {
            // Case 3: Bad settings
            const auto settingsObject = VerifyParseSucceeded(settingsWithEmptyProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            bool caughtExpectedException = false;
            try
            {
                settings->_ValidateProfilesExist();
            }
            catch (const ::TerminalApp::SettingsException& ex)
            {
                VERIFY_IS_TRUE(ex.Error() == ::TerminalApp::SettingsLoadErrors::NoProfiles);
                caughtExpectedException = true;
            }
            VERIFY_IS_TRUE(caughtExpectedException);
        }
    }

    void SettingsTests::ValidateDefaultProfileExists()
    {
        const std::string goodProfiles{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const std::string badProfiles{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const std::string noDefaultAtAll{ R"(
        {
            "alwaysShowTabs": true,
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-5555-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-6666-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const std::string goodProfilesSpecifiedByName{ R"(
        {
            "defaultProfile": "profile1",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        {
            // Case 1: Good settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, and the defaultProfile is one of those guids"));
            const auto settingsObject = VerifyParseSucceeded(goodProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ResolveDefaultProfile();
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings->_warnings.size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_profiles.size());
            VERIFY_ARE_EQUAL(settings->_globals.DefaultProfile(), settings->_profiles.at(0).GetGuid());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, but the defaultProfile is NOT one of those guids"));
            const auto settingsObject = VerifyParseSucceeded(badProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ResolveDefaultProfile();
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.size());
            VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingDefaultProfile, settings->_warnings.at(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_profiles.size());
            VERIFY_ARE_EQUAL(settings->_globals.DefaultProfile(), settings->_profiles.at(0).GetGuid());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, and no defaultProfile at all"));
            const auto settingsObject = VerifyParseSucceeded(badProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ResolveDefaultProfile();
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.size());
            VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingDefaultProfile, settings->_warnings.at(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_profiles.size());
            VERIFY_ARE_EQUAL(settings->_globals.DefaultProfile(), settings->_profiles.at(0).GetGuid());
        }
        {
            // Case 4: Good settings, default profile is a string
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, and the defaultProfile is one of the profile names"));
            const auto settingsObject = VerifyParseSucceeded(goodProfilesSpecifiedByName);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ResolveDefaultProfile();
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings->_warnings.size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_profiles.size());
            VERIFY_ARE_EQUAL(settings->_globals.DefaultProfile(), settings->_profiles.at(1).GetGuid());
        }
    }

    void SettingsTests::ValidateDuplicateProfiles()
    {
        const std::string goodProfiles{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const std::string badProfiles{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const std::string veryBadProfiles{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-5555-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile2",
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile3",
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile4",
                    "guid": "{6239a42c-6666-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile5",
                    "guid": "{6239a42c-5555-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile6",
                    "guid": "{6239a42c-7777-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };
        Profile profile0{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}") };
        profile0._name = L"profile0";
        Profile profile1{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-5555-49a3-80bd-e8fdd045185c}") };
        profile1._name = L"profile1";
        Profile profile2{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}") };
        profile2._name = L"profile2";
        Profile profile3{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}") };
        profile3._name = L"profile3";
        Profile profile4{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-6666-49a3-80bd-e8fdd045185c}") };
        profile4._name = L"profile4";
        Profile profile5{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-5555-49a3-80bd-e8fdd045185c}") };
        profile5._name = L"profile5";
        Profile profile6{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-7777-49a3-80bd-e8fdd045185c}") };
        profile6._name = L"profile6";

        {
            // Case 1: Good settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids"));

            CascadiaSettings settings;
            settings._profiles.push_back(profile0);
            settings._profiles.push_back(profile1);

            settings._ValidateNoDuplicateProfiles();

            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings._warnings.size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings._profiles.size());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with the same guid"));

            CascadiaSettings settings;
            settings._profiles.push_back(profile2);
            settings._profiles.push_back(profile3);

            settings._ValidateNoDuplicateProfiles();

            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings._warnings.size());
            VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::DuplicateProfile, settings._warnings.at(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0).GetName());
        }
        {
            // Case 3: Very bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a set of profiles, many of which with duplicated guids"));

            CascadiaSettings settings;
            settings._profiles.push_back(profile0);
            settings._profiles.push_back(profile1);
            settings._profiles.push_back(profile2);
            settings._profiles.push_back(profile3);
            settings._profiles.push_back(profile4);
            settings._profiles.push_back(profile5);
            settings._profiles.push_back(profile6);

            settings._ValidateNoDuplicateProfiles();

            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings._warnings.size());
            VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::DuplicateProfile, settings._warnings.at(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(4), settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0).GetName());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1).GetName());
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(2).GetName());
            VERIFY_ARE_EQUAL(L"profile6", settings._profiles.at(3).GetName());
        }
    }

    void SettingsTests::ValidateManyWarnings()
    {
        const std::string badProfiles{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile2",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };
        Profile profile4{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}") };
        profile4._name = L"profile4";
        Profile profile5{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}") };
        profile5._name = L"profile5";

        // Case 2: Bad settings
        Log::Comment(NoThrowString().Format(
            L"Testing a pair of profiles with the same guid"));
        const auto settingsObject = VerifyParseSucceeded(badProfiles);
        auto settings = CascadiaSettings::FromJson(settingsObject);

        settings->_profiles.push_back(profile4);
        settings->_profiles.push_back(profile5);

        settings->_ValidateSettings();

        VERIFY_ARE_EQUAL(3u, settings->_warnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::DuplicateProfile, settings->_warnings.at(0));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingDefaultProfile, settings->_warnings.at(1));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::UnknownColorScheme, settings->_warnings.at(2));

        VERIFY_ARE_EQUAL(3u, settings->_profiles.size());
        VERIFY_ARE_EQUAL(settings->_globals.DefaultProfile(), settings->_profiles.at(0).GetGuid());
        VERIFY_IS_TRUE(settings->_profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings->_profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings->_profiles.at(2)._guid.has_value());
    }

    void SettingsTests::LayerGlobalProperties()
    {
        const std::string settings0String{ R"(
        {
            "alwaysShowTabs": true,
            "initialCols" : 120,
            "initialRows" : 30,
            "rowsToScroll" :  4
        })" };
        const std::string settings1String{ R"(
        {
            "showTabsInTitlebar": false,
            "initialCols" : 240,
            "initialRows" : 60,
            "rowsToScroll" : 8
        })" };
        const auto settings0Json = VerifyParseSucceeded(settings0String);
        const auto settings1Json = VerifyParseSucceeded(settings1String);

        CascadiaSettings settings;

        settings.LayerJson(settings0Json);
        VERIFY_ARE_EQUAL(true, settings._globals._AlwaysShowTabs);
        VERIFY_ARE_EQUAL(120, settings._globals._InitialCols);
        VERIFY_ARE_EQUAL(30, settings._globals._InitialRows);
        VERIFY_ARE_EQUAL(4, settings._globals._RowsToScroll);
        VERIFY_ARE_EQUAL(true, settings._globals._ShowTabsInTitlebar);

        settings.LayerJson(settings1Json);
        VERIFY_ARE_EQUAL(true, settings._globals._AlwaysShowTabs);
        VERIFY_ARE_EQUAL(240, settings._globals._InitialCols);
        VERIFY_ARE_EQUAL(60, settings._globals._InitialRows);
        VERIFY_ARE_EQUAL(8, settings._globals._RowsToScroll);
        VERIFY_ARE_EQUAL(false, settings._globals._ShowTabsInTitlebar);
    }

    void SettingsTests::ValidateProfileOrdering()
    {
        const std::string userProfiles0String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const std::string defaultProfilesString{ R"(
        {
            "profiles": [
                {
                    "name" : "profile2",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile3",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const std::string userProfiles1String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile4",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile5",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const auto userProfiles0Json = VerifyParseSucceeded(userProfiles0String);
        const auto userProfiles1Json = VerifyParseSucceeded(userProfiles1String);
        const auto defaultProfilesJson = VerifyParseSucceeded(defaultProfilesString);

        {
            Log::Comment(NoThrowString().Format(
                L"Case 1: Simple swapping of the ordering. The user has the "
                L"default profiles in the opposite order of the default ordering."));

            CascadiaSettings settings;
            settings._ParseJsonString(defaultProfilesString, true);
            settings.LayerJson(settings._defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1)._name);

            settings._ParseJsonString(userProfiles0String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(1)._name);

            settings._ReorderProfilesToMatchUserSettingsOrder();
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1)._name);
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Case 2: Make sure all the user's profiles appear before the defaults."));

            CascadiaSettings settings;
            settings._ParseJsonString(defaultProfilesString, true);
            settings.LayerJson(settings._defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1)._name);

            settings._ParseJsonString(userProfiles1String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(3u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(2)._name);

            settings._ReorderProfilesToMatchUserSettingsOrder();
            VERIFY_ARE_EQUAL(3u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(2)._name);
        }
    }

    void SettingsTests::ValidateHideProfiles()
    {
        const std::string defaultProfilesString{ R"(
        {
            "profiles": [
                {
                    "name" : "profile2",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile3",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const std::string userProfiles0String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "hidden": true
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const std::string userProfiles1String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile4",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "hidden": true
                },
                {
                    "name" : "profile5",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile6",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
                    "hidden": true
                }
            ]
        })" };

        const auto userProfiles0Json = VerifyParseSucceeded(userProfiles0String);
        const auto userProfiles1Json = VerifyParseSucceeded(userProfiles1String);
        const auto defaultProfilesJson = VerifyParseSucceeded(defaultProfilesString);

        {
            CascadiaSettings settings;
            settings._ParseJsonString(defaultProfilesString, true);
            settings.LayerJson(settings._defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(1)._hidden);

            settings._ParseJsonString(userProfiles0String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(true, settings._profiles.at(1)._hidden);

            settings._ReorderProfilesToMatchUserSettingsOrder();
            settings._RemoveHiddenProfiles();
            VERIFY_ARE_EQUAL(1u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
        }

        {
            CascadiaSettings settings;
            settings._ParseJsonString(defaultProfilesString, true);
            settings.LayerJson(settings._defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(1)._hidden);

            settings._ParseJsonString(userProfiles1String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(4u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(2)._name);
            VERIFY_ARE_EQUAL(L"profile6", settings._profiles.at(3)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(true, settings._profiles.at(1)._hidden);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(2)._hidden);
            VERIFY_ARE_EQUAL(true, settings._profiles.at(3)._hidden);

            settings._ReorderProfilesToMatchUserSettingsOrder();
            settings._RemoveHiddenProfiles();
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(1)._hidden);
        }
    }

    void SettingsTests::ValidateProfilesGenerateGuids()
    {
        const std::string profile0String{ R"(
        {
            "name" : "profile0"
        })" };
        const std::string profile1String{ R"(
        {
            "name" : "profile1"
        })" };
        const std::string profile2String{ R"(
        {
            "name" : "profile2",
            "guid" : null
        })" };
        const std::string profile3String{ R"(
        {
            "name" : "profile3",
            "guid" : "{00000000-0000-0000-0000-000000000000}"
        })" };
        const std::string profile4String{ R"(
        {
            "name" : "profile4",
            "guid" : "{6239a42c-1de4-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string profile5String{ R"(
        {
            "name" : "profile2"
        })" };

        const auto profile0Json = VerifyParseSucceeded(profile0String);
        const auto profile1Json = VerifyParseSucceeded(profile1String);
        const auto profile2Json = VerifyParseSucceeded(profile2String);
        const auto profile3Json = VerifyParseSucceeded(profile3String);
        const auto profile4Json = VerifyParseSucceeded(profile4String);
        const auto profile5Json = VerifyParseSucceeded(profile5String);

        const auto profile0 = Profile::FromJson(profile0Json);
        const auto profile1 = Profile::FromJson(profile1Json);
        const auto profile2 = Profile::FromJson(profile2Json);
        const auto profile3 = Profile::FromJson(profile3Json);
        const auto profile4 = Profile::FromJson(profile4Json);
        const auto profile5 = Profile::FromJson(profile5Json);

        const GUID cmdGuid = Utils::GuidFromString(L"{6239a42c-1de4-49a3-80bd-e8fdd045185c}");
        const GUID nullGuid{ 0 };

        VERIFY_IS_FALSE(profile0._guid.has_value());
        VERIFY_IS_FALSE(profile1._guid.has_value());
        VERIFY_IS_FALSE(profile2._guid.has_value());
        VERIFY_IS_TRUE(profile3._guid.has_value());
        VERIFY_IS_TRUE(profile4._guid.has_value());
        VERIFY_IS_FALSE(profile5._guid.has_value());

        VERIFY_ARE_EQUAL(profile3.GetGuid(), nullGuid);
        VERIFY_ARE_EQUAL(profile4.GetGuid(), cmdGuid);

        CascadiaSettings settings;
        settings._profiles.emplace_back(profile0);
        settings._profiles.emplace_back(profile1);
        settings._profiles.emplace_back(profile2);
        settings._profiles.emplace_back(profile3);
        settings._profiles.emplace_back(profile4);
        settings._profiles.emplace_back(profile5);

        settings._ValidateProfilesHaveGuid();
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(4)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(5)._guid.has_value());

        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(0).GetGuid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(1).GetGuid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(2).GetGuid(), nullGuid);
        VERIFY_ARE_EQUAL(settings._profiles.at(3).GetGuid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(4).GetGuid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(5).GetGuid(), nullGuid);

        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(0).GetGuid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(1).GetGuid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(2).GetGuid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(3).GetGuid(), cmdGuid);
        VERIFY_ARE_EQUAL(settings._profiles.at(4).GetGuid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(5).GetGuid(), cmdGuid);

        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(0).GetGuid(), settings._profiles.at(2).GetGuid());
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(1).GetGuid(), settings._profiles.at(2).GetGuid());
        VERIFY_ARE_EQUAL(settings._profiles.at(2).GetGuid(), settings._profiles.at(2).GetGuid());
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(3).GetGuid(), settings._profiles.at(2).GetGuid());
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(4).GetGuid(), settings._profiles.at(2).GetGuid());
        VERIFY_ARE_EQUAL(settings._profiles.at(5).GetGuid(), settings._profiles.at(2).GetGuid());
    }

    void SettingsTests::GeneratedGuidRoundtrips()
    {
        // Parse a profile without a guid.
        // We should automatically generate a GUID for that profile.
        // When that profile is serialized and deserialized again, the GUID we
        // generated for it should persist.
        const std::string profileWithoutGuid{ R"({
                                              "name" : "profile0"
                                              })" };
        const auto profile0Json = VerifyParseSucceeded(profileWithoutGuid);

        const auto profile0 = Profile::FromJson(profile0Json);
        const GUID nullGuid{ 0 };

        VERIFY_IS_FALSE(profile0._guid.has_value());

        const auto serialized0Profile = profile0.GenerateStub();
        const auto profile1 = Profile::FromJson(serialized0Profile);
        VERIFY_IS_FALSE(profile0._guid.has_value());
        VERIFY_ARE_EQUAL(profile1._guid.has_value(), profile0._guid.has_value());

        CascadiaSettings settings;
        settings._profiles.emplace_back(profile1);
        settings._ValidateProfilesHaveGuid();

        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());

        const auto serialized1Profile = settings._profiles.at(0).GenerateStub();

        const auto profile2 = Profile::FromJson(serialized1Profile);
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_ARE_EQUAL(settings._profiles.at(0)._guid.has_value(), profile2._guid.has_value());
        VERIFY_ARE_EQUAL(settings._profiles.at(0).GetGuid(), profile2.GetGuid());
    }

    void SettingsTests::TestAllValidationsWithNullGuids()
    {
        const std::string settings0String{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid" : "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1"
                }
            ],
            "schemes": [
                { "name": "Campbell" }
            ]
        })" };

        const auto settings0Json = VerifyParseSucceeded(settings0String);

        CascadiaSettings settings;
        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(2u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(1)._guid.has_value());

        settings._ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(2u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
    }

    void SettingsTests::TestReorderWithNullGuids()
    {
        const std::string settings0String{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid" : "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1"
                },
                {
                    "name" : "cmdFromUserSettings",
                    "guid" : "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}" // from defaults.json
                }
            ]
        })" };

        const auto settings0Json = VerifyParseSucceeded(settings0String);

        CascadiaSettings settings;
        settings._ParseJsonString(DefaultJson, true);
        settings.LayerJson(settings._defaultSettings);
        VERIFY_ARE_EQUAL(2u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1)._name);

        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(3)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"cmdFromUserSettings", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(3)._name);

        settings._ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._guid.has_value());
        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"cmdFromUserSettings", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(3)._name);
    }

    void SettingsTests::TestReorderingWithoutGuid()
    {
        Log::Comment(NoThrowString().Format(
            L"During the GH#2515 PR, this set of settings was found to cause an"
            L" exception, crashing the terminal. This test ensures that it doesn't."));

        Log::Comment(NoThrowString().Format(
            L"While similar to TestReorderWithNullGuids, there's something else"
            L" about this scenario specifically that causes a crash, when "
            L" TestReorderWithNullGuids did _not_."));

        const std::string settings0String{ R"(
        {
            "defaultProfile" : "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
            "profiles": [
                {
                    "guid" : "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                    "acrylicOpacity" : 0.5,
                    "closeOnExit" : true,
                    "background" : "#8A00FF",
                    "foreground" : "#F2F2F2",
                    "commandline" : "cmd.exe",
                    "cursorColor" : "#FFFFFF",
                    "fontFace" : "Cascadia Code",
                    "fontSize" : 10,
                    "historySize" : 9001,
                    "padding" : "20",
                    "snapOnInput" : true,
                    "startingDirectory" : "%USERPROFILE%",
                    "useAcrylic" : true
                },
                {
                    "name" : "ThisProfileShouldNotCrash",
                    "tabTitle" : "Ubuntu",
                    "acrylicOpacity" : 0.5,
                    "background" : "#2C001E",
                    "closeOnExit" : true,
                    "colorScheme" : "Campbell",
                    "commandline" : "wsl.exe",
                    "cursorColor" : "#FFFFFF",
                    "cursorShape" : "bar",
                    "fontSize" : 10,
                    "historySize" : 9001,
                    "padding" : "0, 0, 0, 0",
                    "snapOnInput" : true,
                    "useAcrylic" : true
                },
                {
                    // This is the same profile that would be generated by the WSL profile generator.
                    "name" : "Ubuntu",
                    "guid" : "{2C4DE342-38B7-51CF-B940-2309A097F518}",
                    "acrylicOpacity" : 0.5,
                    "background" : "#2C001E",
                    "closeOnExit" : false,
                    "cursorColor" : "#FFFFFF",
                    "cursorShape" : "bar",
                    "fontSize" : 10,
                    "historySize" : 9001,
                    "snapOnInput" : true,
                    "useAcrylic" : true
                }
            ]
        })" };

        const auto settings0Json = VerifyParseSucceeded(settings0String);

        CascadiaSettings settings;
        settings._ParseJsonString(DefaultJson, true);
        settings.LayerJson(settings._defaultSettings);
        VERIFY_ARE_EQUAL(2u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1)._name);

        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotCrash", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"Ubuntu", settings._profiles.at(3)._name);

        settings._ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotCrash", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"Ubuntu", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(3)._name);
    }

    void SettingsTests::TestLayeringNameOnlyProfiles()
    {
        // This is a test discovered during GH#2782. When we add a name-only
        // profile, it should only layer with other name-only profiles with the
        // _same name_

        const std::string settings0String{ R"(
        {
            "defaultProfile" : "{00000000-0000-5f56-a8ff-afceeeaa6101}",
            "profiles": [
                {
                    "guid" : "{00000000-0000-5f56-a8ff-afceeeaa6101}",
                    "name" : "ThisProfileIsGood"

                },
                {
                    "name" : "ThisProfileShouldNotLayer"
                },
                {
                    "name" : "NeitherShouldThisOne"
                }
            ]
        })" };

        const auto settings0Json = VerifyParseSucceeded(settings0String);

        CascadiaSettings settings;
        settings._ParseJsonString(DefaultJson, true);
        settings.LayerJson(settings._defaultSettings);
        VERIFY_ARE_EQUAL(2u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1)._name);

        Log::Comment(NoThrowString().Format(
            L"Parse the user settings"));
        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(5u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(3)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(4)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotLayer", settings._profiles.at(3)._name);
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings._profiles.at(4)._name);
    }

    void SettingsTests::TestExplodingNameOnlyProfiles()
    {
        // This is a test for GH#2782. When we add a name-only profile, we'll
        // generate a GUID for it. We should make sure that we don't re-append
        // that profile to the list of profiles.

        const std::string settings0String{ R"(
        {
            "defaultProfile" : "{00000000-0000-5f56-a8ff-afceeeaa6101}",
            "profiles": [
                {
                    "guid" : "{00000000-0000-5f56-a8ff-afceeeaa6101}",
                    "name" : "ThisProfileIsGood"

                },
                {
                    "name" : "ThisProfileShouldNotDuplicate"
                },
                {
                    "name" : "NeitherShouldThisOne"
                }
            ]
        })" };

        const auto settings0Json = VerifyParseSucceeded(settings0String);

        CascadiaSettings settings;
        settings._ParseJsonString(DefaultJson, true);
        settings.LayerJson(settings._defaultSettings);
        VERIFY_ARE_EQUAL(2u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1)._name);

        Log::Comment(NoThrowString().Format(
            L"Parse the user settings"));
        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(5u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(3)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(4)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotDuplicate", settings._profiles.at(3)._name);
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings._profiles.at(4)._name);

        Log::Comment(NoThrowString().Format(
            L"Pretend like we're checking to append dynamic profiles to the "
            L"user's settings file. We absolutely _shouldn't_ be adding anything here."));
        bool const needToWriteFile = settings._AppendDynamicProfilesToUserSettings();
        VERIFY_IS_FALSE(needToWriteFile);
        VERIFY_ARE_EQUAL(settings0String.size(), settings._userSettingsString.size());

        Log::Comment(NoThrowString().Format(
            L"Re-parse the settings file. We should have the _same_ settings as before."));
        Log::Comment(NoThrowString().Format(
            L"Do this to a _new_ settings object, to make sure it turns out the same."));
        {
            CascadiaSettings settings2;
            settings2._ParseJsonString(DefaultJson, true);
            settings2.LayerJson(settings2._defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings2._profiles.size());
            // Initialize the second settings object from the first settings
            // object's settings string, the one that we synthesized.
            const auto firstSettingsString = settings._userSettingsString;
            settings2._ParseJsonString(firstSettingsString, false);
            settings2.LayerJson(settings2._userSettings);
            VERIFY_ARE_EQUAL(5u, settings2._profiles.size());
            VERIFY_IS_TRUE(settings2._profiles.at(0)._guid.has_value());
            VERIFY_IS_TRUE(settings2._profiles.at(1)._guid.has_value());
            VERIFY_IS_TRUE(settings2._profiles.at(2)._guid.has_value());
            VERIFY_IS_FALSE(settings2._profiles.at(3)._guid.has_value());
            VERIFY_IS_FALSE(settings2._profiles.at(4)._guid.has_value());
            VERIFY_ARE_EQUAL(L"Windows PowerShell", settings2._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"Command Prompt", settings2._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings2._profiles.at(2)._name);
            VERIFY_ARE_EQUAL(L"ThisProfileShouldNotDuplicate", settings2._profiles.at(3)._name);
            VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings2._profiles.at(4)._name);
        }

        Log::Comment(NoThrowString().Format(
            L"Validate the settings. All the profiles we have should be valid."));
        settings._ValidateSettings();

        VERIFY_ARE_EQUAL(5u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(4)._guid.has_value());
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotDuplicate", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(3)._name);
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(4)._name);
    }

    void SettingsTests::TestHideAllProfiles()
    {
        const std::string settingsWithProfiles{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "hidden": false
                },
                {
                    "name" : "profile1",
                    "hidden": true
                }
            ]
        })" };

        const std::string settingsWithoutProfiles{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "hidden": true
                },
                {
                    "name" : "profile1",
                    "hidden": true
                }
            ]
        })" };

        VerifyParseSucceeded(settingsWithProfiles);
        VerifyParseSucceeded(settingsWithoutProfiles);

        {
            // Case 1: Good settings
            CascadiaSettings settings;
            settings._ParseJsonString(settingsWithProfiles, false);
            settings.LayerJson(settings._userSettings);

            settings._RemoveHiddenProfiles();
            Log::Comment(NoThrowString().Format(
                L"settingsWithProfiles successfully parsed and validated"));
            VERIFY_ARE_EQUAL(1u, settings._profiles.size());
        }
        {
            // Case 2: Bad settings

            CascadiaSettings settings;
            settings._ParseJsonString(settingsWithoutProfiles, false);
            settings.LayerJson(settings._userSettings);

            bool caughtExpectedException = false;
            try
            {
                settings._RemoveHiddenProfiles();
            }
            catch (const ::TerminalApp::SettingsException& ex)
            {
                VERIFY_IS_TRUE(ex.Error() == ::TerminalApp::SettingsLoadErrors::AllProfilesHidden);
                caughtExpectedException = true;
            }
            VERIFY_IS_TRUE(caughtExpectedException);
        }
    }

    void SettingsTests::TestInvalidColorSchemeName()
    {
        Log::Comment(NoThrowString().Format(
            L"Ensure that setting a profile's scheme to a non-existent scheme causes a warning."));

        const std::string settings0String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "schemeOne"
                },
                {
                    "name" : "profile1",
                    "colorScheme": "InvalidSchemeName"
                },
                {
                    "name" : "profile2"
                    // Will use the Profile default value, "Campbell"
                }
            ],
            "schemes": [
                {
                    "name": "schemeOne",
                    "foreground": "#111111"
                },
                {
                    "name": "schemeTwo",
                    "foreground": "#222222"
                }
            ]
        })" };

        VerifyParseSucceeded(settings0String);

        CascadiaSettings settings;
        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(3u, settings._profiles.size());
        VERIFY_ARE_EQUAL(2u, settings._globals._colorSchemes.size());

        VERIFY_ARE_EQUAL(L"schemeOne", settings._profiles.at(0)._schemeName.value());
        VERIFY_ARE_EQUAL(L"InvalidSchemeName", settings._profiles.at(1)._schemeName.value());
        VERIFY_ARE_EQUAL(L"Campbell", settings._profiles.at(2)._schemeName.value());

        settings._ValidateAllSchemesExist();

        VERIFY_ARE_EQUAL(1u, settings._warnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::UnknownColorScheme, settings._warnings.at(0));

        VERIFY_ARE_EQUAL(3u, settings._profiles.size());
        VERIFY_ARE_EQUAL(2u, settings._globals._colorSchemes.size());

        VERIFY_ARE_EQUAL(L"schemeOne", settings._profiles.at(0)._schemeName.value());
        VERIFY_ARE_EQUAL(L"Campbell", settings._profiles.at(1)._schemeName.value());
        VERIFY_ARE_EQUAL(L"Campbell", settings._profiles.at(2)._schemeName.value());
    }

    void SettingsTests::TestHelperFunctions()
    {
        const std::string settings0String{ R"(
        {
            "defaultProfile" : "{2C4DE342-38B7-51CF-B940-2309A097F518}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-5555-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-6666-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "ThisProfileShouldNotThrow"
                },
                {
                    "name" : "Ubuntu",
                    "guid" : "{2C4DE342-38B7-51CF-B940-2309A097F518}"
                }
            ]
        })" };

        auto name0{ L"profile0" };
        auto name1{ L"profile1" };
        auto name2{ L"Ubuntu" };
        auto name3{ L"ThisProfileShouldNotThrow" };
        auto badName{ L"DoesNotExist" };

        auto guid0{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-5555-49a3-80bd-e8fdd045185c}") };
        auto guid1{ Microsoft::Console::Utils::GuidFromString(L"{6239a42c-6666-49a3-80bd-e8fdd045185c}") };
        auto guid2{ Microsoft::Console::Utils::GuidFromString(L"{2C4DE342-38B7-51CF-B940-2309A097F518}") };
        auto fakeGuid{ Microsoft::Console::Utils::GuidFromString(L"{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}") };
        std::optional<GUID> badGuid{};

        VerifyParseSucceeded(settings0String);

        CascadiaSettings settings;
        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(guid0, settings._GetProfileGuidByName(name0));
        VERIFY_ARE_EQUAL(guid1, settings._GetProfileGuidByName(name1));
        VERIFY_ARE_EQUAL(guid2, settings._GetProfileGuidByName(name2));
        VERIFY_ARE_EQUAL(badGuid, settings._GetProfileGuidByName(name3));
        VERIFY_ARE_EQUAL(badGuid, settings._GetProfileGuidByName(badName));

        auto prof0{ settings.FindProfile(guid0) };
        auto prof1{ settings.FindProfile(guid1) };
        auto prof2{ settings.FindProfile(guid2) };

        auto badProf{ settings.FindProfile(fakeGuid) };
        VERIFY_ARE_EQUAL(badProf, nullptr);

        VERIFY_ARE_EQUAL(name0, prof0->GetName());
        VERIFY_ARE_EQUAL(name1, prof1->GetName());
        VERIFY_ARE_EQUAL(name2, prof2->GetName());
    }

    void SettingsTests::TestProfileIconWithEnvVar()
    {
        const auto expectedPath = wil::ExpandEnvironmentStringsW<std::wstring>(L"%WINDIR%\\System32\\x_80.png");

        const std::string settingsJson{ R"(
        {
            "profiles": [
                {
                    "name": "profile0",
                    "icon": "%WINDIR%\\System32\\x_80.png"
                }
            ]
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);
        VERIFY_IS_FALSE(settings._profiles.empty());
        VERIFY_ARE_EQUAL(expectedPath, settings._profiles[0].GetExpandedIconPath());
    }
    void SettingsTests::TestProfileBackgroundImageWithEnvVar()
    {
        const auto expectedPath = wil::ExpandEnvironmentStringsW<std::wstring>(L"%WINDIR%\\System32\\x_80.png");

        const std::string settingsJson{ R"(
        {
            "profiles": [
                {
                    "name": "profile0",
                    "backgroundImage": "%WINDIR%\\System32\\x_80.png"
                }
            ]
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);
        VERIFY_IS_FALSE(settings._profiles.empty());

        GlobalAppSettings globalSettings{};
        auto terminalSettings = settings._profiles[0].CreateTerminalSettings(globalSettings.GetColorSchemes());
        VERIFY_ARE_EQUAL(expectedPath, terminalSettings.BackgroundImage());
    }
    void SettingsTests::TestCloseOnExitParsing()
    {
        const std::string settingsJson{ R"(
        {
            "profiles": [
                {
                    "name": "profile0",
                    "closeOnExit": "graceful"
                },
                {
                    "name": "profile1",
                    "closeOnExit": "always"
                },
                {
                    "name": "profile2",
                    "closeOnExit": "never"
                },
                {
                    "name": "profile3",
                    "closeOnExit": null
                },
                {
                    "name": "profile4",
                    "closeOnExit": { "clearly": "not a string" }
                }
            ]
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings._profiles[0].GetCloseOnExitMode());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Always, settings._profiles[1].GetCloseOnExitMode());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Never, settings._profiles[2].GetCloseOnExitMode());

        // Unknown modes parse as "Graceful"
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings._profiles[3].GetCloseOnExitMode());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings._profiles[4].GetCloseOnExitMode());
    }
    void SettingsTests::TestCloseOnExitCompatibilityShim()
    {
        const std::string settingsJson{ R"(
        {
            "profiles": [
                {
                    "name": "profile0",
                    "closeOnExit": true
                },
                {
                    "name": "profile1",
                    "closeOnExit": false
                }
            ]
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings._profiles[0].GetCloseOnExitMode());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Never, settings._profiles[1].GetCloseOnExitMode());
    }

    void SettingsTests::TestLayerUserDefaultsBeforeProfiles()
    {
        // Test for microsoft/terminal#2325. For this test, we'll be setting the
        // "historySize" in the "defaultSettings", so it should apply to all
        // profiles, unless they override it. In one of the user's profiles,
        // we'll override that value, and in the other, we'll leave it
        // untouched.

        const std::string settings0String{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": {
                "defaults": {
                    "historySize": 1234
                },
                "list": [
                    {
                        "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                        "name": "profile0",
                        "historySize": 2345
                    },
                    {
                        "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                        "name": "profile1"
                    }
                ]
            }
        })" };
        VerifyParseSucceeded(settings0String);

        const auto guid1String = L"{6239a42c-1111-49a3-80bd-e8fdd045185c}";

        {
            CascadiaSettings settings{ false };
            settings._ParseJsonString(settings0String, false);
            VERIFY_IS_TRUE(settings._userDefaultProfileSettings == Json::Value::null);
            settings._ApplyDefaultsFromUserSettings();
            VERIFY_IS_FALSE(settings._userDefaultProfileSettings == Json::Value::null);
            settings.LayerJson(settings._userSettings);

            VERIFY_ARE_EQUAL(guid1String, settings._globals._unparsedDefaultProfile);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());

            VERIFY_ARE_EQUAL(2345, settings._profiles.at(0)._historySize);
            VERIFY_ARE_EQUAL(1234, settings._profiles.at(1)._historySize);
        }
    }

    void SettingsTests::TestDontLayerGuidFromUserDefaults()
    {
        // Test for microsoft/terminal#2325. We don't want the user to put a
        // "guid" in the "defaultSettings", and have that apply to all the other
        // profiles

        const std::string settings0String{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": {
                "defaults": {
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                },
                "list": [
                    {
                        "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                        "name": "profile0",
                        "historySize": 2345
                    },
                    {
                        // Doesn't have a GUID, we'll auto-generate one
                        "name": "profile1"
                    }
                ]
            }
        })" };
        VerifyParseSucceeded(settings0String);

        const auto guid1String = L"{6239a42c-1111-49a3-80bd-e8fdd045185c}";
        const auto guid1 = Microsoft::Console::Utils::GuidFromString(guid1String);
        const auto guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");

        {
            CascadiaSettings settings{ false };
            settings._ParseJsonString(DefaultJson, true);
            settings.LayerJson(settings._defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());

            settings._ParseJsonString(settings0String, false);
            VERIFY_IS_TRUE(settings._userDefaultProfileSettings == Json::Value::null);
            settings._ApplyDefaultsFromUserSettings();
            VERIFY_IS_FALSE(settings._userDefaultProfileSettings == Json::Value::null);

            Log::Comment(NoThrowString().Format(
                L"Ensure that cmd and powershell don't get their GUIDs overwritten"));
            VERIFY_ARE_NOT_EQUAL(guid2, settings._profiles.at(0)._guid);
            VERIFY_ARE_NOT_EQUAL(guid2, settings._profiles.at(1)._guid);

            settings.LayerJson(settings._userSettings);

            VERIFY_ARE_EQUAL(guid1String, settings._globals._unparsedDefaultProfile);
            VERIFY_ARE_EQUAL(4u, settings._profiles.size());

            VERIFY_ARE_EQUAL(guid1, settings._profiles.at(2)._guid);
            VERIFY_IS_FALSE(settings._profiles.at(3)._guid.has_value());
        }
    }

    void SettingsTests::TestLayerUserDefaultsOnDynamics()
    {
        // Test for microsoft/terminal#2325. For this test, we'll be setting the
        // "historySize" in the "defaultSettings", so it should apply to all
        // profiles, unless they override it. The dynamic profiles will _also_
        // set this value, but from discussion in GH#2325, we decided that
        // settings in defaultSettings should apply _on top_ of settings from
        // dynamic profiles.

        GUID guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        GUID guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        GUID guid3 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        const std::string userProfiles{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": {
                "defaults": {
                    "historySize": 1234
                },
                "list": [
                    {
                        "name" : "profile0FromUserSettings", // this is _profiles.at(0)
                        "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                        "source": "Terminal.App.UnitTest.0"
                    },
                    {
                        "name" : "profile1FromUserSettings", // this is _profiles.at(2)
                        "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                        "source": "Terminal.App.UnitTest.1",
                        "historySize": 4444
                    },
                    {
                        "name" : "profile2FromUserSettings", // this is _profiles.at(3)
                        "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
                        "historySize": 5555
                    }
                ]
            }
        })" };

        auto gen0 = std::make_unique<TerminalAppUnitTests::TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
        gen0->pfnGenerate = [guid1, guid2]() {
            std::vector<Profile> profiles;
            Profile p0{ guid1 };
            p0.SetName(L"profile0"); // this is _profiles.at(0)
            p0._historySize = 1111;
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TerminalAppUnitTests::TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = [guid1, guid2]() {
            std::vector<Profile> profiles;
            Profile p0{ guid1 }, p1{ guid2 };
            p0.SetName(L"profile0"); // this is _profiles.at(1)
            p1.SetName(L"profile1"); // this is _profiles.at(2)
            p0._historySize = 2222;
            profiles.push_back(p0);
            p1._historySize = 3333;
            profiles.push_back(p1);
            return profiles;
        };

        CascadiaSettings settings{ false };
        settings._profileGenerators.emplace_back(std::move(gen0));
        settings._profileGenerators.emplace_back(std::move(gen1));

        Log::Comment(NoThrowString().Format(
            L"All profiles with the same name have the same GUID. However, they"
            L" will not be layered, because they have different source's"));

        // parse userProfiles as the user settings
        settings._ParseJsonString(userProfiles, false);
        VERIFY_ARE_EQUAL(0u, settings._profiles.size(), L"Just parsing the user settings doesn't actually layer them");
        settings._LoadDynamicProfiles();
        VERIFY_ARE_EQUAL(3u, settings._profiles.size());

        VERIFY_ARE_EQUAL(1111, settings._profiles.at(0)._historySize);
        VERIFY_ARE_EQUAL(2222, settings._profiles.at(1)._historySize);
        VERIFY_ARE_EQUAL(3333, settings._profiles.at(2)._historySize);

        settings._ApplyDefaultsFromUserSettings();

        VERIFY_ARE_EQUAL(1234, settings._profiles.at(0)._historySize);
        VERIFY_ARE_EQUAL(1234, settings._profiles.at(1)._historySize);
        VERIFY_ARE_EQUAL(1234, settings._profiles.at(2)._historySize);

        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(4u, settings._profiles.size());

        VERIFY_IS_TRUE(settings._profiles.at(0)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._source.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(3)._source.has_value());

        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.0", settings._profiles.at(0)._source.value());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(1)._source.value());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(2)._source.value());

        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());

        VERIFY_ARE_EQUAL(guid1, settings._profiles.at(0)._guid.value());
        VERIFY_ARE_EQUAL(guid1, settings._profiles.at(1)._guid.value());
        VERIFY_ARE_EQUAL(guid2, settings._profiles.at(2)._guid.value());

        VERIFY_ARE_EQUAL(L"profile0FromUserSettings", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"profile1FromUserSettings", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"profile2FromUserSettings", settings._profiles.at(3)._name);

        Log::Comment(NoThrowString().Format(
            L"This is the real meat of the test: The two dynamic profiles that "
            L"_didn't_ have historySize set in the userSettings should have "
            L"1234 as their historySize(from the defaultSettings).The other two"
            L" profiles should have their custom historySize value."));

        VERIFY_ARE_EQUAL(1234, settings._profiles.at(0)._historySize);
        VERIFY_ARE_EQUAL(1234, settings._profiles.at(1)._historySize);
        VERIFY_ARE_EQUAL(4444, settings._profiles.at(2)._historySize);
        VERIFY_ARE_EQUAL(5555, settings._profiles.at(3)._historySize);
    }

    void SettingsTests::TestTerminalArgsForBinding()
    {
        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "keybindings": [
                { "keys": ["ctrl+a"], "command": { "action": "splitPane", "split": "vertical" } },
                { "keys": ["ctrl+b"], "command": { "action": "splitPane", "split": "vertical", "profile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" } },
                { "keys": ["ctrl+c"], "command": { "action": "splitPane", "split": "vertical", "profile": "profile1" } },
                { "keys": ["ctrl+d"], "command": { "action": "splitPane", "split": "vertical", "profile": "profile2" } },
                { "keys": ["ctrl+e"], "command": { "action": "splitPane", "split": "horizontal", "commandline": "foo.exe" } },
                { "keys": ["ctrl+f"], "command": { "action": "splitPane", "split": "horizontal", "profile": "profile1", "commandline": "foo.exe" } },
                { "keys": ["ctrl+g"], "command": { "action": "newTab" } },
                { "keys": ["ctrl+h"], "command": { "action": "newTab", "startingDirectory": "c:\\foo" } },
                { "keys": ["ctrl+i"], "command": { "action": "newTab", "profile": "profile2", "startingDirectory": "c:\\foo" } },
                { "keys": ["ctrl+j"], "command": { "action": "newTab", "tabTitle": "bar" } },
                { "keys": ["ctrl+k"], "command": { "action": "newTab", "profile": "profile2", "tabTitle": "bar" } },
                { "keys": ["ctrl+l"], "command": { "action": "newTab", "profile": "profile1", "tabTitle": "bar", "startingDirectory": "c:\\foo", "commandline":"foo.exe" } }
            ]
        })" };

        const auto guid0 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}");
        const auto guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);
        settings._ValidateSettings();

        auto appKeyBindings = settings._globals._keybindings;
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        const auto profile2Guid = settings._profiles.at(2).GetGuid();
        VERIFY_ARE_NOT_EQUAL(GUID{ 0 }, profile2Guid);

        VERIFY_ARE_EQUAL(12u, appKeyBindings->_keyShortcuts.size());

        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('A') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('B') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('D') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(profile2Guid, guid);
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('E') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Horizontal, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('F') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Horizontal, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('G') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('H') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"c:\\foo", realArgs.TerminalArgs().StartingDirectory());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('I') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"c:\\foo", realArgs.TerminalArgs().StartingDirectory());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(profile2Guid, guid);
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('J') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"bar", realArgs.TerminalArgs().TabTitle());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('K') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"bar", realArgs.TerminalArgs().TabTitle());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(profile2Guid, guid);
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('L') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(*appKeyBindings, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", realArgs.TerminalArgs().StartingDirectory());
            VERIFY_ARE_EQUAL(L"bar", realArgs.TerminalArgs().TabTitle());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = settings.BuildSettings(realArgs.TerminalArgs());
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
    }

    void SettingsTests::FindMissingProfile()
    {
        // Test that CascadiaSettings::FindProfile returns null for a GUID that
        // doesn't exist
        const std::string settingsString{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };
        const auto settingsJsonObj = VerifyParseSucceeded(settingsString);
        auto settings = CascadiaSettings::FromJson(settingsJsonObj);

        const auto guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        const Profile* const profile1 = settings->FindProfile(guid1);
        const Profile* const profile2 = settings->FindProfile(guid2);
        const Profile* const profile3 = settings->FindProfile(guid3);

        VERIFY_IS_NOT_NULL(profile1);
        VERIFY_IS_NOT_NULL(profile2);
        VERIFY_IS_NULL(profile3);

        VERIFY_ARE_EQUAL(L"profile0", profile1->GetName());
        VERIFY_ARE_EQUAL(L"profile1", profile2->GetName());
    }

    void SettingsTests::MakeSettingsForProfileThatDoesntExist()
    {
        // Test that MakeSettings throws when the GUID doesn't exist
        const std::string settingsString{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 1
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };
        const auto settingsJsonObj = VerifyParseSucceeded(settingsString);
        auto settings = CascadiaSettings::FromJson(settingsJsonObj);
        settings->_ResolveDefaultProfile();

        const auto guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        try
        {
            auto terminalSettings = settings->BuildSettings(guid1);
            VERIFY_ARE_NOT_EQUAL(nullptr, terminalSettings);
            VERIFY_ARE_EQUAL(1, terminalSettings.HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to BuildSettings should succeed");
        }

        try
        {
            auto terminalSettings = settings->BuildSettings(guid2);
            VERIFY_ARE_NOT_EQUAL(nullptr, terminalSettings);
            VERIFY_ARE_EQUAL(2, terminalSettings.HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to BuildSettings should succeed");
        }

        VERIFY_THROWS(auto terminalSettings = settings->BuildSettings(guid3), wil::ResultException, L"This call to BuildSettings should fail");

        try
        {
            const auto [guid, termSettings] = settings->BuildSettings(nullptr);
            VERIFY_ARE_NOT_EQUAL(nullptr, termSettings);
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to BuildSettings should succeed");
        }
    }

    void SettingsTests::MakeSettingsForDefaultProfileThatDoesntExist()
    {
        // Test that MakeSettings _doesnt_ throw when we load settings with a
        // defaultProfile that's not in the list, we validate the settings, and
        // then call MakeSettings(nullopt). The validation should ensure that
        // the default profile is something reasonable
        const std::string settingsString{ R"(
        {
            "defaultProfile": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 1
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };
        const auto settingsJsonObj = VerifyParseSucceeded(settingsString);
        auto settings = CascadiaSettings::FromJson(settingsJsonObj);
        settings->_ValidateSettings();

        VERIFY_ARE_EQUAL(2u, settings->_warnings.size());
        VERIFY_ARE_EQUAL(2u, settings->_profiles.size());
        VERIFY_ARE_EQUAL(settings->_globals.DefaultProfile(), settings->_profiles.at(0).GetGuid());
        try
        {
            const auto [guid, termSettings] = settings->BuildSettings(nullptr);
            VERIFY_ARE_NOT_EQUAL(nullptr, termSettings);
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to BuildSettings should succeed");
        }
    }

    void SettingsTests::TestLayerProfileOnColorScheme()
    {
        Log::Comment(NoThrowString().Format(
            L"Ensure that setting (or not) a property in the profile that should override a property of the color scheme works correctly."));

        const std::string settings0String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "schemeWithCursorColor"
                },
                {
                    "name" : "profile1",
                    "colorScheme": "schemeWithoutCursorColor"
                },
                {
                    "name" : "profile2",
                    "colorScheme": "schemeWithCursorColor",
                    "cursorColor": "#234567"
                },
                {
                    "name" : "profile3",
                    "colorScheme": "schemeWithoutCursorColor",
                    "cursorColor": "#345678"
                },
                {
                    "name" : "profile4",
                    "cursorColor": "#456789"
                },
                {
                    "name" : "profile5"
                }
            ],
            "schemes": [
                {
                    "name": "schemeWithCursorColor",
                    "cursorColor": "#123456"
                },
                {
                    "name": "schemeWithoutCursorColor"
                }
            ]
        })" };

        VerifyParseSucceeded(settings0String);

        CascadiaSettings settings;
        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(6u, settings._profiles.size());
        VERIFY_ARE_EQUAL(2u, settings._globals._colorSchemes.size());

        auto terminalSettings0 = settings._profiles[0].CreateTerminalSettings(settings._globals._colorSchemes);
        auto terminalSettings1 = settings._profiles[1].CreateTerminalSettings(settings._globals._colorSchemes);
        auto terminalSettings2 = settings._profiles[2].CreateTerminalSettings(settings._globals._colorSchemes);
        auto terminalSettings3 = settings._profiles[3].CreateTerminalSettings(settings._globals._colorSchemes);
        auto terminalSettings4 = settings._profiles[4].CreateTerminalSettings(settings._globals._colorSchemes);
        auto terminalSettings5 = settings._profiles[5].CreateTerminalSettings(settings._globals._colorSchemes);

        VERIFY_ARE_EQUAL(ARGB(0, 0x12, 0x34, 0x56), terminalSettings0.CursorColor()); // from color scheme
        VERIFY_ARE_EQUAL(DEFAULT_CURSOR_COLOR, terminalSettings1.CursorColor()); // default
        VERIFY_ARE_EQUAL(ARGB(0, 0x23, 0x45, 0x67), terminalSettings2.CursorColor()); // from profile (trumps color scheme)
        VERIFY_ARE_EQUAL(ARGB(0, 0x34, 0x56, 0x78), terminalSettings3.CursorColor()); // from profile (not set in color scheme)
        VERIFY_ARE_EQUAL(ARGB(0, 0x45, 0x67, 0x89), terminalSettings4.CursorColor()); // from profile (no color scheme)
        VERIFY_ARE_EQUAL(DEFAULT_CURSOR_COLOR, terminalSettings5.CursorColor()); // default
    }

    void SettingsTests::ValidateKeybindingsWarnings()
    {
        const std::string badSettings{ R"(
        {
            "defaultProfile": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
                }
            ],
            "keybindings": [
                { "command": { "action": "splitPane", "split":"auto" }, "keys": [ "ctrl+alt+t", "ctrl+a" ] },
                { "command": { "action": "moveFocus" }, "keys": [ "ctrl+a" ] },
                { "command": { "action": "resizePane" }, "keys": [ "ctrl+b" ] }
            ]
        })" };

        const auto settingsObject = VerifyParseSucceeded(badSettings);
        auto settings = CascadiaSettings::FromJson(settingsObject);

        VERIFY_ARE_EQUAL(0u, settings->_globals._keybindings->_keyShortcuts.size());

        VERIFY_ARE_EQUAL(3u, settings->_globals._keybindingsWarnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::TooManyKeysForChord, settings->_globals._keybindingsWarnings.at(0));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_globals._keybindingsWarnings.at(1));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_globals._keybindingsWarnings.at(2));

        settings->_ValidateKeybindings();

        VERIFY_ARE_EQUAL(4u, settings->_warnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::AtLeastOneKeybindingWarning, settings->_warnings.at(0));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::TooManyKeysForChord, settings->_warnings.at(1));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.at(2));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.at(3));
    }

    void SettingsTests::ValidateLegacyGlobalsWarning()
    {
        const std::string badSettings{ R"(
        {
            "globals": {},
            "defaultProfile": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
                }
            ],
            "keybindings": []
        })" };

        // Create the default settings
        CascadiaSettings settings;
        settings._ParseJsonString(DefaultJson, true);
        settings.LayerJson(settings._defaultSettings);

        settings._ValidateNoGlobalsKey();
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());

        // Now layer on the user's settings
        settings._ParseJsonString(badSettings, false);
        settings.LayerJson(settings._userSettings);

        settings._ValidateNoGlobalsKey();
        VERIFY_ARE_EQUAL(1u, settings._warnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::LegacyGlobalsProperty, settings._warnings.at(0));
    }

    void SettingsTests::TestTrailingCommas()
    {
        const std::string badSettings{ R"(
        {
            "defaultProfile": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
                },
            ],
            "keybindings": [],
        })" };

        // Create the default settings
        CascadiaSettings settings;
        settings._ParseJsonString(DefaultJson, true);
        settings.LayerJson(settings._defaultSettings);

        // Now layer on the user's settings
        try
        {
            settings._ParseJsonString(badSettings, false);
            settings.LayerJson(settings._userSettings);
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to LayerJson should succeed, even with the trailing comma");
        }
    }
}
