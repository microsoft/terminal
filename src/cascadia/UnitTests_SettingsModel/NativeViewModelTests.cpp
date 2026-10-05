// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalSettingsEditor/ViewModelHelpers.h"

using namespace WEX::TestExecution;
namespace Native = Microsoft::Terminal::Settings::Model::Native;

namespace SettingsModelUnitTests
{
    namespace
    {
        struct SettingsProbe
        {
            winrt::com_ptr<Native::Profile> profile;
            std::vector<winrt::hstring> notifications;

            OBSERVABLE_NATIVE_SETTING(profile, HistorySize);
            OBSERVABLE_NATIVE_SETTING(profile, Name);

            void _NotifyChanges(const std::wstring_view first, const std::wstring_view second)
            {
                notifications.emplace_back(first);
                notifications.emplace_back(second);
            }
        };
    }

    class NativeViewModelTests
    {
        TEST_CLASS(NativeViewModelTests);

        TEST_METHOD(NotificationOrdering)
        {
            SettingsProbe probe{ Native::Profile::Create() };
            const auto original = probe.HistorySize();
            probe.HistorySize(original);
            VERIFY_IS_TRUE(probe.notifications.empty());
            VERIFY_IS_FALSE(probe.HasHistorySize());

            probe.HistorySize(1234);
            VERIFY_ARE_EQUAL(size_t{ 2 }, probe.notifications.size());
            VERIFY_IS_TRUE(probe.notifications[0] == L"HasHistorySize");
            VERIFY_IS_TRUE(probe.notifications[1] == L"HistorySize");
            VERIFY_IS_TRUE(probe.HasHistorySize());

            probe.notifications.clear();
            probe.HistorySize(1234);
            VERIFY_IS_TRUE(probe.notifications.empty());
            probe.ClearHistorySize();
            VERIFY_ARE_EQUAL(size_t{ 2 }, probe.notifications.size());
            VERIFY_IS_TRUE(probe.notifications[0] == L"HasHistorySize");
            VERIFY_IS_TRUE(probe.notifications[1] == L"HistorySize");
            VERIFY_ARE_EQUAL(original, probe.HistorySize());
            probe.notifications.clear();
            probe.ClearHistorySize();
            VERIFY_IS_TRUE(probe.notifications.empty());
        }

        TEST_METHOD(InheritedValueAndStringInput)
        {
            const auto parent = Native::Profile::Create();
            parent->HistorySize(700);
            SettingsProbe probe{ parent->CreateChild() };
            probe.HistorySize(700);
            VERIFY_IS_FALSE(probe.HasHistorySize());
            VERIFY_IS_TRUE(probe.notifications.empty());

            const std::wstring name{ L"Native settings draft" };
            probe.Name(name);
            VERIFY_IS_TRUE(probe.profile->Name() == name);
            VERIFY_ARE_EQUAL(size_t{ 2 }, probe.notifications.size());
            VERIFY_IS_TRUE(probe.notifications[0] == L"HasName");
            VERIFY_IS_TRUE(probe.notifications[1] == L"Name");
        }
    };
}
