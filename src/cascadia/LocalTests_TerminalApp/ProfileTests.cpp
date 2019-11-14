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

    class ProfileTests : public JsonTestClass
    {
        // Use a custom manifest to ensure that we can activate winrt types from
        // our test. This property will tell taef to manually use this as the
        // sxs manifest during this test class. It includes all the cppwinrt
        // types we've defined, so if your test is crashing for an unknown
        // reason, make sure it's included in that file.
        // If you want to do anything XAML-y, you'll need to run your test in a
        // packaged context. See TabTests.cpp for more details on that.
        BEGIN_TEST_CLASS(ProfileTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.LocalTests.manifest")
        END_TEST_CLASS()

        TEST_METHOD(CanLayerProfile);
        TEST_METHOD(LayerProfileProperties);
        TEST_METHOD(LayerProfileIcon);
        TEST_METHOD(LayerProfilesOnArray);

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

        const auto profile0 = Profile::FromJson(profile0Json);

        VERIFY_IS_FALSE(profile0.ShouldBeLayered(profile1Json));
        VERIFY_IS_TRUE(profile0.ShouldBeLayered(profile2Json));
        VERIFY_IS_FALSE(profile0.ShouldBeLayered(profile3Json));

        const auto profile1 = Profile::FromJson(profile1Json);

        VERIFY_IS_FALSE(profile1.ShouldBeLayered(profile0Json));
        // A profile _can_ be layered with itself, though what's the point?
        VERIFY_IS_TRUE(profile1.ShouldBeLayered(profile1Json));
        VERIFY_IS_FALSE(profile1.ShouldBeLayered(profile2Json));
        VERIFY_IS_FALSE(profile1.ShouldBeLayered(profile3Json));

        const auto profile3 = Profile::FromJson(profile3Json);

        VERIFY_IS_FALSE(profile3.ShouldBeLayered(profile0Json));
        // A profile _can_ be layered with itself, though what's the point?
        VERIFY_IS_FALSE(profile3.ShouldBeLayered(profile1Json));
        VERIFY_IS_FALSE(profile3.ShouldBeLayered(profile2Json));
        VERIFY_IS_FALSE(profile3.ShouldBeLayered(profile3Json));
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

        auto profile0 = Profile::FromJson(profile0Json);
        VERIFY_IS_TRUE(profile0._defaultForeground.has_value());
        VERIFY_ARE_EQUAL(ARGB(0, 0, 0, 0), profile0._defaultForeground.value());

        VERIFY_IS_TRUE(profile0._defaultBackground.has_value());
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), profile0._defaultBackground.value());

        VERIFY_IS_TRUE(profile0._selectionBackground.has_value());
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), profile0._selectionBackground.value());

        VERIFY_ARE_EQUAL(L"profile0", profile0._name);

        VERIFY_IS_FALSE(profile0._startingDirectory.has_value());

        Log::Comment(NoThrowString().Format(
            L"Layering profile1 on top of profile0"));
        profile0.LayerJson(profile1Json);

        VERIFY_IS_TRUE(profile0._defaultForeground.has_value());
        VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), profile0._defaultForeground.value());

        VERIFY_IS_TRUE(profile0._defaultBackground.has_value());
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), profile0._defaultBackground.value());

        VERIFY_IS_TRUE(profile0._selectionBackground.has_value());
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), profile0._selectionBackground.value());

        VERIFY_ARE_EQUAL(L"profile1", profile0._name);

        VERIFY_IS_TRUE(profile0._startingDirectory.has_value());
        VERIFY_ARE_EQUAL(L"C:/", profile0._startingDirectory.value());

        Log::Comment(NoThrowString().Format(
            L"Layering profile2 on top of (profile0+profile1)"));
        profile0.LayerJson(profile2Json);

        VERIFY_IS_TRUE(profile0._defaultForeground.has_value());
        VERIFY_ARE_EQUAL(ARGB(0, 3, 3, 3), profile0._defaultForeground.value());

        VERIFY_IS_TRUE(profile0._defaultBackground.has_value());
        VERIFY_ARE_EQUAL(ARGB(0, 1, 1, 1), profile0._defaultBackground.value());

        VERIFY_IS_TRUE(profile0._selectionBackground.has_value());
        VERIFY_ARE_EQUAL(ARGB(0, 2, 2, 2), profile0._selectionBackground.value());

        VERIFY_ARE_EQUAL(L"profile2", profile0._name);

        VERIFY_IS_TRUE(profile0._startingDirectory.has_value());
        VERIFY_ARE_EQUAL(L"C:/", profile0._startingDirectory.value());
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

        auto profile0 = Profile::FromJson(profile0Json);
        VERIFY_IS_TRUE(profile0._icon.has_value());
        VERIFY_ARE_EQUAL(L"not-null.png", profile0._icon.value());

        Log::Comment(NoThrowString().Format(
            L"Verify that layering an object the key set to null will clear the key"));
        profile0.LayerJson(profile1Json);
        VERIFY_IS_FALSE(profile0._icon.has_value());

        profile0.LayerJson(profile2Json);
        VERIFY_IS_FALSE(profile0._icon.has_value());

        profile0.LayerJson(profile3Json);
        VERIFY_IS_TRUE(profile0._icon.has_value());
        VERIFY_ARE_EQUAL(L"another-real.png", profile0._icon.value());

        Log::Comment(NoThrowString().Format(
            L"Verify that layering an object _without_ the key will not clear the key"));
        profile0.LayerJson(profile2Json);
        VERIFY_IS_TRUE(profile0._icon.has_value());
        VERIFY_ARE_EQUAL(L"another-real.png", profile0._icon.value());

        auto profile1 = Profile::FromJson(profile1Json);
        VERIFY_IS_FALSE(profile1._icon.has_value());
        profile1.LayerJson(profile3Json);
        VERIFY_IS_TRUE(profile1._icon.has_value());
        VERIFY_ARE_EQUAL(L"another-real.png", profile1._icon.value());
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

        CascadiaSettings settings;

        VERIFY_ARE_EQUAL(0u, settings._profiles.size());
        VERIFY_IS_NULL(settings._FindMatchingProfile(profile0Json));
        VERIFY_IS_NULL(settings._FindMatchingProfile(profile1Json));
        VERIFY_IS_NULL(settings._FindMatchingProfile(profile2Json));
        VERIFY_IS_NULL(settings._FindMatchingProfile(profile3Json));
        VERIFY_IS_NULL(settings._FindMatchingProfile(profile4Json));

        settings._LayerOrCreateProfile(profile0Json);
        VERIFY_ARE_EQUAL(1u, settings._profiles.size());
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile0Json));
        VERIFY_IS_NULL(settings._FindMatchingProfile(profile1Json));
        VERIFY_IS_NULL(settings._FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile4Json));

        settings._LayerOrCreateProfile(profile1Json);
        VERIFY_ARE_EQUAL(2u, settings._profiles.size());
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile0Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile1Json));
        VERIFY_IS_NULL(settings._FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile4Json));

        settings._LayerOrCreateProfile(profile2Json);
        VERIFY_ARE_EQUAL(3u, settings._profiles.size());
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile0Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile1Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile4Json));
        VERIFY_ARE_EQUAL(L"profile0", settings._profiles.at(0)._name);

        settings._LayerOrCreateProfile(profile3Json);
        VERIFY_ARE_EQUAL(3u, settings._profiles.size());
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile0Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile1Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile4Json));
        VERIFY_ARE_EQUAL(L"profile3", settings._profiles.at(0)._name);

        settings._LayerOrCreateProfile(profile4Json);
        VERIFY_ARE_EQUAL(3u, settings._profiles.size());
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile0Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile1Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile2Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile3Json));
        VERIFY_IS_NOT_NULL(settings._FindMatchingProfile(profile4Json));
        VERIFY_ARE_EQUAL(L"profile4", settings._profiles.at(0)._name);
    }

}
