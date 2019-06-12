// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.hpp"
#include "IntegrityTest.hpp"

// WinRT namespaces

using namespace Platform;
using namespace Microsoft::OneCoreUap::Test::AppModel;

static PCWSTR c_pwszLowIntegrity = L"Low Integrity";
static PCWSTR c_pwszMedIntegrity = L"Medium Integrity";
static PCWSTR c_pwszHighIntegrity = L"High Integrity";
static PCWSTR c_pwszSysIntegrity = L"System Integrity";
static PCWSTR c_pwszUnkIntegrity = L"UNKNOWN INTEGIRTY";

static void s_ExpandAnyEnvStrings(_Inout_ std::unique_ptr<wchar_t[]>& cmdline)
{
    const DWORD cchNeeded = ExpandEnvironmentStringsW(cmdline.get(), nullptr, 0);
    THROW_LAST_ERROR_IF(0 == cchNeeded);

    std::unique_ptr<wchar_t[]> cmdlineExpanded = std::make_unique<wchar_t[]>(cchNeeded);

    THROW_LAST_ERROR_IF(0 == ExpandEnvironmentStringsW(cmdline.get(), cmdlineExpanded.get(), cchNeeded));

    cmdline.swap(cmdlineExpanded);
}

static void s_RunViaCreateProcess(_In_ PCWSTR pwszExePath)
{
    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);
    si.wShowWindow = SW_SHOWNORMAL;

    wil::unique_process_information pi;

    // We will need a mutable string to give to CreateProcessW.
    const size_t cchNeeded = wcslen(pwszExePath) + 1;
    std::unique_ptr<wchar_t[]> cmdlineMutable = std::make_unique<wchar_t[]>(cchNeeded);
    THROW_IF_FAILED(StringCchCopyW(cmdlineMutable.get(), cchNeeded, pwszExePath));

    // Replace the environment vars with their actual paths
    s_ExpandAnyEnvStrings(cmdlineMutable);

    LOG_OUTPUT(L"Launching '%s'", cmdlineMutable.get());

    THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(nullptr,
                                             cmdlineMutable.get(),
                                             nullptr,
                                             nullptr,
                                             TRUE,
                                             0,
                                             nullptr,
                                             nullptr,
                                             &si,
                                             &pi));

    WaitForSingleObject(pi.hProcess, INFINITE);
}

static void s_SetConIntegrityLow()
{
    // This is absolute paths because OneCoreUAPTest wouldn't accept relative paths here.
    // We're trying to call this:
    // C:\\windows\\system32\\icacls.exe C:\\data\\test\\bin\\conintegrity.exe /setintegritylevel low

    // First assemble with WinRT strings including the Test Deployment Directory C:\data\test\bin which can vary.
    auto commandLine = ref new String(L"%WINDIR%\\system32\\icacls.exe ") +
                       TAEFHelper::GetTestDeploymentDirectory() + ref new String(L"conintegrity.exe /setintegritylevel low");

    // Now call our helper to munge the environment strings then run it and wait for exit.
    s_RunViaCreateProcess(commandLine->Data());
}

bool ModuleSetup()
{
    IntegrityTest::s_LogMyIntegrityLevel(L"ModSetup");

    THROW_IF_FAILED(::WinRTHelper_Register());
    THROW_IF_FAILED(Windows::Foundation::Initialize(WINRT_INIT_MULTITHREADED));

    TestHelper::Initialize();

    // Set ConIntegrity.exe to low integrity with ICACLS.
    // We have to do this from SYSTEM context.
    s_SetConIntegrityLow();

    return true;
}

bool ModuleCleanup()
{
    IntegrityTest::s_LogMyIntegrityLevel(L"ModCleanup");

    TestHelper::Uninitialize();
    Windows::Foundation::Uninitialize();

    return true;
}

bool IntegrityTest::ClassSetup()
{
    IntegrityTest::s_LogMyIntegrityLevel(L"ClassSetup");

    THROW_IF_FAILED(Windows::Foundation::Initialize(WINRT_INIT_MULTITHREADED));

    // Get Appx location
    auto testDeploymentDir = TAEFHelper::GetTestDeploymentDirectory();
    LOG_OUTPUT(L"Test Deployment Dir: \"%s\"", testDeploymentDir->Data());

    // Deploy App

#if defined(_X86_)
    auto vclibPackage = DeploymentHelper::AddPackageIfNotPresent(testDeploymentDir + ref new String(L"Microsoft.VCLibs.x86.14.00.appx"));
#elif defined(_AMD64_)
    auto vclibPackage = DeploymentHelper::AddPackageIfNotPresent(testDeploymentDir + ref new String(L"Microsoft.VCLibs.x64.14.00.appx"));
#elif defined(_ARM_)
    auto vclibPackage = DeploymentHelper::AddPackageIfNotPresent(testDeploymentDir + ref new String(L"Microsoft.VCLibs.arm.14.00.appx"));
#elif defined(_ARM64_)
    auto vclibPackage = DeploymentHelper::AddPackageIfNotPresent(testDeploymentDir + ref new String(L"Microsoft.VCLibs.arm64.14.00.appx"));
#else
#error Unknown architecture for test.
#endif

    auto appPackage = DeploymentHelper::AddPackage(testDeploymentDir + ref new String(L"ConsoleIntegrityUWP.appx"));
    VERIFY_ARE_EQUAL(appPackage->Size, 1u);

    // Get App's AUMID
    auto appAumids = appPackage->GetAt(0)->AUMIDs;
    VERIFY_IS_NOT_NULL(appAumids);
    VERIFY_ARE_EQUAL(appAumids->Size, 1u);

    // save off aumid
    this->_appAumid = appAumids->GetAt(0);

    return true;
}

bool IntegrityTest::ClassCleanup()
{
    s_LogMyIntegrityLevel(L"ClassCleanup");

    Windows::Foundation::Uninitialize();
    return true;
}

void IntegrityTest::_RunWin32ConIntegrityLowHelper()
{
    s_RunViaCreateProcess(L"conintegrity.exe");
}

void IntegrityTest::_RunUWPConIntegrityAppHelper()
{
    // We need to start the execution alias from the current user's location.
    // We can't assume it will find it in the PATH.
    std::wstring cmdline(L"%localappdata%\\microsoft\\windowsapps\\conintegrityuwp.exe");

    s_RunViaCreateProcess(cmdline.c_str());
}

void IntegrityTest::_RunUWPConIntegrityViaTile()
{
    LOG_OUTPUT(L"Launching %s", _appAumid->Data());

    auto viewDescriptor = NavigationHelper::LaunchApplication(_appAumid);

    LOG_OUTPUT(L" AUMID: \"%s\"", viewDescriptor->AUMID->Data());
    LOG_OUTPUT(L" Args: \"%s\"", viewDescriptor->Args->Data());
    LOG_OUTPUT(L" Tile Id: \"%s\"", viewDescriptor->TileId->Data());
    LOG_OUTPUT(L" View Id: %u", viewDescriptor->ViewId);
    LOG_OUTPUT(L" Process Id: %u, 0x%x", viewDescriptor->ProcessId, viewDescriptor->ProcessId);
    LOG_OUTPUT(L" Host Id: 0x%016llx", viewDescriptor->HostId);
    LOG_OUTPUT(L" PSM Key: \"%s\"", viewDescriptor->PSMKey->Data());

    // There's not really a wait for exit here, so just sleep.
    ::Sleep(5000);

    // Terminate
    LOG_OUTPUT(L"Terminating");
    NavigationHelper::CloseView(viewDescriptor->ViewId);
}

// These are shorthands for the function calls, their True/False return code, and then the GetLastError
// They are serialized into an extremely short string to deal with potentially small console buffers
// on OneCore-derived Windows SKUs.
// Example: RCOW;1;0 = ReadConsoleOutputW returning TRUE and a GetLastError() of 0.
// Please see conintegrity.exe and conintegrityuwp.exe for how they are formed.
static PCWSTR _rgpwszExpectedSuccess[] = {
    L"RCOW;1;0",
    L"RCOA;1;0",
    L"RCOCW;1;0",
    L"RCOCA;1;0",
    L"RCOAttr;1;0",
    L"WCIA;1;0",
    L"WCIW;1;0"
};

static PCWSTR _rgpwszExpectedFail[] = {
    L"RCOW;0;5",
    L"RCOA;0;5",
    L"RCOCW;0;5",
    L"RCOCA;0;5",
    L"RCOAttr;0;5",
    L"WCIA;0;5",
    L"WCIW;0;5"
};

void IntegrityTest::_TestValidationHelper(const bool fIsBlockExpected,
                                          _In_ PCWSTR pwszIntegrityExpected)
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(csbiex);
    GetConsoleScreenBufferInfoEx(GetStdHandle(STD_OUTPUT_HANDLE),
                                 &csbiex);

    LOG_OUTPUT(L"Buffer Size X:%d Y:%d", csbiex.dwSize.X, csbiex.dwSize.Y);

    size_t cch = csbiex.dwSize.X;
    wistd::unique_ptr<wchar_t[]> stringData = wil::make_unique_nothrow<wchar_t[]>(cch);
    THROW_IF_NULL_ALLOC(stringData);

    COORD coordRead = { 0 };
    for (coordRead.Y = 0; coordRead.Y < 8; coordRead.Y++)
    {
        ZeroMemory(stringData.get(), sizeof(wchar_t) * cch);

        DWORD dwRead = 0;

        ReadConsoleOutputCharacterW(GetStdHandle(STD_OUTPUT_HANDLE),
                                    stringData.get(),
                                    (DWORD)cch,
                                    coordRead,
                                    &dwRead);

        WEX::Common::String strExpected;
        WEX::Common::String strActual;

        // At position 0, check the integrity.
        if (coordRead.Y == 0)
        {
            strExpected = pwszIntegrityExpected;
        }
        else
        {
            // For the rest, check whether the API call worked.
            if (fIsBlockExpected)
            {
                strExpected = _rgpwszExpectedFail[coordRead.Y - 1];
            }
            else
            {
                strExpected = _rgpwszExpectedSuccess[coordRead.Y - 1];
            }
        }
        stringData[strExpected.GetLength()] = L'\0';
        strActual = stringData.get();
        VERIFY_ARE_EQUAL(strExpected, strActual);

        LOG_OUTPUT(stringData.get());
    }
}

void IntegrityTest::TestLaunchLowILFromHigh()
{
    s_LogMyIntegrityLevel(L"TestBody");
    _RunWin32ConIntegrityLowHelper();

    PCWSTR pwszIntegrityExpected = s_GetMyIntegrityLevel();
    bool fIsBlockExpected = false;

    // If I'm High, expect low.
    // Otherwise if I'm System, expect system.
    if (0 == wcscmp(pwszIntegrityExpected, c_pwszHighIntegrity))
    {
        pwszIntegrityExpected = c_pwszLowIntegrity;
        fIsBlockExpected = true;
    }

    _TestValidationHelper(fIsBlockExpected, pwszIntegrityExpected);
}

void IntegrityTest::TestLaunchLowILFromMedium()
{
    s_LogMyIntegrityLevel(L"TestBody");
    _RunWin32ConIntegrityLowHelper();
    _TestValidationHelper(true, c_pwszLowIntegrity);
}

void IntegrityTest::TestLaunchAppFromHigh()
{
    s_LogMyIntegrityLevel(L"TestBody");
    _RunUWPConIntegrityAppHelper();
    _TestValidationHelper(true, c_pwszLowIntegrity);
}

void IntegrityTest::TestLaunchAppFromMedium()
{
    s_LogMyIntegrityLevel(L"TestBody");
    _RunUWPConIntegrityAppHelper();
    _TestValidationHelper(true, c_pwszLowIntegrity);
}

void IntegrityTest::TestLaunchAppAlone()
{
    s_LogMyIntegrityLevel(L"TestBody");
    _RunUWPConIntegrityViaTile();
}

PCWSTR IntegrityTest::s_GetMyIntegrityLevel()
{
    DWORD dwIntegrityLevel = 0;

    // Get the Integrity level.
    wistd::unique_ptr<TOKEN_MANDATORY_LABEL> tokenLabel;
    THROW_IF_FAILED(wil::GetTokenInformationNoThrow(tokenLabel, GetCurrentProcessToken()));

    dwIntegrityLevel = *GetSidSubAuthority(tokenLabel->Label.Sid,
                                           (DWORD)(UCHAR)(*GetSidSubAuthorityCount(tokenLabel->Label.Sid) - 1));

    switch (dwIntegrityLevel)
    {
    case SECURITY_MANDATORY_LOW_RID:
        return c_pwszLowIntegrity;
    case SECURITY_MANDATORY_MEDIUM_RID:
        return c_pwszMedIntegrity;
    case SECURITY_MANDATORY_HIGH_RID:
        return c_pwszHighIntegrity;
    case SECURITY_MANDATORY_SYSTEM_RID:
        return c_pwszSysIntegrity;
    default:
        return c_pwszUnkIntegrity;
    }
}

void IntegrityTest::s_LogMyIntegrityLevel(PCWSTR WhoAmI)
{
    LOG_OUTPUT(L"%s: %s", WhoAmI, s_GetMyIntegrityLevel());
}
