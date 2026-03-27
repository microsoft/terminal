// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalApp/TerminalPage.h"
#include "../TerminalApp/TerminalWindow.h"
#include "../TerminalApp/MinMaxCloseControl.h"
#include "../TerminalApp/TabRowControl.h"
#include "../TerminalApp/ShortcutActionDispatch.h"
#include "../TerminalApp/Tab.h"
#include "../TerminalApp/CommandPalette.h"
#include "../TerminalApp/ContentManager.h"
#include "CppWinrtTailored.h"
#include <cwctype>

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings::Model;

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Text;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace TerminalAppLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class TabTests
    {
        // For this set of tests, we need to activate some XAML content. For
        // release builds, the application runs as a centennial application,
        // which lets us run full trust, and means that we need to use XAML
        // Islands to host our UI. However, in these tests, we don't really need
        // to run full trust - we just need to get some UI elements created. So
        // we can just rely on the normal UWP activation to create us.
        //
        // IMPORTANTLY! When tests need to make XAML objects, or do XAML things,
        // make sure to use RunOnUIThread. This helper will dispatch a lambda to
        // be run on the UI thread.

        BEGIN_TEST_CLASS(TabTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        // These four tests act as canary tests. If one of them fails, then they
        // can help you identify if something much lower in the stack has
        // failed.
        TEST_METHOD(EnsureTestsActivate);
        TEST_METHOD(TryCreateConnectionType);
        TEST_METHOD(TryCreateXamlObjects);

        TEST_METHOD(TryInitializePage);

        TEST_METHOD(CreateSimpleTerminalXamlType);
        TEST_METHOD(CreateTerminalMuxXamlType);

        TEST_METHOD(CreateTerminalPage);

        TEST_METHOD(TryDuplicateBadTab);
        TEST_METHOD(TryDuplicateBadPane);
        TEST_METHOD(TrackSelectedTabsVisualState);
        TEST_METHOD(MoveMultipleTabsAsBlock);
        TEST_METHOD(BuildStartupActionsForMultipleTabs);
        TEST_METHOD(AttachContentInsertsDraggedTabsAfterTargetIndex);

        TEST_METHOD(TryZoomPane);
        TEST_METHOD(MoveFocusFromZoomedPane);
        TEST_METHOD(CloseZoomedPane);

        TEST_METHOD(SwapPanes);

        TEST_METHOD(NextMRUTab);
        TEST_METHOD(VerifyCommandPaletteTabSwitcherOrder);

        TEST_METHOD(TestWindowRenameSuccessful);
        TEST_METHOD(TestWindowRenameFailure);

        TEST_METHOD(TestPreviewCommitScheme);
        TEST_METHOD(TestPreviewDismissScheme);
        TEST_METHOD(TestPreviewSchemeWhilePreviewing);

        TEST_METHOD(TestClampSwitchToTab);

        TEST_CLASS_SETUP(ClassSetup)
        {
            return true;
        }

        TEST_METHOD_CLEANUP(MethodCleanup)
        {
            return true;
        }

    private:
        void _primeApplicationResourcesForTabTests();
        void _initializeTerminalPage(winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page,
                                     CascadiaSettings initialSettings,
                                     winrt::TerminalApp::ContentManager contentManager = nullptr);
        void _yieldToLowPriorityDispatcher(const winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page);
        void _ensureStableBaselineTab(const winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page);
        void _waitForTabCount(const winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page, uint32_t expectedCount);
        void _openProfileTab(const winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page, int32_t profileIndex, uint32_t expectedCount);
        winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage> _commonSetup(winrt::TerminalApp::ContentManager contentManager = nullptr);
        winrt::com_ptr<winrt::TerminalApp::implementation::WindowProperties> _windowProperties;
        winrt::com_ptr<winrt::TerminalApp::implementation::ContentManager> _contentManager;
    };

    template<typename TFunction>
    void TestOnUIThread(const TFunction& function)
    {
        const auto result = RunOnUIThread(function);
        VERIFY_SUCCEEDED(result);
    }

    static NewTerminalArgs _profileArgs(const int32_t profileIndex)
    {
        auto index = profileIndex;
        return NewTerminalArgs{ index };
    }

    void TabTests::_yieldToLowPriorityDispatcher(const winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page)
    {
        VERIFY_IS_NOT_NULL(page);

        details::Event completedEvent;
        VERIFY_IS_TRUE(completedEvent.IsValid());

        auto action = page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Low, [] {});
        action.Completed([&completedEvent](auto&&, auto&&) {
            completedEvent.Set();
            return S_OK;
        });

        VERIFY_SUCCEEDED(completedEvent.Wait());
    }

    void TabTests::EnsureTestsActivate()
    {
        // This test was originally used to ensure that XAML Islands was
        // initialized correctly. Now, it's used to ensure that the tests
        // actually deployed and activated. This test _should_ always pass.
        VERIFY_IS_TRUE(true);
    }

    void TabTests::TryCreateConnectionType()
    {
        // Verify we can create a WinRT type we authored
        // Just creating it is enough to know that everything is working.
        winrt::Microsoft::Terminal::TerminalConnection::EchoConnection conn{};
        VERIFY_IS_NOT_NULL(conn);
    }

    void TabTests::TryCreateXamlObjects()
    {
        auto result = RunOnUIThread([]() {
            VERIFY_IS_TRUE(true, L"Congrats! We're running on the UI thread!");

            auto v = winrt::Windows::ApplicationModel::Core::CoreApplication::GetCurrentView();
            VERIFY_IS_NOT_NULL(v, L"Ensure we have a current view");
            // Verify we can create a some XAML objects
            // Just creating all of them is enough to know that everything is working.
            winrt::Windows::UI::Xaml::Controls::UserControl controlRoot;
            VERIFY_IS_NOT_NULL(controlRoot, L"Try making a UserControl");
            winrt::Windows::UI::Xaml::Controls::Grid root;
            VERIFY_IS_NOT_NULL(root, L"Try making a Grid");
            winrt::Windows::UI::Xaml::Controls::SwapChainPanel swapChainPanel;
            VERIFY_IS_NOT_NULL(swapChainPanel, L"Try making a SwapChainPanel");
            winrt::Windows::UI::Xaml::Controls::Primitives::ScrollBar scrollBar;
            VERIFY_IS_NOT_NULL(scrollBar, L"Try making a ScrollBar");
        });

        VERIFY_SUCCEEDED(result);
    }

    void TabTests::CreateSimpleTerminalXamlType()
    {
        winrt::com_ptr<winrt::TerminalApp::implementation::MinMaxCloseControl> mmcc{ nullptr };

        auto result = RunOnUIThread([&mmcc]() {
            mmcc = winrt::make_self<winrt::TerminalApp::implementation::MinMaxCloseControl>();
            VERIFY_IS_NOT_NULL(mmcc);
        });
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::_waitForTabCount(const winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page, const uint32_t expectedCount)
    {
        VERIFY_IS_NOT_NULL(page);

        uint32_t actualCount{};
        uint32_t actualTabItemCount{};
        uint32_t consecutiveMatches{};
        for (auto attempt = 0; attempt < 20; ++attempt)
        {
            _yieldToLowPriorityDispatcher(page);

            const auto result = RunOnUIThread([&]() {
                page->_tabView.UpdateLayout();
                actualCount = page->_tabs.Size();
                actualTabItemCount = page->_tabView.TabItems().Size();
            });
            VERIFY_SUCCEEDED(result);

            if (actualCount == expectedCount && actualTabItemCount == expectedCount)
            {
                if (++consecutiveMatches >= 2)
                {
                    return;
                }
            }
            else
            {
                consecutiveMatches = 0;
            }

            Sleep(50);
        }

        VERIFY_ARE_EQUAL(expectedCount, actualCount);
        VERIFY_ARE_EQUAL(expectedCount, actualTabItemCount);
    }

    void TabTests::_ensureStableBaselineTab(const winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page)
    {
        VERIFY_IS_NOT_NULL(page);

        uint32_t latestCount{};
        uint32_t stableBaselineSamples{};
        uint32_t repairs{};

        for (auto attempt = 0; attempt < 40; ++attempt)
        {
            _yieldToLowPriorityDispatcher(page);

            auto result = RunOnUIThread([&]() {
                page->_tabView.UpdateLayout();
                latestCount = page->_tabs.Size();
            });
            VERIFY_SUCCEEDED(result);

            if (latestCount == 0)
            {
                stableBaselineSamples = 0;
                ++repairs;
                _openProfileTab(page, 0, 1);
                continue;
            }

            if (latestCount == 1)
            {
                if (++stableBaselineSamples >= 3)
                {
                    Log::Comment(NoThrowString().Format(L"Stable baseline final count=%u repairs=%u", latestCount, repairs));
                    return;
                }
            }
            else
            {
                stableBaselineSamples = 0;
            }

            Sleep(50);
        }

        Log::Comment(NoThrowString().Format(L"Stable baseline final count=%u repairs=%u stableSamples=%u", latestCount, repairs, stableBaselineSamples));
        VERIFY_ARE_EQUAL(1u, latestCount);
    }

    void TabTests::_openProfileTab(const winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page, const int32_t profileIndex, const uint32_t expectedCount)
    {
        VERIFY_IS_NOT_NULL(page);

        _yieldToLowPriorityDispatcher(page);

        TestOnUIThread([&]() {
            auto beforeCount = page->_tabs.Size();

            if (profileIndex != 0 && (beforeCount + 1) < expectedCount)
            {
                const auto baselineHr = page->_OpenNewTab(_profileArgs(0));
                const auto baselineCount = page->_tabs.Size();
                Log::Comment(NoThrowString().Format(
                    L"_OpenNewTab repaired missing baseline -> hr=0x%08x before=%u after=%u",
                    static_cast<unsigned int>(baselineHr),
                    beforeCount,
                    baselineCount));
                VERIFY_ARE_EQUAL(S_OK, baselineHr);
                beforeCount = baselineCount;
            }

            const auto hr = page->_OpenNewTab(_profileArgs(profileIndex));
            const auto afterCount = page->_tabs.Size();
            Log::Comment(NoThrowString().Format(
                L"_OpenNewTab(profileIndex=%d) -> hr=0x%08x before=%u after=%u",
                profileIndex,
                static_cast<unsigned int>(hr),
                beforeCount,
                afterCount));
            VERIFY_ARE_EQUAL(S_OK, hr);
        });

        _waitForTabCount(page, expectedCount);
    }

    void TabTests::CreateTerminalMuxXamlType()
    {
        winrt::com_ptr<winrt::TerminalApp::implementation::TabRowControl> tabRowControl{ nullptr };

        auto result = RunOnUIThread([&tabRowControl]() {
            tabRowControl = winrt::make_self<winrt::TerminalApp::implementation::TabRowControl>();
            VERIFY_IS_NOT_NULL(tabRowControl);
        });
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::CreateTerminalPage()
    {
        winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage> page{ nullptr };

        _primeApplicationResourcesForTabTests();

        _windowProperties = winrt::make_self<winrt::TerminalApp::implementation::WindowProperties>();
        winrt::TerminalApp::WindowProperties props = *_windowProperties;

        _contentManager = winrt::make_self<winrt::TerminalApp::implementation::ContentManager>();
        winrt::TerminalApp::ContentManager contentManager = *_contentManager;

        auto result = RunOnUIThread([&page, props, contentManager]() {
            page = winrt::make_self<winrt::TerminalApp::implementation::TerminalPage>(props, contentManager);
            VERIFY_IS_NOT_NULL(page);
        });
        VERIFY_SUCCEEDED(result);

        _yieldToLowPriorityDispatcher(page);
    }

    void TabTests::_primeApplicationResourcesForTabTests()
    {
        const auto result = RunOnUIThread([]() {
            const auto app = winrt::Windows::UI::Xaml::Application::Current();
            VERIFY_IS_NOT_NULL(app);

            // Keep priming intentionally minimal for LocalTests: avoid pulling in
            // full MUX dictionaries, but provide common TabView keys used during
            // TerminalPage InitializeComponent.
            auto resources = app.Resources();
            const auto ensureBrush = [&](const wchar_t* key) {
                const auto boxedKey = winrt::box_value(winrt::hstring{ key });
                if (!resources.HasKey(boxedKey))
                {
                    resources.Insert(boxedKey, winrt::Windows::UI::Xaml::Media::SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });
                }
            };
            const auto ensureDouble = [&](const wchar_t* key, const double value) {
                const auto boxedKey = winrt::box_value(winrt::hstring{ key });
                if (!resources.HasKey(boxedKey))
                {
                    resources.Insert(boxedKey, winrt::box_value(value));
                }
            };
            const auto ensureCornerRadius = [&](const wchar_t* key) {
                const auto boxedKey = winrt::box_value(winrt::hstring{ key });
                if (!resources.HasKey(boxedKey))
                {
                    resources.Insert(boxedKey, winrt::box_value(winrt::Windows::UI::Xaml::CornerRadius{ 0, 0, 0, 0 }));
                }
            };
            const auto ensureThickness = [&](const wchar_t* key) {
                const auto boxedKey = winrt::box_value(winrt::hstring{ key });
                if (!resources.HasKey(boxedKey))
                {
                    resources.Insert(boxedKey, winrt::box_value(winrt::Windows::UI::Xaml::Thickness{ 0, 0, 0, 0 }));
                }
            };
            const auto ensureStyle = [&](const wchar_t* key, const winrt::Windows::UI::Xaml::Interop::TypeName& targetType) {
                const auto boxedKey = winrt::box_value(winrt::hstring{ key });
                if (!resources.HasKey(boxedKey))
                {
                    winrt::Windows::UI::Xaml::Style style;
                    style.TargetType(targetType);
                    resources.Insert(boxedKey, style);
                }
            };

            ensureBrush(L"TabViewBackground");
            Log::Comment(NoThrowString().Format(L"Resource probe: TabViewBackground=%d CardStrokeColorDefaultBrush=%d",
                                                resources.HasKey(winrt::box_value(L"TabViewBackground")),
                                                resources.HasKey(winrt::box_value(L"CardStrokeColorDefaultBrush"))));
        });
        VERIFY_SUCCEEDED(result);
    }

    // Method Description:
    // - This is a helper to set up a TerminalPage for a unittest. This method
    //   does a couple things:
    //   * Create()'s a TerminalPage with the given settings. Constructing a
    //     TerminalPage so that we can get at its implementation is wacky, so
    //     this helper will do it correctly for you, even if this doesn't make a
    //     ton of sense on the surface. This is also why you need to pass both a
    //     projection and a com_ptr to this method.
    //   * It will use the provided settings object to initialize the TerminalPage
    //   * It will add the TerminalPage to the test Application, so that we can
    //     get actual layout events. Much of the Terminal assumes there's a
    //     non-zero ActualSize to the Terminal window, and adding the Page to
    //     the Application will make it behave as expected.
    //   * It will wait for the TerminalPage to finish initialization before
    //     returning control to the caller. It does this by creating an event and
    //     only setting the event when the TerminalPage raises its Initialized
    //     event, to signal that startup is complete. At this point, there will
    //     be one tab with the default profile in the page.
    //   * It will also ensure that the first tab is focused, since that happens
    //     asynchronously in the application typically.
    // Arguments:
    // - page: a TerminalPage implementation ptr that will receive the new TerminalPage instance
    // - initialSettings: a CascadiaSettings to initialize the TerminalPage with.
    // Return Value:
    // - <none>
    void TabTests::_initializeTerminalPage(winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage>& page,
                                           CascadiaSettings initialSettings,
                                           winrt::TerminalApp::ContentManager contentManager)
    {
        _primeApplicationResourcesForTabTests();

        // This is super wacky, but we can't just initialize the
        // com_ptr<impl::TerminalPage> in the lambda and assign it back out of
        // the lambda. We'll crash trying to get a weak_ref to the TerminalPage
        // during TerminalPage::Create() below.
        //
        // Instead, create the winrt object, then get a com_ptr to the
        // implementation _from_ the winrt object. This seems to work, even if
        // it's weird.
        winrt::TerminalApp::TerminalPage projectedPage{ nullptr };

        _windowProperties = winrt::make_self<winrt::TerminalApp::implementation::WindowProperties>();
        winrt::TerminalApp::WindowProperties props = *_windowProperties;
        if (!contentManager)
        {
            _contentManager = winrt::make_self<winrt::TerminalApp::implementation::ContentManager>();
            contentManager = *_contentManager;
        }
        else
        {
            _contentManager.copy_from(winrt::get_self<winrt::TerminalApp::implementation::ContentManager>(contentManager));
        }
        Log::Comment(NoThrowString().Format(L"Construct the TerminalPage"));
        auto result = RunOnUIThread([&projectedPage, &page, initialSettings, props, contentManager]() {
            const auto app = winrt::Windows::UI::Xaml::Application::Current();
            VERIFY_IS_NOT_NULL(app);
            auto resources = app.Resources();
            const auto donorResources = winrt::Microsoft::UI::Xaml::Controls::XamlControlsResources{};
            std::unordered_set<std::wstring> injectedKeys;

            const auto tryCopyFromDonor = [&](const winrt::hstring& key, winrt::Windows::Foundation::IInspectable& value) {
                const auto keyObj = winrt::box_value(key);
                if (donorResources.HasKey(keyObj))
                {
                    value = donorResources.Lookup(keyObj);
                    Log::Comment(NoThrowString().Format(L"Donor hit root key '%s'", key.c_str()));
                    return true;
                }

                const auto donorThemes = donorResources.ThemeDictionaries();
                for (const auto& theme : { L"Default", L"Light", L"Dark", L"HighContrast" })
                {
                    const auto themeObj = winrt::box_value(theme);
                    if (donorThemes.HasKey(themeObj))
                    {
                        const auto themeDictionary = donorThemes.Lookup(themeObj).as<winrt::Windows::UI::Xaml::ResourceDictionary>();
                        if (themeDictionary.HasKey(keyObj))
                        {
                            value = themeDictionary.Lookup(keyObj);
                            Log::Comment(NoThrowString().Format(L"Donor hit theme '%s' key '%s'", theme, key.c_str()));
                            return true;
                        }
                    }
                }
                return false;
            };

            const auto insertFallbackForKey = [&](const std::wstring& key) {
                const auto keyH = winrt::hstring{ key };
                const auto keyObj = winrt::box_value(keyH);
                if (resources.HasKey(keyObj))
                {
                    return;
                }

                winrt::Windows::Foundation::IInspectable value{ nullptr };
                if (!tryCopyFromDonor(keyH, value))
                {
                    if (key.find(L"Thickness") != std::wstring::npos || key.find(L"Padding") != std::wstring::npos || key.find(L"Margin") != std::wstring::npos)
                    {
                        value = winrt::box_value(winrt::Windows::UI::Xaml::Thickness{ 0, 0, 0, 0 });
                    }
                    else if (key.find(L"CornerRadius") != std::wstring::npos || key.find(L"Radius") != std::wstring::npos)
                    {
                        value = winrt::box_value(winrt::Windows::UI::Xaml::CornerRadius{ 0, 0, 0, 0 });
                    }
                    else if (key.find(L"Style") != std::wstring::npos)
                    {
                        winrt::Windows::UI::Xaml::Style style;
                        if (key.find(L"RepeatButtonStyle") != std::wstring::npos)
                        {
                            style.TargetType(winrt::xaml_typename<winrt::Windows::UI::Xaml::Controls::Primitives::RepeatButton>());
                        }
                        else if (key.find(L"ButtonStyle") != std::wstring::npos)
                        {
                            style.TargetType(winrt::xaml_typename<winrt::Windows::UI::Xaml::Controls::Button>());
                        }
                        else
                        {
                            style.TargetType(winrt::xaml_typename<winrt::Windows::UI::Xaml::Controls::Control>());
                        }
                        value = style;
                    }
                    else if (key.find(L"FontSize") != std::wstring::npos ||
                             key.find(L"Width") != std::wstring::npos ||
                             key.find(L"Height") != std::wstring::npos ||
                             key.find(L"Min") != std::wstring::npos ||
                             key.find(L"Max") != std::wstring::npos ||
                             key.find(L"Opacity") != std::wstring::npos)
                    {
                        value = winrt::box_value(0.0);
                    }
                    else
                    {
                        value = winrt::Windows::UI::Xaml::Media::SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() };
                    }
                }

                resources.Insert(keyObj, value);
            };

            for (auto attempt = 0; attempt < 512; attempt++)
            {
                try
                {
                    projectedPage = winrt::TerminalApp::TerminalPage(props, contentManager);
                    page.copy_from(winrt::get_self<winrt::TerminalApp::implementation::TerminalPage>(projectedPage));
                    page->_settings = initialSettings;
                    return;
                }
                catch (const winrt::hresult_error& ex)
                {
                    std::wstring message = ex.message().c_str();
                    constexpr std::wstring_view marker{ L"Cannot find a Resource with the Name/Key " };
                    const auto markerPos = message.find(marker);
                    if (markerPos == std::wstring::npos)
                    {
                        throw;
                    }

                    const auto keyStart = markerPos + marker.size();
                    const auto keyEnd = message.find(L" [", keyStart);
                    std::wstring key = message.substr(keyStart, keyEnd == std::wstring::npos ? std::wstring::npos : keyEnd - keyStart);
                    while (!key.empty() && iswspace(key.front()))
                    {
                        key.erase(key.begin());
                    }
                    while (!key.empty() && iswspace(key.back()))
                    {
                        key.pop_back();
                    }

                    if (key.empty())
                    {
                        throw;
                    }

                    if (!injectedKeys.contains(key))
                    {
                        Log::Comment(NoThrowString().Format(L"Injecting fallback resource for '%s'", key.c_str()));
                        injectedKeys.insert(key);
                    }
                    insertFallbackForKey(key);
                }
            }

            VERIFY_FAIL(L"Failed to initialize TerminalPage after fallback resource retries.");
        });
        VERIFY_SUCCEEDED(result);

        VERIFY_IS_NOT_NULL(page);
        VERIFY_IS_NOT_NULL(page->_settings);

        ::details::Event waitForInitEvent;
        if (!waitForInitEvent.IsValid())
        {
            VERIFY_SUCCEEDED(HRESULT_FROM_WIN32(::GetLastError()));
        }
        page->Initialized([&waitForInitEvent](auto&&, auto&&) {
            waitForInitEvent.Set();
        });

        Log::Comment(L"Create() the TerminalPage");

        result = RunOnUIThread([&page]() {
            VERIFY_IS_NOT_NULL(page);
            VERIFY_IS_NOT_NULL(page->_settings);

            // Build a NewTab action, to make sure we start with one. The real
            // Terminal will always get one from AppCommandlineArgs.
            NewTerminalArgs newTerminalArgs{};
            NewTabArgs args{ newTerminalArgs };
            ActionAndArgs newTabAction{ ShortcutAction::NewTab, args };
            page->SetStartupActions({ std::move(newTabAction) });
            Log::Comment(L"Configured a single startup newTab action");

            page->Create();
            Log::Comment(L"Create()'d the page successfully");

            auto app = ::winrt::Windows::UI::Xaml::Application::Current();

            winrt::TerminalApp::TerminalPage pp = *page;
            winrt::Windows::UI::Xaml::Window::Current().Content(pp);
            winrt::Windows::UI::Xaml::Window::Current().Activate();
            pp.UpdateLayout();
            page->_tabRow.UpdateLayout();
            page->_tabContent.UpdateLayout();
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(L"Wait for the page to finish initializing...");
        VERIFY_SUCCEEDED(waitForInitEvent.Wait());
        Log::Comment(L"...Done");

        uint32_t tabCount{};
        uint32_t tabItemCount{};
        bool hasSelectedItem{};
        for (auto attempt = 0; attempt < 20; ++attempt)
        {
            result = RunOnUIThread([&]() {
                tabCount = page->_tabs.Size();
                tabItemCount = page->_tabView.TabItems().Size();
                hasSelectedItem = static_cast<bool>(page->_tabView.SelectedItem());
            });
            VERIFY_SUCCEEDED(result);

            if (tabCount > 0)
            {
                break;
            }

            Sleep(50);
        }

        const auto selectFirstTab = [&page]() {
            const auto tabCount = page->_tabs.Size();
            const auto tabItemCount = page->_tabView.TabItems().Size();
            const auto hasSelectedItem = static_cast<bool>(page->_tabView.SelectedItem());

            // In the real app, this isn't a problem, but doesn't happen
            // reliably in the unit tests.
            Log::Comment(L"Ensure we set the first tab as the selected one.");
            Log::Comment(NoThrowString().Format(
                L"Selection sync state: tabs=%u tabItems=%u selectedItem=%d",
                tabCount,
                tabItemCount,
                hasSelectedItem ? 1 : 0));
            VERIFY_IS_TRUE(page->_tabs.Size() > 0);
            auto tab = page->_tabs.GetAt(0);
            auto tabImpl = page->_GetTabImpl(tab);
            VERIFY_IS_NOT_NULL(tabImpl);
            VERIFY_IS_TRUE(static_cast<bool>(tabImpl->TabViewItem()));
            Log::Comment(L"About to assign TabView.SelectedItem");
            page->_tabView.SelectedItem(tabImpl->TabViewItem());
            Log::Comment(L"About to call _UpdatedSelectedTab");
            page->_UpdatedSelectedTab(tab);
            Log::Comment(L"Selected first tab successfully");
        };

        result = RunOnUIThread(selectFirstTab);
        VERIFY_SUCCEEDED(result);

        _ensureStableBaselineTab(page);
        _waitForTabCount(page, 1);

        result = RunOnUIThread(selectFirstTab);
        VERIFY_SUCCEEDED(result);

        _yieldToLowPriorityDispatcher(page);
        _ensureStableBaselineTab(page);
        _waitForTabCount(page, 1);

        result = RunOnUIThread(selectFirstTab);
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::TryInitializePage()
    {
        // This is a very simple test to prove we can create settings and a
        // TerminalPage and not only create them successfully, but also create a
        // tab using those settings successfully.

        static constexpr std::wstring_view settingsJson0{ LR"(
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

        CascadiaSettings settings0{ settingsJson0, {} };
        VERIFY_IS_NOT_NULL(settings0);

        // This is super wacky, but we can't just initialize the
        // com_ptr<impl::TerminalPage> in the lambda and assign it back out of
        // the lambda. We'll crash trying to get a weak_ref to the TerminalPage
        // during TerminalPage::Create() below.
        //
        // Instead, create the winrt object, then get a com_ptr to the
        // implementation _from_ the winrt object. This seems to work, even if
        // it's weird.
        winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage> page{ nullptr };
        _initializeTerminalPage(page, settings0);
        _ensureStableBaselineTab(page);
        _waitForTabCount(page, 1);

        auto result = RunOnUIThread([&page]() {
            VERIFY_ARE_EQUAL(1u, page->_tabs.Size());
        });
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::TryDuplicateBadTab()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        // * Create a tab with a profile with GUID 1
        // * Reload the settings so that GUID 1 is no longer in the list of profiles
        // * Try calling _DuplicateFocusedTab on tab 1
        // * No new tab should be created (and more importantly, the app should not crash)
        //
        // Created to test GH#2455

        static constexpr std::wstring_view settingsJson0{ LR"(
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

        static constexpr std::wstring_view settingsJson1{ LR"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };

        CascadiaSettings settings0{ settingsJson0, {} };
        VERIFY_IS_NOT_NULL(settings0);

        CascadiaSettings settings1{ settingsJson1, {} };
        VERIFY_IS_NOT_NULL(settings1);

        const auto guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        // This is super wacky, but we can't just initialize the
        // com_ptr<impl::TerminalPage> in the lambda and assign it back out of
        // the lambda. We'll crash trying to get a weak_ref to the TerminalPage
        // during TerminalPage::Create() below.
        //
        // Instead, create the winrt object, then get a com_ptr to the
        // implementation _from_ the winrt object. This seems to work, even if
        // it's weird.
        winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage> page{ nullptr };
        _initializeTerminalPage(page, settings0);

        auto result = RunOnUIThread([&page]() {
            VERIFY_ARE_EQUAL(1u, page->_tabs.Size());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(L"Duplicate the first tab");
        result = RunOnUIThread([&page]() {
            page->_DuplicateFocusedTab();
            VERIFY_ARE_EQUAL(2u, page->_tabs.Size());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(NoThrowString().Format(
            L"Change the settings of the TerminalPage so the first profile is "
            L"no longer in the list of profiles"));
        result = RunOnUIThread([&page, settings1]() {
            page->_settings = settings1;
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(L"Duplicate the tab, and don't crash");
        result = RunOnUIThread([&page]() {
            page->_DuplicateFocusedTab();
            VERIFY_ARE_EQUAL(3u, page->_tabs.Size(), L"We should successfully duplicate a tab hosting a deleted profile.");
        });
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::TryDuplicateBadPane()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        // * Create a tab with a profile with GUID 1
        // * Reload the settings so that GUID 1 is no longer in the list of profiles
        // * Try calling _SplitPane(Duplicate) on tab 1
        // * No new pane should be created (and more importantly, the app should not crash)
        //
        // Created to test GH#2455

        static constexpr std::wstring_view settingsJson0{ LR"(
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

        static constexpr std::wstring_view settingsJson1{ LR"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };

        CascadiaSettings settings0{ settingsJson0, {} };
        VERIFY_IS_NOT_NULL(settings0);

        CascadiaSettings settings1{ settingsJson1, {} };
        VERIFY_IS_NOT_NULL(settings1);

        const auto guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        // This is super wacky, but we can't just initialize the
        // com_ptr<impl::TerminalPage> in the lambda and assign it back out of
        // the lambda. We'll crash trying to get a weak_ref to the TerminalPage
        // during TerminalPage::Create() below.
        //
        // Instead, create the winrt object, then get a com_ptr to the
        // implementation _from_ the winrt object. This seems to work, even if
        // it's weird.
        winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage> page{ nullptr };
        _initializeTerminalPage(page, settings0);

        auto result = RunOnUIThread([&page]() {
            VERIFY_ARE_EQUAL(1u, page->_tabs.Size());
        });
        VERIFY_SUCCEEDED(result);

        result = RunOnUIThread([&page]() {
            VERIFY_ARE_EQUAL(1u, page->_tabs.Size());
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(1, tab->GetLeafPaneCount());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(NoThrowString().Format(L"Duplicate the first pane"));
        result = RunOnUIThread([&page]() {
            page->_SplitPane(nullptr, SplitDirection::Automatic, 0.5f, page->_MakePane(nullptr, page->_GetFocusedTab(), nullptr));

            VERIFY_ARE_EQUAL(1u, page->_tabs.Size());
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(2, tab->GetLeafPaneCount());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(NoThrowString().Format(
            L"Change the settings of the TerminalPage so the first profile is "
            L"no longer in the list of profiles"));
        result = RunOnUIThread([&page, settings1]() {
            page->_settings = settings1;
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(NoThrowString().Format(L"Duplicate the pane, and don't crash"));
        result = RunOnUIThread([&page]() {
            page->_SplitPane(nullptr, SplitDirection::Automatic, 0.5f, page->_MakePane(nullptr, page->_GetFocusedTab(), nullptr));

            VERIFY_ARE_EQUAL(1u, page->_tabs.Size());
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(3,
                             tab->GetLeafPaneCount(),
                             L"We should successfully duplicate a pane hosting a deleted profile.");
        });
        VERIFY_SUCCEEDED(result);

        auto cleanup = wil::scope_exit([] {
            auto result = RunOnUIThread([]() {
                // There's something causing us to crash north of
                // TSFInputControl::NotifyEnter, or LayoutRequested. It's very
                // unclear what that issue is. Since these tests don't run in
                // CI, simply log a message so that the dev running these tests
                // knows it's expected.
                Log::Comment(L"This test often crashes on cleanup, even when it succeeds. If it succeeded, then crashes, that's okay.");
            });
            VERIFY_SUCCEEDED(result);
        });
    }

    // Method Description:
    // - This is a helper method for setting up a TerminalPage with some common
    //   settings, and creating the first tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - The initialized TerminalPage, ready to use.
    winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage> TabTests::_commonSetup(winrt::TerminalApp::ContentManager contentManager)
    {
        static constexpr std::wstring_view settingsJson0{ LR"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "showTabsInTitlebar": true,
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "tabTitle" : "Profile 0",
                    "historySize": 1
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "tabTitle" : "Profile 1",
                    "historySize": 2
                },
                {
                    "name" : "profile2",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
                    "tabTitle" : "Profile 2",
                    "historySize": 3
                },
                {
                    "name" : "profile3",
                    "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}",
                    "tabTitle" : "Profile 3",
                    "historySize": 4
                }
            ],
            "schemes":
            [
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
                },
                {
                    "name": "Vintage",
                    "foreground": "#C0C0C0",
                    "background": "#000000",
                    "cursorColor": "#FFFFFF",
                    "black": "#000000",
                    "red": "#800000",
                    "green": "#008000",
                    "yellow": "#808000",
                    "blue": "#000080",
                    "purple": "#800080",
                    "cyan": "#008080",
                    "white": "#C0C0C0",
                    "brightBlack": "#808080",
                    "brightRed": "#FF0000",
                    "brightGreen": "#00FF00",
                    "brightYellow": "#FFFF00",
                    "brightBlue": "#0000FF",
                    "brightPurple": "#FF00FF",
                    "brightCyan": "#00FFFF",
                    "brightWhite": "#FFFFFF"
                },
                {
                    "name": "One Half Light",
                    "foreground": "#383A42",
                    "background": "#FAFAFA",
                    "cursorColor": "#4F525D",
                    "black": "#383A42",
                    "red": "#E45649",
                    "green": "#50A14F",
                    "yellow": "#C18301",
                    "blue": "#0184BC",
                    "purple": "#A626A4",
                    "cyan": "#0997B3",
                    "white": "#FAFAFA",
                    "brightBlack": "#4F525D",
                    "brightRed": "#DF6C75",
                    "brightGreen": "#98C379",
                    "brightYellow": "#E4C07A",
                    "brightBlue": "#61AFEF",
                    "brightPurple": "#C577DD",
                    "brightCyan": "#56B5C1",
                    "brightWhite": "#FFFFFF"
                }
            ]
        })" };

        CascadiaSettings settings0{ settingsJson0, {} };
        VERIFY_IS_NOT_NULL(settings0);

        const auto guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");

        // This is super wacky, but we can't just initialize the
        // com_ptr<impl::TerminalPage> in the lambda and assign it back out of
        // the lambda. We'll crash trying to get a weak_ref to the TerminalPage
        // during TerminalPage::Create() below.
        //
        // Instead, create the winrt object, then get a com_ptr to the
        // implementation _from_ the winrt object. This seems to work, even if
        // it's weird.
        winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage> page{ nullptr };
        _initializeTerminalPage(page, settings0, contentManager);
        _ensureStableBaselineTab(page);

        return page;
    }

    void TabTests::TrackSelectedTabsVisualState()
    {
        auto page = _commonSetup();
        VERIFY_IS_NOT_NULL(page);

        _openProfileTab(page, 1, 2);
        _openProfileTab(page, 2, 3);
        _openProfileTab(page, 3, 4);

        TestOnUIThread([&page]() {
            const auto tab0 = page->_tabs.GetAt(0);
            const auto tab2 = page->_tabs.GetAt(2);
            page->_SetSelectedTabs({ tab0, tab2 }, tab0);

            VERIFY_ARE_EQUAL(2u, page->_selectedTabs.size());
            VERIFY_IS_TRUE(page->_selectionAnchor == tab0);
            VERIFY_IS_TRUE(page->_GetTabImpl(tab0)->IsMultiSelected());
            VERIFY_IS_FALSE(page->_GetTabImpl(page->_tabs.GetAt(1))->IsMultiSelected());
            VERIFY_IS_TRUE(page->_GetTabImpl(tab2)->IsMultiSelected());

            page->_RemoveSelectedTab(tab0);
            VERIFY_ARE_EQUAL(1u, page->_selectedTabs.size());
            VERIFY_IS_TRUE(page->_selectionAnchor == tab2);
            VERIFY_IS_FALSE(page->_GetTabImpl(tab0)->IsMultiSelected());
            VERIFY_IS_TRUE(page->_GetTabImpl(tab2)->IsMultiSelected());
        });
    }

    void TabTests::MoveMultipleTabsAsBlock()
    {
        auto page = _commonSetup();
        VERIFY_IS_NOT_NULL(page);

        _openProfileTab(page, 1, 2);
        _openProfileTab(page, 2, 3);
        _openProfileTab(page, 3, 4);

        TestOnUIThread([&page]() {
            const auto tab1 = page->_tabs.GetAt(1);
            const auto tab2 = page->_tabs.GetAt(2);
            page->_MoveTabsToIndex({ tab1, tab2 }, 4);

            VERIFY_ARE_EQUAL(L"Profile 0", page->_tabs.GetAt(0).Title());
            VERIFY_ARE_EQUAL(L"Profile 3", page->_tabs.GetAt(1).Title());
            VERIFY_ARE_EQUAL(L"Profile 1", page->_tabs.GetAt(2).Title());
            VERIFY_ARE_EQUAL(L"Profile 2", page->_tabs.GetAt(3).Title());

            const auto focusedIndex = page->_GetFocusedTabIndex();
            VERIFY_IS_TRUE(focusedIndex.has_value());
            VERIFY_ARE_EQUAL(2u, focusedIndex.value());
        });
    }

    void TabTests::BuildStartupActionsForMultipleTabs()
    {
        auto page = _commonSetup();
        VERIFY_IS_NOT_NULL(page);

        _openProfileTab(page, 1, 2);
        _openProfileTab(page, 2, 3);

        TestOnUIThread([&page]() {
            const auto actions = page->_BuildStartupActionsForTabs({ page->_tabs.GetAt(0), page->_tabs.GetAt(2) });
            const auto newTabCount = std::count_if(actions.begin(), actions.end(), [](const auto& action) {
                return action.Action() == ShortcutAction::NewTab;
            });

            VERIFY_ARE_EQUAL(2u, gsl::narrow_cast<uint32_t>(newTabCount));
            VERIFY_IS_FALSE(actions.empty());
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actions.front().Action());
            VERIFY_IS_TRUE(std::count_if(actions.begin(), actions.end(), [](const auto& action) {
                return action.Action() == ShortcutAction::SwitchToTab;
            }) == 0);
        });
    }

    void TabTests::AttachContentInsertsDraggedTabsAfterTargetIndex()
    {
        const auto sharedContentManager = winrt::make<winrt::TerminalApp::implementation::ContentManager>();
        auto sourcePage = _commonSetup(sharedContentManager);
        auto destinationPage = _commonSetup(sharedContentManager);
        VERIFY_IS_NOT_NULL(sourcePage);
        VERIFY_IS_NOT_NULL(destinationPage);

        _ensureStableBaselineTab(sourcePage);
        _ensureStableBaselineTab(destinationPage);

        _openProfileTab(sourcePage, 1, 2);
        _openProfileTab(sourcePage, 2, 3);
        _waitForTabCount(destinationPage, 1);

        TestOnUIThread([&]() {
            const auto startupActions = sourcePage->_BuildStartupActionsForTabs({ sourcePage->_tabs.GetAt(1), sourcePage->_tabs.GetAt(2) });
            auto serializedActions = winrt::single_threaded_vector<ActionAndArgs>();
            for (const auto& action : startupActions)
            {
                serializedActions.Append(action);
            }

            destinationPage->AttachContent(serializedActions, 1);
        });

        _waitForTabCount(destinationPage, 3);

        TestOnUIThread([&]() {
            VERIFY_ARE_EQUAL(L"Profile 0", destinationPage->_tabs.GetAt(0).Title());
            VERIFY_ARE_EQUAL(L"Profile 1", destinationPage->_tabs.GetAt(1).Title());
            VERIFY_ARE_EQUAL(L"Profile 2", destinationPage->_tabs.GetAt(2).Title());
        });
    }

    void TabTests::TryZoomPane()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        auto page = _commonSetup();

        Log::Comment(L"Create a second pane");
        auto result = RunOnUIThread([&page]() {
            SplitPaneArgs args{ SplitType::Duplicate };
            ActionEventArgs eventArgs{ args };
            page->_HandleSplitPane(nullptr, eventArgs);
            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));

            VERIFY_ARE_EQUAL(2, firstTab->GetLeafPaneCount());
            VERIFY_IS_FALSE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(L"Zoom in on the pane");
        result = RunOnUIThread([&page]() {
            ActionEventArgs eventArgs{};
            page->_HandleTogglePaneZoom(nullptr, eventArgs);
            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(2, firstTab->GetLeafPaneCount());
            VERIFY_IS_TRUE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(L"Zoom out of the pane");
        result = RunOnUIThread([&page]() {
            ActionEventArgs eventArgs{};
            page->_HandleTogglePaneZoom(nullptr, eventArgs);
            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(2, firstTab->GetLeafPaneCount());
            VERIFY_IS_FALSE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::MoveFocusFromZoomedPane()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        auto page = _commonSetup();

        Log::Comment(L"Create a second pane");
        auto result = RunOnUIThread([&page]() {
            // Set up action
            SplitPaneArgs args{ SplitType::Duplicate };
            ActionEventArgs eventArgs{ args };
            page->_HandleSplitPane(nullptr, eventArgs);
            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));

            VERIFY_ARE_EQUAL(2, firstTab->GetLeafPaneCount());
            VERIFY_IS_FALSE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(L"Zoom in on the pane");
        result = RunOnUIThread([&page]() {
            // Set up action
            ActionEventArgs eventArgs{};

            page->_HandleTogglePaneZoom(nullptr, eventArgs);

            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(2, firstTab->GetLeafPaneCount());
            VERIFY_IS_TRUE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(L"Move focus. We should still be zoomed.");
        result = RunOnUIThread([&page]() {
            // Set up action
            MoveFocusArgs args{ FocusDirection::Left };
            ActionEventArgs eventArgs{ args };

            page->_HandleMoveFocus(nullptr, eventArgs);

            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(2, firstTab->GetLeafPaneCount());
            VERIFY_IS_TRUE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::CloseZoomedPane()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        auto page = _commonSetup();

        Log::Comment(L"Create a second pane");
        auto result = RunOnUIThread([&page]() {
            // Set up action
            SplitPaneArgs args{ SplitType::Duplicate };
            ActionEventArgs eventArgs{ args };
            page->_HandleSplitPane(nullptr, eventArgs);
            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));

            VERIFY_ARE_EQUAL(2, firstTab->GetLeafPaneCount());
            VERIFY_IS_FALSE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(L"Zoom in on the pane");
        result = RunOnUIThread([&page]() {
            // Set up action
            ActionEventArgs eventArgs{};

            page->_HandleTogglePaneZoom(nullptr, eventArgs);

            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(2, firstTab->GetLeafPaneCount());
            VERIFY_IS_TRUE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);

        Log::Comment(L"Close Pane. This should cause us to un-zoom, and remove the second pane from the tree");
        result = RunOnUIThread([&page]() {
            // Set up action
            ActionEventArgs eventArgs{};

            page->_HandleClosePane(nullptr, eventArgs);

            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_IS_FALSE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);

        // Introduce a slight delay to let the events finish propagating
        Sleep(250);

        Log::Comment(L"Check to ensure there's only one pane left.");

        result = RunOnUIThread([&page]() {
            auto firstTab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(1, firstTab->GetLeafPaneCount());
            VERIFY_IS_FALSE(firstTab->IsZoomed());
        });
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::SwapPanes()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        auto page = _commonSetup();

        Log::Comment(L"Setup 4 panes.");
        // Create the following layout
        // -------------------
        // |   1    |   2    |
        // |        |        |
        // -------------------
        // |   3    |   4    |
        // |        |        |
        // -------------------
        uint32_t firstId = 0, secondId = 0, thirdId = 0, fourthId = 0;
        TestOnUIThread([&]() {
            VERIFY_ARE_EQUAL(1u, page->_tabs.Size());
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            firstId = tab->_activePane->Id().value();
            // We start with 1 tab, split vertically to get
            // -------------------
            // |   1    |   2    |
            // |        |        |
            // -------------------
            page->_SplitPane(nullptr, SplitDirection::Right, 0.5f, page->_MakePane(nullptr, page->_GetFocusedTab(), nullptr));
            secondId = tab->_activePane->Id().value();
        });
        Sleep(250);
        TestOnUIThread([&]() {
            // After this the `2` pane is focused, go back to `1` being focused
            page->_MoveFocus(FocusDirection::Left);
        });
        Sleep(250);
        TestOnUIThread([&]() {
            // Split again to make the 3rd tab
            // -------------------
            // |   1    |        |
            // |        |        |
            // ---------|   2    |
            // |   3    |        |
            // |        |        |
            // -------------------
            page->_SplitPane(nullptr, SplitDirection::Down, 0.5f, page->_MakePane(nullptr, page->_GetFocusedTab(), nullptr));
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            // Split again to make the 3rd tab
            thirdId = tab->_activePane->Id().value();
        });
        Sleep(250);
        TestOnUIThread([&]() {
            // After this the `3` pane is focused, go back to `2` being focused
            page->_MoveFocus(FocusDirection::Right);
        });
        Sleep(250);
        TestOnUIThread([&]() {
            // Split to create the final pane
            // -------------------
            // |   1    |   2    |
            // |        |        |
            // -------------------
            // |   3    |   4    |
            // |        |        |
            // -------------------
            page->_SplitPane(nullptr, SplitDirection::Down, 0.5f, page->_MakePane(nullptr, page->_GetFocusedTab(), nullptr));
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            fourthId = tab->_activePane->Id().value();
        });

        Sleep(250);
        TestOnUIThread([&]() {
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(4, tab->GetLeafPaneCount());
            // just to be complete, make sure we actually have 4 different ids
            VERIFY_ARE_NOT_EQUAL(firstId, fourthId);
            VERIFY_ARE_NOT_EQUAL(secondId, fourthId);
            VERIFY_ARE_NOT_EQUAL(thirdId, fourthId);
            VERIFY_ARE_NOT_EQUAL(firstId, thirdId);
            VERIFY_ARE_NOT_EQUAL(secondId, thirdId);
            VERIFY_ARE_NOT_EQUAL(firstId, secondId);
        });

        // Gratuitous use of sleep to make sure that the UI has updated properly
        // after each operation.
        Sleep(250);
        // Now try to move the pane through the tree
        Log::Comment(L"Move pane to the left. This should swap panes 3 and 4");
        // -------------------
        // |   1    |   2    |
        // |        |        |
        // -------------------
        // |   4    |   3    |
        // |        |        |
        // -------------------
        TestOnUIThread([&]() {
            // Set up action
            SwapPaneArgs args{ FocusDirection::Left };
            ActionEventArgs eventArgs{ args };

            page->_HandleSwapPane(nullptr, eventArgs);
        });

        Sleep(250);

        TestOnUIThread([&]() {
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(4, tab->GetLeafPaneCount());
            // Our currently focused pane should be `4`
            VERIFY_ARE_EQUAL(fourthId, tab->_activePane->Id().value());

            // Inspect the tree to make sure we swapped
            VERIFY_ARE_EQUAL(fourthId, tab->_rootPane->_firstChild->_secondChild->Id().value());
            VERIFY_ARE_EQUAL(thirdId, tab->_rootPane->_secondChild->_secondChild->Id().value());
        });

        Sleep(250);

        Log::Comment(L"Move pane to up. This should swap panes 1 and 4");
        // -------------------
        // |   4    |   2    |
        // |        |        |
        // -------------------
        // |   1    |   3    |
        // |        |        |
        // -------------------
        TestOnUIThread([&]() {
            // Set up action
            SwapPaneArgs args{ FocusDirection::Up };
            ActionEventArgs eventArgs{ args };

            page->_HandleSwapPane(nullptr, eventArgs);
        });

        Sleep(250);

        TestOnUIThread([&]() {
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(4, tab->GetLeafPaneCount());
            // Our currently focused pane should be `4`
            VERIFY_ARE_EQUAL(fourthId, tab->_activePane->Id().value());

            // Inspect the tree to make sure we swapped
            VERIFY_ARE_EQUAL(fourthId, tab->_rootPane->_firstChild->_firstChild->Id().value());
            VERIFY_ARE_EQUAL(firstId, tab->_rootPane->_firstChild->_secondChild->Id().value());
        });

        Sleep(250);

        Log::Comment(L"Move pane to the right. This should swap panes 2 and 4");
        // -------------------
        // |   2    |   4    |
        // |        |        |
        // -------------------
        // |   1    |   3    |
        // |        |        |
        // -------------------
        TestOnUIThread([&]() {
            // Set up action
            SwapPaneArgs args{ FocusDirection::Right };
            ActionEventArgs eventArgs{ args };

            page->_HandleSwapPane(nullptr, eventArgs);
        });

        Sleep(250);

        TestOnUIThread([&]() {
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(4, tab->GetLeafPaneCount());
            // Our currently focused pane should be `4`
            VERIFY_ARE_EQUAL(fourthId, tab->_activePane->Id().value());

            // Inspect the tree to make sure we swapped
            VERIFY_ARE_EQUAL(fourthId, tab->_rootPane->_secondChild->_firstChild->Id().value());
            VERIFY_ARE_EQUAL(secondId, tab->_rootPane->_firstChild->_firstChild->Id().value());
        });

        Sleep(250);

        Log::Comment(L"Move pane down. This should swap panes 3 and 4");
        // -------------------
        // |   2    |   3    |
        // |        |        |
        // -------------------
        // |   1    |   4    |
        // |        |        |
        // -------------------
        TestOnUIThread([&]() {
            // Set up action
            SwapPaneArgs args{ FocusDirection::Down };
            ActionEventArgs eventArgs{ args };

            page->_HandleSwapPane(nullptr, eventArgs);
        });

        Sleep(250);

        TestOnUIThread([&]() {
            auto tab = page->_GetTabImpl(page->_tabs.GetAt(0));
            VERIFY_ARE_EQUAL(4, tab->GetLeafPaneCount());
            // Our currently focused pane should be `4`
            VERIFY_ARE_EQUAL(fourthId, tab->_activePane->Id().value());

            // Inspect the tree to make sure we swapped
            VERIFY_ARE_EQUAL(fourthId, tab->_rootPane->_secondChild->_secondChild->Id().value());
            VERIFY_ARE_EQUAL(thirdId, tab->_rootPane->_secondChild->_firstChild->Id().value());
        });
    }

    void TabTests::NextMRUTab()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        // This is a test for GH#8025 - we want to make sure that we can do both
        // in-order and MRU tab traversal, using the tab switcher and with the
        // tab switcher disabled.

        auto page = _commonSetup();

        Log::Comment(L"Create a second tab");
        TestOnUIThread([&page]() {
            NewTerminalArgs newTerminalArgs{ 1 };
            page->_OpenNewTab(newTerminalArgs);
        });
        VERIFY_ARE_EQUAL(2u, page->_tabs.Size());

        Log::Comment(L"Create a third tab");
        TestOnUIThread([&page]() {
            NewTerminalArgs newTerminalArgs{ 2 };
            page->_OpenNewTab(newTerminalArgs);
        });
        VERIFY_ARE_EQUAL(3u, page->_tabs.Size());

        Log::Comment(L"Create a fourth tab");
        TestOnUIThread([&page]() {
            NewTerminalArgs newTerminalArgs{ 3 };
            page->_OpenNewTab(newTerminalArgs);
        });
        VERIFY_ARE_EQUAL(4u, page->_tabs.Size());

        TestOnUIThread([&page]() {
            auto focusedIndex = page->_GetFocusedTabIndex().value_or(-1);
            VERIFY_ARE_EQUAL(3u, focusedIndex, L"Verify the fourth tab is the focused one");
        });

        Log::Comment(L"Select the second tab");
        TestOnUIThread([&page]() {
            page->_SelectTab(1);
        });

        TestOnUIThread([&page]() {
            auto focusedIndex = page->_GetFocusedTabIndex().value_or(-1);
            VERIFY_ARE_EQUAL(1u, focusedIndex, L"Verify the second tab is the focused one");
        });

        Log::Comment(L"Change the tab switch order to MRU switching");
        TestOnUIThread([&page]() {
            page->_settings.GlobalSettings().TabSwitcherMode(TabSwitcherMode::MostRecentlyUsed);
        });

        Log::Comment(L"Switch to the next MRU tab, which is the fourth tab");
        TestOnUIThread([&page]() {
            page->_SelectNextTab(true, nullptr);
        });

        Log::Comment(L"Sleep to let events propagate");
        Sleep(250);

        TestOnUIThread([&page]() {
            Log::Comment(L"Hide the command palette, to confirm the selection");
            // If you don't do this, the palette will just stay open, and the
            // next time we call _HandleNextTab, we'll continue traversing the
            // MRU list, instead of just hoping one entry.
            page->LoadCommandPalette().Visibility(Visibility::Collapsed);
        });

        TestOnUIThread([&page]() {
            auto focusedIndex = page->_GetFocusedTabIndex().value_or(-1);
            VERIFY_ARE_EQUAL(3u, focusedIndex, L"Verify the fourth tab is the focused one");
        });

        Log::Comment(L"Switch to the next MRU tab, which is the second tab");
        TestOnUIThread([&page]() {
            page->_SelectNextTab(true, nullptr);
        });

        Log::Comment(L"Sleep to let events propagate");
        Sleep(250);

        TestOnUIThread([&page]() {
            Log::Comment(L"Hide the command palette, to confirm the selection");
            // If you don't do this, the palette will just stay open, and the
            // next time we call _HandleNextTab, we'll continue traversing the
            // MRU list, instead of just hoping one entry.
            page->LoadCommandPalette().Visibility(Visibility::Collapsed);
        });

        TestOnUIThread([&page]() {
            auto focusedIndex = page->_GetFocusedTabIndex().value_or(-1);
            VERIFY_ARE_EQUAL(1u, focusedIndex, L"Verify the second tab is the focused one");
        });

        Log::Comment(L"Change the tab switch order to in-order switching");
        page->_settings.GlobalSettings().TabSwitcherMode(TabSwitcherMode::InOrder);

        Log::Comment(L"Switch to the next in-order tab, which is the third tab");
        TestOnUIThread([&page]() {
            page->_SelectNextTab(true, nullptr);
        });
        TestOnUIThread([&page]() {
            auto focusedIndex = page->_GetFocusedTabIndex().value_or(-1);
            VERIFY_ARE_EQUAL(2u, focusedIndex, L"Verify the third tab is the focused one");
        });

        Log::Comment(L"Change the tab switch order to not use the tab switcher (which is in-order always)");
        page->_settings.GlobalSettings().TabSwitcherMode(TabSwitcherMode::Disabled);

        Log::Comment(L"Switch to the next in-order tab, which is the fourth tab");
        TestOnUIThread([&page]() {
            page->_SelectNextTab(true, nullptr);
        });
        TestOnUIThread([&page]() {
            auto focusedIndex = page->_GetFocusedTabIndex().value_or(-1);
            VERIFY_ARE_EQUAL(3u, focusedIndex, L"Verify the fourth tab is the focused one");
        });
    }

    void TabTests::VerifyCommandPaletteTabSwitcherOrder()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        // This is a test for GH#8188 - we want to make sure that the order of tabs
        // is preserved in the CommandPalette's TabSwitcher

        auto page = _commonSetup();

        Log::Comment(L"Create 3 additional tabs");
        RunOnUIThread([&page]() {
            NewTerminalArgs newTerminalArgs{ 1 };
            page->_OpenNewTab(newTerminalArgs);
            page->_OpenNewTab(newTerminalArgs);
            page->_OpenNewTab(newTerminalArgs);
        });
        VERIFY_ARE_EQUAL(4u, page->_mruTabs.Size());

        Log::Comment(L"give alphabetical names to all switch tab actions");
        TestOnUIThread([&page]() {
            page->_GetTabImpl(page->_tabs.GetAt(0))->Title(L"a");
        });
        TestOnUIThread([&page]() {
            page->_GetTabImpl(page->_tabs.GetAt(1))->Title(L"b");
        });
        TestOnUIThread([&page]() {
            page->_GetTabImpl(page->_tabs.GetAt(2))->Title(L"c");
        });
        TestOnUIThread([&page]() {
            page->_GetTabImpl(page->_tabs.GetAt(3))->Title(L"d");
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Sanity check the titles of our tabs are what we set them to.");

            VERIFY_ARE_EQUAL(L"a", page->_tabs.GetAt(0).Title());
            VERIFY_ARE_EQUAL(L"b", page->_tabs.GetAt(1).Title());
            VERIFY_ARE_EQUAL(L"c", page->_tabs.GetAt(2).Title());
            VERIFY_ARE_EQUAL(L"d", page->_tabs.GetAt(3).Title());

            VERIFY_ARE_EQUAL(L"d", page->_mruTabs.GetAt(0).Title());
            VERIFY_ARE_EQUAL(L"c", page->_mruTabs.GetAt(1).Title());
            VERIFY_ARE_EQUAL(L"b", page->_mruTabs.GetAt(2).Title());
            VERIFY_ARE_EQUAL(L"a", page->_mruTabs.GetAt(3).Title());
        });

        Log::Comment(L"Change the tab switch order to MRU switching");
        TestOnUIThread([&page]() {
            page->_settings.GlobalSettings().TabSwitcherMode(TabSwitcherMode::MostRecentlyUsed);
        });

        Log::Comment(L"Select the tabs from 0 to 3");
        RunOnUIThread([&page]() {
            page->_UpdatedSelectedTab(page->_tabs.GetAt(0));
            page->_UpdatedSelectedTab(page->_tabs.GetAt(1));
            page->_UpdatedSelectedTab(page->_tabs.GetAt(2));
            page->_UpdatedSelectedTab(page->_tabs.GetAt(3));
        });

        VERIFY_ARE_EQUAL(4u, page->_mruTabs.Size());
        VERIFY_ARE_EQUAL(L"d", page->_mruTabs.GetAt(0).Title());
        VERIFY_ARE_EQUAL(L"c", page->_mruTabs.GetAt(1).Title());
        VERIFY_ARE_EQUAL(L"b", page->_mruTabs.GetAt(2).Title());
        VERIFY_ARE_EQUAL(L"a", page->_mruTabs.GetAt(3).Title());

        Log::Comment(L"Switch to the next MRU tab, which is the third tab");
        RunOnUIThread([&page]() {
            page->_SelectNextTab(true, nullptr);
            // In the course of a single tick, the Command Palette will:
            // * open
            // * select the proper tab from the mru's list
            // * raise an event for _filteredActionsView().SelectionChanged to
            //   immediately preview the new tab
            // * raise a _SwitchToTabRequestedHandlers event
            // * then dismiss itself, because we can't fake holing down an
            //   anchor key in the tests
        });

        TestOnUIThread([&page]() {
            VERIFY_ARE_EQUAL(L"c", page->_mruTabs.GetAt(0).Title());
            VERIFY_ARE_EQUAL(L"d", page->_mruTabs.GetAt(1).Title());
            VERIFY_ARE_EQUAL(L"b", page->_mruTabs.GetAt(2).Title());
            VERIFY_ARE_EQUAL(L"a", page->_mruTabs.GetAt(3).Title());
        });

        const auto palette = winrt::get_self<winrt::TerminalApp::implementation::CommandPalette>(page->LoadCommandPalette());

        VERIFY_ARE_EQUAL(winrt::TerminalApp::implementation::CommandPaletteMode::TabSwitchMode, palette->_currentMode, L"Verify we are in the tab switcher mode");
        // At this point, the contents of the command palette's _mruTabs list is
        // still the _old_ ordering (d, c, b, a). The ordering is only updated
        // in TerminalPage::_SelectNextTab, but as we saw before, the palette
        // will also dismiss itself immediately when that's called. So we can't
        // really inspect the contents of the list in this test, unfortunately.
    }

    void TabTests::TestWindowRenameSuccessful()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        auto page = _commonSetup();
        page->RenameWindowRequested([&page, this](auto&&, const winrt::TerminalApp::RenameWindowRequestedArgs args) {
            // In the real terminal, this would bounce up to the monarch and
            // come back down. Instead, immediately call back and set the name.
            //
            // This replicates how TerminalWindow works
            _windowProperties->WindowName(args.ProposedName());
        });

        auto windowNameChanged = false;
        _windowProperties->PropertyChanged([&page, &windowNameChanged](auto&&, const winrt::WUX::Data::PropertyChangedEventArgs& args) mutable {
            if (args.PropertyName() == L"WindowNameForDisplay")
            {
                windowNameChanged = true;
            }
        });

        TestOnUIThread([&page]() {
            page->_RequestWindowRename(winrt::hstring{ L"Foo" });
        });
        TestOnUIThread([&]() {
            VERIFY_ARE_EQUAL(L"Foo", page->WindowProperties().WindowName());
            VERIFY_IS_TRUE(windowNameChanged,
                           L"The window name should have changed, and we should have raised a notification that WindowNameForDisplay changed");
        });
    }
    void TabTests::TestWindowRenameFailure()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        auto page = _commonSetup();
        auto windowNameChanged = false;

        page->PropertyChanged([&page, &windowNameChanged](auto&&, const winrt::WUX::Data::PropertyChangedEventArgs& args) mutable {
            if (args.PropertyName() == L"WindowNameForDisplay")
            {
                windowNameChanged = true;
            }
        });

        TestOnUIThread([&page]() {
            page->_RequestWindowRename(winrt::hstring{ L"Foo" });
        });
        TestOnUIThread([&]() {
            VERIFY_IS_FALSE(windowNameChanged,
                            L"The window name should not have changed, we should have rejected the change.");
        });
    }

    static til::color _getControlBackgroundColor(winrt::TerminalApp::implementation::ContentManager* contentManager,
                                                 const winrt::Microsoft::Terminal::Control::TermControl& c)
    {
        auto interactivity{ contentManager->TryLookupCore(c.ContentId()) };
        VERIFY_IS_NOT_NULL(interactivity);
        const auto core{ interactivity.Core() };
        return til::color{ core.BackgroundColor() };
    }

    void TabTests::TestPreviewCommitScheme()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Preview a color scheme. Make sure it's applied, then committed accordingly");

        auto page = _commonSetup();
        VERIFY_IS_NOT_NULL(page);

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xff0c0c0c }, backgroundColor);
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Emulate previewing the SetColorScheme action");
            SetColorSchemeArgs args{ L"Vintage" };
            ActionAndArgs actionAndArgs{ ShortcutAction::SetColorScheme, args };
            page->_PreviewAction(actionAndArgs);
        });

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            Log::Comment(L"Color should be changed to the preview");
            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xff000000 }, backgroundColor);

            // And we should have stored a function to revert the change.
            VERIFY_ARE_EQUAL(1u, page->_restorePreviewFuncs.size());
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Emulate committing the SetColorScheme action");

            SetColorSchemeArgs args{ L"Vintage" };
            page->_EndPreview();
            page->_HandleSetColorScheme(nullptr, ActionEventArgs{ args });
        });

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            Log::Comment(L"Color should be changed");
            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xff000000 }, backgroundColor);

            // After preview there should be no more restore functions to execute.
            VERIFY_ARE_EQUAL(0u, page->_restorePreviewFuncs.size());
        });

        Log::Comment(L"Sleep to let events propagate");
        // If you don't do this, we will _sometimes_ crash as we're tearing down
        // the control from this test as we start the next one. We crash
        // somewhere in the CursorPositionChanged handler. It's annoying, but
        // this works.
        Sleep(250);
    }

    void TabTests::TestPreviewDismissScheme()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Preview a color scheme. Make sure it's applied, then dismissed accordingly");

        auto page = _commonSetup();
        VERIFY_IS_NOT_NULL(page);

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xff0c0c0c }, backgroundColor);
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Emulate previewing the SetColorScheme action");
            SetColorSchemeArgs args{ L"Vintage" };
            ActionAndArgs actionAndArgs{ ShortcutAction::SetColorScheme, args };
            page->_PreviewAction(actionAndArgs);
        });

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            Log::Comment(L"Color should be changed to the preview");
            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xff000000 }, backgroundColor);
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Emulate dismissing the SetColorScheme action");
            page->_EndPreview();
        });

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            Log::Comment(L"Color should be the same as it originally was");
            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xff0c0c0c }, backgroundColor);
        });
        Log::Comment(L"Sleep to let events propagate");
        Sleep(250);
    }

    void TabTests::TestPreviewSchemeWhilePreviewing()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Preview a color scheme, then preview another scheme. ");

        Log::Comment(L"Preview a color scheme. Make sure it's applied, then committed accordingly");

        auto page = _commonSetup();
        VERIFY_IS_NOT_NULL(page);

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xff0c0c0c }, backgroundColor);
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Emulate previewing the SetColorScheme action");
            SetColorSchemeArgs args{ L"Vintage" };
            page->_PreviewColorScheme(args);
        });

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            Log::Comment(L"Color should be changed to the preview");
            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xff000000 }, backgroundColor);
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Now, preview another scheme");
            SetColorSchemeArgs args{ L"One Half Light" };
            page->_PreviewColorScheme(args);
        });

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            Log::Comment(L"Color should be changed to the preview");
            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xffFAFAFA }, backgroundColor);
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Emulate committing the SetColorScheme action");

            SetColorSchemeArgs args{ L"One Half Light" };
            page->_EndPreview();
            page->_HandleSetColorScheme(nullptr, ActionEventArgs{ args });
        });

        TestOnUIThread([&page, this]() {
            const auto& activeControl{ page->_GetActiveControl() };
            VERIFY_IS_NOT_NULL(activeControl);

            Log::Comment(L"Color should be changed");
            const auto backgroundColor{ _getControlBackgroundColor(_contentManager.get(), activeControl) };
            VERIFY_ARE_EQUAL(til::color{ 0xffFAFAFA }, backgroundColor);
        });
        Log::Comment(L"Sleep to let events propagate");
        Sleep(250);
    }

    void TabTests::TestClampSwitchToTab()
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Ignore", L"True") // GH#19610 tracks re-enabling this test
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Test that switching to a tab index higher than the number of tabs just clamps to the last tab.");

        auto page = _commonSetup();
        VERIFY_IS_NOT_NULL(page);

        Log::Comment(L"Create a second tab");
        TestOnUIThread([&page]() {
            NewTerminalArgs newTerminalArgs{ 1 };
            page->_OpenNewTab(newTerminalArgs);
        });
        VERIFY_ARE_EQUAL(2u, page->_tabs.Size());

        Log::Comment(L"Create a third tab");
        TestOnUIThread([&page]() {
            NewTerminalArgs newTerminalArgs{ 2 };
            page->_OpenNewTab(newTerminalArgs);
        });
        VERIFY_ARE_EQUAL(3u, page->_tabs.Size());

        TestOnUIThread([&page]() {
            auto focusedTabIndexOpt{ page->_GetFocusedTabIndex() };
            VERIFY_IS_TRUE(focusedTabIndexOpt.has_value());
            VERIFY_ARE_EQUAL(2u, focusedTabIndexOpt.value());
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Switch to the first tab");
            page->_SelectTab(0);
        });

        TestOnUIThread([&page]() {
            auto focusedTabIndexOpt{ page->_GetFocusedTabIndex() };

            VERIFY_IS_TRUE(focusedTabIndexOpt.has_value());
            VERIFY_ARE_EQUAL(0u, focusedTabIndexOpt.value());
        });

        TestOnUIThread([&page]() {
            Log::Comment(L"Switch to the tab 6, which is greater than number of tabs. This should switch to the third tab");
            page->_SelectTab(6);
        });

        TestOnUIThread([&page]() {
            auto focusedTabIndexOpt{ page->_GetFocusedTabIndex() };
            VERIFY_IS_TRUE(focusedTabIndexOpt.has_value());
            VERIFY_ARE_EQUAL(2u, focusedTabIndexOpt.value());
        });
    }

}

