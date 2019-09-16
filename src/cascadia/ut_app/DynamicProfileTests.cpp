// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/Profile.h"
#include "../TerminalApp/CascadiaSettings.h"
#include "../TerminalApp/LegacyProfileGeneratorNamespaces.h"

#include "../LocalTests_TerminalApp/JsonTestClass.h"

#include "TestDynamicProfileGenerator.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalAppUnitTests
{
    class DynamicProfileTests : public JsonTestClass
    {
        BEGIN_TEST_CLASS(DynamicProfileTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.Unit.Tests.manifest")
        END_TEST_CLASS()

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }

        TEST_METHOD(TestSimpleGenerate);

        // Simple test of CascadiaSettings generating profiles with _LoadDynamicProfiles
        TEST_METHOD(TestSimpleGenerateMultipleGenerators);

        // Make sure we gen GUIDs for profiles without guids
        TEST_METHOD(TestGenGuidsForProfiles);

        // Profiles without a source should not be layered on those with one
        TEST_METHOD(DontLayerUserProfilesOnDynamicProfiles);
        TEST_METHOD(DoLayerUserProfilesOnDynamicsWhenSourceMatches);

        // Make sure profiles that are disabled in _userSettings don't get generated
        TEST_METHOD(TestDontRunDisabledGenerators);

        // Make sure profiles that are disabled in _userSettings don't get generated
        TEST_METHOD(TestLegacyProfilesMigrate);

        // Both these do similar things:
        // This makes sure that a profile with a `source` _only_ layers, it won't create a new profile
        TEST_METHOD(UserProfilesWithInvalidSourcesAreIgnored);
        // This does the same, but by disabling a profile source
        TEST_METHOD(UserProfilesFromDisabledSourcesDontAppear);
    };

    void DynamicProfileTests::TestSimpleGenerate()
    {
        TestDynamicProfileGenerator gen{ L"Terminal.App.UnitTest" };
        gen.pfnGenerate = []() {
            std::vector<Profile> profiles;
            Profile p0;
            p0.SetName(L"profile0");
            profiles.push_back(p0);
            return profiles;
        };

        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest", gen.GetNamespace());
        std::vector<Profile> profiles = gen.GenerateProfiles();
        VERIFY_ARE_EQUAL(1u, profiles.size());
        VERIFY_ARE_EQUAL(L"profile0", profiles.at(0).GetName());
        VERIFY_IS_FALSE(profiles.at(0)._guid.has_value());
    }

    void DynamicProfileTests::TestSimpleGenerateMultipleGenerators()
    {
        auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
        gen0->pfnGenerate = []() {
            std::vector<Profile> profiles;
            Profile p0;
            p0.SetName(L"profile0");
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = []() {
            std::vector<Profile> profiles;
            Profile p0;
            p0.SetName(L"profile1");
            profiles.push_back(p0);
            return profiles;
        };

        CascadiaSettings settings{ false };
        settings._profileGenerators.emplace_back(std::move(gen0));
        settings._profileGenerators.emplace_back(std::move(gen1));

        settings._LoadDynamicProfiles();
        VERIFY_ARE_EQUAL(2u, settings._profiles.size());

        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0).GetName());
        VERIFY_IS_FALSE(settings._profiles.at(0)._guid.has_value());

        VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1).GetName());
        VERIFY_IS_FALSE(settings._profiles.at(1)._guid.has_value());
    }

    void DynamicProfileTests::TestGenGuidsForProfiles()
    {
        // We'll generate GUIDs during
        // CascadiaSettings::_ValidateProfilesHaveGuid. We should make sure that
        // the GUID generated for a dynamic profile (with a source) is different
        // than that of a profile without a source.

        auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
        gen0->pfnGenerate = []() {
            std::vector<Profile> profiles;
            Profile p0;
            p0.SetName(L"profile0"); // this is _profiles.at(2)
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = []() {
            std::vector<Profile> profiles;
            Profile p0, p1;
            p0.SetName(L"profile0"); // this is _profiles.at(3)
            p1.SetName(L"profile1"); // this is _profiles.at(4)
            profiles.push_back(p0);
            profiles.push_back(p1);
            return profiles;
        };

        CascadiaSettings settings{ false };
        settings._profileGenerators.emplace_back(std::move(gen0));
        settings._profileGenerators.emplace_back(std::move(gen1));

        Profile p0, p1;
        p0.SetName(L"profile0"); // this is _profiles.at(0)
        p1.SetName(L"profile1"); // this is _profiles.at(1)
        settings._profiles.push_back(p0);
        settings._profiles.push_back(p1);

        settings._LoadDynamicProfiles();
        VERIFY_ARE_EQUAL(5u, settings._profiles.size());

        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0).GetName());
        VERIFY_IS_FALSE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(0)._source.has_value());

        VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1).GetName());
        VERIFY_IS_FALSE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(1)._source.has_value());

        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(2).GetName());
        VERIFY_IS_FALSE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._source.has_value());

        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(3).GetName());
        VERIFY_IS_FALSE(settings._profiles.at(3)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._source.has_value());

        VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(4).GetName());
        VERIFY_IS_FALSE(settings._profiles.at(4)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(4)._source.has_value());

        settings._ValidateProfilesHaveGuid();

        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(4)._guid.has_value());

        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(0)._guid.value(),
                             settings._profiles.at(1)._guid.value());
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(0)._guid.value(),
                             settings._profiles.at(2)._guid.value());
        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(0)._guid.value(),
                             settings._profiles.at(3)._guid.value());

        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(1)._guid.value(),
                             settings._profiles.at(4)._guid.value());

        VERIFY_ARE_NOT_EQUAL(settings._profiles.at(3)._guid.value(),
                             settings._profiles.at(4)._guid.value());
    }

    void DynamicProfileTests::DontLayerUserProfilesOnDynamicProfiles()
    {
        GUID guid0 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        GUID guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");

        const std::string userProfiles{ R"(
        {
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

        auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
        gen0->pfnGenerate = [guid0, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid0 };
            p0.SetName(L"profile0"); // this is _profiles.at(0)
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = [guid0, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid0 }, p1{ guid1 };
            p0.SetName(L"profile0"); // this is _profiles.at(1)
            p1.SetName(L"profile1"); // this is _profiles.at(2)
            profiles.push_back(p0);
            profiles.push_back(p1);
            return profiles;
        };

        CascadiaSettings settings{ false };
        settings._profileGenerators.emplace_back(std::move(gen0));
        settings._profileGenerators.emplace_back(std::move(gen1));

        Log::Comment(NoThrowString().Format(
            L"All profiles with the same name have the same GUID. However, they"
            L" will not be layered, because they have different sources"));

        // parse userProfiles as the user settings
        settings._ParseJsonString(userProfiles, false);
        VERIFY_ARE_EQUAL(0u, settings._profiles.size(), L"Just parsing the user settings doesn't actually layer them");
        settings._LoadDynamicProfiles();
        VERIFY_ARE_EQUAL(3u, settings._profiles.size());
        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(5u, settings._profiles.size());

        VERIFY_IS_TRUE(settings._profiles.at(0)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._source.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(3)._source.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(4)._source.has_value());

        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.0", settings._profiles.at(0)._source.value());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(1)._source.value());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(2)._source.value());

        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(4)._guid.has_value());

        VERIFY_ARE_EQUAL(guid0, settings._profiles.at(0)._guid.value());
        VERIFY_ARE_EQUAL(guid0, settings._profiles.at(1)._guid.value());
        VERIFY_ARE_EQUAL(guid1, settings._profiles.at(2)._guid.value());
        VERIFY_ARE_EQUAL(guid0, settings._profiles.at(3)._guid.value());
        VERIFY_ARE_EQUAL(guid1, settings._profiles.at(4)._guid.value());
    }

    void DynamicProfileTests::DoLayerUserProfilesOnDynamicsWhenSourceMatches()
    {
        GUID guid0 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        GUID guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");

        const std::string userProfiles{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0FromUserSettings", // this is _profiles.at(0)
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.0"
                },
                {
                    "name" : "profile1FromUserSettings", // this is _profiles.at(2)
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.1"
                }
            ]
        })" };

        auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
        gen0->pfnGenerate = [guid0, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid0 };
            p0.SetName(L"profile0"); // this is _profiles.at(0)
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = [guid0, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid0 }, p1{ guid1 };
            p0.SetName(L"profile0"); // this is _profiles.at(1)
            p1.SetName(L"profile1"); // this is _profiles.at(2)
            profiles.push_back(p0);
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
        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(3u, settings._profiles.size());

        VERIFY_IS_TRUE(settings._profiles.at(0)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._source.has_value());

        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.0", settings._profiles.at(0)._source.value());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(1)._source.value());
        VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(2)._source.value());

        VERIFY_IS_TRUE(settings._profiles.at(0)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._guid.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._guid.has_value());

        VERIFY_ARE_EQUAL(guid0, settings._profiles.at(0)._guid.value());
        VERIFY_ARE_EQUAL(guid0, settings._profiles.at(1)._guid.value());
        VERIFY_ARE_EQUAL(guid1, settings._profiles.at(2)._guid.value());

        VERIFY_ARE_EQUAL(L"profile0FromUserSettings", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"profile1FromUserSettings", settings._profiles.at(2)._name);
    }

    void DynamicProfileTests::TestDontRunDisabledGenerators()
    {
        const std::string settings0String{ R"(
        {
            "disabledProfileSources": ["Terminal.App.UnitTest.0"]
        })" };
        const std::string settings1String{ R"(
        {
            "disabledProfileSources": ["Terminal.App.UnitTest.0", "Terminal.App.UnitTest.1"]
        })" };

        const auto settings0Json = VerifyParseSucceeded(settings0String);

        auto gen0GenerateFn = []() {
            std::vector<Profile> profiles;
            Profile p0;
            p0.SetName(L"profile0");
            profiles.push_back(p0);
            return profiles;
        };

        auto gen1GenerateFn = []() {
            std::vector<Profile> profiles;
            Profile p0, p1;
            p0.SetName(L"profile1");
            p1.SetName(L"profile2");
            profiles.push_back(p0);
            profiles.push_back(p1);
            return profiles;
        };

        auto gen2GenerateFn = []() {
            std::vector<Profile> profiles;
            Profile p0, p1;
            p0.SetName(L"profile3");
            p1.SetName(L"profile4");
            profiles.push_back(p0);
            profiles.push_back(p1);
            return profiles;
        };

        {
            Log::Comment(NoThrowString().Format(
                L"Case 1: Disable a single profile generator"));
            CascadiaSettings settings{ false };

            auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
            auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
            auto gen2 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.2");
            gen0->pfnGenerate = gen0GenerateFn;
            gen1->pfnGenerate = gen1GenerateFn;
            gen2->pfnGenerate = gen2GenerateFn;
            settings._profileGenerators.emplace_back(std::move(gen0));
            settings._profileGenerators.emplace_back(std::move(gen1));
            settings._profileGenerators.emplace_back(std::move(gen2));

            // Parse as the user settings:
            settings._ParseJsonString(settings0String, false);
            settings._LoadDynamicProfiles();

            VERIFY_ARE_EQUAL(4u, settings._profiles.size());
            VERIFY_IS_TRUE(settings._profiles.at(0)._source.has_value());
            VERIFY_IS_TRUE(settings._profiles.at(1)._source.has_value());
            VERIFY_IS_TRUE(settings._profiles.at(2)._source.has_value());
            VERIFY_IS_TRUE(settings._profiles.at(3)._source.has_value());
            VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(0)._source.value());
            VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.1", settings._profiles.at(1)._source.value());
            VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.2", settings._profiles.at(2)._source.value());
            VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.2", settings._profiles.at(3)._source.value());
            VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(1)._name);
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(2)._name);
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(3)._name);
        }

        {
            Log::Comment(NoThrowString().Format(
                L"Case 2: Disable multiple profile generators"));
            CascadiaSettings settings{ false };
            auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
            auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
            auto gen2 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.2");
            gen0->pfnGenerate = gen0GenerateFn;
            gen1->pfnGenerate = gen1GenerateFn;
            gen2->pfnGenerate = gen2GenerateFn;
            settings._profileGenerators.emplace_back(std::move(gen0));
            settings._profileGenerators.emplace_back(std::move(gen1));
            settings._profileGenerators.emplace_back(std::move(gen2));

            // Parse as the user settings:
            settings._ParseJsonString(settings1String, false);
            settings._LoadDynamicProfiles();

            VERIFY_ARE_EQUAL(2u, settings._profiles.size());
            VERIFY_IS_TRUE(settings._profiles.at(0)._source.has_value());
            VERIFY_IS_TRUE(settings._profiles.at(1)._source.has_value());
            VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.2", settings._profiles.at(0)._source.value());
            VERIFY_ARE_EQUAL(L"Terminal.App.UnitTest.2", settings._profiles.at(1)._source.value());
            VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(0)._name);
            VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(1)._name);
        }
    }

    void DynamicProfileTests::TestLegacyProfilesMigrate()
    {
        GUID guid0 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}");
        GUID guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        GUID guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        GUID guid3 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");
        GUID guid4 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-4444-49a3-80bd-e8fdd045185c}");

        const std::string settings0String{ R"(
        {
            "profiles": [
                {
                    // This pwsh profile does not have a source, but should still be layered
                    "name" : "profile0FromUserSettings", // this is _profiles.at(0)
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                },
                {
                    // This Azure profile does not have a source, but should still be layered
                    "name" : "profile3FromUserSettings", // this is _profiles.at(3)
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}"
                },
                {
                    // This profile did not come from a dynamic source
                    "name" : "profile4FromUserSettings", // this is _profiles.at(4)
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}"
                },
                {
                    // This WSL profile does not have a source, but should still be layered
                    "name" : "profile1FromUserSettings", // this is _profiles.at(1)
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                },
                {
                    // This WSL profile does have a source, and should be layered
                    "name" : "profile2FromUserSettings", // this is _profiles.at(2)
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "source": "Windows.Terminal.Wsl"
                }
            ]
        })" };

        auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Windows.Terminal.PowershellCore");
        gen0->pfnGenerate = [guid0, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid0 };
            p0.SetName(L"profile0");
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Windows.Terminal.Wsl");
        gen1->pfnGenerate = [guid2, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid1 }, p1{ guid2 };
            p0.SetName(L"profile1");
            p1.SetName(L"profile2");
            profiles.push_back(p0);
            profiles.push_back(p1);
            return profiles;
        };
        auto gen2 = std::make_unique<TestDynamicProfileGenerator>(L"Windows.Terminal.Azure");
        gen2->pfnGenerate = [guid3]() {
            std::vector<Profile> profiles;
            Profile p0{ guid3 };
            p0.SetName(L"profile3");
            profiles.push_back(p0);
            return profiles;
        };

        CascadiaSettings settings{ false };
        settings._profileGenerators.emplace_back(std::move(gen0));
        settings._profileGenerators.emplace_back(std::move(gen1));
        settings._profileGenerators.emplace_back(std::move(gen2));

        settings._ParseJsonString(settings0String, false);
        VERIFY_ARE_EQUAL(0u, settings._profiles.size());

        settings._LoadDynamicProfiles();
        VERIFY_ARE_EQUAL(4u, settings._profiles.size());

        VERIFY_IS_TRUE(settings._profiles.at(0)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._source.has_value());
        VERIFY_ARE_EQUAL(L"Windows.Terminal.PowershellCore", settings._profiles.at(0)._source.value());
        VERIFY_ARE_EQUAL(L"Windows.Terminal.Wsl", settings._profiles.at(1)._source.value());
        VERIFY_ARE_EQUAL(L"Windows.Terminal.Wsl", settings._profiles.at(2)._source.value());
        VERIFY_ARE_EQUAL(L"Windows.Terminal.Azure", settings._profiles.at(3)._source.value());
        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"profile1", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"profile2", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(3)._name);

        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(5u, settings._profiles.size());

        VERIFY_IS_TRUE(settings._profiles.at(0)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(1)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(2)._source.has_value());
        VERIFY_IS_TRUE(settings._profiles.at(3)._source.has_value());
        VERIFY_IS_FALSE(settings._profiles.at(4)._source.has_value());
        VERIFY_ARE_EQUAL(L"Windows.Terminal.PowershellCore", settings._profiles.at(0)._source.value());
        VERIFY_ARE_EQUAL(L"Windows.Terminal.Wsl", settings._profiles.at(1)._source.value());
        VERIFY_ARE_EQUAL(L"Windows.Terminal.Wsl", settings._profiles.at(2)._source.value());
        VERIFY_ARE_EQUAL(L"Windows.Terminal.Azure", settings._profiles.at(3)._source.value());
        // settings._profiles.at(4) does not have a soruce
        VERIFY_ARE_EQUAL(L"profile0FromUserSettings", settings._profiles.at(0)._name);
        VERIFY_ARE_EQUAL(L"profile1FromUserSettings", settings._profiles.at(1)._name);
        VERIFY_ARE_EQUAL(L"profile2FromUserSettings", settings._profiles.at(2)._name);
        VERIFY_ARE_EQUAL(L"profile3FromUserSettings", settings._profiles.at(3)._name);
        VERIFY_ARE_EQUAL(L"profile4FromUserSettings", settings._profiles.at(4)._name);
    }

    void DynamicProfileTests::UserProfilesWithInvalidSourcesAreIgnored()
    {
        GUID guid0 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        GUID guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");

        const std::string settings0String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0FromUserSettings", // this is _profiles.at(0)
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.0"
                },
                {
                    "name" : "profile2", // this shouldn't be in the profiles at all
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.1"
                },
                {
                    "name" : "profile3", // this is _profiles.at(3)
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
        gen0->pfnGenerate = [guid0, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid0 };
            p0.SetName(L"profile0"); // this is _profiles.at(0)
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = [guid0, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid0 }, p1{ guid1 };
            p0.SetName(L"profile0"); // this is _profiles.at(1)
            p1.SetName(L"profile1"); // this is _profiles.at(2)
            profiles.push_back(p0);
            profiles.push_back(p1);
            return profiles;
        };

        CascadiaSettings settings{ false };
        settings._profileGenerators.emplace_back(std::move(gen0));
        settings._profileGenerators.emplace_back(std::move(gen1));

        settings._ParseJsonString(settings0String, false);
        VERIFY_ARE_EQUAL(0u, settings._profiles.size());

        settings._LoadDynamicProfiles();
        VERIFY_ARE_EQUAL(3u, settings._profiles.size());

        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(4u, settings._profiles.size());
    }

    void DynamicProfileTests::UserProfilesFromDisabledSourcesDontAppear()
    {
        GUID guid0 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        GUID guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");

        const std::string settings0String{ R"(
        {
            "disabledProfileSources": ["Terminal.App.UnitTest.1"],
            "profiles": [
                {
                    "name" : "profile0FromUserSettings", // this is _profiles.at(0)
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.0"
                },
                {
                    "name" : "profile1FromUserSettings", // this shouldn't be in the profiles at all
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.1"
                },
                {
                    "name" : "profile3", // this is _profiles.at(1)
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}"
                }
            ]
        })" };

        auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
        gen0->pfnGenerate = [guid0, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid0 };
            p0.SetName(L"profile0"); // this is _profiles.at(0)
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = [guid0, guid1]() {
            std::vector<Profile> profiles;
            Profile p0{ guid0 }, p1{ guid1 };
            p0.SetName(L"profile0"); // this shouldn't be in the profiles at all
            p1.SetName(L"profile1"); // this shouldn't be in the profiles at all
            profiles.push_back(p0);
            profiles.push_back(p1);
            return profiles;
        };

        CascadiaSettings settings{ false };
        settings._profileGenerators.emplace_back(std::move(gen0));
        settings._profileGenerators.emplace_back(std::move(gen1));

        settings._ParseJsonString(settings0String, false);
        VERIFY_ARE_EQUAL(0u, settings._profiles.size());

        settings._LoadDynamicProfiles();
        VERIFY_ARE_EQUAL(1u, settings._profiles.size());

        settings.LayerJson(settings._userSettings);
        VERIFY_ARE_EQUAL(2u, settings._profiles.size());
    }

};
