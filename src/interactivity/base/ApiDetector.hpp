// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#pragma hdrstop

namespace Microsoft::Console::Interactivity
{
    enum class ApiLevel
    {
        Win32,
        OneCore
    };

    class ApiDetector final
    {
    public:
        [[nodiscard]] static NTSTATUS DetectNtUserWindow(_Out_ ApiLevel* level);

    private:
        [[nodiscard]] static NTSTATUS DetectApiSupport(_In_ LPCWSTR lpLibrary, _In_ LPCSTR lpApi, _Out_ ApiLevel* level);
        [[nodiscard]] static NTSTATUS TryLoadWellKnownLibrary(_In_ LPCWSTR library, _Outptr_result_nullonfailure_ HMODULE* module);
        [[nodiscard]] static NTSTATUS TryLocateProcedure(_In_ HMODULE hModule, _In_ LPCSTR lpProcedure);
        static void SetLevelAndFreeIfNecessary(_In_ NTSTATUS status, _In_ HMODULE hModule, _Out_ ApiLevel* level);
        [[nodiscard]] static NTSTATUS TryLoadWellKnownLibrary(_In_ LPCWSTR lpLibrary,
                                                              _In_ DWORD dwLoaderFlags,
                                                              _Outptr_result_nullonfailure_ HMODULE* phModule);
    };
}
