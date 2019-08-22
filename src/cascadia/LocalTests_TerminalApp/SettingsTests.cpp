// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "JsonTestClass.h"

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
        // If you want to do anything XAML-y, you'll need to run yor test in a
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

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }
    };

    void SettingsTests::TryCreateWinRTType()
    {
        winrt::Microsoft::Terminal::Settings::TerminalSettings settings{};
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

        {
            // Case 1: Good settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids"));
            const auto settingsObject = VerifyParseSucceeded(goodProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ValidateNoDuplicateProfiles();
            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings->_warnings.size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_profiles.size());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with the same guid"));
            const auto settingsObject = VerifyParseSucceeded(badProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);

            settings->_ValidateNoDuplicateProfiles();

            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.size());
            VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::DuplicateProfile, settings->_warnings.at(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_profiles.size());
            VERIFY_ARE_EQUAL(L"profile0", settings->_profiles.at(0).GetName());
        }
        {
            // Case 3: Very bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a set of profiles, many of which with duplicated guids"));
            const auto settingsObject = VerifyParseSucceeded(veryBadProfiles);
            auto settings = CascadiaSettings::FromJson(settingsObject);
            settings->_ValidateNoDuplicateProfiles();
            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.size());
            VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::DuplicateProfile, settings->_warnings.at(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(4), settings->_profiles.size());
            VERIFY_ARE_EQUAL(L"profile0", settings->_profiles.at(0).GetName());
            VERIFY_ARE_EQUAL(L"profile1", settings->_profiles.at(1).GetName());
            VERIFY_ARE_EQUAL(L"profile4", settings->_profiles.at(2).GetName());
            VERIFY_ARE_EQUAL(L"profile6", settings->_profiles.at(3).GetName());
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

        // Case 2: Bad settings
        Log::Comment(NoThrowString().Format(
            L"Testing a pair of profiles with the same guid"));
        const auto settingsObject = VerifyParseSucceeded(badProfiles);
        auto settings = CascadiaSettings::FromJson(settingsObject);

        settings->_ValidateSettings();

        VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_warnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::DuplicateProfile, settings->_warnings.at(0));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingDefaultProfile, settings->_warnings.at(1));

        VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_profiles.size());
        VERIFY_ARE_EQUAL(settings->_globals.GetDefaultProfile(), settings->_profiles.at(0).GetGuid());
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

        CascadiaSettings settings{};

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

            CascadiaSettings settings{};
            settings._LayerJsonString(defaultProfilesString, true);
            VERIFY_ARE_EQUAL(2, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1)._name);

            settings._LayerJsonString(userProfiles0String, false);
            VERIFY_ARE_EQUAL(2, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(1)._name);

            settings._ValidateProfilesMatchUserSettingsOrder();
            VERIFY_ARE_EQUAL(2, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1)._name);
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Case 2: Make sure all the user's profiles appear before the defaults."));

            CascadiaSettings settings{};
            settings._LayerJsonString(defaultProfilesString, true);
            VERIFY_ARE_EQUAL(2, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1)._name);

            settings._LayerJsonString(userProfiles1String, false);
            VERIFY_ARE_EQUAL(3, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(2)._name);

            settings._ValidateProfilesMatchUserSettingsOrder();
            VERIFY_ARE_EQUAL(3, settings._profiles.size());
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
            CascadiaSettings settings{};
            settings._LayerJsonString(defaultProfilesString, true);
            VERIFY_ARE_EQUAL(2, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(1)._hidden);

            settings._LayerJsonString(userProfiles0String, false);
            VERIFY_ARE_EQUAL(2, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(true, settings._profiles.at(1)._hidden);

            settings._ValidateProfilesMatchUserSettingsOrder();
            settings._ValidateRemoveHiddenProfiles();
            VERIFY_ARE_EQUAL(1, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
        }

        {
            CascadiaSettings settings{};
            settings._LayerJsonString(defaultProfilesString, true);
            VERIFY_ARE_EQUAL(2, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(1)._hidden);

            settings._LayerJsonString(userProfiles1String, false);
            VERIFY_ARE_EQUAL(4, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(2)._name);
            VERIFY_ARE_EQUAL(L"profile6", settings._profiles.at(3)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(true, settings._profiles.at(1)._hidden);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(2)._hidden);
            VERIFY_ARE_EQUAL(true, settings._profiles.at(3)._hidden);

            settings._ValidateProfilesMatchUserSettingsOrder();
            settings._ValidateRemoveHiddenProfiles();
            VERIFY_ARE_EQUAL(2, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0)._hidden);
            VERIFY_ARE_EQUAL(false, settings._profiles.at(1)._hidden);
        }
    }

}
