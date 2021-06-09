// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "JsonTestClass.h"

using namespace Microsoft::Console;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class ProfileTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(ProfileTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(CanLayerProfile);
        TEST_METHOD(LayerProfileProperties);
        TEST_METHOD(LayerProfileIcon);
        TEST_METHOD(LayerProfilesOnArray);
        TEST_METHOD(DuplicateProfileTest);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }
    };

    void ProfileTests::CanLayerProfile()
    {
        const std::string profile0String{ R"({
            "name" : "profile0",
            "guid" : "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string profile1String{ R"({
            "name" : "profile1",
            "guid" : "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string profile2String{ R"({
            "name" : "profile2",
            "guid" : "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string profile3String{ R"({
            "name" : "profile3"
        })" };

        const auto profile0Json = VerifyParseSucceeded(profile0String);
        const auto profile1Json = VerifyParseSucceeded(profile1String);
        const auto profile2Json = VerifyParseSucceeded(profile2String);
        const auto profile3Json = VerifyParseSucceeded(profile3String);

        const auto profile0 = implementation::Profile::FromJson(profile0Json);

        VERIFY_IS_FALSE(profile0->ShouldBeLayered(profile1Json));
        VERIFY_IS_TRUE(profile0->ShouldBeLayered(profile2Json));
        VERIFY_IS_FALSE(profile0->ShouldBeLayered(profile3Json));

        const auto profile1 = implementation::Profile::FromJson(profile1Json);

        VERIFY_IS_FALSE(profile1->ShouldBeLayered(profile0Json));
        // A profile _can_ be layered with itself, though what's the point?
        VERIFY_IS_TRUE(profile1->ShouldBeLayered(profile1Json));
        VERIFY_IS_FALSE(profile1->ShouldBeLayered(profile2Json));
        VERIFY_IS_FALSE(profile1->ShouldBeLayered(profile3Json));

        const auto profile3 = implementation::Profile::FromJson(profile3Json);

        VERIFY_IS_FALSE(profile3->ShouldBeLayered(profile0Json));
        // A profile _can_ be layered with itself, though what's the point?
        VERIFY_IS_FALSE(profile3->ShouldBeLayered(profile1Json));
        VERIFY_IS_FALSE(profile3->ShouldBeLayered(profile2Json));
        VERIFY_IS_TRUE(profile3->ShouldBeLayered(profile3Json));
    }

    void ProfileTests::LayerProfileProperties()
    {
        const std::string profile0String{ R"({
            "name": "profile0",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "foreground": "#000000",
            "background": "#010101",
            "selectionBackground": "#010101"
        })" };
        const std::string profile1String{ R"({
            "name": "profile1",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "foreground": "#020202",
            "startingDirectory": "C:/"
        })" };
        const std::string profile2String{ R"({
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
        const std::string profile0String{ R"({
            "name": "profile0",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "icon": "not-null.png"
        })" };
        const std::string profile1String{ R"({
            "name": "profile1",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "icon": null
        })" };
        const std::string profile2String{ R"({
            "name": "profile2",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string profile3String{ R"({
            "name": "profile3",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "icon": "another-real.png"
        })" };

        const auto profile0Json = VerifyParseSucceeded(profile0String);
        const auto profile1Json = VerifyParseSucceeded(profile1String);
        const auto profile2Json = VerifyParseSucceeded(profile2String);
        const auto profile3Json = VerifyParseSucceeded(profile3String);

        auto profile0 = implementation::Profile::FromJson(profile0Json);
        VERIFY_IS_FALSE(profile0->Icon().empty());
        VERIFY_ARE_EQUAL(L"not-null.png", profile0->Icon());

        Log::Comment(NoThrowString().Format(
            L"Verify that layering an object the key set to null will clear the key"));
        profile0->LayerJson(profile1Json);
        VERIFY_IS_TRUE(profile0->Icon().empty());

        profile0->LayerJson(profile2Json);
        VERIFY_IS_TRUE(profile0->Icon().empty());

        profile0->LayerJson(profile3Json);
        VERIFY_IS_FALSE(profile0->Icon().empty());
        VERIFY_ARE_EQUAL(L"another-real.png", profile0->Icon());

        Log::Comment(NoThrowString().Format(
            L"Verify that layering an object _without_ the key will not clear the key"));
        profile0->LayerJson(profile2Json);
        VERIFY_IS_FALSE(profile0->Icon().empty());
        VERIFY_ARE_EQUAL(L"another-real.png", profile0->Icon());

        auto profile1 = implementation::Profile::FromJson(profile1Json);
        VERIFY_IS_TRUE(profile1->Icon().empty());
        profile1->LayerJson(profile3Json);
        VERIFY_IS_FALSE(profile1->Icon().empty());
        VERIFY_ARE_EQUAL(L"another-real.png", profile1->Icon());
    }

    void ProfileTests::LayerProfilesOnArray()
    {
        const std::string profile0String{ R"({
            "name" : "profile0",
            "guid" : "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string profile1String{ R"({
            "name" : "profile1",
            "guid" : "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string profile2String{ R"({
            "name" : "profile2",
            "guid" : "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string profile3String{ R"({
            "name" : "profile3",
            "guid" : "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
        })" };
        const std::string profile4String{ R"({
            "name" : "profile4",
            "guid" : "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
        })" };

        const auto profile0Json = VerifyParseSucceeded(profile0String);
        const auto profile1Json = VerifyParseSucceeded(profile1String);
        const auto profile2Json = VerifyParseSucceeded(profile2String);
        const auto profile3Json = VerifyParseSucceeded(profile3String);
        const auto profile4Json = VerifyParseSucceeded(profile4String);

        auto settings = winrt::make_self<implementation::CascadiaSettings>();

        VERIFY_ARE_EQUAL(0u, settings->_allProfiles.Size());
        VERIFY_IS_NULL(settings->_FindMatchingProfile(profile0Json));
        VERIFY_IS_NULL(settings->_FindMatchingProfile(profile1Json));
        VERIFY_IS_NULL(settings->_FindMatchingProfile(profile2Json));
        VERIFY_IS_NULL(settings->_FindMatchingProfile(profile3Json));
        VERIFY_IS_NULL(settings->_FindMatchingProfile(profile4Json));

        settings->_LayerOrCreateProfile(profile0Json);
        VERIFY_ARE_EQUAL(1u, settings->_allProfiles.Size());
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile0Json));
        VERIFY_IS_NULL(settings->_FindMatchingProfile(profile1Json));
        VERIFY_IS_NULL(settings->_FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile4Json));

        settings->_LayerOrCreateProfile(profile1Json);
        VERIFY_ARE_EQUAL(2u, settings->_allProfiles.Size());
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile0Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile1Json));
        VERIFY_IS_NULL(settings->_FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile4Json));

        settings->_LayerOrCreateProfile(profile2Json);
        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile0Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile1Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile4Json));
        VERIFY_ARE_EQUAL(L"profile0", settings->_allProfiles.GetAt(0).Name());

        settings->_LayerOrCreateProfile(profile3Json);
        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile0Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile1Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile4Json));
        VERIFY_ARE_EQUAL(L"profile3", settings->_allProfiles.GetAt(0).Name());

        settings->_LayerOrCreateProfile(profile4Json);
        VERIFY_ARE_EQUAL(3u, settings->_allProfiles.Size());
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile0Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile1Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings->_FindMatchingProfile(profile4Json));
        VERIFY_ARE_EQUAL(L"profile4", settings->_allProfiles.GetAt(0).Name());
    }

    void ProfileTests::DuplicateProfileTest()
    {
        const std::string profile0String{ R"({
            "name" : "profile0",
            "backgroundImage" : "some//path"
        })" };

        const auto profile0Json = VerifyParseSucceeded(profile0String);

        auto settings = winrt::make_self<implementation::CascadiaSettings>();

        settings->_LayerOrCreateProfile(profile0Json);
        auto duplicatedProfile = settings->DuplicateProfile(*settings->_FindMatchingProfile(profile0Json));
        duplicatedProfile.Name(L"profile0");

        const auto duplicatedJson = winrt::get_self<implementation::Profile>(duplicatedProfile)->ToJson();
        VERIFY_ARE_EQUAL(profile0Json, duplicatedJson);
    }
}
