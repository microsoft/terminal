//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// WARNING!!!
// This is a fork of conpty.h
// It has some small modifications to help debug conhost-backed pseudoconsoles
//      within the context of Universal Applications. Notably:
// * SetHandleInformation and HANDLE_FLAG_INHERIT are not present in
//      WINAPI_PARTITION_APP, so we're just leaving the handles inheritable for
//      now. This is definitely a bug, but the ConhostConnection isn't meant to
//      be shipping code. Conhosts created by this version of CreateConPty will
//      only go away when the app is closed, not when the pipes are broken.
//      Fortunately, because the universal app is containered, they'll be
//      cleaned up when the app is terminated. IF YOU USE THIS HEADER OUTSIDE OF
//      A UNIVERSAL APP, THE CHILD CONHOST.EXE PROCESSES WILL NOT BE TERMINATED.
// * Whoever includes this will also need to define STARTF_USESTDHANDLES:
//   ```
//   #ifndef STARTF_USESTDHANDLES
//   #define STARTF_USESTDHANDLES       0x00000100
//   #endif
//   ```

#include <windows.h>
#include <string>
#include <sstream>
#include <strsafe.h>
#include <memory>
#pragma once

const unsigned int PTY_SIGNAL_RESIZE_WINDOW = 8u;

// A case-insensitive wide-character map is used to store environment variables
// due to documented requirements:
//
//      "All strings in the environment block must be sorted alphabetically by name.
//      The sort is case-insensitive, Unicode order, without regard to locale.
//      Because the equal sign is a separator, it must not be used in the name of
//      an environment variable."
//      https://docs.microsoft.com/en-us/windows/desktop/ProcThread/changing-environment-variables
//
struct WStringCaseInsensitiveCompare
{
    [[nodiscard]] bool operator()(const std::wstring& lhs, const std::wstring& rhs) const noexcept
    {
        return (::_wcsicmp(lhs.c_str(), rhs.c_str()) < 0);
    }
};

using EnvironmentVariableMapW = std::map<std::wstring, std::wstring, WStringCaseInsensitiveCompare>;

[[nodiscard]] HRESULT CreateConPty(const std::wstring& cmdline,
                                   const unsigned short w,
                                   const unsigned short h,
                                   HANDLE* const hInput,
                                   HANDLE* const hOutput,
                                   HANDLE* const hSignal,
                                   PROCESS_INFORMATION* const piPty,
                                   DWORD dwCreationFlags = 0,
                                   const EnvironmentVariableMapW& extraEnvVars = {}) noexcept;

bool SignalResizeWindow(const HANDLE hSignal,
                        const unsigned short w,
                        const unsigned short h);

[[nodiscard]] HRESULT UpdateEnvironmentMapW(EnvironmentVariableMapW& map) noexcept;

[[nodiscard]] HRESULT EnvironmentMapToEnvironmentStringsW(EnvironmentVariableMapW& map,
                                                          std::vector<wchar_t>& newEnvVars) noexcept;

// Function Description:
// - Updates an EnvironmentVariableMapW with the current process's unicode
//   environment variables ignoring ones already set in the provided map.
// Arguments:
// - map: The map to populate with the current processes's environment variables.
// Return Value:
// - S_OK if we succeeded, or an appropriate HRESULT for failing
[[nodiscard]] __declspec(noinline) inline HRESULT UpdateEnvironmentMapW(EnvironmentVariableMapW& map) noexcept
{
    LPWCH currentEnvVars{};
    auto freeCurrentEnv = wil::scope_exit([&] {
        if (currentEnvVars)
        {
            (void)FreeEnvironmentStringsW(currentEnvVars);
            currentEnvVars = nullptr;
        }
    });

    RETURN_IF_NULL_ALLOC(currentEnvVars = ::GetEnvironmentStringsW());

    // Each entry is NULL-terminated; block is guaranteed to be double-NULL terminated at a minimum.
    for (wchar_t const* lastCh{ currentEnvVars }; *lastCh != '\0'; ++lastCh)
    {
        // Copy current entry into temporary map.
        const size_t cchEntry{ ::wcslen(lastCh) };
        const std::wstring_view entry{ lastCh, cchEntry };

        // Every entry is of the form "name=value\0".
        auto pos = entry.find_first_of(L"=", 0, 1);
        RETURN_HR_IF(E_UNEXPECTED, pos == std::wstring::npos);

        std::wstring name{ entry.substr(0, pos) }; // portion before '='
        std::wstring value{ entry.substr(pos + 1) }; // portion after '='

        // Don't replace entries that already exist.
        map.try_emplace(std::move(name), std::move(value));
        lastCh += cchEntry;
    }

    return S_OK;
}

// Function Description:
// - Creates a new environment block using the provided vector as appropriate
//   (resizing if needed) based on the provided environment variable map
//   matching the format of GetEnvironmentStringsW.
// Arguments:
// - map: The map to populate the new environment block vector with.
// - newEnvVars: The vector that will be used to create the new environment block.
// Return Value:
// - S_OK if we succeeded, or an appropriate HRESULT for failing
[[nodiscard]] __declspec(noinline) inline HRESULT EnvironmentMapToEnvironmentStringsW(EnvironmentVariableMapW& map, std::vector<wchar_t>& newEnvVars) noexcept
{
    // Clear environment block before use.
    constexpr size_t cbChar{ sizeof(decltype(newEnvVars.begin())::value_type) };

    if (!newEnvVars.empty())
    {
        ::SecureZeroMemory(newEnvVars.data(), newEnvVars.size() * cbChar);
    }

    // Resize environment block to fit map.
    size_t cchEnv{ 2 }; // For the block's double NULL-terminator.
    for (const auto& [name, value] : map)
    {
        // Final form of "name=value\0".
        cchEnv += name.size() + 1 + value.size() + 1;
    }
    newEnvVars.resize(cchEnv);

    // Ensure new block is wiped if we exit due to failure.
    auto zeroNewEnv = wil::scope_exit([&] {
        ::SecureZeroMemory(newEnvVars.data(), newEnvVars.size() * cbChar);
    });

    // Transform each map entry and copy it into the new environment block.
    LPWCH pEnvVars{ newEnvVars.data() };
    size_t cbRemaining{ cchEnv * cbChar };
    for (const auto& [name, value] : map)
    {
        // Final form of "name=value\0" for every entry.
        {
            const size_t cchSrc{ name.size() };
            const size_t cbSrc{ cchSrc * cbChar };
            RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, name.c_str(), cbSrc) != 0);
            pEnvVars += cchSrc;
            cbRemaining -= cbSrc;
        }

        RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, L"=", cbChar) != 0);
        ++pEnvVars;
        cbRemaining -= cbChar;

        {
            const size_t cchSrc{ value.size() };
            const size_t cbSrc{ cchSrc * cbChar };
            RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, value.c_str(), cbSrc) != 0);
            pEnvVars += cchSrc;
            cbRemaining -= cbSrc;
        }

        RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, L"\0", cbChar) != 0);
        ++pEnvVars;
        cbRemaining -= cbChar;
    }

    // Environment block only has to be NULL-terminated, but double NULL-terminate anyway.
    RETURN_HR_IF(E_OUTOFMEMORY, memcpy_s(pEnvVars, cbRemaining, L"\0\0", cbChar * 2) != 0);
    cbRemaining -= cbChar * 2;

    RETURN_HR_IF(E_UNEXPECTED, cbRemaining != 0);

    zeroNewEnv.release(); // success; don't wipe new environment block on exit

    return S_OK;
}

// Function Description:
// - Creates a headless conhost in "pty mode" and launches the given commandline
//      attached to the conhost. Gives back handles to three different pipes:
//   * hInput: The caller can write input to the conhost, encoded in utf-8, on
//      this pipe. For keys that don't have character representations, the
//      caller should use the `TERM=xterm` VT sequences for encoding the input.
//   * hOutput: The caller should read from this pipe. The headless conhost will
//      "render" it's state to a stream of utf-8 encoded text with VT sequences.
//   * hSignal: The caller can use this to resize the size of the underlying PTY
//      using the SignalResizeWindow function.
// Arguments:
// - cmdline: The commandline to launch as a console process attached to the pty
//      that's created.
// - startingDirectory: The directory to start the process in
// - w: The initial width of the pty, in characters
// - h: The initial height of the pty, in characters
// - hInput: A handle to the pipe for writing input to the pty.
// - hOutput: A handle to the pipe for reading the output of the pty.
// - hSignal: A handle to the pipe for writing signal messages to the pty.
// - piPty: The PROCESS_INFORMATION of the pty process. NOTE: This is *not* the
//      PROCESS_INFORMATION of the process that's created as a result the cmdline.
// - extraEnvVars : A map of pairs of (Name, Value) representing additional
//      environment variable strings and values to be set in the client process
//      environment.  May override any already present in parent process.
// Return Value:
// - S_OK if we succeeded, or an appropriate HRESULT for failing format the
//      commandline or failing to launch the conhost
[[nodiscard]] __declspec(noinline) inline HRESULT CreateConPty(const std::wstring& cmdline,
                                                               std::optional<std::wstring> startingDirectory,
                                                               const unsigned short w,
                                                               const unsigned short h,
                                                               HANDLE* const hInput,
                                                               HANDLE* const hOutput,
                                                               HANDLE* const hSignal,
                                                               PROCESS_INFORMATION* const piPty,
                                                               DWORD dwCreationFlags = 0,
                                                               const EnvironmentVariableMapW& extraEnvVars = {}) noexcept
{
    // Create some anon pipes so we can pass handles down and into the console.
    // IMPORTANT NOTE:
    // We're creating the pipe here with un-inheritable handles, then marking
    //      the conhost sides of the pipes as inheritable. We do this because if
    //      the entire pipe is marked as inheritable, when we pass the handles
    //      to CreateProcess, at some point the entire pipe object is copied to
    //      the conhost process, which includes the terminal side of the pipes
    //      (_inPipe and _outPipe). This means that if we die, there's still
    //      outstanding handles to our side of the pipes, and those handles are
    //      in conhost, despite conhost being unable to reference those handles
    //      and close them.
    // CRITICAL: Close our side of the handles. Otherwise you'll get the same
    //      problem if you close conhost, but not us (the terminal).
    HANDLE outPipeConhostSide;
    HANDLE inPipeConhostSide;
    HANDLE signalPipeConhostSide;

    SECURITY_ATTRIBUTES sa;
    sa = { 0 };
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = nullptr;

    CreatePipe(&inPipeConhostSide, hInput, &sa, 0);
    CreatePipe(hOutput, &outPipeConhostSide, &sa, 0);
    CreatePipe(&signalPipeConhostSide, hSignal, &sa, 0);

    SetHandleInformation(inPipeConhostSide, HANDLE_FLAG_INHERIT, 1);
    SetHandleInformation(outPipeConhostSide, HANDLE_FLAG_INHERIT, 1);
    SetHandleInformation(signalPipeConhostSide, HANDLE_FLAG_INHERIT, 1);

    std::wstring conhostCmdline = L"conhost.exe";
    conhostCmdline += L" --headless";
    std::wstringstream ss;
    if (w != 0 && h != 0)
    {
        ss << L" --width " << (unsigned long)w;
        ss << L" --height " << (unsigned long)h;
    }

    ss << L" --signal 0x" << std::hex << HandleToUlong(signalPipeConhostSide);
    conhostCmdline += ss.str();
    conhostCmdline += L" -- ";
    conhostCmdline += cmdline;

    STARTUPINFO si = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.hStdInput = inPipeConhostSide;
    si.hStdOutput = outPipeConhostSide;
    si.hStdError = outPipeConhostSide;
    si.dwFlags |= STARTF_USESTDHANDLES;

    std::unique_ptr<wchar_t[]> mutableCommandline = std::make_unique<wchar_t[]>(conhostCmdline.length() + 1);
    if (mutableCommandline == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    HRESULT hr = StringCchCopy(mutableCommandline.get(), conhostCmdline.length() + 1, conhostCmdline.c_str());
    if (!SUCCEEDED(hr))
    {
        return hr;
    }

    LPCWSTR lpCurrentDirectory = startingDirectory.has_value() ? startingDirectory.value().c_str() : nullptr;

    std::vector<wchar_t> newEnvVars;
    auto zeroNewEnv = wil::scope_exit([&] {
        ::SecureZeroMemory(newEnvVars.data(),
                           newEnvVars.size() * sizeof(decltype(newEnvVars.begin())::value_type));
    });

    if (!extraEnvVars.empty())
    {
        EnvironmentVariableMapW tempEnvMap{ extraEnvVars };
        auto zeroEnvMap = wil::scope_exit([&] {
            // Can't zero the keys, but at least we can zero the values.
            for (auto& [name, value] : tempEnvMap)
            {
                ::SecureZeroMemory(value.data(), value.size() * sizeof(decltype(value.begin())::value_type));
            }

            tempEnvMap.clear();
        });

        RETURN_IF_FAILED(UpdateEnvironmentMapW(tempEnvMap));
        RETURN_IF_FAILED(EnvironmentMapToEnvironmentStringsW(tempEnvMap, newEnvVars));

        // Required when using a unicode environment block.
        dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;
    }

    LPWCH lpEnvironment = newEnvVars.empty() ? nullptr : newEnvVars.data();
    bool fSuccess = !!CreateProcessW(
        nullptr,
        mutableCommandline.get(),
        nullptr, // lpProcessAttributes
        nullptr, // lpThreadAttributes
        true, // bInheritHandles
        dwCreationFlags, // dwCreationFlags
        lpEnvironment, // lpEnvironment
        lpCurrentDirectory, // lpCurrentDirectory
        &si, // lpStartupInfo
        piPty // lpProcessInformation
    );

    CloseHandle(inPipeConhostSide);
    CloseHandle(outPipeConhostSide);
    CloseHandle(signalPipeConhostSide);

    return fSuccess ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

// Function Description:
// - Resizes the pty that's connected to hSignal.
// Arguments:
// - hSignal: A signal pipe as returned by CreateConPty.
// - w: The new width of the pty, in characters
// - h: The new height of the pty, in characters
// Return Value:
// - true if the resize succeeded, else false.
__declspec(noinline) inline bool SignalResizeWindow(HANDLE hSignal, const unsigned short w, const unsigned short h)
{
    unsigned short signalPacket[3];
    signalPacket[0] = PTY_SIGNAL_RESIZE_WINDOW;
    signalPacket[1] = w;
    signalPacket[2] = h;

    return !!WriteFile(hSignal, signalPacket, sizeof(signalPacket), nullptr, nullptr);
}
