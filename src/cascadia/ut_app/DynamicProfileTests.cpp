// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/Profile.h"
#include "../TerminalApp/CascadiaSettings.h"

#include "TestDynamicProfileGenerator.h"
#include "../LocalTests_TerminalApp/JsonTestClass.h"

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

        // Make sure that if multiple generators generate a profile with the same GUID, we only layer them if they have the same source namespace
        TEST_METHOD(TestGeneratorsCantLayerOthers);

        // Make sure profiles that are disabled in _userSettings don't get generated
        TEST_METHOD(TestDontRunDisabledGenerators);

        // Make sure profiles that are disabled in _userSettings don't get generated
        TEST_METHOD(TestLegacyProfilesMigrate);
    };

    void DynamicProfileTests::TestSimpleGenerate()
    {
        TestDynamicProfileGenerator gen{ L"Terminal.App.UnitTest" };
        gen.pfnGenerate = []() {
            std::vector<Profile> profiles{};
            Profile p0{};
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
            std::vector<Profile> profiles{};
            Profile p0{};
            p0.SetName(L"profile0");
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = []() {
            std::vector<Profile> profiles{};
            Profile p0{};
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
        // then that of a profile without a source.

        auto gen0 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.0");
        gen0->pfnGenerate = []() {
            std::vector<Profile> profiles{};
            Profile p0{};
            p0.SetName(L"profile0"); // this is _profiles.at(2)
            profiles.push_back(p0);
            return profiles;
        };
        auto gen1 = std::make_unique<TestDynamicProfileGenerator>(L"Terminal.App.UnitTest.1");
        gen1->pfnGenerate = []() {
            std::vector<Profile> profiles{};
            Profile p0{}, p1{};
            p0.SetName(L"profile0"); // this is _profiles.at(3)
            p1.SetName(L"profile1"); // this is _profiles.at(4)
            profiles.push_back(p0);
            profiles.push_back(p1);
            return profiles;
        };

        CascadiaSettings settings{ false };
        settings._profileGenerators.emplace_back(std::move(gen0));
        settings._profileGenerators.emplace_back(std::move(gen1));

        Profile p0{}, p1{};
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
    void DynamicProfileTests::TestGeneratorsCantLayerOthers()
    {
        VERIFY_IS_TRUE(false);
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
            std::vector<Profile> profiles{};
            Profile p0{};
            p0.SetName(L"profile0");
            profiles.push_back(p0);
            return profiles;
        };

        auto gen1GenerateFn = []() {
            std::vector<Profile> profiles{};
            Profile p0{}, p1{};
            p0.SetName(L"profile1");
            p1.SetName(L"profile2");
            profiles.push_back(p0);
            profiles.push_back(p1);
            return profiles;
        };

        auto gen2GenerateFn = []() {
            std::vector<Profile> profiles{};
            Profile p0{}, p1{};
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
            // DebugBreak();
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
        VERIFY_IS_TRUE(false);
    }

};
