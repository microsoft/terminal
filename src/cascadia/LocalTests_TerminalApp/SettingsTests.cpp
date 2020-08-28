// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "../TerminalApp/TerminalPage.h"
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
using namespace winrt::Microsoft::Terminal::TerminalControl;

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

        TEST_METHOD(ValidateExecuteCommandlineWarning);

        TEST_METHOD(ValidateLegacyGlobalsWarning);

        TEST_METHOD(TestTrailingCommas);

        TEST_METHOD(TestCommandsAndKeybindings);

        TEST_METHOD(TestIterateCommands);
        TEST_METHOD(TestIterateOnGeneratedNamedCommands);
        TEST_METHOD(TestIterateOnBadJson);
        TEST_METHOD(TestNestedCommands);
        TEST_METHOD(TestNestedInNestedCommand);
        TEST_METHOD(TestNestedInIterableCommand);
        TEST_METHOD(TestIterableInNestedCommand);
        TEST_METHOD(TestMixedNestedAndIterableCommand);
        TEST_METHOD(TestNestedCommandWithoutName);
        TEST_METHOD(TestUnbindNestedCommand);
        TEST_METHOD(TestRebindNestedCommand);

        TEST_METHOD(TestIterableColorSchemeCommands);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }

    private:
        void _logCommandNames(winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::TerminalApp::Command> commands, const int indentation = 1)
        {
            if (indentation == 1)
            {
                Log::Comment((commands.Size() == 0) ? L"Commands:\n  <none>" : L"Commands:");
            }
            for (const auto& nameAndCommand : commands)
            {
                Log::Comment(fmt::format(L"{0:>{1}}* {2}->{3}",
                                         L"",
                                         indentation,
                                         nameAndCommand.Key(),
                                         nameAndCommand.Value().Name())
                                 .c_str());

                winrt::com_ptr<implementation::Command> cmdImpl;
                cmdImpl.copy_from(winrt::get_self<implementation::Command>(nameAndCommand.Value()));
                if (cmdImpl->HasNestedCommands())
                {
                    _logCommandNames(cmdImpl->_subcommands.GetView(), indentation + 2);
                }
            }
        }
    };

    void SettingsTests::TryCreateWinRTType()
    {
        TerminalSettings settings;
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
            VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_profiles.at(0).Guid());
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
            VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_profiles.at(0).Guid());
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
            VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_profiles.at(0).Guid());
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
            VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_profiles.at(1).Guid());
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
        Profile profile0 = winrt::make<implementation::Profile>(::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}"));
        profile0.Name(L"profile0");
        Profile profile1 = winrt::make<implementation::Profile>(::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-5555-49a3-80bd-e8fdd045185c}"));
        profile1.Name(L"profile1");
        Profile profile2 = winrt::make<implementation::Profile>(::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}"));
        profile2.Name(L"profile2");
        Profile profile3 = winrt::make<implementation::Profile>(::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}"));
        profile3.Name(L"profile3");
        Profile profile4 = winrt::make<implementation::Profile>(::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-6666-49a3-80bd-e8fdd045185c}"));
        profile4.Name(L"profile4");
        Profile profile5 = winrt::make<implementation::Profile>(::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-5555-49a3-80bd-e8fdd045185c}"));
        profile5.Name(L"profile5");
        Profile profile6 = winrt::make<implementation::Profile>(::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-7777-49a3-80bd-e8fdd045185c}"));
        profile6.Name(L"profile6");

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
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0).Name());
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
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1).Name());
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(2).Name());
            VERIFY_ARE_EQUAL(L"profile6", settings._profiles.at(3).Name());
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
        Profile profile4 = winrt::make<implementation::Profile>(::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}"));
        profile4.Name(L"profile4");
        Profile profile5 = winrt::make<implementation::Profile>(::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}"));
        profile5.Name(L"profile5");

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
        VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_profiles.at(0).Guid());
        VERIFY_IS_TRUE(settings->_profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings->_profiles.at(1).HasGuid());
        VERIFY_IS_TRUE(settings->_profiles.at(2).HasGuid());
    }

    void SettingsTests::LayerGlobalProperties()
    {
        const std::string settings0String{ R"(
        {
            "alwaysShowTabs": true,
            "initialCols" : 120,
            "initialRows" : 30
        })" };
        const std::string settings1String{ R"(
        {
            "showTabsInTitlebar": false,
            "initialCols" : 240,
            "initialRows" : 60
        })" };
        const auto settings0Json = VerifyParseSucceeded(settings0String);
        const auto settings1Json = VerifyParseSucceeded(settings1String);

        CascadiaSettings settings;

        settings.LayerJson(settings0Json);
        VERIFY_ARE_EQUAL(true, settings._globals->AlwaysShowTabs());
        VERIFY_ARE_EQUAL(120, settings._globals->InitialCols());
        VERIFY_ARE_EQUAL(30, settings._globals->InitialRows());
        VERIFY_ARE_EQUAL(true, settings._globals->ShowTabsInTitlebar());

        settings.LayerJson(settings1Json);
        VERIFY_ARE_EQUAL(true, settings._globals->AlwaysShowTabs());
        VERIFY_ARE_EQUAL(240, settings._globals->InitialCols());
        VERIFY_ARE_EQUAL(60, settings._globals->InitialRows());
        VERIFY_ARE_EQUAL(false, settings._globals->ShowTabsInTitlebar());
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
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1).Name());

            settings._ParseJsonString(userProfiles0String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(1).Name());

            settings._ReorderProfilesToMatchUserSettingsOrder();
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1).Name());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Case 2: Make sure all the user's profiles appear before the defaults."));

            CascadiaSettings settings;
            settings._ParseJsonString(defaultProfilesString, true);
            settings.LayerJson(settings._defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1).Name());

            settings._ParseJsonString(userProfiles1String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(3u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(1).Name());
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(2).Name());

            settings._ReorderProfilesToMatchUserSettingsOrder();
            VERIFY_ARE_EQUAL(3u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(1).Name());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(2).Name());
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
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1).Name());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0).Hidden());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(1).Hidden());

            settings._ParseJsonString(userProfiles0String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(1).Name());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0).Hidden());
            VERIFY_ARE_EQUAL(true, settings._profiles.at(1).Hidden());

            settings._ReorderProfilesToMatchUserSettingsOrder();
            settings._RemoveHiddenProfiles();
            VERIFY_ARE_EQUAL(1u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0).Hidden());
        }

        {
            CascadiaSettings settings;
            settings._ParseJsonString(defaultProfilesString, true);
            settings.LayerJson(settings._defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(1).Name());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0).Hidden());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(1).Hidden());

            settings._ParseJsonString(userProfiles1String, false);
            settings.LayerJson(settings._userSettings);
            VERIFY_ARE_EQUAL(4u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(1).Name());
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(2).Name());
            VERIFY_ARE_EQUAL(L"profile6", settings._profiles.at(3).Name());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0).Hidden());
            VERIFY_ARE_EQUAL(true, settings._profiles.at(1).Hidden());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(2).Hidden());
            VERIFY_ARE_EQUAL(true, settings._profiles.at(3).Hidden());

            settings._ReorderProfilesToMatchUserSettingsOrder();
            settings._RemoveHiddenProfiles();
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_ARE_EQUAL(L"profile5", settings._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(1).Name());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(0).Hidden());
            VERIFY_ARE_EQUAL(false, settings._profiles.at(1).Hidden());
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

        const auto profile0 = implementation::Profile::FromJson(profile0Json);
        const auto profile1 = implementation::Profile::FromJson(profile1Json);
        const auto profile2 = implementation::Profile::FromJson(profile2Json);
        const auto profile3 = implementation::Profile::FromJson(profile3Json);
        const auto profile4 = implementation::Profile::FromJson(profile4Json);
        const auto profile5 = implementation::Profile::FromJson(profile5Json);

        const winrt::guid cmdGuid{ Utils::GuidFromString(L"{6239a42c-1de4-49a3-80bd-e8fdd045185c}") };
        const winrt::guid nullGuid{};

        VERIFY_IS_FALSE(profile0->HasGuid());
        VERIFY_IS_FALSE(profile1->HasGuid());
        VERIFY_IS_FALSE(profile2->HasGuid());
        VERIFY_IS_TRUE(profile3->HasGuid());
        VERIFY_IS_TRUE(profile4->HasGuid());
        VERIFY_IS_FALSE(profile5->HasGuid());

        VERIFY_ARE_EQUAL(profile3->Guid(), nullGuid);
        VERIFY_ARE_EQUAL(profile4->Guid(), cmdGuid);

        CascadiaSettings settings;
        settings._profiles.emplace_back(profile0.as<Profile>());
        settings._profiles.emplace_back(profile1.as<Profile>());
        settings._profiles.emplace_back(profile2.as<Profile>());
        settings._profiles.emplace_back(profile3.as<Profile>());
        settings._profiles.emplace_back(profile4.as<Profile>());
        settings._profiles.emplace_back(profile5.as<Profile>());

        settings._ValidateProfilesHaveGuid();
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(2).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(3).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(4).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(5).HasGuid());

        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(0).Guid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(1).Guid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(2).Guid(), nullGuid);
        VERIFY_ARE_EQUAL(settings._profiles.at(3).Guid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(4).Guid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(5).Guid(), nullGuid);

        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(0).Guid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(1).Guid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(2).Guid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(3).Guid(), cmdGuid);
        VERIFY_ARE_EQUAL(settings._profiles.at(4).Guid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(5).Guid(), cmdGuid);

        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(0).Guid(), settings._profiles.at(2).Guid());
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(1).Guid(), settings._profiles.at(2).Guid());
        VERIFY_ARE_EQUAL(settings._profiles.at(2).Guid(), settings._profiles.at(2).Guid());
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(3).Guid(), settings._profiles.at(2).Guid());
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(4).Guid(), settings._profiles.at(2).Guid());
        VERIFY_ARE_EQUAL(settings._profiles.at(5).Guid(), settings._profiles.at(2).Guid());
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

        const auto profile0 = implementation::Profile::FromJson(profile0Json);
        const GUID nullGuid{ 0 };

        VERIFY_IS_FALSE(profile0->HasGuid());

        const auto serialized0Profile = profile0->GenerateStub();
        const auto profile1 = implementation::Profile::FromJson(serialized0Profile);
        VERIFY_IS_FALSE(profile0->HasGuid());
        VERIFY_IS_FALSE(profile1->HasGuid());

        CascadiaSettings settings;
        settings._profiles.emplace_back(profile1.as<Profile>());
        settings._ValidateProfilesHaveGuid();

        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());

        const auto profileImpl = winrt::get_self<implementation::Profile>(settings._profiles.at(0));
        const auto serialized1Profile = profileImpl->GenerateStub();

        const auto profile2 = implementation::Profile::FromJson(serialized1Profile);
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(profile2->HasGuid());
        VERIFY_ARE_EQUAL(settings._profiles.at(0).Guid(), profile2->Guid());
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
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_FALSE(settings._profiles.at(1).HasGuid());

        settings._ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(2u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
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
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1).Name());

        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(2).HasGuid());
        VERIFY_IS_FALSE(settings._profiles.at(3).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"cmdFromUserSettings", settings._profiles.at(1).Name());
        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(2).Name());
        VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(3).Name());

        settings._ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(2).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(3).HasGuid());
        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1).Name());
        VERIFY_ARE_EQUAL(L"cmdFromUserSettings", settings._profiles.at(2).Name());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(3).Name());
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
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1).Name());

        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_IS_FALSE(settings._profiles.at(2).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(3).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotCrash", settings._profiles.at(2).Name());
        VERIFY_ARE_EQUAL(L"Ubuntu", settings._profiles.at(3).Name());

        settings._ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(2).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(3).HasGuid());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotCrash", settings._profiles.at(1).Name());
        VERIFY_ARE_EQUAL(L"Ubuntu", settings._profiles.at(2).Name());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(3).Name());
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
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1).Name());

        Log::Comment(NoThrowString().Format(
            L"Parse the user settings"));
        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(5u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(2).HasGuid());
        VERIFY_IS_FALSE(settings._profiles.at(3).HasGuid());
        VERIFY_IS_FALSE(settings._profiles.at(4).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings._profiles.at(2).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotLayer", settings._profiles.at(3).Name());
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings._profiles.at(4).Name());
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
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1).Name());

        Log::Comment(NoThrowString().Format(
            L"Parse the user settings"));
        settings._ParseJsonString(settings0String, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(5u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(2).HasGuid());
        VERIFY_IS_FALSE(settings._profiles.at(3).HasGuid());
        VERIFY_IS_FALSE(settings._profiles.at(4).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(1).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings._profiles.at(2).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotDuplicate", settings._profiles.at(3).Name());
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings._profiles.at(4).Name());

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
            VERIFY_IS_TRUE(settings2._profiles.at(0).HasGuid());
            VERIFY_IS_TRUE(settings2._profiles.at(1).HasGuid());
            VERIFY_IS_TRUE(settings2._profiles.at(2).HasGuid());
            VERIFY_IS_FALSE(settings2._profiles.at(3).HasGuid());
            VERIFY_IS_FALSE(settings2._profiles.at(4).HasGuid());
            VERIFY_ARE_EQUAL(L"Windows PowerShell", settings2._profiles.at(0).Name());
            VERIFY_ARE_EQUAL(L"Command Prompt", settings2._profiles.at(1).Name());
            VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings2._profiles.at(2).Name());
            VERIFY_ARE_EQUAL(L"ThisProfileShouldNotDuplicate", settings2._profiles.at(3).Name());
            VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings2._profiles.at(4).Name());
        }

        Log::Comment(NoThrowString().Format(
            L"Validate the settings. All the profiles we have should be valid."));
        settings._ValidateSettings();

        VERIFY_ARE_EQUAL(5u, settings._profiles.size());
        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(2).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(3).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(4).HasGuid());
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotDuplicate", settings._profiles.at(1).Name());
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings._profiles.at(2).Name());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings._profiles.at(3).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings._profiles.at(4).Name());
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
        VERIFY_ARE_EQUAL(2u, settings._globals->GetColorSchemes().Size());

        VERIFY_ARE_EQUAL(L"schemeOne", settings._profiles.at(0).ColorSchemeName());
        VERIFY_ARE_EQUAL(L"InvalidSchemeName", settings._profiles.at(1).ColorSchemeName());
        VERIFY_ARE_EQUAL(L"Campbell", settings._profiles.at(2).ColorSchemeName());

        settings._ValidateAllSchemesExist();

        VERIFY_ARE_EQUAL(1u, settings._warnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::UnknownColorScheme, settings._warnings.at(0));

        VERIFY_ARE_EQUAL(3u, settings._profiles.size());
        VERIFY_ARE_EQUAL(2u, settings._globals->GetColorSchemes().Size());

        VERIFY_ARE_EQUAL(L"schemeOne", settings._profiles.at(0).ColorSchemeName());
        VERIFY_ARE_EQUAL(L"Campbell", settings._profiles.at(1).ColorSchemeName());
        VERIFY_ARE_EQUAL(L"Campbell", settings._profiles.at(2).ColorSchemeName());
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

        const winrt::guid guid0{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-5555-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid1{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-6666-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid2{ ::Microsoft::Console::Utils::GuidFromString(L"{2C4DE342-38B7-51CF-B940-2309A097F518}") };
        const winrt::guid fakeGuid{ ::Microsoft::Console::Utils::GuidFromString(L"{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}") };
        const std::optional<winrt::guid> badGuid{};

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

        VERIFY_ARE_EQUAL(name0, prof0.Name());
        VERIFY_ARE_EQUAL(name1, prof1.Name());
        VERIFY_ARE_EQUAL(name2, prof2.Name());
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

        auto globalSettings{ winrt::make<implementation::GlobalAppSettings>() };
        const auto profileImpl = winrt::get_self<implementation::Profile>(settings._profiles[0]);
        auto terminalSettings = profileImpl->CreateTerminalSettings(globalSettings.GetColorSchemes());
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
                }
            ]
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings._profiles[0].CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Always, settings._profiles[1].CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Never, settings._profiles[2].CloseOnExit());

        // Unknown modes parse as "Graceful"
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings._profiles[3].CloseOnExit());
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
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings._profiles[0].CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Never, settings._profiles[1].CloseOnExit());
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

            VERIFY_ARE_EQUAL(guid1String, settings._globals->UnparsedDefaultProfile());
            VERIFY_ARE_EQUAL(2u, settings._profiles.size());

            VERIFY_ARE_EQUAL(2345, settings._profiles.at(0).HistorySize());
            VERIFY_ARE_EQUAL(1234, settings._profiles.at(1).HistorySize());
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
        const winrt::guid guid1{ ::Microsoft::Console::Utils::GuidFromString(guid1String) };
        const winrt::guid guid2{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}") };

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
            VERIFY_ARE_NOT_EQUAL(guid2, settings._profiles.at(0).Guid());
            VERIFY_ARE_NOT_EQUAL(guid2, settings._profiles.at(1).Guid());

            settings.LayerJson(settings._userSettings);

            VERIFY_ARE_EQUAL(guid1String, settings._globals->UnparsedDefaultProfile());
            VERIFY_ARE_EQUAL(4u, settings._profiles.size());

            VERIFY_ARE_EQUAL(guid1, settings._profiles.at(2).Guid());
            VERIFY_IS_FALSE(settings._profiles.at(3).HasGuid());
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

        const winrt::guid guid1{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid2{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid3{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}") };

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
            Profile p0 = winrt::make<implementation::Profile>(guid1);
            p0.Name(L"profile0"); // this is _profiles.at(0)
            p0.HistorySize(1111);
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TerminalAppUnitTests::TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = [guid1, guid2]() {
            std::vector<Profile> profiles;
            Profile p0 = winrt::make<implementation::Profile>(guid1);
            Profile p1 = winrt::make<implementation::Profile>(guid2);
            p0.Name(L"profile0"); // this is _profiles.at(1)
            p1.Name(L"profile1"); // this is _profiles.at(2)
            p0.HistorySize(2222);
            profiles.push_back(p0);
            p1.HistorySize(3333);
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

        VERIFY_ARE_EQUAL(1111, settings._profiles.at(0).HistorySize());
        VERIFY_ARE_EQUAL(2222, settings._profiles.at(1).HistorySize());
        VERIFY_ARE_EQUAL(3333, settings._profiles.at(2).HistorySize());

        settings._ApplyDefaultsFromUserSettings();

        VERIFY_ARE_EQUAL(1234, settings._profiles.at(0).HistorySize());
        VERIFY_ARE_EQUAL(1234, settings._profiles.at(1).HistorySize());
        VERIFY_ARE_EQUAL(1234, settings._profiles.at(2).HistorySize());

        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(4u, settings._profiles.size());

        VERIFY_IS_FALSE(settings._profiles.at(0).Source().empty());
        VERIFY_IS_FALSE(settings._profiles.at(1).Source().empty());
        VERIFY_IS_FALSE(settings._profiles.at(2).Source().empty());
        VERIFY_IS_TRUE(settings._profiles.at(3).Source().empty());

        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.0", settings._profiles.at(0).Source());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(1).Source());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(2).Source());

        VERIFY_IS_TRUE(settings._profiles.at(0).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(1).HasGuid());
        VERIFY_IS_TRUE(settings._profiles.at(2).HasGuid());

        VERIFY_ARE_EQUAL(guid1, settings._profiles.at(0).Guid());
        VERIFY_ARE_EQUAL(guid1, settings._profiles.at(1).Guid());
        VERIFY_ARE_EQUAL(guid2, settings._profiles.at(2).Guid());

        VERIFY_ARE_EQUAL(L"profile0FromUserSettings", settings._profiles.at(0).Name());
        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(1).Name());
        VERIFY_ARE_EQUAL(L"profile1FromUserSettings", settings._profiles.at(2).Name());
        VERIFY_ARE_EQUAL(L"profile2FromUserSettings", settings._profiles.at(3).Name());

        Log::Comment(NoThrowString().Format(
            L"This is the real meat of the test: The two dynamic profiles that "
            L"_didn't_ have historySize set in the userSettings should have "
            L"1234 as their historySize(from the defaultSettings).The other two"
            L" profiles should have their custom historySize value."));

        VERIFY_ARE_EQUAL(1234, settings._profiles.at(0).HistorySize());
        VERIFY_ARE_EQUAL(1234, settings._profiles.at(1).HistorySize());
        VERIFY_ARE_EQUAL(4444, settings._profiles.at(2).HistorySize());
        VERIFY_ARE_EQUAL(5555, settings._profiles.at(3).HistorySize());
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

        const winrt::guid guid0{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid1{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}") };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);
        settings._ValidateSettings();

        auto appKeyBindingsProj = settings._globals->GetKeybindings();
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        const auto profile2Guid = settings._profiles.at(2).Guid();
        VERIFY_ARE_NOT_EQUAL(winrt::guid{}, profile2Guid);

        const auto appKeyBindings = winrt::get_self<implementation::AppKeyBindings>(appKeyBindingsProj);
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

        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        const auto profile1 = settings->FindProfile(guid1);
        const auto profile2 = settings->FindProfile(guid2);
        const auto profile3 = settings->FindProfile(guid3);

        VERIFY_IS_NOT_NULL(profile1);
        VERIFY_IS_NOT_NULL(profile2);
        VERIFY_IS_NULL(profile3);

        VERIFY_ARE_EQUAL(L"profile0", profile1.Name());
        VERIFY_ARE_EQUAL(L"profile1", profile2.Name());
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

        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

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
        VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_profiles.at(0).Guid());
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
        VERIFY_ARE_EQUAL(2u, settings._globals->GetColorSchemes().Size());

        auto terminalSettings0 = winrt::get_self<implementation::Profile>(settings._profiles[0])->CreateTerminalSettings(settings._globals->GetColorSchemes());
        auto terminalSettings1 = winrt::get_self<implementation::Profile>(settings._profiles[1])->CreateTerminalSettings(settings._globals->GetColorSchemes());
        auto terminalSettings2 = winrt::get_self<implementation::Profile>(settings._profiles[2])->CreateTerminalSettings(settings._globals->GetColorSchemes());
        auto terminalSettings3 = winrt::get_self<implementation::Profile>(settings._profiles[3])->CreateTerminalSettings(settings._globals->GetColorSchemes());
        auto terminalSettings4 = winrt::get_self<implementation::Profile>(settings._profiles[4])->CreateTerminalSettings(settings._globals->GetColorSchemes());
        auto terminalSettings5 = winrt::get_self<implementation::Profile>(settings._profiles[5])->CreateTerminalSettings(settings._globals->GetColorSchemes());

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

        VERIFY_ARE_EQUAL(0u, settings->_globals->_keybindings->_keyShortcuts.size());

        VERIFY_ARE_EQUAL(3u, settings->_globals->_keybindingsWarnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::TooManyKeysForChord, settings->_globals->_keybindingsWarnings.at(0));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(1));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(2));

        settings->_ValidateKeybindings();

        VERIFY_ARE_EQUAL(4u, settings->_warnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::AtLeastOneKeybindingWarning, settings->_warnings.at(0));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::TooManyKeysForChord, settings->_warnings.at(1));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.at(2));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.at(3));
    }

    void SettingsTests::ValidateExecuteCommandlineWarning()
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
                { "name":null, "command": { "action": "wt" }, "keys": [ "ctrl+a" ] },
                { "name":null, "command": { "action": "wt", "commandline":"" }, "keys": [ "ctrl+b" ] },
                { "name":null, "command": { "action": "wt", "commandline":null }, "keys": [ "ctrl+c" ] }
            ]
        })" };

        const auto settingsObject = VerifyParseSucceeded(badSettings);

        auto settings = CascadiaSettings::FromJson(settingsObject);

        VERIFY_ARE_EQUAL(0u, settings->_globals->_keybindings->_keyShortcuts.size());

        for (const auto& warning : settings->_globals->_keybindingsWarnings)
        {
            Log::Comment(NoThrowString().Format(
                L"warning:%d", warning));
        }
        VERIFY_ARE_EQUAL(3u, settings->_globals->_keybindingsWarnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(0));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(1));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(2));

        settings->_ValidateKeybindings();

        VERIFY_ARE_EQUAL(4u, settings->_warnings.size());
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::AtLeastOneKeybindingWarning, settings->_warnings.at(0));
        VERIFY_ARE_EQUAL(::TerminalApp::SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.at(1));
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

    void SettingsTests::TestCommandsAndKeybindings()
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
            "actions": [
                { "keys": "ctrl+a",                   "command": { "action": "splitPane", "split": "vertical" } },
                {                   "name": "ctrl+b", "command": { "action": "splitPane", "split": "vertical" } },
                { "keys": "ctrl+c", "name": "ctrl+c", "command": { "action": "splitPane", "split": "vertical" } },
                { "keys": "ctrl+d",                   "command": { "action": "splitPane", "split": "vertical" } },
                { "keys": "ctrl+e",                   "command": { "action": "splitPane", "split": "horizontal" } },
                { "keys": "ctrl+f", "name":null,      "command": { "action": "splitPane", "split": "horizontal" } }
            ]
        })" };

        const auto guid0 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}");
        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);
        settings._ValidateSettings();

        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        const auto profile2Guid = settings._profiles.at(2).Guid();
        VERIFY_ARE_NOT_EQUAL(winrt::guid{}, profile2Guid);

        auto appKeyBindings = winrt::get_self<implementation::AppKeyBindings>(settings._globals->GetKeybindings());
        VERIFY_ARE_EQUAL(5u, appKeyBindings->_keyShortcuts.size());

        // A/D, B, C, E will be in the list of commands, for 4 total.
        // * A and D share the same name, so they'll only generate a single action.
        // * F's name is set manually to `null`
        auto commands = settings._globals->GetCommands();
        VERIFY_ARE_EQUAL(4u, commands.Size());

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
        }

        Log::Comment(L"Note that we're skipping ctrl+B, since that doesn't have `keys` set.");

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
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
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
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
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
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
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
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }

        Log::Comment(L"Now verify the commands");
        _logCommandNames(commands);
        {
            auto command = commands.Lookup(L"Split pane, split: vertical");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
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
        }
        {
            auto command = commands.Lookup(L"ctrl+b");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
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
        }
        {
            auto command = commands.Lookup(L"ctrl+c");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
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
        }
        {
            auto command = commands.Lookup(L"Split pane, split: horizontal");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Horizontal, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }
    }
    void SettingsTests::TestIterateCommands()
    {
        // For this test, put an iterable command with a given `name`,
        // containing a ${profile.name} to replace. When we expand it, it should
        // have created one command for each profile.

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
            "actions": [
                {
                    "name": "iterable command ${profile.name}",
                    "iterateOn": "profiles",
                    "command": { "action": "splitPane", "profile": "${profile.name}" }
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        const auto guid0 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}");
        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());

        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            auto command = commands.Lookup(L"iterable command ${profile.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${profile.name}", realArgs.TerminalArgs().Profile());
        }

        auto expandedCommands = implementation::TerminalPage::_ExpandCommands(commands, settings.GetProfiles(), settings._globals->GetColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        {
            auto command = expandedCommands.Lookup(L"iterable command profile0");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command profile1");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command profile2");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestIterateOnGeneratedNamedCommands()
    {
        // For this test, put an iterable command without a given `name` to
        // replace. When we expand it, it should still work.

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
            "actions": [
                {
                    "iterateOn": "profiles",
                    "command": { "action": "splitPane", "profile": "${profile.name}" }
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        const auto guid0 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}");
        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());

        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            auto command = commands.Lookup(L"Split pane, profile: ${profile.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${profile.name}", realArgs.TerminalArgs().Profile());
        }

        auto expandedCommands = implementation::TerminalPage::_ExpandCommands(commands, settings.GetProfiles(), settings._globals->GetColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        {
            auto command = expandedCommands.Lookup(L"Split pane, profile: profile0");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"Split pane, profile: profile1");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"Split pane, profile: profile2");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestIterateOnBadJson()
    {
        // For this test, put an iterable command with a profile name that would
        // cause bad json to be filled in. Something like a profile with a name
        // of "Foo\"", so the trailing '"' might break the json parsing.

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
                    "name": "profile1\"",
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
            "actions": [
                {
                    "name": "iterable command ${profile.name}",
                    "iterateOn": "profiles",
                    "command": { "action": "splitPane", "profile": "${profile.name}" }
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        const auto guid0 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}");
        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());

        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            auto command = commands.Lookup(L"iterable command ${profile.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${profile.name}", realArgs.TerminalArgs().Profile());
        }

        settings._ValidateSettings();
        auto expandedCommands = implementation::TerminalPage::_ExpandCommands(commands, settings.GetProfiles(), settings._globals->GetColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        {
            auto command = expandedCommands.Lookup(L"iterable command profile0");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command profile1\"");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1\"", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command profile2");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestNestedCommands()
    {
        // This test checks a nested command.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ Connect to ssh...
        //    ├─ first.com
        //    └─ second.com

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
            "actions": [
                {
                    "name": "Connect to ssh...",
                    "commands": [
                        {
                            "name": "first.com",
                            "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                        },
                        {
                            "name": "second.com",
                            "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                        }
                    ]
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        settings._ValidateSettings();
        auto expandedCommands = implementation::TerminalPage::_ExpandCommands(commands, settings.GetProfiles(), settings._globals->GetColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto rootCommandProj = expandedCommands.Lookup(L"Connect to ssh...");
        VERIFY_IS_NOT_NULL(rootCommandProj);
        auto rootActionAndArgs = rootCommandProj.Action();
        VERIFY_IS_NULL(rootActionAndArgs);

        winrt::com_ptr<implementation::Command> rootCommandImpl;
        rootCommandImpl.copy_from(winrt::get_self<implementation::Command>(rootCommandProj));

        VERIFY_ARE_EQUAL(2u, rootCommandImpl->_subcommands.Size());

        {
            winrt::hstring commandName{ L"first.com" };
            auto commandProj = rootCommandImpl->_subcommands.Lookup(commandName);
            VERIFY_IS_NOT_NULL(commandProj);
            auto actionAndArgs = commandProj.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);

            winrt::com_ptr<implementation::Command> commandImpl;
            commandImpl.copy_from(winrt::get_self<implementation::Command>(commandProj));

            VERIFY_IS_FALSE(commandImpl->HasNestedCommands());
        }
        {
            winrt::hstring commandName{ L"second.com" };
            auto commandProj = rootCommandImpl->_subcommands.Lookup(commandName);
            VERIFY_IS_NOT_NULL(commandProj);
            auto actionAndArgs = commandProj.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);

            winrt::com_ptr<implementation::Command> commandImpl;
            commandImpl.copy_from(winrt::get_self<implementation::Command>(commandProj));

            VERIFY_IS_FALSE(commandImpl->HasNestedCommands());
        }
    }

    void SettingsTests::TestNestedInNestedCommand()
    {
        // This test checks a nested command that includes nested commands.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ grandparent
        //    └─ parent
        //       ├─ child1
        //       └─ child2

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
            "actions": [
                {
                    "name": "grandparent",
                    "commands": [
                        {
                            "name": "parent",
                            "commands": [
                                {
                                    "name": "child1",
                                    "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                                },
                                {
                                    "name": "child2",
                                    "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                                }
                            ]
                        },
                    ]
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        settings._ValidateSettings();
        auto expandedCommands = implementation::TerminalPage::_ExpandCommands(commands, settings.GetProfiles(), settings._globals->GetColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto grandparentCommandProj = expandedCommands.Lookup(L"grandparent");
        VERIFY_IS_NOT_NULL(grandparentCommandProj);
        auto grandparentActionAndArgs = grandparentCommandProj.Action();
        VERIFY_IS_NULL(grandparentActionAndArgs);

        winrt::com_ptr<implementation::Command> grandparentCommandImpl;
        grandparentCommandImpl.copy_from(winrt::get_self<implementation::Command>(grandparentCommandProj));

        VERIFY_ARE_EQUAL(1u, grandparentCommandImpl->_subcommands.Size());

        winrt::hstring parentName{ L"parent" };
        auto parentProj = grandparentCommandImpl->_subcommands.Lookup(parentName);
        VERIFY_IS_NOT_NULL(parentProj);
        auto parentActionAndArgs = parentProj.Action();
        VERIFY_IS_NULL(parentActionAndArgs);

        winrt::com_ptr<implementation::Command> parentImpl;
        parentImpl.copy_from(winrt::get_self<implementation::Command>(parentProj));

        VERIFY_ARE_EQUAL(2u, parentImpl->_subcommands.Size());
        {
            winrt::hstring childName{ L"child1" };
            auto childProj = parentImpl->_subcommands.Lookup(childName);
            VERIFY_IS_NOT_NULL(childProj);
            auto childActionAndArgs = childProj.Action();
            VERIFY_IS_NOT_NULL(childActionAndArgs);

            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, childActionAndArgs.Action());
            const auto& realArgs = childActionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"ssh me@first.com", realArgs.TerminalArgs().Commandline());

            winrt::com_ptr<implementation::Command> childImpl;
            childImpl.copy_from(winrt::get_self<implementation::Command>(childProj));

            VERIFY_IS_FALSE(childImpl->HasNestedCommands());
        }
        {
            winrt::hstring childName{ L"child2" };
            auto childProj = parentImpl->_subcommands.Lookup(childName);
            VERIFY_IS_NOT_NULL(childProj);
            auto childActionAndArgs = childProj.Action();
            VERIFY_IS_NOT_NULL(childActionAndArgs);

            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, childActionAndArgs.Action());
            const auto& realArgs = childActionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"ssh me@second.com", realArgs.TerminalArgs().Commandline());

            winrt::com_ptr<implementation::Command> childImpl;
            childImpl.copy_from(winrt::get_self<implementation::Command>(childProj));

            VERIFY_IS_FALSE(childImpl->HasNestedCommands());
        }
    }

    void SettingsTests::TestNestedInIterableCommand()
    {
        // This test checks a iterable command that includes a nested command.
        // The commands should look like:
        //
        // <Command Palette>
        //  ├─ profile0...
        //  |  ├─ Split pane, profile: profile0
        //  |  ├─ Split pane, direction: vertical, profile: profile0
        //  |  └─ Split pane, direction: horizontal, profile: profile0
        //  ├─ profile1...
        //  |  ├─Split pane, profile: profile1
        //  |  ├─Split pane, direction: vertical, profile: profile1
        //  |  └─Split pane, direction: horizontal, profile: profile1
        //  └─ profile2...
        //     ├─ Split pane, profile: profile2
        //     ├─ Split pane, direction: vertical, profile: profile2
        //     └─ Split pane, direction: horizontal, profile: profile2

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
            "actions": [
                {
                    "iterateOn": "profiles",
                    "name": "${profile.name}...",
                    "commands": [
                        { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "auto" } },
                        { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "vertical" } },
                        { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "horizontal" } }
                    ]
                }
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        settings._ValidateSettings();
        auto expandedCommands = implementation::TerminalPage::_ExpandCommands(commands, settings.GetProfiles(), settings._globals->GetColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());

        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        for (auto name : std::vector<std::wstring>({ L"profile0", L"profile1", L"profile2" }))
        {
            winrt::hstring commandName{ name + L"..." };
            auto commandProj = expandedCommands.Lookup(commandName);
            VERIFY_IS_NOT_NULL(commandProj);
            auto actionAndArgs = commandProj.Action();
            VERIFY_IS_NULL(actionAndArgs);

            winrt::com_ptr<implementation::Command> commandImpl;
            commandImpl.copy_from(winrt::get_self<implementation::Command>(commandProj));

            VERIFY_IS_TRUE(commandImpl->HasNestedCommands());
            VERIFY_ARE_EQUAL(3u, commandImpl->_subcommands.Size());
            _logCommandNames(commandImpl->_subcommands.GetView());
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, profile: {}", name) };
                auto childCommandProj = commandImpl->_subcommands.Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommandProj);
                auto childActionAndArgs = childCommandProj.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                winrt::com_ptr<implementation::Command> childCommandImpl;
                childCommandImpl.copy_from(winrt::get_self<implementation::Command>(childCommandProj));

                VERIFY_IS_FALSE(childCommandImpl->HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: horizontal, profile: {}", name) };
                auto childCommandProj = commandImpl->_subcommands.Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommandProj);
                auto childActionAndArgs = childCommandProj.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Horizontal, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                winrt::com_ptr<implementation::Command> childCommandImpl;
                childCommandImpl.copy_from(winrt::get_self<implementation::Command>(childCommandProj));

                VERIFY_IS_FALSE(childCommandImpl->HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: vertical, profile: {}", name) };
                auto childCommandProj = commandImpl->_subcommands.Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommandProj);
                auto childActionAndArgs = childCommandProj.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                winrt::com_ptr<implementation::Command> childCommandImpl;
                childCommandImpl.copy_from(winrt::get_self<implementation::Command>(childCommandProj));

                VERIFY_IS_FALSE(childCommandImpl->HasNestedCommands());
            }
        }
    }

    void SettingsTests::TestIterableInNestedCommand()
    {
        // This test checks a nested command that includes an iterable command.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ New Tab With Profile...
        //    ├─ Profile 1
        //    ├─ Profile 2
        //    └─ Profile 3

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
            "actions": [
                {
                    "name": "New Tab With Profile...",
                    "commands": [
                        {
                            "iterateOn": "profiles",
                            "command": { "action": "newTab", "profile": "${profile.name}" }
                        }
                    ]
                }
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        settings._ValidateSettings();
        auto expandedCommands = implementation::TerminalPage::_ExpandCommands(commands, settings.GetProfiles(), settings._globals->GetColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto rootCommandProj = expandedCommands.Lookup(L"New Tab With Profile...");
        VERIFY_IS_NOT_NULL(rootCommandProj);
        auto rootActionAndArgs = rootCommandProj.Action();
        VERIFY_IS_NULL(rootActionAndArgs);

        winrt::com_ptr<implementation::Command> rootCommandImpl;
        rootCommandImpl.copy_from(winrt::get_self<implementation::Command>(rootCommandProj));

        VERIFY_ARE_EQUAL(3u, rootCommandImpl->_subcommands.Size());

        for (auto name : std::vector<std::wstring>({ L"profile0", L"profile1", L"profile2" }))
        {
            winrt::hstring commandName{ fmt::format(L"New tab, profile: {}", name) };
            auto commandProj = rootCommandImpl->_subcommands.Lookup(commandName);
            VERIFY_IS_NOT_NULL(commandProj);
            auto actionAndArgs = commandProj.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);

            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

            winrt::com_ptr<implementation::Command> commandImpl;
            commandImpl.copy_from(winrt::get_self<implementation::Command>(commandProj));

            VERIFY_IS_FALSE(commandImpl->HasNestedCommands());
        }
    }
    void SettingsTests::TestMixedNestedAndIterableCommand()
    {
        // This test checks a nested commands that includes an iterable command
        // that includes a nested command.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ New Pane...
        //    ├─ profile0...
        //    |  ├─ Split automatically
        //    |  ├─ Split vertically
        //    |  └─ Split horizontally
        //    ├─ profile1...
        //    |  ├─ Split automatically
        //    |  ├─ Split vertically
        //    |  └─ Split horizontally
        //    └─ profile2...
        //       ├─ Split automatically
        //       ├─ Split vertically
        //       └─ Split horizontally

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
            "actions": [
                {
                    "name": "New Pane...",
                    "commands": [
                        {
                            "iterateOn": "profiles",
                            "name": "${profile.name}...",
                            "commands": [
                                { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "auto" } },
                                { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "vertical" } },
                                { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "horizontal" } }
                            ]
                        }
                    ]
                }
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        settings._ValidateSettings();
        auto expandedCommands = implementation::TerminalPage::_ExpandCommands(commands, settings.GetProfiles(), settings._globals->GetColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto rootCommandProj = expandedCommands.Lookup(L"New Pane...");
        VERIFY_IS_NOT_NULL(rootCommandProj);
        auto rootActionAndArgs = rootCommandProj.Action();
        VERIFY_IS_NULL(rootActionAndArgs);

        winrt::com_ptr<implementation::Command> rootCommandImpl;
        rootCommandImpl.copy_from(winrt::get_self<implementation::Command>(rootCommandProj));

        VERIFY_ARE_EQUAL(3u, rootCommandImpl->_subcommands.Size());

        for (auto name : std::vector<std::wstring>({ L"profile0", L"profile1", L"profile2" }))
        {
            winrt::hstring commandName{ name + L"..." };
            auto commandProj = rootCommandImpl->_subcommands.Lookup(commandName);
            VERIFY_IS_NOT_NULL(commandProj);
            auto actionAndArgs = commandProj.Action();
            VERIFY_IS_NULL(actionAndArgs);

            winrt::com_ptr<implementation::Command> commandImpl;
            commandImpl.copy_from(winrt::get_self<implementation::Command>(commandProj));

            VERIFY_IS_TRUE(commandImpl->HasNestedCommands());
            VERIFY_ARE_EQUAL(3u, commandImpl->_subcommands.Size());

            _logCommandNames(commandImpl->_subcommands.GetView());
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, profile: {}", name) };
                auto childCommandProj = commandImpl->_subcommands.Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommandProj);
                auto childActionAndArgs = childCommandProj.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                winrt::com_ptr<implementation::Command> childCommandImpl;
                childCommandImpl.copy_from(winrt::get_self<implementation::Command>(childCommandProj));

                VERIFY_IS_FALSE(childCommandImpl->HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: horizontal, profile: {}", name) };
                auto childCommandProj = commandImpl->_subcommands.Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommandProj);
                auto childActionAndArgs = childCommandProj.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Horizontal, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                winrt::com_ptr<implementation::Command> childCommandImpl;
                childCommandImpl.copy_from(winrt::get_self<implementation::Command>(childCommandProj));

                VERIFY_IS_FALSE(childCommandImpl->HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: vertical, profile: {}", name) };
                auto childCommandProj = commandImpl->_subcommands.Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommandProj);
                auto childActionAndArgs = childCommandProj.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Vertical, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                winrt::com_ptr<implementation::Command> childCommandImpl;
                childCommandImpl.copy_from(winrt::get_self<implementation::Command>(childCommandProj));

                VERIFY_IS_FALSE(childCommandImpl->HasNestedCommands());
            }
        }
    }

    void SettingsTests::TestNestedCommandWithoutName()
    {
        // This test tests a nested command without a name specified. This type
        // of command should just be ignored, since we can't auto-generate names
        // for nested commands, they _must_ have names specified.

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
            "actions": [
                {
                    "commands": [
                        {
                            "name": "child1",
                            "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                        },
                        {
                            "name": "child2",
                            "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                        }
                    ]
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        settings._ValidateSettings();
        _logCommandNames(commands);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());

        // Because the "parent" command didn't have a name, it couldn't be
        // placed into the list of commands. It and it's children are just
        // ignored.
        VERIFY_ARE_EQUAL(0u, commands.Size());
    }

    void SettingsTests::TestUnbindNestedCommand()
    {
        // Test that layering a command with `"commands": null` set will unbind a command that already exists.

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
            "actions": [
                {
                    "name": "parent",
                    "commands": [
                        {
                            "name": "child1",
                            "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                        },
                        {
                            "name": "child2",
                            "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                        }
                    ]
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        const std::string settings1Json{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "actions": [
                {
                    "name": "parent",
                    "commands": null
                },
            ],
        })" };

        VerifyParseSucceeded(settingsJson);
        VerifyParseSucceeded(settings1Json);

        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        settings._ValidateSettings();
        _logCommandNames(commands);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(1u, commands.Size());

        Log::Comment(L"Layer second bit of json, to unbind the original command.");

        settings._ParseJsonString(settings1Json, false);
        settings.LayerJson(settings._userSettings);
        settings._ValidateSettings();
        _logCommandNames(commands);
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(0u, commands.Size());
    }

    void SettingsTests::TestRebindNestedCommand()
    {
        // Test that layering a command with an action set on top of a command
        // with nested commands replaces the nested commands with an action.

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
            "actions": [
                {
                    "name": "parent",
                    "commands": [
                        {
                            "name": "child1",
                            "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                        },
                        {
                            "name": "child2",
                            "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                        }
                    ]
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        const std::string settings1Json{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "actions": [
                {
                    "name": "parent",
                    "command": "newTab"
                },
            ],
        })" };

        VerifyParseSucceeded(settingsJson);
        VerifyParseSucceeded(settings1Json);

        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        settings._ValidateSettings();
        _logCommandNames(commands);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            winrt::hstring commandName{ L"parent" };
            auto commandProj = commands.Lookup(commandName);
            VERIFY_IS_NOT_NULL(commandProj);

            winrt::com_ptr<implementation::Command> commandImpl;
            commandImpl.copy_from(winrt::get_self<implementation::Command>(commandProj));

            VERIFY_IS_TRUE(commandImpl->HasNestedCommands());
            VERIFY_ARE_EQUAL(2u, commandImpl->_subcommands.Size());
        }

        Log::Comment(L"Layer second bit of json, to unbind the original command.");
        settings._ParseJsonString(settings1Json, false);
        settings.LayerJson(settings._userSettings);
        settings._ValidateSettings();
        _logCommandNames(commands);
        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            winrt::hstring commandName{ L"parent" };
            auto commandProj = commands.Lookup(commandName);

            VERIFY_IS_NOT_NULL(commandProj);
            auto actionAndArgs = commandProj.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);

            winrt::com_ptr<implementation::Command> commandImpl;
            commandImpl.copy_from(winrt::get_self<implementation::Command>(commandProj));

            VERIFY_IS_FALSE(commandImpl->HasNestedCommands());
        }
    }

    void SettingsTests::TestIterableColorSchemeCommands()
    {
        // For this test, put an iterable command with a given `name`,
        // containing a ${profile.name} to replace. When we expand it, it should
        // have created one command for each profile.

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
            "schemes": [
                { "name": "scheme_0" },
                { "name": "scheme_1" },
                { "name": "scheme_2" },
            ],
            "bindings": [
                {
                    "name": "iterable command ${scheme.name}",
                    "iterateOn": "schemes",
                    "command": { "action": "splitPane", "profile": "${scheme.name}" }
                },
            ]
        })" };

        VerifyParseSucceeded(settingsJson);
        CascadiaSettings settings{};
        settings._ParseJsonString(settingsJson, false);
        settings.LayerJson(settings._userSettings);

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());

        VERIFY_ARE_EQUAL(3u, settings.GetProfiles().size());

        auto commands = settings._globals->GetCommands();
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            auto command = commands.Lookup(L"iterable command ${scheme.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${scheme.name}", realArgs.TerminalArgs().Profile());
        }

        auto expandedCommands = implementation::TerminalPage::_ExpandCommands(commands, settings.GetProfiles(), settings._globals->GetColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings._warnings.size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        // Yes, this test is testing splitPane with profiles named after each
        // color scheme. These would obviously not work in real life, they're
        // just easy tests to write.

        {
            auto command = expandedCommands.Lookup(L"iterable command scheme_0");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"scheme_0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command scheme_1");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"scheme_1", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command scheme_2");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(winrt::TerminalApp::SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"scheme_2", realArgs.TerminalArgs().Profile());
        }
    }

}
