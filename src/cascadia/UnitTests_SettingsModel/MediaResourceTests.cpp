// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "JsonTestClass.h"
#include "TestUtils.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static std::function<void(winrt::Microsoft::Terminal::Settings::Model::OriginTag,
                          std::wstring_view,
                          const winrt::Microsoft::Terminal::Settings::Model::IMediaResource&)>
    g_mediaResolverHook;

bool TestHook_CascadiaSettings_ResolveSingleMediaResource(
    winrt::Microsoft::Terminal::Settings::Model::OriginTag origin,
    std::wstring_view basePath,
    const winrt::Microsoft::Terminal::Settings::Model::IMediaResource& resource)
{
    if (g_mediaResolverHook)
    {
        g_mediaResolverHook(origin, basePath, resource);
        return true;
    }
    return false;
}

static auto requireCalled(auto&& func)
{
    static bool called{ false };
    called = false;
    return std::tuple{
        [=](auto&&... a) {
            func(a...);
            called = true;
        },
        wil::scope_exit([] {
            WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
            VERIFY_IS_TRUE(called);
        })
    };
}

static auto requireCalled(int times, auto&& func)
{
    static int called{ 0 };
    called = 0;
    return std::tuple{
        [=](auto&&... a) {
            ++called;
            func(a...);
        },
        wil::scope_exit([=] {
            WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
            VERIFY_ARE_EQUAL(called, times);
        })
    };
}

namespace SettingsModelUnitTests
{
    class MediaResourceTests : public JsonTestClass
    {
        TEST_CLASS(MediaResourceTests);

        TEST_METHOD_CLEANUP(ResetMediaHook);

        TEST_METHOD(ValidateResolverCalledForInbox);
        TEST_METHOD(ValidateResolverCalledForInboxAndUser);

        // PROFILE BEHAVIORS
        TEST_METHOD(ProfileDefaultsContainsInvalidIcon);
        TEST_METHOD(ProfileSpecifiesInvalidIconAndCommandline);
        TEST_METHOD(ProfileSpecifiesInvalidIconAndNoCommandline);
        TEST_METHOD(ProfileInheritsInvalidIconAndHasCommandline);
        TEST_METHOD(ProfileInheritsInvalidIconAndHasNoCommandline);
        TEST_METHOD(ProfileSpecifiesNullIcon);
        TEST_METHOD(ProfileSpecifiesNullIconAndHasNoCommandline);

        // REAL RESOLVER
        TEST_METHOD(RealResolverFilePaths);
        //TEST_METHOD(RealResolverSpecialKeywords);
        //TEST_METHOD(RealResolverEmoji);
        //TEST_METHOD(RealResolverUrlCases);

    private:
        static winrt::com_ptr<implementation::CascadiaSettings> createSettings(const std::string_view& userJSON)
        {
            static constexpr std::string_view inboxJSON{ R"({
    "actions": [
        {
            "command": "closeWindow",
            "icon": "fakeCommandIconPath",
            "id": "Terminal.CloseWindow"
        }
    ],
    "profiles": {
        "list": [
            {
                "backgroundImage": "imagePathFromBase",
                "guid": "{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}",
                "icon": "iconFromBase",
                "name": "Base"
            }
        ]
    },
    "schemes": [
        {
            "background": "#0C0C0C",
            "black": "#0C0C0C",
            "blue": "#0037DA",
            "brightBlack": "#767676",
            "brightBlue": "#3B78FF",
            "brightCyan": "#61D6D6",
            "brightGreen": "#16C60C",
            "brightPurple": "#B4009E",
            "brightRed": "#E74856",
            "brightWhite": "#F2F2F2",
            "brightYellow": "#F9F1A5",
            "cursorColor": "#FFFFFF",
            "cyan": "#3A96DD",
            "foreground": "#CCCCCC",
            "green": "#13A10E",
            "name": "Campbell",
            "purple": "#881798",
            "red": "#C50F1F",
            "white": "#CCCCCC",
            "yellow": "#C19C00"
        }
    ]
})" };

            return winrt::make_self<implementation::CascadiaSettings>(userJSON, inboxJSON);
        }
    };

    bool MediaResourceTests::ResetMediaHook()
    {
        g_mediaResolverHook = nullptr;
        return true;
    }

    void MediaResourceTests::ValidateResolverCalledForInbox()
    {
        auto [t, e] = requireCalled(3,
                                    [&](auto&& origin, auto&&, auto&& resource) {
                                        VERIFY_ARE_EQUAL(OriginTag::InBox, origin);
                                        resource.Resolve(L"resolved");
                                    });
        g_mediaResolverHook = t;
        auto settings{ createSettings(R"({})") };

        auto profile{ settings->GetProfileByIndex(0) };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok());
        VERIFY_ARE_EQUAL(L"resolved", icon.Resolved());

        auto backgroundImage{ profile.DefaultAppearance().BackgroundImagePath() };
        VERIFY_IS_TRUE(backgroundImage.Ok());
        VERIFY_ARE_EQUAL(L"resolved", backgroundImage.Resolved());
    }

    void MediaResourceTests::ValidateResolverCalledForInboxAndUser()
    {
        std::unordered_map<OriginTag, int> origins;

        // The icon in profiles.defaults erases the icon in the Base Profile; that one will NOT be resolved.

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled(4,
                                        [&](auto&& origin, auto&& basePath, auto&& resource) {
                                            if (origin == OriginTag::User || origin == OriginTag::ProfilesDefaults)
                                            {
                                                VERIFY_ARE_NOT_EQUAL(L"", basePath);
                                            }
                                            origins[origin]++;
                                            resource.Resolve(L"resolved");
                                        });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "profiles": {
        "defaults": {
            "icon": "iconFromDefaults"
        },
        "list": [
            {
                "guid": "{2cdb0be2-f601-4f70-9a6c-3472b3257883}",
                "icon": "iconFromUser",
                "name": "UserProfile1"
            }
        ]
    }
})");
        }

        VERIFY_ARE_EQUAL(origins[OriginTag::InBox], 2); // Base profile icon not resolved because of profiles.defaults.icon
        VERIFY_ARE_EQUAL(origins[OriginTag::ProfilesDefaults], 1);
        VERIFY_ARE_EQUAL(origins[OriginTag::User], 1);

        auto profile0{ settings->GetProfileByName(L"Base") };
        auto icon0{ profile0.Icon() };
        VERIFY_IS_TRUE(icon0.Ok());
        VERIFY_ARE_NOT_EQUAL(L"iconFromBase", icon0.Path()); // the icon was overridden by defaults.
        VERIFY_ARE_EQUAL(L"iconFromDefaults", icon0.Path()); // the icon was overridden by defaults.
        VERIFY_ARE_EQUAL(L"resolved", icon0.Resolved());

        auto profile1{ settings->GetProfileByName(L"UserProfile1") };
        auto icon1{ profile1.Icon() };
        VERIFY_IS_TRUE(icon1.Ok());
        VERIFY_ARE_EQUAL(L"iconFromUser", icon1.Path());
        VERIFY_ARE_EQUAL(L"resolved", icon1.Resolved());
    }

    // PROFILE BEHAVIORS
    constexpr std::wstring_view defaultsCommandline{ LR"(C:\Windows\System32\PING.EXE)" }; // Normalized by Profile (this is the casing that Windows stores on disk)
    constexpr std::wstring_view overrideCommandline{ LR"(C:\Windows\System32\cscript.exe)" };

    // The invalid resource came from the Defaults profile, which specifies ping as the command line
    void MediaResourceTests::ProfileDefaultsContainsInvalidIcon()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled([&](auto&&, auto&&, auto&& resource) {
                // All resources are invalid.
                resource.Reject();
            });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "profiles": {
        "defaults": {
            "icon": "DoesNotMatter",
            "commandline": "C:\\Windows\\System32\\ping.exe",
        }
    }
})");
        }

        auto profile{ settings->GetProfileByName(L"Base") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok()); // Profile with commandline always has an icon
        VERIFY_ARE_EQUAL(defaultsCommandline, icon.Resolved());
    }

    // The invalid resource came from the profile itself, which has its own commandline.
    void MediaResourceTests::ProfileSpecifiesInvalidIconAndCommandline()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled([&](auto&&, auto&&, auto&& resource) {
                // All resources are invalid.
                resource.Reject();
            });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "profiles": {
        "defaults": {
            "icon": "DoesNotMatter",
            "commandline": "C:\\Windows\\System32\\ping.exe",
        },
        "list": [
            {
                "guid": "{2cdb0be2-f601-4f70-9a6c-3472b3257883}",
                "icon": "DoesNotMatter",
                "commandline": "C:\\Windows\\System32\\cscript.exe",
                "name": "ProfileSpecifiesInvalidIconAndCommandline"
            }
        ]
    }
})");
        }

        auto profile{ settings->GetProfileByName(L"ProfileSpecifiesInvalidIconAndCommandline") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok()); // Profile with commandline always has an icon
        VERIFY_ARE_EQUAL(overrideCommandline, icon.Resolved());
    }

    // The invalid resource came from the profile itself, which inherits a commandline from the parent (defaults, ping.exe)
    void MediaResourceTests::ProfileSpecifiesInvalidIconAndNoCommandline()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled([&](auto&&, auto&&, auto&& resource) {
                // All resources are invalid.
                resource.Reject();
            });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "profiles": {
        "defaults": {
            "icon": "DoesNotMatter",
            "commandline": "C:\\Windows\\System32\\ping.exe",
        },
        "list": [
            {
                "guid": "{af9dec6c-1337-4278-897d-69ca04920b27}",
                "icon": "DoesNotMatter",
                "name": "ProfileSpecifiesInvalidIconAndNoCommandline"
            }
        ]
    }
})");
        }

        auto profile{ settings->GetProfileByName(L"ProfileSpecifiesInvalidIconAndNoCommandline") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok());
        VERIFY_ARE_EQUAL(defaultsCommandline, icon.Resolved());
    }

    // The invalid resource came from the Defaults profile, which has the Defaults command line (PROFILE COMMANDLINE IGNORED)
    void MediaResourceTests::ProfileInheritsInvalidIconAndHasCommandline()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled([&](auto&&, auto&&, auto&& resource) {
                // All resources are invalid.
                resource.Reject();
            });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "profiles": {
        "defaults": {
            "icon": "DoesNotMatter",
            "commandline": "C:\\Windows\\System32\\ping.exe",
        },
        "list": [
            {
                "guid": "{b0f32281-7173-46ef-aa2f-ddcf36670cf0}",
                "commandline": "C:\\Windows\\System32\\cscript.exe",
                "name": "ProfileInheritsInvalidIconAndHasCommandline"
            }
        ]
    }
})");
        }

        auto profile{ settings->GetProfileByName(L"ProfileInheritsInvalidIconAndHasCommandline") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok());
        VERIFY_ARE_EQUAL(defaultsCommandline, icon.Resolved());
    }

    // The invalid resource came from the Defaults profile, which has the Defaults command line (PROFILE COMMANDLINE MISSING)
    void MediaResourceTests::ProfileInheritsInvalidIconAndHasNoCommandline()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled([&](auto&&, auto&&, auto&& resource) {
                // All resources are invalid.
                resource.Reject();
            });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "profiles": {
        "defaults": {
            "icon": "DoesNotMatter",
            "commandline": "C:\\Windows\\System32\\ping.exe",
        },
        "list": [
            {
                "guid": "{21c4122a-b094-4436-9e9c-a06f05f35ad2}",
                "name": "ProfileInheritsInvalidIconAndHasNoCommandline"
            }
        ]
    }
})");
        }

        auto profile{ settings->GetProfileByName(L"ProfileInheritsInvalidIconAndHasNoCommandline") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok());
        VERIFY_ARE_EQUAL(defaultsCommandline, icon.Resolved());
    }

    // The invalid resource came from the profile itself, which has its own commandline.
    void MediaResourceTests::ProfileSpecifiesNullIcon()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled([&](auto&&, auto&&, auto&& resource) {
                // All resources are invalid.
                resource.Reject();
            });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "profiles": {
        "defaults": {
            "icon": "DoesNotMatter",
            "commandline": "C:\\Windows\\System32\\ping.exe",
        },
        "list": [
            {
                "guid": "{e1332dad-232c-4019-b3ff-05a4386c8c46}",
                "icon": null,
                "commandline": "C:\\Windows\\System32\\cscript.exe",
                "name": "ProfileSpecifiesNullIcon"
            }
        ]
    }
})");
        }

        auto profile{ settings->GetProfileByName(L"ProfileSpecifiesNullIcon") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok()); // Profile with commandline always has an icon
        VERIFY_ARE_EQUAL(overrideCommandline, icon.Resolved());
    }

    // The invalid resource came from the profile itself, which inherits a commandline from the parent (defaults, ping.exe)
    void MediaResourceTests::ProfileSpecifiesNullIconAndHasNoCommandline()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled([&](auto&&, auto&&, auto&& resource) {
                // All resources are invalid.
                resource.Reject();
            });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "profiles": {
        "defaults": {
            "icon": "DoesNotMatter",
            "commandline": "C:\\Windows\\System32\\ping.exe",
        },
        "list": [
            {
                "guid": "{b4053177-ae5c-4600-8b77-5f81a5d313e1}",
                "icon": null,
                "name": "ProfileSpecifiesNullIconAndHasNoCommandline"
            },
        ]
    }
})");
        }

        auto profile{ settings->GetProfileByName(L"ProfileSpecifiesNullIconAndHasNoCommandline") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok()); // Profile with commandline always has an icon
        VERIFY_ARE_EQUAL(defaultsCommandline, icon.Resolved());
    }

    // REAL RESOLVER TESTS
    void MediaResourceTests::RealResolverFilePaths()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        g_mediaResolverHook = nullptr; // Use the real resolver

        // For profile, we test images instead of icon because Icon has a fallback behavior.
        auto settings = createSettings(R"({
    "profiles": {
        "list": [
            {
                "backgroundImage": "C:\\Windows\\System32\\cmd.exe",
                "name": "ProfileAbsolutePathImage"
            },
            {
                "backgroundImage": "C:/Windows/System32/cmd.exe",
                "name": "ProfileAbsolutePathImageSlashes"
            },
            {
                "backgroundImage": "explorer.exe",
                "name": "ProfileRelativePathImage"
            },
            {
                "backgroundImage": "..\\Windows\\explorer.exe",
                "name": "ProfileRelativePathImageTraversal"
            },
            {
                "backgroundImage": "%ComSpec%",
                "name": "ProfileEnvironmentVariableImage"
            },
            {
                "backgroundImage": "X:\foobar.ico",
                "name": "ProfileInvalidImage"
            },
        ]
    }
})");

        // All relative paths are relative to the fake testing user settings path of C:\Windows
        constexpr std::wstring_view expectedPath1{ LR"(C:\Windows\System32\cmd.exe)" };
        constexpr std::wstring_view expectedPath2{ LR"(C:\Windows\explorer.exe)" };

        {
            auto profile{ settings->GetProfileByName(L"ProfileAbsolutePathImage") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_EQUAL(expectedPath1, image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileAbsolutePathImageSlashes") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_EQUAL(expectedPath1, image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileRelativePathImage") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_EQUAL(expectedPath2, image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileRelativePathImageTraversal") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_EQUAL(expectedPath2, image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileEnvironmentVariableImage") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            // The casing is different on this one...
            VERIFY_ARE_EQUAL(LR"(C:\WINDOWS\system32\cmd.exe)", image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileInvalidImage") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_FALSE(image.Ok());
            VERIFY_ARE_EQUAL(L"", image.Resolved());
        }
    }
}
