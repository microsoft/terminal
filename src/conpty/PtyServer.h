#pragma once

#include <conpty.h>

using unique_nthandle = wil::unique_any_handle_null<decltype(&::NtClose), ::NtClose>;

struct PtyServer : IPtyServer
{
    PtyServer();

    virtual ~PtyServer() = default;

#pragma region IUnknown

    HRESULT QueryInterface(const IID& riid, void** ppvObject) override;
    ULONG AddRef() override;
    ULONG Release() override;

#pragma endregion

#pragma region IPtyServer

    HRESULT Run() override;
    HRESULT CreateProcessW(
        LPCWSTR lpApplicationName,
        LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes,
        BOOL bInheritHandles,
        DWORD dwCreationFlags,
        LPVOID lpEnvironment,
        LPCWSTR lpCurrentDirectory,
        LPPROCESS_INFORMATION lpProcessInformation) override;

#pragma endregion

private:
    static unique_nthandle createHandle(HANDLE parent, const wchar_t* typeName, bool inherit, bool synchronous);
    NTSTATUS ioctl(DWORD code, void* in, DWORD inLen, void* out, DWORD outLen) const;

    std::atomic<ULONG> m_refCount{ 1 };
    unique_nthandle m_server;
    wil::unique_event m_inputAvailableEvent;
};
