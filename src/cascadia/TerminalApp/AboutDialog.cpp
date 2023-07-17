
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AboutDialog.h"
#include "AboutDialog.g.cpp"

#include <LibraryResources.h>
#include <WtExeUtils.h>

#include "../../types/inc/utils.hpp"
#include "Utils.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal;
using namespace ::TerminalApp;
using namespace std::chrono_literals;

namespace winrt
{
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    AboutDialog::AboutDialog()
    {
        InitializeComponent();
        _queueUpdateCheck();
    }

    winrt::hstring AboutDialog::ApplicationDisplayName()
    {
        return CascadiaSettings::ApplicationDisplayName();
    }

    winrt::hstring AboutDialog::ApplicationVersion()
    {
        return CascadiaSettings::ApplicationVersion();
    }

    void AboutDialog::_SendFeedbackOnClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs& /*eventArgs*/)
    {
#if defined(WT_BRANDING_RELEASE)
        ShellExecute(nullptr, nullptr, L"https://go.microsoft.com/fwlink/?linkid=2125419", nullptr, nullptr, SW_SHOW);
#else
        ShellExecute(nullptr, nullptr, L"https://go.microsoft.com/fwlink/?linkid=2204904", nullptr, nullptr, SW_SHOW);
#endif
    }

    void AboutDialog::_ThirdPartyNoticesOnClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        std::filesystem::path currentPath{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
        currentPath.replace_filename(L"NOTICE.html");
        ShellExecute(nullptr, nullptr, currentPath.c_str(), nullptr, nullptr, SW_SHOW);
    }

    winrt::fire_and_forget AboutDialog::_queueUpdateCheck()
    {
        auto strongThis = get_strong();
        auto now{ std::chrono::system_clock::now() };
        if (now - _lastUpdateCheck < std::chrono::days{ 1 })
        {
            co_return;
        }
        _lastUpdateCheck = now;

        if (!IsPackaged())
        {
            co_return;
        }

        co_await wil::resume_foreground(strongThis->Dispatcher());
        UpdatesAvailable(false);
        CheckingForUpdates(true);

        try
        {
#ifdef WT_BRANDING_DEV
            // **DEV BRANDING**: Always sleep for three seconds and then report that
            // there is an update available. This lets us test the system.
            co_await winrt::resume_after(std::chrono::seconds{ 3 });
            co_await wil::resume_foreground(strongThis->Dispatcher());
            UpdatesAvailable(true);
#else // release build, likely has a store context
            if (auto storeContext{ winrt::Windows::Services::Store::StoreContext::GetDefault() })
            {
                const auto updates = co_await storeContext.GetAppAndOptionalStorePackageUpdatesAsync();
                co_await wil::resume_foreground(strongThis->Dispatcher());
                const auto numUpdates = updates.Size();
                if (numUpdates > 0)
                {
                    UpdatesAvailable(true);
                }
            }
#endif
        }
        catch (...)
        {
            // do nothing on failure
        }

        co_await wil::resume_foreground(strongThis->Dispatcher());
        CheckingForUpdates(false);
    }
}
