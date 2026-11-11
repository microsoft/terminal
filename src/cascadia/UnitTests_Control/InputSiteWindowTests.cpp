// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../TerminalControl/InputSiteWindow.h"

using namespace WEX::TestExecution;
using winrt::Microsoft::Terminal::Control::implementation::FindInputSiteWindow;

namespace ControlUnitTests
{
    class InputSiteWindowTests
    {
        TEST_CLASS(InputSiteWindowTests);

        static constexpr auto inputSiteClass = L"Windows.UI.Input.InputSite.WindowClass";

        static wil::unique_hwnd MakeWindow(const wchar_t* className, HWND parent = nullptr)
        {
            wil::unique_hwnd window{ CreateWindowExW(0, className, L"", parent ? WS_CHILD : WS_POPUP, 0, 0, 1, 1, parent, nullptr, GetModuleHandleW(nullptr), nullptr) };
            VERIFY_IS_NOT_NULL(window.get());
            return window;
        }

        TEST_CLASS_SETUP(ClassSetup)
        {
            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc = DefWindowProcW;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.lpszClassName = inputSiteClass;
            VERIFY_IS_TRUE(RegisterClassW(&windowClass) != 0);
            return true;
        }

        TEST_CLASS_CLEANUP(ClassCleanup)
        {
            VERIFY_IS_TRUE(UnregisterClassW(inputSiteClass, GetModuleHandleW(nullptr)));
            return true;
        }

        TEST_METHOD(DoesNotUseAnotherOwnersInputSite)
        {
            const auto firstOwner = MakeWindow(L"STATIC");
            const auto firstSite = MakeWindow(inputSiteClass, firstOwner.get());
            const auto secondOwner = MakeWindow(L"STATIC");
            const auto secondSite = MakeWindow(inputSiteClass, secondOwner.get());

            VERIFY_IS_TRUE(FindInputSiteWindow(firstOwner.get()) == firstSite.get());
            VERIFY_IS_TRUE(FindInputSiteWindow(secondOwner.get()) == secondSite.get());
        }

        TEST_METHOD(NullOwnerDoesNotEnumerateOtherWindows)
        {
            const auto unrelated = MakeWindow(inputSiteClass);
            VERIFY_IS_NULL(FindInputSiteWindow(nullptr));
        }

        TEST_METHOD(RetriesAfterInputSiteCreation)
        {
            const auto otherOwner = MakeWindow(L"STATIC");
            const auto otherSite = MakeWindow(inputSiteClass, otherOwner.get());
            const auto owner = MakeWindow(L"STATIC");
            VERIFY_IS_TRUE(FindInputSiteWindow(owner.get()) == owner.get());

            const auto site = MakeWindow(inputSiteClass, owner.get());
            VERIFY_IS_TRUE(FindInputSiteWindow(owner.get()) == site.get());
        }

        TEST_METHOD(FindsNestedInputSiteAndFollowsOwnerChanges)
        {
            const auto firstOwner = MakeWindow(L"STATIC");
            const auto secondOwner = MakeWindow(L"STATIC");
            const auto container = MakeWindow(L"STATIC", firstOwner.get());
            const auto site = MakeWindow(inputSiteClass, container.get());
            VERIFY_IS_TRUE(FindInputSiteWindow(firstOwner.get()) == site.get());

            VERIFY_IS_TRUE(SetParent(container.get(), secondOwner.get()) == firstOwner.get());
            VERIFY_IS_TRUE(FindInputSiteWindow(firstOwner.get()) == firstOwner.get());
            VERIFY_IS_TRUE(FindInputSiteWindow(secondOwner.get()) == site.get());
        }

        TEST_METHOD(DoesNotRetainDestroyedInputSite)
        {
            const auto owner = MakeWindow(L"STATIC");
            auto site = MakeWindow(inputSiteClass, owner.get());
            VERIFY_IS_TRUE(FindInputSiteWindow(owner.get()) == site.get());
            site.reset();
            VERIFY_IS_TRUE(FindInputSiteWindow(owner.get()) == owner.get());

            site = MakeWindow(inputSiteClass, owner.get());
            VERIFY_IS_TRUE(FindInputSiteWindow(owner.get()) == site.get());
        }
    };
}
