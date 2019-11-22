// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "JsonTestClass.h"
#include <defaults.h>

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppLocalTests
{
    // Unfortunately, these tests _WILL NOT_ work in our CI, until we have a lab
    // machine available that can run Windows version 18362.

    class SettingsTests : public JsonTestClass
    {
        // Use a custom manifest to ensure that we can activate winrt types from
        // our test. This property will tell taef to manually use this as the
        // sxs manifest during this test class. It includes all the cppwinrt
        // types we've defined, so if your test is crashing for an unknown
        // reason, make sure it's included in that file.
        // If you want to do anything XAML-y, you'll need to run your test in a
        // packaged context. See TabTests.cpp for more details on that.
        BEGIN_TEST_CLASS(SettingsTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.LocalTests.manifest")
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

        TEST_METHOD(TestLayerGlobalsOnRoot);

        TEST_METHOD(TestProfileIconWithEnvVar);
        TEST_METHOD(TestProfileBackgroundImageWithEnvVar);

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
            "globals": {
                "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            },
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
            "globals": {
                "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            },
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
            "globals": {
                "alwaysShowTabs": true
            },
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

        {
            // Case 1: Good settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, and the defaultProfile is one of those guids"));
            const auto settingsObject = VerifyParseSucceeded(goodProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings->_warnings.size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_profiles.size());
            VERIFY_ARE_EQUAL(settings->_globals.GetDefaultProfile(), settings->_profiles.at(0).GetGuid());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, but the defaultProfile is NOT one of those guids"));
            const auto settingsObject = VerifyParseSucceeded(badProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.size());
            VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingDefaultProfile, settings->_warnings.at(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_profiles.size());
            VERIFY_ARE_EQUAL(settings->_globals.GetDefaultProfile(), settings->_profiles.at(0).GetGuid());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, and no defaultProfile at all"));
            const auto settingsObject = VerifyParseSucceeded(badProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.size());
            VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingDefaultProfile, settings->_warnings.at(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_profiles.size());
            VERIFY_ARE_EQUAL(settings->_globals.GetDefaultProfile(), settings->_profiles.at(0).GetGuid());
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
            "globals": {
                "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            },
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
        VERIFY_ARE_EQUAL(settings->_globals.GetDefaultProfile(), settings->_profiles.at(0).GetGuid());
        VERIFY_IS_TRUE(settings->_profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings->_profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings->_profiles.at(2)._guid.has_value());
    }

    void SettingsTests::LayerGlobalProperties()
    {
        const std::string settings0String{ R"(
        {
            "globals": {
                "alwaysShowTabs": true,
                "initialCols" : 120,
                "initialRows" : 30
            }
        })" };
        const std::string settings1String{ R"(
        {
            "globals": {
                "showTabsInTitlebar": false,
                "initialCols" : 240,
                "initialRows" : 60
            }
        })" };
        const auto settings0Json = VerifyParseSucceeded(settings0String);
        const auto settings1Json = VerifyParseSucceeded(settings1String);

        CascadiaSettings settings;

        settings.LayerJson(settings0Json);
        VERIFY_ARE_EQUAL(true, settings._globals._alwaysShowTabs);
        VERIFY_ARE_EQUAL(120, settings._globals._initialCols);
        VERIFY_ARE_EQUAL(30, settings._globals._initialRows);
        VERIFY_ARE_EQUAL(true, settings._globals._showTabsInTitlebar);

        settings.LayerJson(settings1Json);
        VERIFY_ARE_EQUAL(true, settings._globals._alwaysShowTabs);
        VERIFY_ARE_EQUAL(240, settings._globals._initialCols);
        VERIFY_ARE_EQUAL(60, settings._globals._initialRows);
        VERIFY_ARE_EQUAL(false, settings._globals._showTabsInTitlebar);
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

        const auto serialized0Profile = profile0.ToJson();
        const auto profile1 = Profile::FromJson(serialized0Profile);
        VERIFY_IS_FALSE(profile0._guid.has_value());
        VERIFY_ARE_EQUAL(profile1._guid.has_value(), profile0._guid.has_value());

        CascadiaSettings settings;
        settings._profiles.emplace_back(profile1);
        settings._ValidateProfilesHaveGuid();

        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());

        const auto serialized1Profile = settings._profiles.at(0).ToJson();

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
        VERIFY_ARE_EQUAL(L"cmd", settings._profiles.at(1)._name);

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
        VERIFY_ARE_EQUAL(L"cmd", settings._profiles.at(1)._name);

        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._guid.has_value());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"cmd", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotCrash", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"Ubuntu", settings._profiles.at(3)._name);

        settings._ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._guid.has_value());
        VERIFY_ARE_EQUAL(L"cmd", settings._profiles.at(0)._name);
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
        VERIFY_ARE_EQUAL(L"cmd", settings._profiles.at(1)._name);

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
        VERIFY_ARE_EQUAL(L"cmd", settings._profiles.at(1)._name);
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
        VERIFY_ARE_EQUAL(L"cmd", settings._profiles.at(1)._name);

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
        VERIFY_ARE_EQUAL(L"cmd", settings._profiles.at(1)._name);
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
            VERIFY_ARE_EQUAL(L"cmd", settings2._profiles.at(1)._name);
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
        VERIFY_ARE_EQUAL(L"cmd", settings._profiles.at(4)._name);
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

    void SettingsTests::TestLayerGlobalsOnRoot()
    {
        // Test for microsoft/terminal#2906. We added the ability for the root
        // to be used as the globals object in #2515. However, if you have a
        // globals object, then the settings in the root would get ignored.
        // This test ensures that settings from a child "globals" element
        // _layer_ on top of root properties, and they don't cause the root
        // properties to be totally ignored.

        const std::string settings0String{ R"(
        {
            "globals": {
                "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                "initialRows": 123
            }
        })" };
        const std::string settings1String{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "initialRows": 234
        })" };
        const std::string settings2String{ R"(
        {
            "defaultProfile": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
            "initialRows": 345,
            "globals": {
                "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                // initialRows should not be cleared here
            }
        })" };
        const std::string settings3String{ R"(
        {
            "defaultProfile": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
            "globals": {
                "initialRows": 456
                // defaultProfile should not be cleared here
            }
        })" };
        const std::string settings4String{ R"(
        {
            "defaultProfile": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
            "globals": {
                "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            },
            "defaultProfile": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string settings5String{ R"(
        {
            "globals": {
                "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            },
            "defaultProfile": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
            "globals": {
                "defaultProfile": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
            }
        })" };

        VerifyParseSucceeded(settings0String);
        VerifyParseSucceeded(settings1String);
        VerifyParseSucceeded(settings2String);
        VerifyParseSucceeded(settings3String);
        VerifyParseSucceeded(settings4String);
        VerifyParseSucceeded(settings5String);
        const auto guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        {
            CascadiaSettings settings;
            settings._ParseJsonString(settings0String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(guid1, settings._globals._defaultProfile);
            VERIFY_ARE_EQUAL(123, settings._globals._initialRows);
        }
        {
            CascadiaSettings settings;
            settings._ParseJsonString(settings1String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(guid1, settings._globals._defaultProfile);
            VERIFY_ARE_EQUAL(234, settings._globals._initialRows);
        }
        {
            CascadiaSettings settings;
            settings._ParseJsonString(settings2String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(guid1, settings._globals._defaultProfile);
            VERIFY_ARE_EQUAL(345, settings._globals._initialRows);
        }
        {
            CascadiaSettings settings;
            settings._ParseJsonString(settings3String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(guid2, settings._globals._defaultProfile);
            VERIFY_ARE_EQUAL(456, settings._globals._initialRows);
        }
        {
            CascadiaSettings settings;
            settings._ParseJsonString(settings4String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(guid1, settings._globals._defaultProfile);
        }
        {
            CascadiaSettings settings;
            settings._ParseJsonString(settings5String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(guid3, settings._globals._defaultProfile);
        }
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
        VERIFY_IS_FALSE(settings._profiles.empty(), 0);
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
        VERIFY_IS_FALSE(settings._profiles.empty(), 0);

        GlobalAppSettings globalSettings{};
        auto terminalSettings = settings._profiles[0].CreateTerminalSettings(globalSettings.GetColorSchemes());
        VERIFY_ARE_EQUAL(expectedPath, terminalSettings.BackgroundImage());
    }
}
