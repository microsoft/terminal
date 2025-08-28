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

        TEST_CLASS_SETUP(DisableFSRedirection);
        TEST_CLASS_CLEANUP(RestoreFSRedirection);

        TEST_METHOD_CLEANUP(ResetMediaHook);

        // BASIC OPERATION
        TEST_METHOD(ValidateResolverCalledForInbox);
        TEST_METHOD(ValidateResolverCalledForInboxAndUser);
        TEST_METHOD(ValidateResolverCalledForFragments);
        TEST_METHOD(ValidateResolverCalledForNewTabMenuEntries);
        TEST_METHOD(ValidateResolverCalledIncrementallyOnChange);
        TEST_METHOD(ValidateResolverNotCalledForEmojiIcons);

        // PROFILE BEHAVIORS
        TEST_METHOD(ProfileDefaultsContainsInvalidIcon);
        TEST_METHOD(ProfileSpecifiesInvalidIconAndCommandline);
        TEST_METHOD(ProfileSpecifiesInvalidIconAndNoCommandline);
        TEST_METHOD(ProfileInheritsInvalidIconAndHasCommandline);
        TEST_METHOD(ProfileInheritsInvalidIconAndHasNoCommandline);
        TEST_METHOD(ProfileSpecifiesNullIcon);
        TEST_METHOD(ProfileSpecifiesNullIconAndHasNoCommandline);
        TEST_METHOD(ProfileOverwritesBellSound);

        // FRAGMENT BEHAVIORS
        TEST_METHOD(FragmentUpdatesBaseProfile);
        TEST_METHOD(FragmentActionResourcesGetResolved);
        TEST_METHOD(DisabledFragmentNotResolved);
        TEST_METHOD(FragmentAppearanceAndUserAppearanceInteraction);

        // REAL RESOLVER
        TEST_METHOD(RealResolverFilePaths);
        TEST_METHOD(RealResolverSpecialKeywords);
        TEST_METHOD(RealResolverUrlCases);

        static constexpr std::wstring_view pingCommandline{ LR"(C:\Windows\System32\PING.EXE)" }; // Normalized by Profile (this is the casing that Windows stores on disk)
        static constexpr std::wstring_view overrideCommandline{ LR"(C:\Windows\System32\cscript.exe)" };
        static constexpr std::wstring_view cmdCommandline{ LR"(C:\Windows\System32\cmd.exe)" }; // The default commandline for a profile
        static constexpr std::wstring_view fragmentBasePath1{ LR"(C:\Windows\Media)" };

    private:
        PVOID redirectionFlag{ nullptr };

        static constexpr std::string_view staticDefaultSettings{ R"({
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
                "name": "Base",
                "bellSound": [
                    "C:\\Windows\\Media\\Alarm01.wav",
                    "C:\\Windows\\Media\\Alarm02.wav"
                ]
            },
            {
                "backgroundImage": "focusedImagePathFromBase",
                "experimental.pixelShaderPath": "focusedPixelShaderPathFromBase",
                "experimental.pixelShaderImagePath": "focusedPixelShaderImagePathFromBase",
                "unfocusedAppearance": {
                    "backgroundImage": "unfocusedImagePathFromBase",
                    "experimental.pixelShaderPath": "unfocusedPixelShaderPathFromBase",
                    "experimental.pixelShaderImagePath": "unfocusedPixelShaderImagePathFromBase",
                },
                "guid": "{84f3d5cc-ecd9-49a9-96be-8bced39d4290}",
                "name": "BaseFullyLoaded"
            },
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

        static constexpr int numberOfMediaResourcesInDefaultSettings{ 11 };

        struct Fragment
        {
            std::wstring_view source;
            std::wstring_view basePath;
            std::string_view content;
        };

        // This is annoyingly fragile because we do not have a test hook helper to do this in SettingsLoader.
        static winrt::com_ptr<implementation::CascadiaSettings> createSettingsWithFragments(const std::string_view& userJSON, std::initializer_list<Fragment> fragments)
        {
            implementation::SettingsLoader loader{ userJSON, staticDefaultSettings };
            const winrt::hstring baseUserSettingsPath{ LR"(C:\Windows)" };
            loader.userSettings.baseLayerProfile->SourceBasePath = baseUserSettingsPath;
            loader.userSettings.globals->SourceBasePath = baseUserSettingsPath;
            for (auto&& userProfile : loader.userSettings.profiles)
            {
                userProfile->SourceBasePath = baseUserSettingsPath;
            }

            loader.MergeInboxIntoUserSettings();
            for (const auto& fragment : fragments)
            {
                loader.MergeFragmentIntoUserSettings(winrt::hstring{ fragment.source },
                                                     winrt::hstring{ fragment.basePath },
                                                     fragment.content);
            }
            loader.FinalizeLayering();
            loader.FixupUserSettings();
            return winrt::make_self<implementation::CascadiaSettings>(std::move(loader));
        }

        static winrt::com_ptr<implementation::CascadiaSettings> createSettings(const std::string_view& userJSON)
        {
            return createSettingsWithFragments(userJSON, {});
        }
    };

    bool MediaResourceTests::DisableFSRedirection()
    {
#if defined(_M_IX86)
        // Some of our tests use paths under system32. Just don't redirect them.
        Wow64DisableWow64FsRedirection(&redirectionFlag);
#endif
        return true;
    }

    bool MediaResourceTests::RestoreFSRedirection()
    {
#if defined(_M_IX86)
        Wow64RevertWow64FsRedirection(redirectionFlag);
#endif
        return true;
    }

    bool MediaResourceTests::ResetMediaHook()
    {
        g_mediaResolverHook = nullptr;
        return true;
    }

#pragma region Basic Operation
    void MediaResourceTests::ValidateResolverCalledForInbox()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        auto [t, e] = requireCalled(numberOfMediaResourcesInDefaultSettings,
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
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        std::unordered_map<OriginTag, int> origins;

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            // The icon in profiles.defaults erases the icon in the Base Profile and the one on the command; they will not be resolved
            // TODO GH#19201: This should be called one fewer time because overriding the command's icon should delete it before it gets resolved
            auto [t, e] = requireCalled(numberOfMediaResourcesInDefaultSettings - 1 /* base icon deleted */ + 2 /* icons specified by user */,
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
    },
    "actions": [
        {
            "command": {
                "action": "sendInput",
                "input": "IT CAME FROM BEYOND THE STARS"
            },
            "icon": null,
            "id": "Terminal.CloseWindow"
        }
    ],
})");
        }

        // TODO GH#19201: This should be base-2, 1, 1 (because we deleted the InBox command icon)
        VERIFY_ARE_EQUAL(origins[OriginTag::InBox], numberOfMediaResourcesInDefaultSettings - 1); // Base profile icon not resolved because of profiles.defaults.icon
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

    void MediaResourceTests::ValidateResolverCalledForFragments()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        std::unordered_map<OriginTag, int> origins;

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled(numberOfMediaResourcesInDefaultSettings + 2 /* fragment resources */,
                                        [&](auto&& origin, auto&& basePath, auto&& resource) {
                                            if (origin == OriginTag::Fragment)
                                            {
                                                VERIFY_ARE_EQUAL(fragmentBasePath1, basePath);
                                            }
                                            origins[origin]++;
                                            resource.Resolve(L"resolved");
                                        });
            g_mediaResolverHook = t;
            settings = createSettingsWithFragments(R"({})", { Fragment{ L"fragment", fragmentBasePath1, R"(
{
    "profiles": [
         {
            "guid": "{4e7c2b36-642f-4694-83f8-8a5052038a23}",
            "name": "FragmentProfile",
            "commandline": "not_a_real_path",
            "icon": "DoesNotMatterIgnoredByMockResolver"
        }
    ],
    "actions": [
        {
            "command": {
                "action": "sendInput",
                "input": "SOME DAY SOMETHING'S COMING"
            },
            "icon": "foo.ico",
            "id": "Dustin.SendInput"
        }
    ],
}
)" } });
        }

        VERIFY_ARE_EQUAL(origins[OriginTag::Fragment], 2);

        auto profile{ settings->GetProfileByName(L"FragmentProfile") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok());
        VERIFY_ARE_EQUAL(LR"(resolved)", icon.Resolved());
    }

    void MediaResourceTests::ValidateResolverCalledForNewTabMenuEntries()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        std::unordered_map<OriginTag, int> origins;

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled(numberOfMediaResourcesInDefaultSettings + 6 /* menu entry resources */,
                                        [&](auto&& origin, auto&& basePath, auto&& resource) {
                                            if (origin == OriginTag::User)
                                            {
                                                VERIFY_ARE_NOT_EQUAL(L"", basePath);
                                            }
                                            origins[origin]++;
                                            resource.Resolve(L"resolved");
                                        });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "newTabMenu": [ 
        {
            "icon": "menuItemIcon1",
            "id": "Terminal.CloseWindow",
            "type": "action"
        },
        {
            "icon": "menuItemIcon2",
            "profile": "{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}",
            "type": "profile"
        },
        {
            "allowEmpty": true,
            "entries": [
                {
                    "icon": "menuItemIcon4",
                    "profile": "{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}",
                    "type": "profile"
                },
                {
                    "allowEmpty": true,
                    "entries": [
                        {
                            "icon": "menuItemIcon6",
                            "profile": "{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}",
                            "type": "profile"
                        },
                    ],
                    "icon": "menuItemIcon5",
                    "inline": "never",
                    "name": "Or was it...?",
                    "type": "folder"
                }
            ],
            "icon": "menuItemIcon3",
            "inline": "never",
            "name": "Lovecraft in Brooklyn",
            "type": "folder"
        }
    ]
})");
        }

        VERIFY_ARE_EQUAL(origins[OriginTag::InBox], numberOfMediaResourcesInDefaultSettings);
        VERIFY_ARE_EQUAL(origins[OriginTag::User], 6);
    }

    void MediaResourceTests::ValidateResolverCalledIncrementallyOnChange()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        std::unordered_map<OriginTag, int> origins;

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            // The icon in profiles.defaults erases the icon in the Base Profile; that one will NOT be resolved.
            auto [t, e] = requireCalled(numberOfMediaResourcesInDefaultSettings - 1 /* base deleted */ + 2 /* user profile and defaults */,
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

        VERIFY_ARE_EQUAL(origins[OriginTag::InBox], numberOfMediaResourcesInDefaultSettings - 1); // Base profile icon not resolved because of profiles.defaults.icon
        VERIFY_ARE_EQUAL(origins[OriginTag::ProfilesDefaults], 1);
        VERIFY_ARE_EQUAL(origins[OriginTag::User], 1);

        auto profile{ settings->GetProfileByName(L"Base") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok());
        VERIFY_ARE_NOT_EQUAL(L"iconFromBase", icon.Path());
        VERIFY_ARE_EQUAL(L"iconFromDefaults", icon.Path());
        VERIFY_ARE_EQUAL(L"resolved", icon.Resolved());

        icon = MediaResourceHelper::FromString(L"NewIconFromRuntime");
        profile.Icon(icon);

        // Not OK until resolved!
        VERIFY_IS_FALSE(icon.Ok());

        {
            origins.clear();
            // We should be called only once, for the newly changed icon.
            auto [t, e] = requireCalled(1,
                                        [&](auto&& origin, auto&&, auto&& resource) {
                                            origins[origin]++;
                                            resource.Resolve(L"newResolvedValue");
                                        });
            g_mediaResolverHook = t;
            settings->ResolveMediaResources();
        }

        VERIFY_ARE_EQUAL(origins[OriginTag::User], 1); // This should be on the User's copy (not Defaults) of the Profile now.

        VERIFY_IS_TRUE(icon.Ok());
        VERIFY_ARE_EQUAL(L"NewIconFromRuntime", icon.Path());
        VERIFY_ARE_EQUAL(L"newResolvedValue", icon.Resolved());
    }

    void MediaResourceTests::ValidateResolverNotCalledForEmojiIcons()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        std::unordered_map<OriginTag, int> origins;

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled(numberOfMediaResourcesInDefaultSettings, // only called for inbox resources, none of the emoji icon profiles
                                        [&](auto&& origin, auto&&, auto&& resource) {
                                            VERIFY_ARE_NOT_EQUAL(OriginTag::User, origin);
                                            origins[origin]++;
                                            resource.Reject();
                                        });
            g_mediaResolverHook = t;
            settings = createSettings(R"({
    "profiles": {
        "list": [
            {
                "icon": "\u2665",
                "name": "Basic"
            },
            {
                "icon": "\ue720",
                "name": "MDL2"
            },
            {
                "icon": "👨‍👩‍👧‍👦",
                "name": "GraphemeCluster"
            },
            {
                "icon": "🕴️",
                "name": "SurrogatePair"
            },
            {
                "icon": "#\ufe0f\u20e3",
                "name": "VariantWithEnclosingCombiner"
            },
        ]
    }
})");
        }

        VERIFY_ARE_EQUAL(origins[OriginTag::User], 0);

        {
            auto profile{ settings->GetProfileByName(L"Basic") };
            auto icon{ profile.Icon() };
            VERIFY_IS_TRUE(icon.Ok());
            VERIFY_ARE_EQUAL(icon.Resolved(), icon.Path());
            VERIFY_ARE_EQUAL(L"\u2665", icon.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"MDL2") };
            auto icon{ profile.Icon() };
            VERIFY_IS_TRUE(icon.Ok());
            VERIFY_ARE_EQUAL(icon.Resolved(), icon.Path());
            VERIFY_ARE_EQUAL(L"\ue720", icon.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"GraphemeCluster") };
            auto icon{ profile.Icon() };
            VERIFY_IS_TRUE(icon.Ok());
            VERIFY_ARE_EQUAL(icon.Resolved(), icon.Path());
            VERIFY_ARE_EQUAL(L"\U0001F468\u200d\U0001F469\u200d\U0001F467\u200d\U0001F466", icon.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"SurrogatePair") };
            auto icon{ profile.Icon() };
            VERIFY_IS_TRUE(icon.Ok());
            VERIFY_ARE_EQUAL(icon.Resolved(), icon.Path());
            VERIFY_ARE_EQUAL(L"\U0001F574\uFE0F", icon.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"VariantWithEnclosingCombiner") };
            auto icon{ profile.Icon() };
            VERIFY_IS_TRUE(icon.Ok());
            VERIFY_ARE_EQUAL(icon.Resolved(), icon.Path());
            VERIFY_ARE_EQUAL(L"#\uFE0F\u20E3", icon.Resolved());
        }
    }
#pragma endregion

#pragma region Fragment Behaviors
    void MediaResourceTests::FragmentUpdatesBaseProfile()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled(numberOfMediaResourcesInDefaultSettings - 1 /* base deleted */ + 1 /* icon in fragment */,
                                        [&](auto&&, auto&& basePath, auto&& resource) {
                                            resource.Resolve(basePath);
                                        });
            g_mediaResolverHook = t;
            settings = createSettingsWithFragments(R"({})", { Fragment{ L"fragment", fragmentBasePath1, R"(
{
    "profiles": [
         {
            "updates": "{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}",
            "icon": "IconFromFragment"
        }
    ]
}
)" } });
        }

        auto profile{ settings->GetProfileByName(L"Base") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok());
        VERIFY_ARE_EQUAL(LR"(IconFromFragment)", icon.Path());
        // This was resolved by the mock resolver to the supplied base path; it's a quick way to check the right one got resolved :)
        VERIFY_ARE_EQUAL(fragmentBasePath1, icon.Resolved());
    }

    void MediaResourceTests::FragmentActionResourcesGetResolved()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled([&](auto&&, auto&& basePath, auto&& resource) {
                resource.Resolve(basePath);
            });
            g_mediaResolverHook = t;
            settings = createSettingsWithFragments(R"({})", { Fragment{ L"fragment", fragmentBasePath1, R"(
{
    "profiles": [
         {
            "updates": "{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}",
            "icon": "IconFromFragment"
        }
    ],
    "actions": [
        {
            "command": {
                "action": "sendInput",
                "input": "FROM WAY OUT BEYOND THE STARS"
            },
            "icon": "foo.ico",
            "id": "Dustin.SendInput"
        }
    ],
}
)" } });
        }

        {
            auto command{ settings->ActionMap().GetActionByID(L"Dustin.SendInput") };
            auto icon{ command.Icon() };
            VERIFY_IS_TRUE(icon.Ok());
            VERIFY_ARE_EQUAL(LR"(foo.ico)", icon.Path());
            // This was resolved by the mock resolver to the supplied base path; it's a quick way to check the right one got resolved :)
            VERIFY_ARE_EQUAL(fragmentBasePath1, icon.Resolved());
        }
    }

    void MediaResourceTests::DisabledFragmentNotResolved()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            // This should only be called baseline number of times, because the fragment is disabled.
            auto [t, e] = requireCalled(numberOfMediaResourcesInDefaultSettings,
                                        [&](auto&& origin, auto&&, auto&& resource) {
                                            // If we get a Fragment here, we messed up.
                                            VERIFY_ARE_NOT_EQUAL(origin, OriginTag::Fragment);
                                            resource.Resolve(L"resolved");
                                        });
            g_mediaResolverHook = t;
            settings = createSettingsWithFragments(R"({ "disabledProfileSources": [ "fragment" ] })",
                                                   { Fragment{ L"fragment", fragmentBasePath1, R"(
{
    "profiles": [
         {
            "guid": "{4e7c2b36-642f-4694-83f8-8a5052038a23}",
            "name": "FragmentProfile",
            "commandline": "not_a_real_path",
            "icon": "DoesNotMatterIgnoredByMockResolver"
        }
    ]
}
)" } });
        }

        auto profile{ settings->GetProfileByName(L"Base") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok());
        VERIFY_ARE_EQUAL(LR"(iconFromBase)", icon.Path());
        VERIFY_ARE_EQUAL(L"resolved", icon.Resolved());
    }

    // This is more of a test of how unfocused appearances are inherited (in whole), but it's worth
    // making sure that the fragment appearance doesn't impact and the unfocused appearance's base paths.
    void MediaResourceTests::FragmentAppearanceAndUserAppearanceInteraction()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        winrt::com_ptr<implementation::CascadiaSettings> settings;
        {
            auto [t, e] = requireCalled([&](auto&&, auto&& basePath, auto&& resource) {
                resource.Resolve(fmt::format(FMT_COMPILE(L"{}-{}"), basePath, resource.Path()));
            });
            g_mediaResolverHook = t;
            settings = createSettingsWithFragments(R"(
{
    "profiles": [
         {
            "guid": "{4e7c2b36-642f-4694-83f8-8a5052038a23}",
            "unfocusedAppearance": {
                "experimental.pixelShaderPath": "unfocusedPixelShaderPath1"
            }
        },
        {
            "guid": "{94df2990-d645-4675-8d9d-f8c89f842e6b}",
            "unfocusedAppearance": {
                "backgroundImage": "userSpecifiedUnfocusedBackgroundImage"
            }
        }
    ]
}
)",
                                                   { Fragment{ L"fragment", L"FRAGMENT", R"(
{
    "profiles": [
         {
            "guid": "{4e7c2b36-642f-4694-83f8-8a5052038a23}",
            "name": "FragmentProfileWithUnfocusedBackgroundImage",
            "commandline": "not_a_real_path",
            "backgroundImage": "focusedBackgroundImage1",
            "unfocusedAppearance": {
                "backgroundImage": "unfocusedBackgroundImage1"
            }
        },
        {
            "guid": "{94df2990-d645-4675-8d9d-f8c89f842e6b}",
            "name": "FragmentProfileWithNoUnfocusedBackgroundImage",
            "commandline": "not_a_real_path",
            "backgroundImage": "focusedBackgroundImage2",
        }
    ]
}
)" } });
        }

        // The resolver produces finalized resource paths by taking base paths (c:\windows, or FRAGMENT) and
        // combining them with the input paths. This lets us more easily track which resource came from where.

        {
            auto profile{ settings->GetProfileByName(L"FragmentProfileWithUnfocusedBackgroundImage") };
            auto defaultAppearance{ profile.DefaultAppearance() };
            auto unfocusedAppearance{ profile.UnfocusedAppearance() };

            VERIFY_IS_NOT_NULL(unfocusedAppearance);

            auto focusedBackground{ defaultAppearance.BackgroundImagePath() };
            auto unfocusedBackground{ unfocusedAppearance.BackgroundImagePath() };
            auto unfocusedPixelShader{ unfocusedAppearance.PixelShaderPath() };

            VERIFY_IS_TRUE(focusedBackground.Ok());
            VERIFY_IS_TRUE(unfocusedBackground.Ok());
            VERIFY_IS_TRUE(unfocusedPixelShader.Ok());

            VERIFY_ARE_EQUAL(LR"(FRAGMENT-focusedBackgroundImage1)", focusedBackground.Resolved());
            // The user changing the unfocusedAppearance object caused it to revert back to the focused one in the profile (!)
            VERIFY_ARE_EQUAL(focusedBackground.Resolved(), unfocusedBackground.Resolved());
            const void* focusedBackgroundAbi{ winrt::get_abi(focusedBackground) };
            const void* unfocusedBackgroundAbi{ winrt::get_abi(unfocusedBackground) };
            VERIFY_ARE_EQUAL(focusedBackgroundAbi, unfocusedBackgroundAbi); // Objects should be identical in this case
            VERIFY_ARE_EQUAL(LR"(C:\Windows-unfocusedPixelShaderPath1)", unfocusedPixelShader.Resolved()); // This is resolved to the user's base path
        }

        {
            auto profile{ settings->GetProfileByName(L"FragmentProfileWithNoUnfocusedBackgroundImage") };
            auto defaultAppearance{ profile.DefaultAppearance() };
            auto unfocusedAppearance{ profile.UnfocusedAppearance() };

            VERIFY_IS_NOT_NULL(unfocusedAppearance);

            auto focusedBackground{ defaultAppearance.BackgroundImagePath() };
            auto unfocusedBackground{ unfocusedAppearance.BackgroundImagePath() };

            VERIFY_IS_TRUE(focusedBackground.Ok());
            VERIFY_IS_TRUE(unfocusedBackground.Ok());

            VERIFY_ARE_EQUAL(LR"(FRAGMENT-focusedBackgroundImage2)", focusedBackground.Resolved());
            VERIFY_ARE_EQUAL(LR"(C:\Windows-userSpecifiedUnfocusedBackgroundImage)", unfocusedBackground.Resolved());
        }
    }
#pragma endregion

#pragma region Profile Behaviors
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
        }
    }
})");
        }

        auto profile{ settings->GetProfileByName(L"Base") };
        auto icon{ profile.Icon() };
        VERIFY_IS_TRUE(icon.Ok()); // Profile with commandline always has an icon
        VERIFY_ARE_EQUAL(cmdCommandline, icon.Resolved());
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

    // The invalid resource came from the profile itself, where the commandline is the default value (profile.commandline default value is CMD.exe)
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
        VERIFY_ARE_EQUAL(cmdCommandline, icon.Resolved());
    }

    // The invalid resource came from the Defaults profile, where the commandline falls back to the default value of CMD.exe (PROFILE COMMANDLINE IGNORED)
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
            "icon": "DoesNotMatter"
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
        VERIFY_ARE_EQUAL(cmdCommandline, icon.Resolved());
    }

    // The invalid resource came from the Defaults profile, which has the default command line of CMD.exe (PROFILE COMMANDLINE MISSING)
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
            "icon": "DoesNotMatter"
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
        VERIFY_ARE_EQUAL(cmdCommandline, icon.Resolved());
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

    // The invalid resource came from the profile itself, where the commandline falls back to the default value of CMD.exe
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
            "icon": "DoesNotMatter"
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
        VERIFY_ARE_EQUAL(cmdCommandline, icon.Resolved());
    }

    // A profile replaces the bell sounds (2) in the base settings; all bell sounds retained
    void MediaResourceTests::ProfileOverwritesBellSound()
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
        "list": [
            {
                "guid": "{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}",
                "bellSound": [
                    "does not matter; resolved rejected"
                ],
            },
        ]
    }
})");
        }

        auto profile{ settings->GetProfileByName(L"Base") };
        auto bellSounds{ profile.BellSound() };
        VERIFY_ARE_EQUAL(1u, bellSounds.Size());
        VERIFY_IS_FALSE(bellSounds.GetAt(0).Ok());
    }
#pragma endregion

#pragma region Real Resolver Tests
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
            VERIFY_IS_TRUE(til::equals_insensitive_ascii(LR"(c:\windows\system32\cmd.exe)", image.Resolved()), WEX::Common::NoThrowString{ image.Resolved().c_str() });
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileInvalidImage") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_FALSE(image.Ok());
            VERIFY_ARE_EQUAL(L"", image.Resolved());
        }
    }

    static std::optional<winrt::hstring> _getDesktopWallpaper()
    {
        WCHAR desktopWallpaper[MAX_PATH];

        // "The returned string will not exceed MAX_PATH characters" as of 2020
        if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, desktopWallpaper, SPIF_UPDATEINIFILE))
        {
            return winrt::hstring{ &desktopWallpaper[0] };
        }

        return std::nullopt;
    }

    void MediaResourceTests::RealResolverSpecialKeywords()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        g_mediaResolverHook = nullptr; // Use the real resolver

        // For profile, we test images instead of icon because Icon has a fallback behavior.
        auto settings = createSettings(R"({
    "profiles": {
        "list": [
            {
                "backgroundImage": "none",
                "name": "ProfileNoneImage"
            },
            {
                "backgroundImage": "desktopWallpaper",
                "name": "ProfileDesktopWallpaperImage"
            }
        ]
    }
})");

        {
            auto profile{ settings->GetProfileByName(L"ProfileNoneImage") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_NOT_EQUAL(L"none", image.Resolved());
            VERIFY_ARE_EQUAL(L"", image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileDesktopWallpaperImage") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            if (const auto desktopWallpaper{ _getDesktopWallpaper() })
            {
                VERIFY_IS_TRUE(image.Ok());
                VERIFY_ARE_NOT_EQUAL(L"desktopWallpaper", image.Resolved());
                VERIFY_ARE_EQUAL(*desktopWallpaper, image.Resolved());
            }
            else
            {
                WEX::Logging::Log::Comment(L"No wallpaper is set; testing failure case instead");
                VERIFY_IS_FALSE(image.Ok());
                VERIFY_ARE_EQUAL(L"", image.Resolved());
            }
        }
    }

    void MediaResourceTests::RealResolverUrlCases()
    {
        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};

        g_mediaResolverHook = nullptr; // Use the real resolver

        // For profile, we test images instead of icon because Icon has a fallback behavior.
        auto settings = createSettings(R"({
    "profiles": {
        "list": [
            {
                "backgroundImage": "https://contoso.com/explorer.exe",
                "name": "ProfileWebUri"
            },
            {
                "backgroundImage": "https://contoso.com/it_would_be_a_real_surprise_if_windows_added_a_file_named_this.ico",
                "name": "ProfileWebUriDoesNotExistLocally"
            },
            {
                "backgroundImage": "file:///C:/Windows/System32/cmd.exe",
                "name": "ProfileAbsoluteFileUri"
            },
            {
                "backgroundImage": "ms-resource:///ProfileIcons/foo.png",
                "name": "ProfileAppResourceUri"
            },
            {
                "backgroundImage": "ms-appx:///ProfileIcons/foo.png",
                "name": "ProfileAppxUriLocal"
            },
            {
                "backgroundImage": "ms-appx://Microsoft.Burrito/Resources/explorer.exe",
                "name": "ProfileAppxUriOtherApp"
            },
            {
                "backgroundImage": "ftp://0.0.0.0/share/file.png",
                "name": "ProfileIllegalWebUri"
            },
            {
                "backgroundImage": "x://is_this_a_file_or_a_path",
                "name": "ProfileIllegalUri1"
            },
            {
                "backgroundImage": "fake-scheme://foo",
                "name": "ProfileIllegalUri2"
            },
            {
                "backgroundImage": "http:/e/x",
                "name": "ProfileIllegalUri3"
            },
        ]
    }
})");

        // All relative paths are relative to the fake testing user settings path of C:\Windows
        constexpr std::wstring_view expectedCmdPath{ LR"(C:\Windows\System32\cmd.exe)" };
        constexpr std::wstring_view expectedExplorerPath{ LR"(C:\Windows\explorer.exe)" };

        // http URLs are resolved to the base path (in this case, user settings path of C:\Windows) plus leaf filename.
        // ms-appx URLs pointing to *any app* (which implies it is not our app) are treated the same.

        {
            auto profile{ settings->GetProfileByName(L"ProfileWebUri") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_EQUAL(expectedExplorerPath, image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileWebUriDoesNotExistLocally") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_FALSE(image.Ok());
            VERIFY_ARE_NOT_EQUAL(image.Resolved(), image.Path());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileAbsoluteFileUri") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_EQUAL(expectedCmdPath, image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileAppResourceUri") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_EQUAL(LR"(ms-resource:///ProfileIcons/foo.png)", image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileAppxUriLocal") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_EQUAL(LR"(ms-appx:///ProfileIcons/foo.png)", image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileAppxUriOtherApp") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_TRUE(image.Ok());
            VERIFY_ARE_EQUAL(expectedExplorerPath, image.Resolved());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileIllegalWebUri") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_FALSE(image.Ok());
            VERIFY_ARE_EQUAL(L"", image.Resolved());
            VERIFY_ARE_NOT_EQUAL(image.Resolved(), image.Path());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileIllegalUri1") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_FALSE(image.Ok());
            VERIFY_ARE_EQUAL(L"", image.Resolved());
            VERIFY_ARE_NOT_EQUAL(image.Resolved(), image.Path());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileIllegalUri2") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_FALSE(image.Ok());
            VERIFY_ARE_EQUAL(L"", image.Resolved());
            VERIFY_ARE_NOT_EQUAL(image.Resolved(), image.Path());
        }

        {
            auto profile{ settings->GetProfileByName(L"ProfileIllegalUri3") };
            auto image{ profile.DefaultAppearance().BackgroundImagePath() };
            VERIFY_IS_FALSE(image.Ok());
            VERIFY_ARE_EQUAL(L"", image.Resolved());
            VERIFY_ARE_NOT_EQUAL(image.Resolved(), image.Path());
        }
    }
#pragma endregion
}
