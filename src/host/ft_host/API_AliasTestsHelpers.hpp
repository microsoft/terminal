#include "Common.hpp"
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifdef TestGetConsoleAliasHelper
#undef TestGetConsoleAliasHelper
#endif

#ifdef TCH
#undef TCH
#endif

#ifdef TLEN
#undef TLEN
#endif

#ifdef AddConsoleAliasT
#undef AddConsoleAliasT
#endif

#ifdef StringCbCopyT
#undef StringCbCopyT
#endif

#ifdef GetConsoleAliasT
#undef GetConsoleAliasT
#endif

#ifdef TSTRFORMAT
#undef TSTRFORMAT
#endif

#ifdef TCHFORMAT
#undef TCHFORMAT
#endif

#if defined(UNICODE) || defined(_UNICODE)
#define TestGetConsoleAliasHelper TestGetConsoleAliasHelperW
#define TCH wchar_t
#define TLEN wcslen
#define AddConsoleAliasT OneCoreDelay::AddConsoleAliasW
#define StringCbCopyT StringCbCopyW
#define GetConsoleAliasT OneCoreDelay::GetConsoleAliasW
#define TSTRFORMAT L"%s"
#define TCHFORMAT L"%c"
#else
#define TestGetConsoleAliasHelper TestGetConsoleAliasHelperA
#define TCH char
#define TLEN strlen
#define AddConsoleAliasT OneCoreDelay::AddConsoleAliasA
#define StringCbCopyT StringCbCopyA
#define GetConsoleAliasT OneCoreDelay::GetConsoleAliasA
#define TSTRFORMAT L"%S"
#define TCHFORMAT L"%C"
#endif

void TestGetConsoleAliasHelper(TCH* ptszSourceGiven,
                               TCH* ptszExpectedTargetGiven,
                               TCH* ptszExeNameGiven,
                               DWORD& dwSource,
                               DWORD& dwTarget,
                               DWORD& dwExeName,
                               bool& /*bUnicode*/,
                               bool& bSetFirst)
{
    TCH* ptszSource = nullptr;
    TCH* ptszExeName = nullptr;
    TCH* ptszExpectedTarget = ptszExpectedTargetGiven;
    TCH* ptchTargetBuffer = nullptr;
    DWORD cbTargetBuffer = 0;

    switch (dwSource)
    {
    case 0:
        ptszSource = nullptr;
        WEX::Logging::Log::Comment(L"Using null source arg.");
        break;
    case 1:
        ptszSource = ptszSourceGiven;
        WEX::Logging::Log::Comment(WEX::Common::String().Format(L"Using source arg: '" TSTRFORMAT "'", ptszSource));
        break;
    default:
        VERIFY_FAIL(L"Unknown type.");
    }

    switch (dwExeName)
    {
    case 0:
        ptszExeName = nullptr;
        WEX::Logging::Log::Comment(L"Using null exe name.");
        break;
    case 1:
        ptszExeName = ptszExeNameGiven;
        WEX::Logging::Log::Comment(WEX::Common::String().Format(L"Using exe name arg: '" TSTRFORMAT "'", ptszExeName));
        break;
    default:
        VERIFY_FAIL(L"Unknown type.");
    }

    DWORD const cbExpectedTargetString = (DWORD)TLEN(ptszExpectedTargetGiven) * sizeof(TCH);

    switch (dwTarget)
    {
    case 0:
        cbTargetBuffer = 0;
        break;
    case 1:
        cbTargetBuffer = sizeof(TCH);
        break;
    case 2:
        cbTargetBuffer = cbExpectedTargetString - sizeof(TCH);
        break;
    case 3:
        cbTargetBuffer = cbExpectedTargetString;
        break;
    case 4:
        cbTargetBuffer = cbExpectedTargetString + sizeof(TCH);
        break;
    case 5:
        cbTargetBuffer = cbExpectedTargetString + sizeof(TCH) + sizeof(TCH);
        break;
    case 6:
        cbTargetBuffer = MAX_PATH * sizeof(TCH);
        break;
    default:
        VERIFY_FAIL(L"Unknown type.");
    }

    if (cbTargetBuffer == 0)
    {
        ptchTargetBuffer = nullptr;
    }
    else
    {
        ptchTargetBuffer = new TCH[cbTargetBuffer / sizeof(TCH)];
        ZeroMemory(ptchTargetBuffer, cbTargetBuffer);
    }

    auto freeTargetBuffer = wil::scope_exit([&]() {
        if (ptchTargetBuffer != nullptr)
        {
            delete[] ptchTargetBuffer;
        }
    });

    WEX::Logging::Log::Comment(WEX::Common::String().Format(L"Using target buffer size: '%d'", cbTargetBuffer));

    // Set the alias if we're supposed to and prepare for cleanup later.
    if (bSetFirst)
    {
        AddConsoleAliasT(ptszSource, ptszExpectedTarget, ptszExeName);
    }
    // This is strange because it's a scope exit so we need to declare in the parent scope, then let it go if we didn't actually need it.
    // I just prefer keeping the exit next to the allocation so it doesn't get lost.
    auto removeAliasOnExit = wil::scope_exit([&] {
        AddConsoleAliasT(ptszSource, NULL, ptszExeName);
    });
    if (!bSetFirst)
    {
        removeAliasOnExit.release();
    }

    // Determine what the result codes should be
    // See console client side in conlibk...
    // a->TargetLength on the server side will become the return value
    // The returned status will be put into SetLastError
    // If there is an error and it's not STATUS_BUFFER_TOO_SMALL, then a->TargetLength (and the return) will be zeroed.
    // Some sample errors:
    // - 87 = 0x57 = ERROR_INVALID_PARAMETER
    // - 122 = 0x7a = ERROR_INSUFFICIENT_BUFFER

    DWORD dwExpectedResult;
    DWORD dwExpectedLastError;

    // NOTE: This order is important. Don't rearrange IF statements.
    if (nullptr == ptszSource ||
        nullptr == ptszExeName)
    {
        // If the source or exe name aren't valid, invalid parameter.
        dwExpectedResult = 0;
        dwExpectedLastError = ERROR_INVALID_PARAMETER;
    }
    else if (!bSetFirst)
    {
        // If we didn't set an alias, generic failure.
        dwExpectedResult = 0;
        dwExpectedLastError = ERROR_GEN_FAILURE;
    }
    else if (ptchTargetBuffer == nullptr ||
             cbTargetBuffer < (cbExpectedTargetString + sizeof(TCH))) // expected target plus a null terminator.
    {
        // If the target isn't enough space, insufficient buffer.
        dwExpectedResult = cbTargetBuffer;

// For some reason, the console API *ALWAYS* says it needs enough space as if we were copying Unicode,
// even if the final result will be ANSI.
// Therefore, if we're mathing based on a char size buffer, multiple the expected result by 2.
#pragma warning(suppress : 4127) // This is a constant, but conditionally compiled twice so we need the check.
        if (1 == sizeof(TCH))
        {
            dwExpectedResult *= sizeof(wchar_t);
        }

        dwExpectedLastError = ERROR_INSUFFICIENT_BUFFER;
    }
    else
    {
        // Otherwise, success. API should always null terminate string.
        dwExpectedResult = cbExpectedTargetString + sizeof(TCH); // expected target plus a null terminator.
        dwExpectedLastError = 0;
    }

    TCH* ptchExpectedTarget;
    auto freeExpectedTarget = wil::scope_exit([&] {
        if (ptchExpectedTarget != nullptr)
        {
            delete[] ptchExpectedTarget;
            ptchExpectedTarget = nullptr;
        }
    });

    if (0 == cbTargetBuffer)
    {
        // If no buffer, we should expect null back out.
        ptchExpectedTarget = nullptr;
    }
    else
    {
        // If there is buffer space, allocate it.
        ptchExpectedTarget = new TCH[cbTargetBuffer / sizeof(TCH)];
        ZeroMemory(ptchExpectedTarget, cbTargetBuffer);
    }

    if (0 == dwExpectedLastError)
    {
        // If it was successful, it should have been filled. Otherwise it will be zeroed as when it started.
        StringCbCopyT(ptchExpectedTarget, cbTargetBuffer, ptszExpectedTargetGiven);
    }

    // Perform the test
    SetLastError(S_OK);
    DWORD const dwActualResult = GetConsoleAliasT(ptszSource, ptchTargetBuffer, cbTargetBuffer, ptszExeName);
    DWORD const dwActualLastError = GetLastError();

    VERIFY_ARE_EQUAL(dwExpectedResult, dwActualResult, L"Ensure result code/return value matches expected.");
    VERIFY_ARE_EQUAL(dwExpectedLastError, dwActualLastError, L"Ensure last error code matches expected.");

    WEX::Logging::Log::Comment(L"Compare target buffer character by character...");
    for (size_t i = 0; i < (cbTargetBuffer / sizeof(TCH)); i++)
    {
        if (ptchExpectedTarget[i] != ptchTargetBuffer[i])
        {
            VERIFY_FAIL(WEX::Common::String().Format(L"Target mismatch at %d. Expected: '" TCHFORMAT "'  Actual: '" TCHFORMAT "'", i, ptchExpectedTarget[i], ptchTargetBuffer[i]));
        }
    }
}
