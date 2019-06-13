// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

BEGIN_MODULE()
    // We need setup fixtures to run as system to ensure we have authority.
    MODULE_PROPERTY(L"RunFixtureAs:Module", L"System")
END_MODULE()

MODULE_SETUP(ModuleSetup);
MODULE_CLEANUP(ModuleCleanup);

class IntegrityTest
{
public:
    BEGIN_TEST_CLASS(IntegrityTest)
        // We need each method to start it in its own console.
        TEST_CLASS_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup)
    TEST_CLASS_CLEANUP(ClassCleanup)

    BEGIN_TEST_METHOD(TestLaunchLowILFromHigh)
        TEST_METHOD_PROPERTY(L"RunAs", L"ElevatedUserOrSystem")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestLaunchLowILFromMedium)
        TEST_METHOD_PROPERTY(L"RunAs", L"RestrictedUser")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestLaunchAppFromHigh)
        TEST_METHOD_PROPERTY(L"RunAs", L"ElevatedUserOrSystem")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestLaunchAppFromMedium)
        TEST_METHOD_PROPERTY(L"RunAs", L"RestrictedUser")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestLaunchAppAlone)
        TEST_METHOD_PROPERTY(L"RunAs", L"RestrictedUser")
    END_TEST_METHOD()

    static PCWSTR s_GetMyIntegrityLevel();
    static void s_LogMyIntegrityLevel(PCWSTR WhoAmI);

private:
    Platform::String ^ _appAumid;

    void _RunWin32ConIntegrityLowHelper();
    void _RunUWPConIntegrityAppHelper();

    void _RunUWPConIntegrityViaTile();

    void _TestValidationHelper(const bool fIsBlockExpected,
                               _In_ PCWSTR pwszIntegrityExpected);
};
