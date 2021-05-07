// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "JsonTestClass.h"
#include "TestUtils.h"
#include <defaults.h>
#include "../ut_app/TestDynamicProfileGenerator.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace SettingsModelLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class DeserializationTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(DeserializationTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

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

        TEST_METHOD(TestProfileBackgroundImageWithEnvVar);
        TEST_METHOD(TestProfileBackgroundImageWithDesktopWallpaper);

        TEST_METHOD(TestCloseOnExitParsing);
        TEST_METHOD(TestCloseOnExitCompatibilityShim);

        TEST_METHOD(TestLayerUserDefaultsBeforeProfiles);
        TEST_METHOD(TestDontLayerGuidFromUserDefaults);
        TEST_METHOD(TestLayerUserDefaultsOnDynamics);

        TEST_METHOD(FindMissingProfile);

        TEST_METHOD(ValidateKeybindingsWarnings);

        TEST_METHOD(ValidateColorSchemeInCommands);

        TEST_METHOD(ValidateExecuteCommandlineWarning);

        TEST_METHOD(ValidateLegacyGlobalsWarning);

        TEST_METHOD(TestTrailingCommas);

        TEST_METHOD(TestCommandsAndKeybindings);

        TEST_METHOD(TestNestedCommandWithoutName);
        TEST_METHOD(TestNestedCommandWithBadSubCommands);
        TEST_METHOD(TestUnbindNestedCommand);
        TEST_METHOD(TestRebindNestedCommand);

        TEST_METHOD(TestCopy);
        TEST_METHOD(TestCloneInheritanceTree);

        TEST_METHOD(TestValidDefaults);

        TEST_METHOD(TestInheritedCommand);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }

    private:
        void _logCommandNames(winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, Command> commands, const int indentation = 1)
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

    void DeserializationTests::ValidateProfilesExist()
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
            auto settings = implementation::CascadiaSettings::FromJson(settingsObject);
            settings->_ValidateProfilesExist();
        }
        {
            // Case 2: Bad settings
            const auto settingsObject = VerifyParseSucceeded(settingsWithoutProfiles);
            auto settings = implementation::CascadiaSettings::FromJson(settingsObject);
            bool caughtExpectedException = false;
            try
            {
                settings->_ValidateProfilesExist();
            }
            catch (const implementation::SettingsException& ex)
            {
                VERIFY_IS_TRUE(ex.Error() == SettingsLoadErrors::NoProfiles);
                caughtExpectedException = true;
            }
            VERIFY_IS_TRUE(caughtExpectedException);
        }
        {
            // Case 3: Bad settings
            const auto settingsObject = VerifyParseSucceeded(settingsWithEmptyProfiles);
            auto settings = implementation::CascadiaSettings::FromJson(settingsObject);
            bool caughtExpectedException = false;
            try
            {
                settings->_ValidateProfilesExist();
            }
            catch (const implementation::SettingsException& ex)
            {
                VERIFY_IS_TRUE(ex.Error() == SettingsLoadErrors::NoProfiles);
                caughtExpectedException = true;
            }
            VERIFY_IS_TRUE(caughtExpectedException);
        }
    }

    void DeserializationTests::ValidateDefaultProfileExists()
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
            auto settings = implementation::CascadiaSettings::FromJson(settingsObject);
            settings->_ResolveDefaultProfile();
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings->_warnings.Size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_allProfiles.GetAt(0).Guid());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, but the defaultProfile is NOT one of those guids"));
            const auto settingsObject = VerifyParseSucceeded(badProfiles);
            auto settings = implementation::CascadiaSettings::FromJson(settingsObject);
            settings->_ResolveDefaultProfile();
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingDefaultProfile, settings->_warnings.GetAt(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_allProfiles.GetAt(0).Guid());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, and no defaultProfile at all"));
            const auto settingsObject = VerifyParseSucceeded(badProfiles);
            auto settings = implementation::CascadiaSettings::FromJson(settingsObject);
            settings->_ResolveDefaultProfile();
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingDefaultProfile, settings->_warnings.GetAt(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_allProfiles.GetAt(0).Guid());
        }
        {
            // Case 4: Good settings, default profile is a string
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, and the defaultProfile is one of the profile names"));
            const auto settingsObject = VerifyParseSucceeded(goodProfilesSpecifiedByName);
            auto settings = implementation::CascadiaSettings::FromJson(settingsObject);
            settings->_ResolveDefaultProfile();
            settings->_ValidateDefaultProfileExists();
            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings->_warnings.Size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_allProfiles.GetAt(1).Guid());
        }
    }

    void DeserializationTests::ValidateDuplicateProfiles()
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

            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_allProfiles.Append(profile0);
            settings->_allProfiles.Append(profile1);

            settings->_ValidateNoDuplicateProfiles();

            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings->_warnings.Size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->_allProfiles.Size());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with the same guid"));

            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_allProfiles.Append(profile2);
            settings->_allProfiles.Append(profile3);

            settings->_ValidateNoDuplicateProfiles();

            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::DuplicateProfile, settings->_warnings.GetAt(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile2", settings->_allProfiles.GetAt(0).Name());
        }
        {
            // Case 3: Very bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a set of profiles, many of which with duplicated guids"));

            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_allProfiles.Append(profile0);
            settings->_allProfiles.Append(profile1);
            settings->_allProfiles.Append(profile2);
            settings->_allProfiles.Append(profile3);
            settings->_allProfiles.Append(profile4);
            settings->_allProfiles.Append(profile5);
            settings->_allProfiles.Append(profile6);

            settings->_ValidateNoDuplicateProfiles();

            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->_warnings.Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::DuplicateProfile, settings->_warnings.GetAt(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(4), settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile0", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile1", settings->_allProfiles.GetAt(1).Name());
            VERIFY_ARE_EQUAL(L"profile4", settings->_allProfiles.GetAt(2).Name());
            VERIFY_ARE_EQUAL(L"profile6", settings->_allProfiles.GetAt(3).Name());
        }
    }

    void DeserializationTests::ValidateManyWarnings()
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
        auto settings = implementation::CascadiaSettings::FromJson(settingsObject);

        settings->_allProfiles.Append(profile4);
        settings->_allProfiles.Append(profile5);

        settings->_ValidateSettings();

        VERIFY_ARE_EQUAL(3u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::DuplicateProfile, settings->_warnings.GetAt(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingDefaultProfile, settings->_warnings.GetAt(1));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::UnknownColorScheme, settings->_warnings.GetAt(2));

        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());
        VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), settings->_allProfiles.GetAt(0).Guid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(2).HasGuid());
    }

    void DeserializationTests::LayerGlobalProperties()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();

        settings->LayerJson(settings0Json);
        VERIFY_ARE_EQUAL(true, settings->_globals->AlwaysShowTabs());
        VERIFY_ARE_EQUAL(120, settings->_globals->InitialCols());
        VERIFY_ARE_EQUAL(30, settings->_globals->InitialRows());
        VERIFY_ARE_EQUAL(true, settings->_globals->ShowTabsInTitlebar());

        settings->LayerJson(settings1Json);
        VERIFY_ARE_EQUAL(true, settings->_globals->AlwaysShowTabs());
        VERIFY_ARE_EQUAL(240, settings->_globals->InitialCols());
        VERIFY_ARE_EQUAL(60, settings->_globals->InitialRows());
        VERIFY_ARE_EQUAL(false, settings->_globals->ShowTabsInTitlebar());
    }

    void DeserializationTests::ValidateProfileOrdering()
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

            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_ParseJsonString(defaultProfilesString, true);
            settings->LayerJson(settings->_defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile2", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile3", settings->_allProfiles.GetAt(1).Name());

            settings->_ParseJsonString(userProfiles0String, false);
            settings->LayerJson(settings->_userSettings);
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile1", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile0", settings->_allProfiles.GetAt(1).Name());

            settings->_ReorderProfilesToMatchUserSettingsOrder();
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile0", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile1", settings->_allProfiles.GetAt(1).Name());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Case 2: Make sure all the user's profiles appear before the defaults."));

            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_ParseJsonString(defaultProfilesString, true);
            settings->LayerJson(settings->_defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile2", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile3", settings->_allProfiles.GetAt(1).Name());

            settings->_ParseJsonString(userProfiles1String, false);
            settings->LayerJson(settings->_userSettings);
            VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile2", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile4", settings->_allProfiles.GetAt(1).Name());
            VERIFY_ARE_EQUAL(L"profile5", settings->_allProfiles.GetAt(2).Name());

            settings->_ReorderProfilesToMatchUserSettingsOrder();
            VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile4", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile5", settings->_allProfiles.GetAt(1).Name());
            VERIFY_ARE_EQUAL(L"profile2", settings->_allProfiles.GetAt(2).Name());
        }
    }

    void DeserializationTests::ValidateHideProfiles()
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
            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_ParseJsonString(defaultProfilesString, true);
            settings->LayerJson(settings->_defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile2", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile3", settings->_allProfiles.GetAt(1).Name());
            VERIFY_ARE_EQUAL(false, settings->_allProfiles.GetAt(0).Hidden());
            VERIFY_ARE_EQUAL(false, settings->_allProfiles.GetAt(1).Hidden());

            settings->_ParseJsonString(userProfiles0String, false);
            settings->LayerJson(settings->_userSettings);
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile1", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile0", settings->_allProfiles.GetAt(1).Name());
            VERIFY_ARE_EQUAL(false, settings->_allProfiles.GetAt(0).Hidden());
            VERIFY_ARE_EQUAL(true, settings->_allProfiles.GetAt(1).Hidden());

            settings->_ReorderProfilesToMatchUserSettingsOrder();
            settings->_UpdateActiveProfiles();
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(1u, settings->_activeProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile1", settings->_activeProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(false, settings->_activeProfiles.GetAt(0).Hidden());
        }

        {
            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_ParseJsonString(defaultProfilesString, true);
            settings->LayerJson(settings->_defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile2", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile3", settings->_allProfiles.GetAt(1).Name());
            VERIFY_ARE_EQUAL(false, settings->_allProfiles.GetAt(0).Hidden());
            VERIFY_ARE_EQUAL(false, settings->_allProfiles.GetAt(1).Hidden());

            settings->_ParseJsonString(userProfiles1String, false);
            settings->LayerJson(settings->_userSettings);
            VERIFY_ARE_EQUAL(4u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile2", settings->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile4", settings->_allProfiles.GetAt(1).Name());
            VERIFY_ARE_EQUAL(L"profile5", settings->_allProfiles.GetAt(2).Name());
            VERIFY_ARE_EQUAL(L"profile6", settings->_allProfiles.GetAt(3).Name());
            VERIFY_ARE_EQUAL(false, settings->_allProfiles.GetAt(0).Hidden());
            VERIFY_ARE_EQUAL(true, settings->_allProfiles.GetAt(1).Hidden());
            VERIFY_ARE_EQUAL(false, settings->_allProfiles.GetAt(2).Hidden());
            VERIFY_ARE_EQUAL(true, settings->_allProfiles.GetAt(3).Hidden());

            settings->_ReorderProfilesToMatchUserSettingsOrder();
            settings->_UpdateActiveProfiles();
            VERIFY_ARE_EQUAL(4u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(2u, settings->_activeProfiles.Size());
            VERIFY_ARE_EQUAL(L"profile5", settings->_activeProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile2", settings->_activeProfiles.GetAt(1).Name());
            VERIFY_ARE_EQUAL(false, settings->_activeProfiles.GetAt(0).Hidden());
            VERIFY_ARE_EQUAL(false, settings->_activeProfiles.GetAt(1).Hidden());
        }
    }

    void DeserializationTests::ValidateProfilesGenerateGuids()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_allProfiles.Append(*profile0);
        settings->_allProfiles.Append(*profile1);
        settings->_allProfiles.Append(*profile2);
        settings->_allProfiles.Append(*profile3);
        settings->_allProfiles.Append(*profile4);
        settings->_allProfiles.Append(*profile5);

        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(2).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(3).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(4).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(5).HasGuid());

        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(0).Guid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(1).Guid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(2).Guid(), nullGuid);
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(3).Guid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(4).Guid(), nullGuid);
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(5).Guid(), nullGuid);

        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(0).Guid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(1).Guid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(2).Guid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(3).Guid(), cmdGuid);
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(4).Guid(), cmdGuid);
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(5).Guid(), cmdGuid);

        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(0).Guid(), settings->_allProfiles.GetAt(2).Guid());
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(1).Guid(), settings->_allProfiles.GetAt(2).Guid());
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(2).Guid(), settings->_allProfiles.GetAt(2).Guid());
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(3).Guid(), settings->_allProfiles.GetAt(2).Guid());
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(4).Guid(), settings->_allProfiles.GetAt(2).Guid());
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(5).Guid(), settings->_allProfiles.GetAt(2).Guid());
    }

    void DeserializationTests::GeneratedGuidRoundtrips()
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
        VERIFY_IS_TRUE(profile1->HasGuid());

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_allProfiles.Append(*profile1);

        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());

        const auto profileImpl = winrt::get_self<implementation::Profile>(settings->_allProfiles.GetAt(0));
        const auto serialized1Profile = profileImpl->GenerateStub();

        const auto profile2 = implementation::Profile::FromJson(serialized1Profile);
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(profile2->HasGuid());
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(0).Guid(), profile2->Guid());
    }

    void DeserializationTests::TestAllValidationsWithNullGuids()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settings0String, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(1).HasGuid());

        settings->_ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(1).HasGuid());
    }

    void DeserializationTests::TestReorderWithNullGuids()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(DefaultJson, true);
        settings->LayerJson(settings->_defaultSettings);
        VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->_allProfiles.GetAt(1).Name());

        settings->_ParseJsonString(settings0String, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(4u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(2).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(3).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"cmdFromUserSettings", settings->_allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"profile0", settings->_allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"profile1", settings->_allProfiles.GetAt(3).Name());

        settings->_ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(4u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(2).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(3).HasGuid());
        VERIFY_ARE_EQUAL(L"profile0", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"profile1", settings->_allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"cmdFromUserSettings", settings->_allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(3).Name());
    }

    void DeserializationTests::TestReorderingWithoutGuid()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(DefaultJson, true);
        settings->LayerJson(settings->_defaultSettings);
        VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->_allProfiles.GetAt(1).Name());

        settings->_ParseJsonString(settings0String, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(4u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(2).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(3).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->_allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotCrash", settings->_allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"Ubuntu", settings->_allProfiles.GetAt(3).Name());

        settings->_ValidateSettings();
        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(4u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(2).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(3).HasGuid());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotCrash", settings->_allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"Ubuntu", settings->_allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(3).Name());
    }

    void DeserializationTests::TestLayeringNameOnlyProfiles()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(DefaultJson, true);
        settings->LayerJson(settings->_defaultSettings);
        VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->_allProfiles.GetAt(1).Name());

        Log::Comment(NoThrowString().Format(
            L"Parse the user settings"));
        settings->_ParseJsonString(settings0String, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(5u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(2).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(3).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(4).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->_allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings->_allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotLayer", settings->_allProfiles.GetAt(3).Name());
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings->_allProfiles.GetAt(4).Name());
    }

    void DeserializationTests::TestExplodingNameOnlyProfiles()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(DefaultJson, true);
        settings->LayerJson(settings->_defaultSettings);
        VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->_allProfiles.GetAt(1).Name());

        Log::Comment(NoThrowString().Format(
            L"Parse the user settings"));
        settings->_ParseJsonString(settings0String, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(5u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(2).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(3).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(4).HasGuid());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->_allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings->_allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotDuplicate", settings->_allProfiles.GetAt(3).Name());
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings->_allProfiles.GetAt(4).Name());

        Log::Comment(NoThrowString().Format(
            L"Pretend like we're checking to append dynamic profiles to the "
            L"user's settings file. We absolutely _shouldn't_ be adding anything here."));
        bool const needToWriteFile = settings->_AppendDynamicProfilesToUserSettings();
        VERIFY_IS_FALSE(needToWriteFile);
        VERIFY_ARE_EQUAL(settings0String.size(), settings->_userSettingsString.size());

        Log::Comment(NoThrowString().Format(
            L"Re-parse the settings file. We should have the _same_ settings as before."));
        Log::Comment(NoThrowString().Format(
            L"Do this to a _new_ settings object, to make sure it turns out the same."));
        {
            auto settings2 = winrt::make_self<implementation::CascadiaSettings>();
            settings2->_ParseJsonString(DefaultJson, true);
            settings2->LayerJson(settings2->_defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings2->_allProfiles.Size());
            // Initialize the second settings object from the first settings
            // object's settings string, the one that we synthesized.
            const auto firstSettingsString = settings->_userSettingsString;
            settings2->_ParseJsonString(firstSettingsString, false);
            settings2->LayerJson(settings2->_userSettings);
            VERIFY_ARE_EQUAL(5u, settings2->_allProfiles.Size());
            VERIFY_IS_TRUE(settings2->_allProfiles.GetAt(0).HasGuid());
            VERIFY_IS_TRUE(settings2->_allProfiles.GetAt(1).HasGuid());
            VERIFY_IS_TRUE(settings2->_allProfiles.GetAt(2).HasGuid());
            VERIFY_IS_FALSE(settings2->_allProfiles.GetAt(3).HasGuid());
            VERIFY_IS_FALSE(settings2->_allProfiles.GetAt(4).HasGuid());
            VERIFY_ARE_EQUAL(L"Windows PowerShell", settings2->_allProfiles.GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"Command Prompt", settings2->_allProfiles.GetAt(1).Name());
            VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings2->_allProfiles.GetAt(2).Name());
            VERIFY_ARE_EQUAL(L"ThisProfileShouldNotDuplicate", settings2->_allProfiles.GetAt(3).Name());
            VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings2->_allProfiles.GetAt(4).Name());
        }

        Log::Comment(NoThrowString().Format(
            L"Validate the settings. All the profiles we have should be valid."));
        settings->_ValidateSettings();

        VERIFY_ARE_EQUAL(5u, settings->_allProfiles.Size());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(0).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(1).HasGuid());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(2).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(3).HasGuid());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(4).HasGuid());
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotDuplicate", settings->_allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", settings->_allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->_allProfiles.GetAt(3).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->_allProfiles.GetAt(4).Name());
    }

    void DeserializationTests::TestHideAllProfiles()
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
            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_ParseJsonString(settingsWithProfiles, false);
            settings->LayerJson(settings->_userSettings);

            settings->_UpdateActiveProfiles();
            Log::Comment(NoThrowString().Format(
                L"settingsWithProfiles successfully parsed and validated"));
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
            VERIFY_ARE_EQUAL(1u, settings->_activeProfiles.Size());
        }
        {
            // Case 2: Bad settings
            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_ParseJsonString(settingsWithoutProfiles, false);
            settings->LayerJson(settings->_userSettings);

            bool caughtExpectedException = false;
            try
            {
                settings->_UpdateActiveProfiles();
            }
            catch (const implementation::SettingsException& ex)
            {
                VERIFY_IS_TRUE(ex.Error() == SettingsLoadErrors::AllProfilesHidden);
                caughtExpectedException = true;
            }
            VERIFY_IS_TRUE(caughtExpectedException);
        }
    }

    void DeserializationTests::TestInvalidColorSchemeName()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settings0String, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());
        VERIFY_ARE_EQUAL(2u, settings->_globals->ColorSchemes().Size());

        VERIFY_ARE_EQUAL(L"schemeOne", settings->_allProfiles.GetAt(0).DefaultAppearance().ColorSchemeName());
        VERIFY_ARE_EQUAL(L"InvalidSchemeName", settings->_allProfiles.GetAt(1).DefaultAppearance().ColorSchemeName());
        VERIFY_ARE_EQUAL(L"Campbell", settings->_allProfiles.GetAt(2).DefaultAppearance().ColorSchemeName());

        settings->_ValidateAllSchemesExist();

        VERIFY_ARE_EQUAL(1u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::UnknownColorScheme, settings->_warnings.GetAt(0));

        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());
        VERIFY_ARE_EQUAL(2u, settings->_globals->ColorSchemes().Size());

        VERIFY_ARE_EQUAL(L"schemeOne", settings->_allProfiles.GetAt(0).DefaultAppearance().ColorSchemeName());
        VERIFY_ARE_EQUAL(L"Campbell", settings->_allProfiles.GetAt(1).DefaultAppearance().ColorSchemeName());
        VERIFY_ARE_EQUAL(L"Campbell", settings->_allProfiles.GetAt(2).DefaultAppearance().ColorSchemeName());
    }

    void DeserializationTests::ValidateColorSchemeInCommands()
    {
        Log::Comment(NoThrowString().Format(
            L"Ensure that setting a command's color scheme to a non-existent scheme causes a warning."));

        const std::string settings0String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "schemeOne"
                }
            ],
            "schemes": [
                {
                    "name": "schemeOne",
                    "foreground": "#111111"
                }
            ],
            "actions": [
                {
                    "command": { "action": "setColorScheme", "colorScheme": "schemeOne" }
                },
                {
                    "command": { "action": "setColorScheme", "colorScheme": "invalidScheme" }
                }
            ]
        })" };

        const std::string settings1String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "schemeOne"
                }
            ],
            "schemes": [
                {
                    "name": "schemeOne",
                    "foreground": "#111111"
                }
            ],
            "actions": [
                {
                    "command": { "action": "setColorScheme", "colorScheme": "schemeOne" }
                },
                {
                    "name": "parent",
                    "commands": [
                        { "command": { "action": "setColorScheme", "colorScheme": "invalidScheme" } }
                    ]
                }
            ]
        })" };

        const std::string settings2String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "schemeOne"
                }
            ],
            "schemes": [
                {
                    "name": "schemeOne",
                    "foreground": "#111111"
                }
            ],
            "actions": [
                {
                    "command": { "action": "setColorScheme", "colorScheme": "schemeOne" }
                },
                {
                    "name": "grandparent",
                    "commands": [
                        {
                            "name": "parent",
                            "commands": [
                                {
                                    "command": { "action": "setColorScheme", "colorScheme": "invalidScheme" }
                                }
                            ]
                        }
                    ]
                }
            ]
        })" };

        {
            // Case 1: setColorScheme command with invalid scheme
            Log::Comment(NoThrowString().Format(
                L"Testing a simple command with invalid scheme"));
            VerifyParseSucceeded(settings0String);

            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_ParseJsonString(settings0String, false);
            settings->LayerJson(settings->_userSettings);
            settings->_ValidateColorSchemesInCommands();

            VERIFY_ARE_EQUAL(1u, settings->_warnings.Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::InvalidColorSchemeInCmd, settings->_warnings.GetAt(0));
        }
        {
            // Case 2: nested setColorScheme command with invalid scheme
            Log::Comment(NoThrowString().Format(
                L"Testing a nested command with invalid scheme"));
            VerifyParseSucceeded(settings1String);

            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_ParseJsonString(settings1String, false);
            settings->LayerJson(settings->_userSettings);
            settings->_ValidateColorSchemesInCommands();

            VERIFY_ARE_EQUAL(1u, settings->_warnings.Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::InvalidColorSchemeInCmd, settings->_warnings.GetAt(0));
        }
        {
            // Case 3: nested-in-nested setColorScheme command with invalid scheme
            Log::Comment(NoThrowString().Format(
                L"Testing a nested-in-nested command with invalid scheme"));
            VerifyParseSucceeded(settings2String);

            auto settings = winrt::make_self<implementation::CascadiaSettings>();
            settings->_ParseJsonString(settings2String, false);
            settings->LayerJson(settings->_userSettings);
            settings->_ValidateColorSchemesInCommands();

            VERIFY_ARE_EQUAL(1u, settings->_warnings.Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::InvalidColorSchemeInCmd, settings->_warnings.GetAt(0));
        }
    }

    void DeserializationTests::TestHelperFunctions()
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
        const winrt::guid autogeneratedGuid{ implementation::Profile::_GenerateGuidForProfile(name3, L"") };
        const std::optional<winrt::guid> badGuid{};

        VerifyParseSucceeded(settings0String);

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settings0String, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(guid0, settings->_GetProfileGuidByName(name0));
        VERIFY_ARE_EQUAL(guid1, settings->_GetProfileGuidByName(name1));
        VERIFY_ARE_EQUAL(guid2, settings->_GetProfileGuidByName(name2));
        VERIFY_ARE_EQUAL(autogeneratedGuid, settings->_GetProfileGuidByName(name3));
        VERIFY_ARE_EQUAL(badGuid, settings->_GetProfileGuidByName(badName));

        auto prof0{ settings->FindProfile(guid0) };
        auto prof1{ settings->FindProfile(guid1) };
        auto prof2{ settings->FindProfile(guid2) };

        auto badProf{ settings->FindProfile(fakeGuid) };
        VERIFY_ARE_EQUAL(badProf, nullptr);

        VERIFY_ARE_EQUAL(name0, prof0.Name());
        VERIFY_ARE_EQUAL(name1, prof1.Name());
        VERIFY_ARE_EQUAL(name2, prof2.Name());
    }

    void DeserializationTests::TestProfileBackgroundImageWithEnvVar()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);
        VERIFY_ARE_NOT_EQUAL(0u, settings->_allProfiles.Size());
        VERIFY_ARE_EQUAL(expectedPath, settings->_allProfiles.GetAt(0).DefaultAppearance().ExpandedBackgroundImagePath());
    }
    void DeserializationTests::TestProfileBackgroundImageWithDesktopWallpaper()
    {
        const winrt::hstring expectedBackgroundImagePath{ L"desktopWallpaper" };

        const std::string settingsJson{ R"(
        {
            "profiles": [
                {
                    "name": "profile0",
                    "backgroundImage": "desktopWallpaper"
                }
            ]
        })" };

        VerifyParseSucceeded(settingsJson);

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);
        VERIFY_ARE_EQUAL(expectedBackgroundImagePath, settings->_allProfiles.GetAt(0).DefaultAppearance().BackgroundImagePath());
        VERIFY_ARE_NOT_EQUAL(expectedBackgroundImagePath, settings->_allProfiles.GetAt(0).DefaultAppearance().ExpandedBackgroundImagePath());
    }
    void DeserializationTests::TestCloseOnExitParsing()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings->_allProfiles.GetAt(0).CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Always, settings->_allProfiles.GetAt(1).CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Never, settings->_allProfiles.GetAt(2).CloseOnExit());

        // Unknown modes parse as "Graceful"
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings->_allProfiles.GetAt(3).CloseOnExit());
    }
    void DeserializationTests::TestCloseOnExitCompatibilityShim()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings->_allProfiles.GetAt(0).CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Never, settings->_allProfiles.GetAt(1).CloseOnExit());
    }

    void DeserializationTests::TestLayerUserDefaultsBeforeProfiles()
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
            auto settings = winrt::make_self<implementation::CascadiaSettings>(false);
            settings->_ParseJsonString(settings0String, false);
            VERIFY_IS_NULL(settings->_userDefaultProfileSettings);
            settings->_ApplyDefaultsFromUserSettings();
            VERIFY_IS_NOT_NULL(settings->_userDefaultProfileSettings);
            settings->LayerJson(settings->_userSettings);

            VERIFY_ARE_EQUAL(guid1String, settings->_globals->UnparsedDefaultProfile());
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());

            VERIFY_ARE_EQUAL(2345, settings->_allProfiles.GetAt(0).HistorySize());
            VERIFY_ARE_EQUAL(1234, settings->_allProfiles.GetAt(1).HistorySize());
        }
    }

    void DeserializationTests::TestDontLayerGuidFromUserDefaults()
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
            auto settings = winrt::make_self<implementation::CascadiaSettings>(false);
            settings->_ParseJsonString(DefaultJson, true);
            settings->LayerJson(settings->_defaultSettings);
            VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());

            settings->_ParseJsonString(settings0String, false);
            VERIFY_IS_NULL(settings->_userDefaultProfileSettings);
            settings->_ApplyDefaultsFromUserSettings();
            VERIFY_IS_NOT_NULL(settings->_userDefaultProfileSettings);

            Log::Comment(NoThrowString().Format(
                L"Ensure that cmd and powershell don't get their GUIDs overwritten"));
            VERIFY_ARE_NOT_EQUAL(guid2, settings->_allProfiles.GetAt(0).Guid());
            VERIFY_ARE_NOT_EQUAL(guid2, settings->_allProfiles.GetAt(1).Guid());

            settings->LayerJson(settings->_userSettings);

            VERIFY_ARE_EQUAL(guid1String, settings->_globals->UnparsedDefaultProfile());
            VERIFY_ARE_EQUAL(4u, settings->_allProfiles.Size());

            VERIFY_ARE_EQUAL(guid1, settings->_allProfiles.GetAt(2).Guid());
            VERIFY_IS_FALSE(settings->_allProfiles.GetAt(3).HasGuid());
        }
    }

    void DeserializationTests::TestLayerUserDefaultsOnDynamics()
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
                        "name" : "profile0FromUserSettings", // this is _allProfiles.GetAt(0)
                        "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                        "source": "Terminal.App.UnitTest.0"
                    },
                    {
                        "name" : "profile1FromUserSettings", // this is _allProfiles.GetAt(2)
                        "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                        "source": "Terminal.App.UnitTest.1",
                        "historySize": 4444
                    },
                    {
                        "name" : "profile2FromUserSettings", // this is _allProfiles.GetAt(3)
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
            p0.Name(L"profile0"); // this is _allProfiles.GetAt(0)
            p0.HistorySize(1111);
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TerminalAppUnitTests::TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = [guid1, guid2]() {
            std::vector<Profile> profiles;
            Profile p0 = winrt::make<implementation::Profile>(guid1);
            Profile p1 = winrt::make<implementation::Profile>(guid2);
            p0.Name(L"profile0"); // this is _allProfiles.GetAt(1)
            p1.Name(L"profile1"); // this is _allProfiles.GetAt(2)
            p0.HistorySize(2222);
            profiles.push_back(p0);
            p1.HistorySize(3333);
            profiles.push_back(p1);
            return profiles;
        };

        auto settings = winrt::make_self<implementation::CascadiaSettings>(false);
        settings->_profileGenerators.emplace_back(std::move(gen0));
        settings->_profileGenerators.emplace_back(std::move(gen1));

        Log::Comment(NoThrowString().Format(
            L"All profiles with the same name have the same GUID. However, they"
            L" will not be layered, because they have different source's"));

        // parse userProfiles as the user settings
        settings->_ParseJsonString(userProfiles, false);
        VERIFY_ARE_EQUAL(0u, settings->_allProfiles.Size(), L"Just parsing the user settings doesn't actually layer them");
        settings->_LoadDynamicProfiles();
        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());

        VERIFY_ARE_EQUAL(1111, settings->_allProfiles.GetAt(0).HistorySize());
        VERIFY_ARE_EQUAL(2222, settings->_allProfiles.GetAt(1).HistorySize());
        VERIFY_ARE_EQUAL(3333, settings->_allProfiles.GetAt(2).HistorySize());

        settings->_ApplyDefaultsFromUserSettings();

        VERIFY_ARE_EQUAL(1234, settings->_allProfiles.GetAt(0).HistorySize());
        VERIFY_ARE_EQUAL(1234, settings->_allProfiles.GetAt(1).HistorySize());
        VERIFY_ARE_EQUAL(1234, settings->_allProfiles.GetAt(2).HistorySize());

        settings->LayerJson(settings->_userSettings);
        VERIFY_ARE_EQUAL(4u, settings->_allProfiles.Size());

        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(0).Source().empty());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(1).Source().empty());
        VERIFY_IS_FALSE(settings->_allProfiles.GetAt(2).Source().empty());
        VERIFY_IS_TRUE(settings->_allProfiles.GetAt(3).Source().empty());

        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.0", settings->_allProfiles.GetAt(0).Source());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings->_allProfiles.GetAt(1).Source());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings->_allProfiles.GetAt(2).Source());

        VERIFY_ARE_EQUAL(guid1, settings->_allProfiles.GetAt(0).Guid());
        VERIFY_ARE_EQUAL(guid1, settings->_allProfiles.GetAt(1).Guid());
        VERIFY_ARE_EQUAL(guid2, settings->_allProfiles.GetAt(2).Guid());

        VERIFY_ARE_EQUAL(L"profile0FromUserSettings", settings->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"profile0", settings->_allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"profile1FromUserSettings", settings->_allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"profile2FromUserSettings", settings->_allProfiles.GetAt(3).Name());

        Log::Comment(NoThrowString().Format(
            L"This is the real meat of the test: The two dynamic profiles that "
            L"_didn't_ have historySize set in the userSettings should have "
            L"1234 as their historySize(from the defaultSettings).The other two"
            L" profiles should have their custom historySize value."));

        VERIFY_ARE_EQUAL(1234, settings->_allProfiles.GetAt(0).HistorySize());
        VERIFY_ARE_EQUAL(1234, settings->_allProfiles.GetAt(1).HistorySize());
        VERIFY_ARE_EQUAL(4444, settings->_allProfiles.GetAt(2).HistorySize());
        VERIFY_ARE_EQUAL(5555, settings->_allProfiles.GetAt(3).HistorySize());
    }

    void DeserializationTests::FindMissingProfile()
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
        auto settings = implementation::CascadiaSettings::FromJson(settingsJsonObj);

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

    void DeserializationTests::ValidateKeybindingsWarnings()
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
                { "command": { "action": "resizePane" }, "keys": [ "ctrl+b" ] },
                { "name": "invalid nested", "commands":[ { "name" : "hello" }, { "name" : "world" } ] }
            ]
        })" };

        const auto settingsObject = VerifyParseSucceeded(badSettings);
        auto settings = implementation::CascadiaSettings::FromJson(settingsObject);

        // KeyMap: ctrl+a/b are mapped to "invalid"
        // ActionMap: "splitPane" and "invalid" are the only deserialized actions
        // NameMap: "splitPane" has no key binding, but it is still added to the name map
        VERIFY_ARE_EQUAL(2u, settings->_globals->_actionMap->_KeyMap.size());
        VERIFY_ARE_EQUAL(2u, settings->_globals->_actionMap->_ActionMap.size());
        VERIFY_ARE_EQUAL(1u, settings->_globals->_actionMap->NameMap().Size());

        VERIFY_ARE_EQUAL(4u, settings->_globals->_keybindingsWarnings.size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::TooManyKeysForChord, settings->_globals->_keybindingsWarnings.at(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(1));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(2));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::FailedToParseSubCommands, settings->_globals->_keybindingsWarnings.at(3));

        settings->_ValidateKeybindings();

        VERIFY_ARE_EQUAL(5u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::AtLeastOneKeybindingWarning, settings->_warnings.GetAt(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::TooManyKeysForChord, settings->_warnings.GetAt(1));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.GetAt(2));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.GetAt(3));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::FailedToParseSubCommands, settings->_warnings.GetAt(4));
    }

    void DeserializationTests::ValidateExecuteCommandlineWarning()
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

        auto settings = implementation::CascadiaSettings::FromJson(settingsObject);

        VERIFY_ARE_EQUAL(3u, settings->_globals->_actionMap->_KeyMap.size());
        VERIFY_IS_NULL(settings->_globals->_actionMap->GetActionByKeyChord({ KeyModifiers::Ctrl, static_cast<int32_t>('a') }));
        VERIFY_IS_NULL(settings->_globals->_actionMap->GetActionByKeyChord({ KeyModifiers::Ctrl, static_cast<int32_t>('b') }));
        VERIFY_IS_NULL(settings->_globals->_actionMap->GetActionByKeyChord({ KeyModifiers::Ctrl, static_cast<int32_t>('c') }));

        for (const auto& warning : settings->_globals->_keybindingsWarnings)
        {
            Log::Comment(NoThrowString().Format(
                L"warning:%d", warning));
        }
        VERIFY_ARE_EQUAL(3u, settings->_globals->_keybindingsWarnings.size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(1));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_globals->_keybindingsWarnings.at(2));

        settings->_ValidateKeybindings();

        VERIFY_ARE_EQUAL(4u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::AtLeastOneKeybindingWarning, settings->_warnings.GetAt(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.GetAt(1));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.GetAt(2));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->_warnings.GetAt(3));
    }

    void DeserializationTests::ValidateLegacyGlobalsWarning()
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
        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(DefaultJson, true);
        settings->LayerJson(settings->_defaultSettings);

        settings->_ValidateNoGlobalsKey();
        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());

        // Now layer on the user's settings
        settings->_ParseJsonString(badSettings, false);
        settings->LayerJson(settings->_userSettings);

        settings->_ValidateNoGlobalsKey();
        VERIFY_ARE_EQUAL(1u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::LegacyGlobalsProperty, settings->_warnings.GetAt(0));
    }

    void DeserializationTests::TestTrailingCommas()
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
        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(DefaultJson, true);
        settings->LayerJson(settings->_defaultSettings);

        // Now layer on the user's settings
        try
        {
            settings->_ParseJsonString(badSettings, false);
            settings->LayerJson(settings->_userSettings);
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to LayerJson should succeed, even with the trailing comma");
        }
    }

    void DeserializationTests::TestCommandsAndKeybindings()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());

        const auto profile2Guid = settings->_allProfiles.GetAt(2).Guid();
        VERIFY_ARE_NOT_EQUAL(winrt::guid{}, profile2Guid);

        auto actionMap = winrt::get_self<implementation::ActionMap>(settings->_globals->ActionMap());
        VERIFY_ARE_EQUAL(5u, actionMap->_KeyMap.size());

        // A/D, B, C, E will be in the list of commands, for 4 total.
        // * A and D share the same name, so they'll only generate a single action.
        // * F's name is set manually to `null`
        const auto& nameMap{ actionMap->NameMap() };
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('A') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }

        Log::Comment(L"Note that we're skipping ctrl+B, since that doesn't have `keys` set.");

        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('D') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('E') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Horizontal, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('F') };
            auto actionAndArgs = ::TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Horizontal, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }

        Log::Comment(L"Now verify the commands");
        _logCommandNames(nameMap);
        {
            // This was renamed to "ctrl+c" in C. So this does not exist.
            auto command = nameMap.TryLookup(L"Split pane, split: vertical");
            VERIFY_IS_NULL(command);
        }
        {
            // This was renamed to "ctrl+c" in C. So this does not exist.
            auto command = nameMap.TryLookup(L"ctrl+b");
            VERIFY_IS_NULL(command);
        }
        {
            auto command = nameMap.TryLookup(L"ctrl+c");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }
        {
            // This was renamed to null (aka removed from the name map) in F. So this does not exist.
            auto command = nameMap.TryLookup(L"Split pane, split: horizontal");
            VERIFY_IS_NULL(command);
        }
    }

    void DeserializationTests::TestNestedCommandWithoutName()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());

        settings->_ValidateSettings();
        const auto& nameMap{ settings->ActionMap().NameMap() };
        _logCommandNames(nameMap);

        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());

        // Because the "parent" command didn't have a name, it couldn't be
        // placed into the list of commands. It and it's children are just
        // ignored.
        VERIFY_ARE_EQUAL(0u, nameMap.Size());
    }

    void DeserializationTests::TestNestedCommandWithBadSubCommands()
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
                }
            ],
            "actions": [
                {
                    "name": "nested command",
                    "commands": [
                        {
                            "name": "child1"
                        },
                        {
                            "name": "child2"
                        }
                    ]
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        VerifyParseSucceeded(settingsJson);

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        VERIFY_ARE_EQUAL(2u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::AtLeastOneKeybindingWarning, settings->_warnings.GetAt(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::FailedToParseSubCommands, settings->_warnings.GetAt(1));
        const auto& nameMap{ settings->ActionMap().NameMap() };
        VERIFY_ARE_EQUAL(0u, nameMap.Size());
    }

    void DeserializationTests::TestUnbindNestedCommand()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());

        settings->_ValidateSettings();
        auto nameMap{ settings->ActionMap().NameMap() };
        _logCommandNames(nameMap);

        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        Log::Comment(L"Layer second bit of json, to unbind the original command.");

        settings->_ParseJsonString(settings1Json, false);
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        nameMap = settings->ActionMap().NameMap();
        _logCommandNames(nameMap);
        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(0u, nameMap.Size());
    }

    void DeserializationTests::TestRebindNestedCommand()
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

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());

        const auto& actionMap{ settings->ActionMap() };
        settings->_ValidateSettings();
        auto nameMap{ actionMap.NameMap() };
        _logCommandNames(nameMap);

        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        {
            winrt::hstring commandName{ L"parent" };
            auto commandProj = nameMap.TryLookup(commandName);
            VERIFY_IS_NOT_NULL(commandProj);

            winrt::com_ptr<implementation::Command> commandImpl;
            commandImpl.copy_from(winrt::get_self<implementation::Command>(commandProj));

            VERIFY_IS_TRUE(commandImpl->HasNestedCommands());
            VERIFY_ARE_EQUAL(2u, commandImpl->_subcommands.Size());
        }

        Log::Comment(L"Layer second bit of json, to unbind the original command.");
        settings->_ParseJsonString(settings1Json, false);
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        nameMap = settings->ActionMap().NameMap();
        _logCommandNames(nameMap);
        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        {
            winrt::hstring commandName{ L"parent" };
            auto commandProj = nameMap.TryLookup(commandName);

            VERIFY_IS_NOT_NULL(commandProj);
            auto actionAndArgs = commandProj.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);

            winrt::com_ptr<implementation::Command> commandImpl;
            commandImpl.copy_from(winrt::get_self<implementation::Command>(commandProj));

            VERIFY_IS_FALSE(commandImpl->HasNestedCommands());
        }
    }

    void DeserializationTests::TestCopy()
    {
        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "initialCols": 50,
            "profiles":
            [
                {
                    "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                    "name": "Custom Profile",
                    "fontFace": "Cascadia Code"
                }
            ],
            "schemes":
            [
                {
                    "name": "Campbell, but for a test",
                    "foreground": "#CCCCCC",
                    "background": "#0C0C0C",
                    "cursorColor": "#FFFFFF",
                    "black": "#0C0C0C",
                    "red": "#C50F1F",
                    "green": "#13A10E",
                    "yellow": "#C19C00",
                    "blue": "#0037DA",
                    "purple": "#881798",
                    "cyan": "#3A96DD",
                    "white": "#CCCCCC",
                    "brightBlack": "#767676",
                    "brightRed": "#E74856",
                    "brightGreen": "#16C60C",
                    "brightYellow": "#F9F1A5",
                    "brightBlue": "#3B78FF",
                    "brightPurple": "#B4009E",
                    "brightCyan": "#61D6D6",
                    "brightWhite": "#F2F2F2"
                }
            ],
            "actions":
            [
                { "command": "openSettings", "keys": "ctrl+," },
                { "command": { "action": "openSettings", "target": "defaultsFile" }, "keys": "ctrl+alt+," },

                {
                    "name": { "key": "SetColorSchemeParentCommandName" },
                    "commands": [
                        {
                            "iterateOn": "schemes",
                            "name": "${scheme.name}",
                            "command": { "action": "setColorScheme", "colorScheme": "${scheme.name}" }
                        }
                    ]
                }
            ]
        })" };

        VerifyParseSucceeded(settingsJson);

        auto settings{ winrt::make_self<implementation::CascadiaSettings>() };
        settings->_ParseJsonString(settingsJson, false);
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        const auto copy{ settings->Copy() };
        const auto copyImpl{ winrt::get_self<implementation::CascadiaSettings>(copy) };

        // test globals
        VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), copyImpl->_globals->DefaultProfile());

        // test profiles
        VERIFY_ARE_EQUAL(settings->_allProfiles.Size(), copyImpl->_allProfiles.Size());
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(0).Name(), copyImpl->_allProfiles.GetAt(0).Name());

        // test schemes
        const auto schemeName{ L"Campbell, but for a test" };
        VERIFY_ARE_EQUAL(settings->_globals->_colorSchemes.Size(), copyImpl->_globals->_colorSchemes.Size());
        VERIFY_ARE_EQUAL(settings->_globals->_colorSchemes.HasKey(schemeName), copyImpl->_globals->_colorSchemes.HasKey(schemeName));

        // test actions
        VERIFY_ARE_EQUAL(settings->_globals->_actionMap->_KeyMap.size(), copyImpl->_globals->_actionMap->_KeyMap.size());
        const auto& nameMapOriginal{ settings->_globals->_actionMap->NameMap() };
        const auto& nameMapCopy{ copyImpl->_globals->_actionMap->NameMap() };
        VERIFY_ARE_EQUAL(nameMapOriginal.Size(), nameMapCopy.Size());

        // Test that changing the copy should not change the original
        VERIFY_ARE_EQUAL(settings->_globals->WordDelimiters(), copyImpl->_globals->WordDelimiters());
        copyImpl->_globals->WordDelimiters(L"changed value");
        VERIFY_ARE_NOT_EQUAL(settings->_globals->WordDelimiters(), copyImpl->_globals->WordDelimiters());
    }

    void DeserializationTests::TestCloneInheritanceTree()
    {
        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
            "profiles":
            {
                "defaults": {
                    "name": "PROFILE DEFAULTS"
                },
                "list": [
                    {
                        "guid": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
                        "name": "CMD"
                    },
                    {
                        "guid": "{61c54bbd-2222-5271-96e7-009a87ff44bf}",
                        "name": "PowerShell"
                    },
                    {
                        "guid": "{61c54bbd-3333-5271-96e7-009a87ff44bf}"
                    }
                ]
            }
        })" };

        VerifyParseSucceeded(settingsJson);

        auto settings{ winrt::make_self<implementation::CascadiaSettings>() };
        settings->_ParseJsonString(settingsJson, false);
        settings->_ApplyDefaultsFromUserSettings();
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        const auto copy{ settings->Copy() };
        const auto copyImpl{ winrt::get_self<implementation::CascadiaSettings>(copy) };

        // test globals
        VERIFY_ARE_EQUAL(settings->_globals->DefaultProfile(), copyImpl->_globals->DefaultProfile());

        // test profiles
        VERIFY_ARE_EQUAL(settings->_allProfiles.Size(), copyImpl->_allProfiles.Size());
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(0).Name(), copyImpl->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(1).Name(), copyImpl->_allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(2).Name(), copyImpl->_allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(settings->_userDefaultProfileSettings->Name(), copyImpl->_userDefaultProfileSettings->Name());

        // Modifying profile.defaults should...
        VERIFY_ARE_EQUAL(settings->_userDefaultProfileSettings->HasName(), copyImpl->_userDefaultProfileSettings->HasName());
        copyImpl->_userDefaultProfileSettings->Name(L"changed value");

        // ...keep the same name for the first two profiles
        VERIFY_ARE_EQUAL(settings->_allProfiles.Size(), copyImpl->_allProfiles.Size());
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(0).Name(), copyImpl->_allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(settings->_allProfiles.GetAt(1).Name(), copyImpl->_allProfiles.GetAt(1).Name());

        // ...but change the name for the one that inherited it from profile.defaults
        VERIFY_ARE_NOT_EQUAL(settings->_allProfiles.GetAt(2).Name(), copyImpl->_allProfiles.GetAt(2).Name());

        // profile.defaults should be different between the two graphs
        VERIFY_ARE_EQUAL(settings->_userDefaultProfileSettings->HasName(), copyImpl->_userDefaultProfileSettings->HasName());
        VERIFY_ARE_NOT_EQUAL(settings->_userDefaultProfileSettings->Name(), copyImpl->_userDefaultProfileSettings->Name());

        Log::Comment(L"Test empty profiles.defaults");
        const std::string emptyPDJson{ R"(
        {
            "defaultProfile": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
            "profiles":
            {
                "defaults": {
                },
                "list": [
                    {
                        "guid": "{61c54bbd-2222-5271-96e7-009a87ff44bf}",
                        "name": "PowerShell"
                    }
                ]
            }
        })" };

        const std::string missingPDJson{ R"(
        {
            "defaultProfile": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
            "profiles":
            [
                {
                    "guid": "{61c54bbd-2222-5271-96e7-009a87ff44bf}",
                    "name": "PowerShell"
                }
            ]
        })" };

        auto verifyEmptyPD = [this](const std::string json) {
            VerifyParseSucceeded(json);

            auto settings{ winrt::make_self<implementation::CascadiaSettings>() };
            settings->_ParseJsonString(json, false);
            settings->_ApplyDefaultsFromUserSettings();
            settings->LayerJson(settings->_userSettings);
            settings->_ValidateSettings();

            const auto copy{ settings->Copy() };
            const auto copyImpl{ winrt::get_self<implementation::CascadiaSettings>(copy) };

            // if we don't have profiles.defaults, it should still be in the tree
            VERIFY_IS_NOT_NULL(settings->_userDefaultProfileSettings);
            VERIFY_IS_NOT_NULL(copyImpl->_userDefaultProfileSettings);

            VERIFY_ARE_EQUAL(settings->ActiveProfiles().Size(), 1u);
            VERIFY_ARE_EQUAL(settings->ActiveProfiles().Size(), copyImpl->ActiveProfiles().Size());

            // so we should only have one parent, instead of two
            auto srcProfile{ winrt::get_self<implementation::Profile>(settings->ActiveProfiles().GetAt(0)) };
            auto copyProfile{ winrt::get_self<implementation::Profile>(copyImpl->ActiveProfiles().GetAt(0)) };
            VERIFY_ARE_EQUAL(srcProfile->Parents().size(), 1u);
            VERIFY_ARE_EQUAL(srcProfile->Parents().size(), copyProfile->Parents().size());
        };

        verifyEmptyPD(emptyPDJson);
        verifyEmptyPD(missingPDJson);
    }

    void DeserializationTests::TestValidDefaults()
    {
        // GH#8146: A LoadDefaults call should populate the list of active profiles

        const auto settings{ CascadiaSettings::LoadDefaults() };
        VERIFY_ARE_EQUAL(settings.ActiveProfiles().Size(), settings.AllProfiles().Size());
        VERIFY_ARE_EQUAL(settings.AllProfiles().Size(), 2u);
    }

    void DeserializationTests::TestInheritedCommand()
    {
        // Test unbinding a command's key chord or name that originated in another layer.

        const std::string settings1Json{ R"(
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
                    "name": "foo",
                    "command": "closePane",
                    "keys": "ctrl+shift+w"
                }
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        const std::string settings2Json{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "actions": [
                {
                    "command": null,
                    "keys": "ctrl+shift+w"
                },
            ],
        })" };

        const std::string settings3Json{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "actions": [
                {
                    "name": "bar",
                    "command": "closePane"
                },
            ],
        })" };

        VerifyParseSucceeded(settings1Json);
        VerifyParseSucceeded(settings2Json);
        VerifyParseSucceeded(settings3Json);

        auto settings = winrt::make_self<implementation::CascadiaSettings>();
        settings->_ParseJsonString(settings1Json, false);
        settings->LayerJson(settings->_userSettings);

        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());

        settings->_ValidateSettings();
        auto nameMap{ settings->ActionMap().NameMap() };
        _logCommandNames(nameMap);

        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        const KeyChord expectedKeyChord{ true, false, true, static_cast<int>('W') };
        {
            // Verify NameMap returns correct value
            const auto& cmd{ nameMap.TryLookup(L"foo") };
            VERIFY_IS_NOT_NULL(cmd);
            VERIFY_IS_NOT_NULL(cmd.Keys());
            VERIFY_IS_TRUE(cmd.Keys().Modifiers() == expectedKeyChord.Modifiers() && cmd.Keys().Vkey() == expectedKeyChord.Vkey());
        }
        {
            // Verify ActionMap::GetActionByKeyChord API
            const auto& cmd{ settings->ActionMap().GetActionByKeyChord(expectedKeyChord) };
            VERIFY_IS_NOT_NULL(cmd);
            VERIFY_IS_NOT_NULL(cmd.Keys());
            VERIFY_IS_TRUE(cmd.Keys().Modifiers() == expectedKeyChord.Modifiers() && cmd.Keys().Vkey() == expectedKeyChord.Vkey());
        }
        {
            // Verify ActionMap::GetKeyBindingForAction API
            const auto& actualKeyChord{ settings->ActionMap().GetKeyBindingForAction(ShortcutAction::ClosePane) };
            VERIFY_IS_NOT_NULL(actualKeyChord);
            VERIFY_IS_TRUE(actualKeyChord.Modifiers() == expectedKeyChord.Modifiers() && actualKeyChord.Vkey() == expectedKeyChord.Vkey());
        }

        Log::Comment(L"Layer second bit of json, to unbind the key chord of the original command.");

        settings->_ParseJsonString(settings2Json, false);
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        nameMap = settings->ActionMap().NameMap();
        _logCommandNames(nameMap);
        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(1u, nameMap.Size());
        {
            // Verify NameMap returns correct value
            const auto& cmd{ nameMap.TryLookup(L"foo") };
            VERIFY_IS_NOT_NULL(cmd);
            VERIFY_IS_NULL(cmd.Keys());
        }
        {
            // Verify ActionMap::GetActionByKeyChord API
            const auto& cmd{ settings->ActionMap().GetActionByKeyChord(expectedKeyChord) };
            VERIFY_IS_NULL(cmd);
        }
        {
            // Verify ActionMap::GetKeyBindingForAction API
            const auto& actualKeyChord{ settings->ActionMap().GetKeyBindingForAction(ShortcutAction::ClosePane) };
            VERIFY_IS_NULL(actualKeyChord);
        }

        Log::Comment(L"Layer third bit of json, to unbind name of the original command.");

        settings->_ParseJsonString(settings3Json, false);
        settings->LayerJson(settings->_userSettings);
        settings->_ValidateSettings();

        nameMap = settings->ActionMap().NameMap();
        _logCommandNames(nameMap);
        VERIFY_ARE_EQUAL(0u, settings->_warnings.Size());
        VERIFY_ARE_EQUAL(1u, nameMap.Size());
        {
            // Verify NameMap returns correct value
            const auto& cmd{ nameMap.TryLookup(L"bar") };
            VERIFY_IS_NOT_NULL(cmd);
            VERIFY_IS_NULL(cmd.Keys());
            VERIFY_ARE_EQUAL(L"bar", cmd.Name());
        }
        {
            // Verify ActionMap::GetActionByKeyChord API
            const auto& cmd{ settings->ActionMap().GetActionByKeyChord(expectedKeyChord) };
            VERIFY_IS_NULL(cmd);
        }
        {
            // Verify ActionMap::GetKeyBindingForAction API
            const auto& actualKeyChord{ settings->ActionMap().GetKeyBindingForAction(ShortcutAction::ClosePane) };
            VERIFY_IS_NULL(actualKeyChord);
        }
    }
}
