// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsEditor/IconPicker.h"
#include "CppWinrtTailored.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace winrt
{
    namespace Editor = Microsoft::Terminal::Settings::Editor;
}

namespace TerminalAppLocalTests
{
    class SettingsEditorTests
    {
        BEGIN_TEST_CLASS(SettingsEditorTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(IconPickerKeepsFileControlsAfterNone);
        TEST_METHOD(IconPickerRestoresLastImagePath);
    };

    void SettingsEditorTests::IconPickerKeepsFileControlsAfterNone()
    {
        auto result = RunOnUIThread([]() {
            const WEX::TestExecution::DisableVerifyExceptions disableExceptionsScope;

            winrt::Editor::IconPicker picker;
            const auto noIconType = picker.IconTypes().GetAt(0);
            const auto fileIconType = picker.IconTypes().GetAt(3);

            picker.CurrentIconPath(L"none");
            VERIFY_IS_TRUE(picker.UsingNoIcon());

            Log::Comment(L"Switching from an explicit no-icon state to File, then back through None and File.");
            picker.CurrentIconType(fileIconType);
            VERIFY_IS_TRUE(picker.UsingImageIcon());

            picker.CurrentIconType(noIconType);
            VERIFY_IS_TRUE(picker.UsingNoIcon());

            picker.CurrentIconType(fileIconType);
            VERIFY_IS_TRUE(picker.UsingImageIcon());
            VERIFY_ARE_EQUAL(winrt::hstring{ L"none" }, picker.CurrentIconPath());
        });

        VERIFY_SUCCEEDED(result);
    }

    void SettingsEditorTests::IconPickerRestoresLastImagePath()
    {
        auto result = RunOnUIThread([]() {
            const WEX::TestExecution::DisableVerifyExceptions disableExceptionsScope;

            winrt::Editor::IconPicker picker;
            const auto noIconType = picker.IconTypes().GetAt(0);
            const auto fileIconType = picker.IconTypes().GetAt(3);
            const winrt::hstring imagePath{ L"C:\\icon.png" };

            picker.CurrentIconPath(imagePath);
            VERIFY_IS_TRUE(picker.UsingImageIcon());

            picker.CurrentIconType(noIconType);
            VERIFY_IS_TRUE(picker.UsingNoIcon());

            picker.CurrentIconType(fileIconType);
            VERIFY_IS_TRUE(picker.UsingImageIcon());
            VERIFY_ARE_EQUAL(imagePath, picker.CurrentIconPath());
        });

        VERIFY_SUCCEEDED(result);
    }
}
