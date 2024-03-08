// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "JsonTestClass.h"
#include "TestUtils.h"

#include <defaults.h>
#include <userDefaults.h>

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using VirtualKeyModifiers = winrt::Windows::System::VirtualKeyModifiers;

namespace SettingsModelUnitTests
{
    class DeserializationTests : public JsonTestClass
    {
        TEST_CLASS(DeserializationTests);

        TEST_METHOD(ValidateProfilesExist);
        TEST_METHOD(ValidateDefaultProfileExists);
        TEST_METHOD(ValidateDuplicateProfiles);
        TEST_METHOD(ValidateManyWarnings);
        TEST_METHOD(LayerGlobalProperties);
        TEST_METHOD(ValidateProfileOrdering);
        TEST_METHOD(ValidateHideProfiles);
        TEST_METHOD(TestReorderWithNullGuids);
        TEST_METHOD(TestReorderingWithoutGuid);
        TEST_METHOD(TestLayeringNameOnlyProfiles);
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
        TEST_METHOD(LoadFragmentsWithMultipleUpdates);

        TEST_METHOD(FragmentActionSimple);
        TEST_METHOD(FragmentActionNoKeys);
        TEST_METHOD(FragmentActionNested);
        TEST_METHOD(FragmentActionNestedNoName);
        TEST_METHOD(FragmentActionIterable);
        TEST_METHOD(FragmentActionRoundtrip);

        TEST_METHOD(MigrateReloadEnvVars);

    private:
        static winrt::com_ptr<implementation::CascadiaSettings> createSettings(const std::string_view& userJSON)
        {
            static constexpr std::string_view inboxJSON{ R"({
                "schemes": [
                    {
                        "name": "Campbell",
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
                ]
            })" };

            return winrt::make_self<implementation::CascadiaSettings>(userJSON, inboxJSON);
        }

        static void _logCommandNames(winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, Command> commands, const int indentation = 1)
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
        static constexpr std::string_view settingsWithProfiles{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0"
                }
            ]
        })" };

        static constexpr std::string_view settingsWithoutProfiles{ R"(
        {
            "defaultProfile": "{6239a42c-1de4-49a3-80bd-e8fdd045185c}"
        })" };

        static constexpr std::string_view settingsWithEmptyProfiles{ R"(
        {
            "profiles": []
        })" };

        {
            // Case 1: Good settings
            auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsWithProfiles);
        }
        {
            // Case 2: Bad settings
            auto caughtExpectedException = false;
            try
            {
                auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsWithoutProfiles);
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
            auto caughtExpectedException = false;
            try
            {
                auto settings = winrt::make_self<implementation::CascadiaSettings>(settingsWithEmptyProfiles);
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
        static constexpr std::string_view goodProfiles{ R"(
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

        static constexpr std::string_view badProfiles{ R"(
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

        static constexpr std::string_view goodProfilesSpecifiedByName{ R"(
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
            const auto settings = createSettings(goodProfiles);
            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings->Warnings().Size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->AllProfiles().Size());
            VERIFY_ARE_EQUAL(settings->GlobalSettings().DefaultProfile(), settings->AllProfiles().GetAt(0).Guid());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, but the defaultProfile is NOT one of those guids"));
            const auto settings = createSettings(badProfiles);
            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->Warnings().Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingDefaultProfile, settings->Warnings().GetAt(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->AllProfiles().Size());
            VERIFY_ARE_EQUAL(settings->GlobalSettings().DefaultProfile(), settings->AllProfiles().GetAt(0).Guid());
        }
        {
            // Case 2: Bad settings
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, and no defaultProfile at all"));
            const auto settings = createSettings(badProfiles);
            VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->Warnings().Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingDefaultProfile, settings->Warnings().GetAt(0));

            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->AllProfiles().Size());
            VERIFY_ARE_EQUAL(settings->GlobalSettings().DefaultProfile(), settings->AllProfiles().GetAt(0).Guid());
        }
        {
            // Case 4: Good settings, default profile is a string
            Log::Comment(NoThrowString().Format(
                L"Testing a pair of profiles with unique guids, and the defaultProfile is one of the profile names"));
            const auto settings = createSettings(goodProfilesSpecifiedByName);
            VERIFY_ARE_EQUAL(static_cast<size_t>(0), settings->Warnings().Size());
            VERIFY_ARE_EQUAL(static_cast<size_t>(2), settings->AllProfiles().Size());
            VERIFY_ARE_EQUAL(settings->GlobalSettings().DefaultProfile(), settings->AllProfiles().GetAt(1).Guid());
        }
    }

    void DeserializationTests::ValidateDuplicateProfiles()
    {
        static constexpr std::string_view veryBadProfiles{ R"(
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

        const auto settings = createSettings(veryBadProfiles);

        VERIFY_ARE_EQUAL(static_cast<size_t>(1), settings->Warnings().Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::DuplicateProfile, settings->Warnings().GetAt(0));

        VERIFY_ARE_EQUAL(static_cast<size_t>(4), settings->AllProfiles().Size());
        VERIFY_ARE_EQUAL(L"profile0", settings->AllProfiles().GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"profile1", settings->AllProfiles().GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"profile4", settings->AllProfiles().GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"profile6", settings->AllProfiles().GetAt(3).Name());
    }

    void DeserializationTests::ValidateManyWarnings()
    {
        static constexpr std::string_view badProfiles{ R"(
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
                },
                {
                    "name" : "profile3",
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name" : "profile4",
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const auto settings = createSettings(badProfiles);

        VERIFY_ARE_EQUAL(2u, settings->Warnings().Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::DuplicateProfile, settings->Warnings().GetAt(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingDefaultProfile, settings->Warnings().GetAt(1));

        VERIFY_ARE_EQUAL(3u, settings->AllProfiles().Size());
        VERIFY_ARE_EQUAL(settings->AllProfiles().GetAt(0).Guid(), settings->GlobalSettings().DefaultProfile());
    }

    void DeserializationTests::LayerGlobalProperties()
    {
        static constexpr std::string_view inboxSettings{ R"({
            "alwaysShowTabs": true,
            "initialCols" : 120,
            "initialRows" : 30
        })" };
        static constexpr std::string_view userSettings{ R"({
            "showTabsInTitlebar": false,
            "initialCols" : 240,
            "initialRows" : 60,
            "profiles": [
                {
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(userSettings, inboxSettings);
        VERIFY_ARE_EQUAL(true, settings->GlobalSettings().AlwaysShowTabs());
        VERIFY_ARE_EQUAL(240, settings->GlobalSettings().InitialCols());
        VERIFY_ARE_EQUAL(60, settings->GlobalSettings().InitialRows());
        VERIFY_ARE_EQUAL(false, settings->GlobalSettings().ShowTabsInTitlebar());
    }

    void DeserializationTests::ValidateProfileOrdering()
    {
        static constexpr std::string_view userProfiles0String{ R"(
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

        static constexpr std::string_view defaultProfilesString{ R"(
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

        static constexpr std::string_view userProfiles1String{ R"(
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

        {
            Log::Comment(NoThrowString().Format(
                L"Case 1: Simple swapping of the ordering. The user has the "
                L"default profiles in the opposite order of the default ordering."));

            const auto settings = winrt::make_self<implementation::CascadiaSettings>(userProfiles0String, defaultProfilesString);
            VERIFY_ARE_EQUAL(2u, settings->AllProfiles().Size());
            VERIFY_ARE_EQUAL(L"profile0", settings->AllProfiles().GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile1", settings->AllProfiles().GetAt(1).Name());
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Case 2: Make sure all the user's profiles appear before the defaults."));

            const auto settings = winrt::make_self<implementation::CascadiaSettings>(userProfiles1String, defaultProfilesString);
            VERIFY_ARE_EQUAL(3u, settings->AllProfiles().Size());
            VERIFY_ARE_EQUAL(L"profile4", settings->AllProfiles().GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile5", settings->AllProfiles().GetAt(1).Name());
            VERIFY_ARE_EQUAL(L"profile2", settings->AllProfiles().GetAt(2).Name());
        }
    }

    void DeserializationTests::ValidateHideProfiles()
    {
        static constexpr std::string_view defaultProfilesString{ R"(
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

        static constexpr std::string_view userProfiles0String{ R"(
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

        static constexpr std::string_view userProfiles1String{ R"(
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

        {
            const auto settings = winrt::make_self<implementation::CascadiaSettings>(userProfiles0String, defaultProfilesString);
            VERIFY_ARE_EQUAL(2u, settings->AllProfiles().Size());
            VERIFY_ARE_EQUAL(1u, settings->ActiveProfiles().Size());
            VERIFY_ARE_EQUAL(L"profile1", settings->ActiveProfiles().GetAt(0).Name());
            VERIFY_ARE_EQUAL(false, settings->ActiveProfiles().GetAt(0).Hidden());
        }

        {
            const auto settings = winrt::make_self<implementation::CascadiaSettings>(userProfiles1String, defaultProfilesString);
            VERIFY_ARE_EQUAL(4u, settings->AllProfiles().Size());
            VERIFY_ARE_EQUAL(2u, settings->ActiveProfiles().Size());
            VERIFY_ARE_EQUAL(L"profile5", settings->ActiveProfiles().GetAt(0).Name());
            VERIFY_ARE_EQUAL(L"profile2", settings->ActiveProfiles().GetAt(1).Name());
            VERIFY_ARE_EQUAL(false, settings->ActiveProfiles().GetAt(0).Hidden());
            VERIFY_ARE_EQUAL(false, settings->ActiveProfiles().GetAt(1).Hidden());
        }
    }

    void DeserializationTests::TestReorderWithNullGuids()
    {
        static constexpr std::string_view settings0String{ R"(
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

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settings0String, DefaultJson);

        VERIFY_ARE_EQUAL(0u, settings->Warnings().Size());
        VERIFY_ARE_EQUAL(4u, settings->AllProfiles().Size());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(1).HasGuid());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(2).HasGuid());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(3).HasGuid());
        VERIFY_ARE_EQUAL(L"profile0", settings->AllProfiles().GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"profile1", settings->AllProfiles().GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"cmdFromUserSettings", settings->AllProfiles().GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->AllProfiles().GetAt(3).Name());
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

        static constexpr std::string_view settings0String{ R"(
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

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settings0String, DefaultJson);

        VERIFY_ARE_EQUAL(0u, settings->Warnings().Size());
        VERIFY_ARE_EQUAL(4u, settings->AllProfiles().Size());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(0).HasGuid());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(1).HasGuid());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(2).HasGuid());
        VERIFY_IS_TRUE(settings->AllProfiles().GetAt(3).HasGuid());
        VERIFY_ARE_EQUAL(L"Command Prompt", settings->AllProfiles().GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotCrash", settings->AllProfiles().GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"Ubuntu", settings->AllProfiles().GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", settings->AllProfiles().GetAt(3).Name());
    }

    void DeserializationTests::TestLayeringNameOnlyProfiles()
    {
        // This is a test discovered during GH#2782. When we add a name-only
        // profile, it should only layer with other name-only profiles with the
        // _same name_

        static constexpr std::string_view settings0String{ R"(
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

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settings0String, DefaultJson);
        const auto profiles = settings->AllProfiles();
        VERIFY_ARE_EQUAL(5u, profiles.Size());
        VERIFY_ARE_EQUAL(L"ThisProfileIsGood", profiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"ThisProfileShouldNotLayer", profiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"NeitherShouldThisOne", profiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"Windows PowerShell", profiles.GetAt(3).Name());
        VERIFY_ARE_EQUAL(L"Command Prompt", profiles.GetAt(4).Name());
    }

    void DeserializationTests::TestHideAllProfiles()
    {
        static constexpr std::string_view settingsWithProfiles{ R"(
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

        static constexpr std::string_view settingsWithoutProfiles{ R"(
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

        {
            // Case 1: Good settings
            const auto settings = createSettings(settingsWithProfiles);
            VERIFY_ARE_EQUAL(2u, settings->AllProfiles().Size());
            VERIFY_ARE_EQUAL(1u, settings->ActiveProfiles().Size());
        }
        {
            // Case 2: Bad settings
            VERIFY_THROWS_SPECIFIC(winrt::make_self<implementation::CascadiaSettings>(settingsWithoutProfiles), const implementation::SettingsException, [](const auto& ex) {
                return ex.Error() == SettingsLoadErrors::AllProfilesHidden;
            });
        }
    }

    void DeserializationTests::TestInvalidColorSchemeName()
    {
        Log::Comment(NoThrowString().Format(
            L"Ensure that setting a profile's scheme to a nonexistent scheme causes a warning."));

        static constexpr std::string_view settings0String{ R"({
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "Campbell"
                },
                {
                    "name" : "profile1",
                    "colorScheme": "InvalidSchemeName"
                },
                {
                    "name" : "profile2"
                    // Will use the Profile default value, "Campbell"
                }
            ]
        })" };

        const auto settings = createSettings(settings0String);

        VERIFY_ARE_EQUAL(1u, settings->Warnings().Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::UnknownColorScheme, settings->Warnings().GetAt(0));

        VERIFY_ARE_EQUAL(3u, settings->AllProfiles().Size());
        for (const auto& profile : settings->AllProfiles())
        {
            VERIFY_ARE_EQUAL(L"Campbell", profile.DefaultAppearance().DarkColorSchemeName());
            VERIFY_ARE_EQUAL(L"Campbell", profile.DefaultAppearance().LightColorSchemeName());
        }
    }

    void DeserializationTests::ValidateColorSchemeInCommands()
    {
        Log::Comment(NoThrowString().Format(
            L"Ensure that setting a command's color scheme to a nonexistent scheme causes a warning."));

        static constexpr std::string_view settings0String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "Campbell"
                }
            ],
            "actions": [
                {
                    "command": { "action": "setColorScheme", "colorScheme": "Campbell" }
                },
                {
                    "command": { "action": "setColorScheme", "colorScheme": "invalidScheme" }
                }
            ]
        })" };

        static constexpr std::string_view settings1String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "Campbell"
                }
            ],
            "actions": [
                {
                    "command": { "action": "setColorScheme", "colorScheme": "Campbell" }
                },
                {
                    "name": "parent",
                    "commands": [
                        { "command": { "action": "setColorScheme", "colorScheme": "invalidScheme" } }
                    ]
                }
            ]
        })" };

        static constexpr std::string_view settings2String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "Campbell"
                }
            ],
            "actions": [
                {
                    "command": { "action": "setColorScheme", "colorScheme": "Campbell" }
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

            const auto settings = createSettings(settings0String);

            VERIFY_ARE_EQUAL(1u, settings->Warnings().Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::InvalidColorSchemeInCmd, settings->Warnings().GetAt(0));
        }
        {
            // Case 2: nested setColorScheme command with invalid scheme
            Log::Comment(NoThrowString().Format(
                L"Testing a nested command with invalid scheme"));

            const auto settings = createSettings(settings1String);

            VERIFY_ARE_EQUAL(1u, settings->Warnings().Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::InvalidColorSchemeInCmd, settings->Warnings().GetAt(0));
        }
        {
            // Case 3: nested-in-nested setColorScheme command with invalid scheme
            Log::Comment(NoThrowString().Format(
                L"Testing a nested-in-nested command with invalid scheme"));

            const auto settings = createSettings(settings2String);

            VERIFY_ARE_EQUAL(1u, settings->Warnings().Size());
            VERIFY_ARE_EQUAL(SettingsLoadWarnings::InvalidColorSchemeInCmd, settings->Warnings().GetAt(0));
        }
    }

    void DeserializationTests::TestHelperFunctions()
    {
        static constexpr std::string_view settings0String{ R"(
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

        const auto name0{ L"profile0" };
        const auto name1{ L"profile1" };
        const auto name2{ L"Ubuntu" };
        const auto name3{ L"ThisProfileShouldNotThrow" };
        const auto badName{ L"DoesNotExist" };

        const winrt::guid guid0{ Utils::GuidFromString(L"{6239a42c-5555-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid1{ Utils::GuidFromString(L"{6239a42c-6666-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{2C4DE342-38B7-51CF-B940-2309A097F518}") };
        const winrt::guid fakeGuid{ Utils::GuidFromString(L"{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}") };
        const auto autogeneratedGuid{ implementation::Profile::_GenerateGuidForProfile(name3, L"") };

        const auto settings = createSettings(settings0String);

        VERIFY_ARE_EQUAL(guid0, settings->GetProfileByName(name0).Guid());
        VERIFY_ARE_EQUAL(guid1, settings->GetProfileByName(name1).Guid());
        VERIFY_ARE_EQUAL(guid2, settings->GetProfileByName(name2).Guid());
        VERIFY_ARE_EQUAL(autogeneratedGuid, settings->GetProfileByName(name3).Guid());
        VERIFY_IS_NULL(settings->GetProfileByName(badName));

        VERIFY_ARE_EQUAL(name0, settings->FindProfile(guid0).Name());
        VERIFY_ARE_EQUAL(name1, settings->FindProfile(guid1).Name());
        VERIFY_ARE_EQUAL(name2, settings->FindProfile(guid2).Name());
        VERIFY_IS_NULL(settings->FindProfile(fakeGuid));
    }

    void DeserializationTests::TestProfileBackgroundImageWithEnvVar()
    {
        const auto expectedPath = wil::ExpandEnvironmentStringsW<std::wstring>(L"%WINDIR%\\System32\\x_80.png");

        static constexpr std::string_view settingsJson{ R"(
        {
            "profiles": [
                {
                    "name": "profile0",
                    "backgroundImage": "%WINDIR%\\System32\\x_80.png"
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        VERIFY_ARE_NOT_EQUAL(0u, settings->AllProfiles().Size());
        VERIFY_ARE_EQUAL(expectedPath, settings->AllProfiles().GetAt(0).DefaultAppearance().ExpandedBackgroundImagePath());
    }

    void DeserializationTests::TestProfileBackgroundImageWithDesktopWallpaper()
    {
        const winrt::hstring expectedBackgroundImagePath{ L"desktopWallpaper" };

        static constexpr std::string_view settingsJson{ R"(
        {
            "profiles": [
                {
                    "name": "profile0",
                    "backgroundImage": "desktopWallpaper"
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        VERIFY_ARE_EQUAL(expectedBackgroundImagePath, settings->AllProfiles().GetAt(0).DefaultAppearance().BackgroundImagePath());
        VERIFY_ARE_NOT_EQUAL(expectedBackgroundImagePath, settings->AllProfiles().GetAt(0).DefaultAppearance().ExpandedBackgroundImagePath());
    }

    void DeserializationTests::TestCloseOnExitParsing()
    {
        static constexpr std::string_view settingsJson{ R"(
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
                    "closeOnExit": "automatic"
                },
                {
                    "name": "profile4",
                    "closeOnExit": null
                }
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings->AllProfiles().GetAt(0).CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Always, settings->AllProfiles().GetAt(1).CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Never, settings->AllProfiles().GetAt(2).CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Automatic, settings->AllProfiles().GetAt(3).CloseOnExit());

        // Unknown modes parse as "Automatic"
        VERIFY_ARE_EQUAL(CloseOnExitMode::Automatic, settings->AllProfiles().GetAt(4).CloseOnExit());
    }

    void DeserializationTests::TestCloseOnExitCompatibilityShim()
    {
        static constexpr std::string_view settingsJson{ R"(
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

        const auto settings = createSettings(settingsJson);
        VERIFY_ARE_EQUAL(CloseOnExitMode::Graceful, settings->AllProfiles().GetAt(0).CloseOnExit());
        VERIFY_ARE_EQUAL(CloseOnExitMode::Never, settings->AllProfiles().GetAt(1).CloseOnExit());
    }

    void DeserializationTests::TestLayerUserDefaultsBeforeProfiles()
    {
        // Test for microsoft/terminal#2325. For this test, we'll be setting the
        // "historySize" in the "defaultSettings", so it should apply to all
        // profiles, unless they override it. In one of the user's profiles,
        // we'll override that value, and in the other, we'll leave it
        // untouched.

        static constexpr std::string_view settings0String{ R"(
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

        const auto settings = createSettings(settings0String);

        VERIFY_IS_NOT_NULL(settings->ProfileDefaults());

        VERIFY_ARE_EQUAL(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}", settings->GlobalSettings().UnparsedDefaultProfile());
        VERIFY_ARE_EQUAL(2u, settings->AllProfiles().Size());

        VERIFY_ARE_EQUAL(2345, settings->AllProfiles().GetAt(0).HistorySize());
        VERIFY_ARE_EQUAL(1234, settings->AllProfiles().GetAt(1).HistorySize());
    }

    void DeserializationTests::TestDontLayerGuidFromUserDefaults()
    {
        // Test for microsoft/terminal#2325. We don't want the user to put a
        // "guid" in the "defaultSettings", and have that apply to all the other
        // profiles

        static constexpr std::string_view settings0String{ R"({
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

        const auto guid1String = L"{6239a42c-1111-49a3-80bd-e8fdd045185c}";
        const winrt::guid guid1{ Utils::GuidFromString(guid1String) };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settings0String, DefaultJson);

        VERIFY_ARE_EQUAL(guid1String, settings->GlobalSettings().UnparsedDefaultProfile());
        VERIFY_ARE_EQUAL(4u, settings->AllProfiles().Size());
        VERIFY_ARE_EQUAL(guid1, settings->AllProfiles().GetAt(0).Guid());
        VERIFY_ARE_NOT_EQUAL(guid1, settings->AllProfiles().GetAt(1).Guid());
    }

    void DeserializationTests::TestLayerUserDefaultsOnDynamics()
    {
        // Test for microsoft/terminal#2325. For this test, we'll be setting the
        // "historySize" in the "defaultSettings", so it should apply to all
        // profiles, unless they override it. The dynamic profiles will _also_
        // set this value, but from discussion in GH#2325, we decided that
        // settings in defaultSettings should apply _on top_ of settings from
        // dynamic profiles.

        const winrt::guid guid1{ Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid3{ Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid4{ Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}") };

        static constexpr std::string_view dynamicProfiles{ R"({
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.0",
                    "historySize": 1111
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.1",
                    "historySize": 2222
                },
                {
                    "name": "profile2",
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.1",
                    "historySize": 4444
                }
            ]
        })" };

        static constexpr std::string_view userProfiles{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": {
                "defaults": {
                    "historySize": 1234
                },
                "list": [
                    {
                        "name" : "profile0FromUserSettings",
                        "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                        "source": "Terminal.App.UnitTest.0"
                    },
                    {
                        "name" : "profile1FromUserSettings",
                        "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                        "source": "Terminal.App.UnitTest.1",
                        "historySize": 4444
                    },
                    {
                        "name" : "profile2FromUserSettings",
                        "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
                        "historySize": 5555
                    }
                ]
            }
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(userProfiles, dynamicProfiles);
        const auto allProfiles = settings->AllProfiles();

        Log::Comment(NoThrowString().Format(
            L"All profiles with the same name have the same GUID. However, they"
            L" will not be layered, because they have different source's"));

        VERIFY_ARE_EQUAL(4u, allProfiles.Size());

        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.0", allProfiles.GetAt(0).Source());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", allProfiles.GetAt(1).Source());
        VERIFY_ARE_EQUAL(L"", allProfiles.GetAt(2).Source());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", allProfiles.GetAt(3).Source());

        VERIFY_ARE_EQUAL(guid1, allProfiles.GetAt(0).Guid());
        VERIFY_ARE_EQUAL(guid2, allProfiles.GetAt(1).Guid());
        VERIFY_ARE_EQUAL(guid3, allProfiles.GetAt(2).Guid());
        VERIFY_ARE_EQUAL(guid4, allProfiles.GetAt(3).Guid());

        VERIFY_ARE_EQUAL(L"profile0FromUserSettings", allProfiles.GetAt(0).Name());
        VERIFY_ARE_EQUAL(L"profile1FromUserSettings", allProfiles.GetAt(1).Name());
        VERIFY_ARE_EQUAL(L"profile2FromUserSettings", allProfiles.GetAt(2).Name());
        VERIFY_ARE_EQUAL(L"profile2", allProfiles.GetAt(3).Name());

        Log::Comment(NoThrowString().Format(
            L"This is the real meat of the test: The two dynamic profiles that "
            L"_didn't_ have historySize set in the userSettings should have "
            L"1234 as their historySize(from the defaultSettings).The other two"
            L" profiles should have their custom historySize value."));

        VERIFY_ARE_EQUAL(1234, allProfiles.GetAt(0).HistorySize());
        VERIFY_ARE_EQUAL(4444, allProfiles.GetAt(1).HistorySize());
        VERIFY_ARE_EQUAL(5555, allProfiles.GetAt(2).HistorySize());
        VERIFY_ARE_EQUAL(1234, allProfiles.GetAt(3).HistorySize());
    }

    void DeserializationTests::FindMissingProfile()
    {
        // Test that CascadiaSettings::FindProfile returns null for a GUID that
        // doesn't exist
        static constexpr std::string_view settingsString{ R"(
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
        const auto settings = createSettings(settingsString);

        const auto guid1 = Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

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
        static constexpr std::string_view badSettings{ R"(
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

        const auto settings = createSettings(badSettings);

        // KeyMap: ctrl+a/b are mapped to "invalid"
        // ActionMap: "splitPane" and "invalid" are the only deserialized actions
        // NameMap: "splitPane" has no key binding, but it is still added to the name map
        const auto actionMap = winrt::get_self<implementation::ActionMap>(settings->GlobalSettings().ActionMap());
        VERIFY_ARE_EQUAL(2u, actionMap->_KeyMap.size());
        VERIFY_ARE_EQUAL(2u, actionMap->_ActionMap.size());
        VERIFY_ARE_EQUAL(1u, actionMap->NameMap().Size());
        VERIFY_ARE_EQUAL(5u, settings->Warnings().Size());

        const auto globalAppSettings = winrt::get_self<implementation::GlobalAppSettings>(settings->GlobalSettings());
        const auto& keybindingsWarnings = globalAppSettings->KeybindingsWarnings();
        VERIFY_ARE_EQUAL(4u, keybindingsWarnings.size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::TooManyKeysForChord, keybindingsWarnings.at(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, keybindingsWarnings.at(1));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, keybindingsWarnings.at(2));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::FailedToParseSubCommands, keybindingsWarnings.at(3));

        VERIFY_ARE_EQUAL(SettingsLoadWarnings::AtLeastOneKeybindingWarning, settings->Warnings().GetAt(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::TooManyKeysForChord, settings->Warnings().GetAt(1));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->Warnings().GetAt(2));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->Warnings().GetAt(3));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::FailedToParseSubCommands, settings->Warnings().GetAt(4));
    }

    void DeserializationTests::ValidateExecuteCommandlineWarning()
    {
        static constexpr std::string_view badSettings{ R"(
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

        const auto settings = createSettings(badSettings);

        const auto actionMap = winrt::get_self<implementation::ActionMap>(settings->GlobalSettings().ActionMap());
        VERIFY_ARE_EQUAL(3u, actionMap->_KeyMap.size());
        VERIFY_IS_NULL(actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Control, static_cast<int32_t>('A'), 0 }));
        VERIFY_IS_NULL(actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Control, static_cast<int32_t>('B'), 0 }));
        VERIFY_IS_NULL(actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Control, static_cast<int32_t>('C'), 0 }));

        const auto globalAppSettings = winrt::get_self<implementation::GlobalAppSettings>(settings->GlobalSettings());
        const auto& keybindingsWarnings = globalAppSettings->KeybindingsWarnings();
        VERIFY_ARE_EQUAL(3u, keybindingsWarnings.size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, keybindingsWarnings.at(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, keybindingsWarnings.at(1));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, keybindingsWarnings.at(2));

        VERIFY_ARE_EQUAL(4u, settings->Warnings().Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::AtLeastOneKeybindingWarning, settings->Warnings().GetAt(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->Warnings().GetAt(1));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->Warnings().GetAt(2));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::MissingRequiredParameter, settings->Warnings().GetAt(3));
    }

    void DeserializationTests::TestTrailingCommas()
    {
        static constexpr std::string_view badSettings{ R"({
            "profiles": [{ "name": "profile0" }],
        })" };

        try
        {
            const auto settings = createSettings(badSettings);
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to LayerJson should succeed, even with the trailing comma");
        }
    }

    void DeserializationTests::TestCommandsAndKeybindings()
    {
        static constexpr std::string_view settingsJson{ R"(
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

        const auto settings = createSettings(settingsJson);

        VERIFY_ARE_EQUAL(3u, settings->AllProfiles().Size());

        const auto profile2Guid = settings->AllProfiles().GetAt(2).Guid();
        VERIFY_ARE_NOT_EQUAL(winrt::guid{}, profile2Guid);

        auto actionMap = winrt::get_self<implementation::ActionMap>(settings->GlobalSettings().ActionMap());
        VERIFY_ARE_EQUAL(5u, actionMap->KeyBindings().Size());

        // A/D, B, C, E will be in the list of commands, for 4 total.
        // * A and D share the same name, so they'll only generate a single action.
        // * F's name is set manually to `null`
        const auto& nameMap{ actionMap->NameMap() };
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        {
            const KeyChord kc{ true, false, false, false, static_cast<int32_t>('A'), 0 };
            const auto actionAndArgs = TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }

        Log::Comment(L"Note that we're skipping ctrl+B, since that doesn't have `keys` set.");

        {
            const KeyChord kc{ true, false, false, false, static_cast<int32_t>('C'), 0 };
            const auto actionAndArgs = TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }
        {
            const KeyChord kc{ true, false, false, false, static_cast<int32_t>('D'), 0 };
            const auto actionAndArgs = TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }
        {
            const KeyChord kc{ true, false, false, false, static_cast<int32_t>('E'), 0 };
            const auto actionAndArgs = TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Down, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }
        {
            const KeyChord kc{ true, false, false, false, static_cast<int32_t>('F'), 0 };
            const auto actionAndArgs = TestUtils::GetActionAndArgs(*actionMap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Down, realArgs.SplitDirection());
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
            const auto command = nameMap.TryLookup(L"Split pane, split: vertical");
            VERIFY_IS_NULL(command);
        }
        {
            // This was renamed to "ctrl+c" in C. So this does not exist.
            const auto command = nameMap.TryLookup(L"ctrl+b");
            VERIFY_IS_NULL(command);
        }
        {
            const auto command = nameMap.TryLookup(L"ctrl+c");
            VERIFY_IS_NOT_NULL(command);
            const auto actionAndArgs = command.ActionAndArgs();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitDirection::Right, realArgs.SplitDirection());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
        }
        {
            // This was renamed to null (aka removed from the name map) in F. So this does not exist.
            const auto command = nameMap.TryLookup(L"Split pane, split: horizontal");
            VERIFY_IS_NULL(command);
        }
    }

    void DeserializationTests::TestNestedCommandWithoutName()
    {
        // This test tests a nested command without a name specified. This type
        // of command should just be ignored, since we can't auto-generate names
        // for nested commands, they _must_ have names specified.

        static constexpr std::string_view settingsJson{ R"(
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
            ]
        })" };

        const auto settings = createSettings(settingsJson);
        VERIFY_ARE_EQUAL(0u, settings->Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings->AllProfiles().Size());
        // Because the "parent" command didn't have a name, it couldn't be
        // placed into the list of commands. It and its children are just
        // ignored.
        VERIFY_ARE_EQUAL(0u, settings->ActionMap().NameMap().Size());
    }

    void DeserializationTests::TestNestedCommandWithBadSubCommands()
    {
        // This test tests a nested command without a name specified. This type
        // of command should just be ignored, since we can't auto-generate names
        // for nested commands, they _must_ have names specified.

        static constexpr std::string_view settingsJson{ R"(
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
            ]
        })" };

        const auto settings = createSettings(settingsJson);

        VERIFY_ARE_EQUAL(2u, settings->Warnings().Size());
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::AtLeastOneKeybindingWarning, settings->Warnings().GetAt(0));
        VERIFY_ARE_EQUAL(SettingsLoadWarnings::FailedToParseSubCommands, settings->Warnings().GetAt(1));
        const auto& nameMap{ settings->ActionMap().NameMap() };
        VERIFY_ARE_EQUAL(0u, nameMap.Size());
    }

    void DeserializationTests::TestUnbindNestedCommand()
    {
        // Test that layering a command with `"commands": null` set will unbind a command that already exists.

        static constexpr std::string_view settingsJson{ R"(
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
            ]
        })" };

        static constexpr std::string_view settings1Json{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "actions": [
                {
                    "name": "parent",
                    "commands": null
                },
            ],
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settings1Json, settingsJson);
        VERIFY_ARE_EQUAL(3u, settings->AllProfiles().Size());
        VERIFY_ARE_EQUAL(0u, settings->ActionMap().NameMap().Size());
    }

    void DeserializationTests::TestRebindNestedCommand()
    {
        // Test that layering a command with an action set on top of a command
        // with nested commands replaces the nested commands with an action.

        static constexpr std::string_view settingsJson{ R"(
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
            ]
        })" };

        static constexpr std::string_view settings1Json{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "actions": [
                {
                    "name": "parent",
                    "command": "newTab"
                },
            ],
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settings1Json, settingsJson);

        const auto nameMap = settings->ActionMap().NameMap();
        VERIFY_ARE_EQUAL(1u, nameMap.Size());

        {
            const winrt::hstring commandName{ L"parent" };
            const auto commandProj = nameMap.TryLookup(commandName);

            VERIFY_IS_NOT_NULL(commandProj);
            const auto actionAndArgs = commandProj.ActionAndArgs();
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
        static constexpr std::string_view settingsJson{ R"(
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

        const auto settings{ winrt::make_self<implementation::CascadiaSettings>(settingsJson) };
        const auto copy{ settings->Copy() };
        const auto copyImpl{ winrt::get_self<implementation::CascadiaSettings>(copy) };

        // test globals
        VERIFY_ARE_EQUAL(settings->GlobalSettings().DefaultProfile(), copyImpl->GlobalSettings().DefaultProfile());

        // test profiles
        VERIFY_ARE_EQUAL(settings->AllProfiles().Size(), copyImpl->AllProfiles().Size());
        VERIFY_ARE_EQUAL(settings->AllProfiles().GetAt(0).Name(), copyImpl->AllProfiles().GetAt(0).Name());

        // test schemes
        const auto schemeName{ L"Campbell, but for a test" };
        VERIFY_ARE_EQUAL(settings->GlobalSettings().ColorSchemes().Size(), copyImpl->GlobalSettings().ColorSchemes().Size());
        VERIFY_ARE_EQUAL(settings->GlobalSettings().ColorSchemes().HasKey(schemeName), copyImpl->GlobalSettings().ColorSchemes().HasKey(schemeName));

        // test actions
        VERIFY_ARE_EQUAL(settings->GlobalSettings().ActionMap().KeyBindings().Size(), copyImpl->GlobalSettings().ActionMap().KeyBindings().Size());
        const auto& nameMapOriginal{ settings->GlobalSettings().ActionMap().NameMap() };
        const auto& nameMapCopy{ copyImpl->GlobalSettings().ActionMap().NameMap() };
        VERIFY_ARE_EQUAL(nameMapOriginal.Size(), nameMapCopy.Size());

        // Test that changing the copy should not change the original
        VERIFY_ARE_EQUAL(settings->GlobalSettings().WordDelimiters(), copyImpl->GlobalSettings().WordDelimiters());
        copyImpl->GlobalSettings().WordDelimiters(L"changed value");
        VERIFY_ARE_NOT_EQUAL(settings->GlobalSettings().WordDelimiters(), copyImpl->GlobalSettings().WordDelimiters());
    }

    void DeserializationTests::TestCloneInheritanceTree()
    {
        static constexpr std::string_view settingsJson{ R"(
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

        const auto settings{ winrt::make_self<implementation::CascadiaSettings>(settingsJson) };
        const auto copy{ settings->Copy() };
        const auto copyImpl{ winrt::get_self<implementation::CascadiaSettings>(copy) };

        // test globals
        VERIFY_ARE_EQUAL(settings->GlobalSettings().DefaultProfile(), copyImpl->GlobalSettings().DefaultProfile());

        // test profiles
        VERIFY_ARE_EQUAL(settings->AllProfiles().Size(), copyImpl->AllProfiles().Size());
        VERIFY_ARE_EQUAL(settings->AllProfiles().GetAt(0).Name(), copyImpl->AllProfiles().GetAt(0).Name());
        VERIFY_ARE_EQUAL(settings->AllProfiles().GetAt(1).Name(), copyImpl->AllProfiles().GetAt(1).Name());
        VERIFY_ARE_EQUAL(settings->AllProfiles().GetAt(2).Name(), copyImpl->AllProfiles().GetAt(2).Name());
        VERIFY_ARE_EQUAL(settings->ProfileDefaults().Name(), copyImpl->ProfileDefaults().Name());

        // Modifying profile.defaults should...
        VERIFY_ARE_EQUAL(settings->ProfileDefaults().HasName(), copyImpl->ProfileDefaults().HasName());
        copyImpl->ProfileDefaults().Name(L"changed value");

        // ...keep the same name for the first two profiles
        VERIFY_ARE_EQUAL(settings->AllProfiles().Size(), copyImpl->AllProfiles().Size());
        VERIFY_ARE_EQUAL(settings->AllProfiles().GetAt(0).Name(), copyImpl->AllProfiles().GetAt(0).Name());
        VERIFY_ARE_EQUAL(settings->AllProfiles().GetAt(1).Name(), copyImpl->AllProfiles().GetAt(1).Name());

        // ...but change the name for the one that inherited it from profile.defaults
        VERIFY_ARE_NOT_EQUAL(settings->AllProfiles().GetAt(2).Name(), copyImpl->AllProfiles().GetAt(2).Name());

        // profile.defaults should be different between the two graphs
        VERIFY_ARE_EQUAL(settings->ProfileDefaults().HasName(), copyImpl->ProfileDefaults().HasName());
        VERIFY_ARE_NOT_EQUAL(settings->ProfileDefaults().Name(), copyImpl->ProfileDefaults().Name());

        Log::Comment(L"Test empty profiles.defaults");
        static constexpr std::string_view emptyPDJson{ R"(
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

        static constexpr std::string_view missingPDJson{ R"(
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

        auto verifyEmptyPD = [this](const auto json) {
            const auto settings{ winrt::make_self<implementation::CascadiaSettings>(json) };
            const auto copy{ settings->Copy() };
            const auto copyImpl{ winrt::get_self<implementation::CascadiaSettings>(copy) };

            // if we don't have profiles.defaults, it should still be in the tree
            VERIFY_IS_NOT_NULL(settings->ProfileDefaults());
            VERIFY_IS_NOT_NULL(copyImpl->ProfileDefaults());

            VERIFY_ARE_EQUAL(settings->ActiveProfiles().Size(), 1u);
            VERIFY_ARE_EQUAL(settings->ActiveProfiles().Size(), copyImpl->ActiveProfiles().Size());

            // so we should only have one parent, instead of two
            const auto srcProfile{ winrt::get_self<implementation::Profile>(settings->ActiveProfiles().GetAt(0)) };
            const auto copyProfile{ winrt::get_self<implementation::Profile>(copyImpl->ActiveProfiles().GetAt(0)) };
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

        static constexpr std::string_view settings1Json{ R"(
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
            ]
        })" };

        static constexpr std::string_view settings2Json{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "actions": [
                {
                    "command": null,
                    "keys": "ctrl+shift+w"
                },
                {
                    "name": "bar",
                    "command": "closePane"
                },
            ],
        })" };

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(settings2Json, settings1Json);
        const KeyChord expectedKeyChord{ true, false, true, false, static_cast<int>('W'), 0 };

        const auto nameMap = settings->ActionMap().NameMap();
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

    // This test ensures GH#11597, GH#12520 don't regress.
    void DeserializationTests::LoadFragmentsWithMultipleUpdates()
    {
        static constexpr std::wstring_view fragmentSource{ L"fragment" };
        static constexpr std::string_view fragmentJson{ R"({
            "profiles": [
                {
                    "updates": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                    "name": "NewName"
                },
                {
                    "updates": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                    "cursorShape": "filledBox"
                },
                {
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "commandline": "cmd.exe"
                }
            ]
        })" };

        implementation::SettingsLoader loader{ std::string_view{}, DefaultJson };
        loader.MergeInboxIntoUserSettings();
        loader.MergeFragmentIntoUserSettings(winrt::hstring{ fragmentSource }, fragmentJson);
        loader.FinalizeLayering();

        VERIFY_IS_FALSE(loader.duplicateProfile);
        VERIFY_ARE_EQUAL(3u, loader.userSettings.profiles.size());
        // GH#12520: Fragments should be able to override the name of builtin profiles.
        VERIFY_ARE_EQUAL(L"NewName", loader.userSettings.profiles[0]->Name());
    }

    void DeserializationTests::FragmentActionSimple()
    {
        static constexpr std::wstring_view fragmentSource{ L"fragment" };
        static constexpr std::string_view fragmentJson{ R"({
            "actions": [
                {
                    "command": { "action": "addMark" },
                    "name": "Test Action"
                },
            ]
        })" };

        implementation::SettingsLoader loader{ std::string_view{}, DefaultJson };
        loader.MergeInboxIntoUserSettings();
        loader.MergeFragmentIntoUserSettings(winrt::hstring{ fragmentSource }, fragmentJson);
        loader.FinalizeLayering();

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));

        const auto actionMap = winrt::get_self<implementation::ActionMap>(settings->GlobalSettings().ActionMap());
        const auto actionsByName = actionMap->NameMap();
        VERIFY_IS_NOT_NULL(actionsByName.TryLookup(L"Test Action"));
    }

    void DeserializationTests::FragmentActionNoKeys()
    {
        static constexpr std::wstring_view fragmentSource{ L"fragment" };
        static constexpr std::string_view fragmentJson{ R"({
            "actions": [
                {
                    "command": { "action": "addMark" },
                    "keys": "ctrl+f",
                    "name": "Test Action"
                },
            ]
        })" };

        implementation::SettingsLoader loader{ std::string_view{}, DefaultJson };
        loader.MergeInboxIntoUserSettings();
        loader.MergeFragmentIntoUserSettings(winrt::hstring{ fragmentSource }, fragmentJson);
        loader.FinalizeLayering();

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));

        const auto actionMap = winrt::get_self<implementation::ActionMap>(settings->GlobalSettings().ActionMap());
        const auto actionsByName = actionMap->NameMap();
        VERIFY_IS_NOT_NULL(actionsByName.TryLookup(L"Test Action"));
        VERIFY_IS_NULL(actionMap->GetActionByKeyChord({ VirtualKeyModifiers::Control, static_cast<int32_t>('F'), 0 }));
    }

    void DeserializationTests::FragmentActionNested()
    {
        static constexpr std::wstring_view fragmentSource{ L"fragment" };
        static constexpr std::string_view fragmentJson{ R"({
            "actions": [
                {
                    "name": "nested command",
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
        })" };

        implementation::SettingsLoader loader{ std::string_view{}, DefaultJson };
        loader.MergeInboxIntoUserSettings();
        loader.MergeFragmentIntoUserSettings(winrt::hstring{ fragmentSource }, fragmentJson);
        loader.FinalizeLayering();

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));

        const auto actionMap = winrt::get_self<implementation::ActionMap>(settings->GlobalSettings().ActionMap());
        const auto actionsByName = actionMap->NameMap();
        const auto& nested{ actionsByName.TryLookup(L"nested command") };
        VERIFY_IS_NOT_NULL(nested);
        VERIFY_IS_TRUE(nested.HasNestedCommands());
    }

    void DeserializationTests::FragmentActionNestedNoName()
    {
        // Basically the same as TestNestedCommandWithoutName
        static constexpr std::wstring_view fragmentSource{ L"fragment" };
        static constexpr std::string_view fragmentJson{ R"({
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
            ]
        })" };

        implementation::SettingsLoader loader{ std::string_view{}, DefaultJson };
        loader.MergeInboxIntoUserSettings();
        loader.MergeFragmentIntoUserSettings(winrt::hstring{ fragmentSource }, fragmentJson);
        loader.FinalizeLayering();

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));
        VERIFY_ARE_EQUAL(0u, settings->Warnings().Size());
    }
    void DeserializationTests::FragmentActionIterable()
    {
        static constexpr std::wstring_view fragmentSource{ L"fragment" };
        static constexpr std::string_view fragmentJson{ R"({
            "actions": [
                {
                    "name": "nested",
                    "commands": [
                        {
                            "iterateOn": "schemes",
                            "name": "${scheme.name}",
                            "command": { "action": "setColorScheme", "colorScheme": "${scheme.name}" }
                        }
                    ]
                },
            ]
        })" };

        implementation::SettingsLoader loader{ std::string_view{}, DefaultJson };
        loader.MergeInboxIntoUserSettings();
        loader.MergeFragmentIntoUserSettings(winrt::hstring{ fragmentSource }, fragmentJson);
        loader.FinalizeLayering();

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));

        const auto actionMap = winrt::get_self<implementation::ActionMap>(settings->GlobalSettings().ActionMap());
        const auto actionsByName = actionMap->NameMap();
        const auto& nested{ actionsByName.TryLookup(L"nested") };
        VERIFY_IS_NOT_NULL(nested);
        VERIFY_IS_TRUE(nested.HasNestedCommands());
        VERIFY_ARE_EQUAL(settings->GlobalSettings().ColorSchemes().Size(), nested.NestedCommands().Size());
    }
    void DeserializationTests::FragmentActionRoundtrip()
    {
        static constexpr std::wstring_view fragmentSource{ L"fragment" };
        static constexpr std::string_view fragmentJson{ R"({
            "actions": [
                {
                    "command": { "action": "addMark" },
                    "name": "Test Action"
                },
            ]
        })" };

        implementation::SettingsLoader loader{ std::string_view{}, DefaultJson };
        loader.MergeInboxIntoUserSettings();
        loader.MergeFragmentIntoUserSettings(winrt::hstring{ fragmentSource }, fragmentJson);
        loader.FinalizeLayering();

        const auto oldSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));

        const auto actionMap = winrt::get_self<implementation::ActionMap>(oldSettings->GlobalSettings().ActionMap());
        const auto actionsByName = actionMap->NameMap();
        VERIFY_IS_NOT_NULL(actionsByName.TryLookup(L"Test Action"));

        const auto oldResult{ oldSettings->ToJson() };

        Log::Comment(L"Now, create a _new_ settings object from the re-serialization of the first");
        implementation::SettingsLoader newLoader{ toString(oldResult), DefaultJson };
        // NOTABLY! Don't load the fragment here.
        newLoader.MergeInboxIntoUserSettings();
        newLoader.FinalizeLayering();
        const auto newSettings = winrt::make_self<implementation::CascadiaSettings>(std::move(newLoader));

        const auto& newActionMap = winrt::get_self<implementation::ActionMap>(newSettings->GlobalSettings().ActionMap());
        const auto newActionsByName = newActionMap->NameMap();
        VERIFY_IS_NULL(newActionsByName.TryLookup(L"Test Action"));
    }

    void DeserializationTests::MigrateReloadEnvVars()
    {
        static constexpr std::string_view settings1Json{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "compatibility.reloadEnvironmentVariables": false,
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
                    "name": "foo",
                    "command": "closePane",
                    "keys": "ctrl+shift+w"
                }
            ]
        })" };

        implementation::SettingsLoader loader{ settings1Json, DefaultJson };
        loader.MergeInboxIntoUserSettings();
        loader.FinalizeLayering();

        VERIFY_IS_TRUE(loader.FixupUserSettings(), L"Validate that this will indicate we need to write them back to disk");

        const auto settings = winrt::make_self<implementation::CascadiaSettings>(std::move(loader));

        Log::Comment(L"Ensure that the profile defaults have the new setting added");
        VERIFY_IS_TRUE(settings->ProfileDefaults().HasReloadEnvironmentVariables());
        VERIFY_IS_FALSE(settings->ProfileDefaults().ReloadEnvironmentVariables());
    }
}
