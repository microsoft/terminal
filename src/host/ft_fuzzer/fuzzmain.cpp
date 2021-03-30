// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../ConsoleArguments.hpp"
#include "../srvinit.h"
#include "../../server/Entrypoints.h"
#include "../../interactivity/inc/ServiceLocator.hpp"
#include "../../server/DeviceHandle.h"
#include "../../server/IoThread.h"
#include "../_stream.h"
#include "../getset.h"
#include <til/u8u16convert.h>

struct NullDeviceComm : public IDeviceComm
{
    HRESULT SetServerInformation(CD_IO_SERVER_INFORMATION* const) const override
    {
        return S_FALSE;
    }
    HRESULT ReadIo(PCONSOLE_API_MSG const, CONSOLE_API_MSG* const) const override
    {
        // The easiest way to get the IO thread to stop reading from us us to simply
        // suspend it. The fuzzer doesn't need a device IO thread.
        SuspendThread(GetCurrentThread());
        return S_FALSE;
    }
    HRESULT CompleteIo(CD_IO_COMPLETE* const) const override
    {
        return S_FALSE;
    }
    HRESULT ReadInput(CD_IO_OPERATION* const) const override
    {
        SuspendThread(GetCurrentThread());
        return S_FALSE;
    }
    HRESULT WriteOutput(CD_IO_OPERATION* const) const override
    {
        return S_FALSE;
    }
    HRESULT AllowUIAccess() const override
    {
        return S_FALSE;
    }
    ULONG_PTR PutHandle(const void*) override
    {
        return 0;
    }
    void* GetHandle(ULONG_PTR) const override
    {
        return nullptr;
    }
    HRESULT GetServerHandle(HANDLE*) const override
    {
        return S_FALSE;
    }
};

[[nodiscard]] HRESULT StartNullConsole(const ConsoleArguments* const args)
{
    auto& globals = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals();
    globals.pDeviceComm = new NullDeviceComm{}; // quickly, before we "connect". Leak this.

    // it is safe to pass INVALID_HANDLE_VALUE here because the null handle would have been detected
    // in ConDrvDeviceComm (which has been avoided by setting a global device comm beforehand)
    RETURN_IF_NTSTATUS_FAILED(ConsoleCreateIoThreadLegacy(INVALID_HANDLE_VALUE, args));

    auto& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();

    // Process handle list manipulation must be done under lock
    gci.LockConsole();
    ConsoleProcessHandle* pProcessHandle{ nullptr };
    RETURN_IF_FAILED(gci.ProcessHandleList.AllocProcessData(GetCurrentProcessId(),
                                                            GetCurrentThreadId(),
                                                            0,
                                                            nullptr,
                                                            &pProcessHandle));
    pProcessHandle->fRootProcess = true;

    constexpr static std::wstring_view fakeTitle{ L"Fuzzing Harness" };

    CONSOLE_API_CONNECTINFO fakeConnectInfo{};
    fakeConnectInfo.ConsoleInfo.SetShowWindow(SW_NORMAL);
    fakeConnectInfo.ConsoleInfo.SetScreenBufferSize(til::size{ 80, 25 });
    fakeConnectInfo.ConsoleInfo.SetWindowSize(til::size{ 80, 25 });
    fakeConnectInfo.ConsoleInfo.SetStartupFlags(STARTF_USECOUNTCHARS);
    wcscpy_s(fakeConnectInfo.Title, fakeTitle.data());
    fakeConnectInfo.TitleLength = gsl::narrow_cast<DWORD>(fakeTitle.size() * sizeof(wchar_t)); // bytes, not wchars
    wcscpy_s(fakeConnectInfo.AppName, fakeTitle.data());
    fakeConnectInfo.AppNameLength = gsl::narrow_cast<DWORD>(fakeTitle.size() * sizeof(wchar_t)); // bytes, not wchars
    fakeConnectInfo.ConsoleApp = TRUE;
    fakeConnectInfo.WindowVisible = TRUE;
    RETURN_IF_NTSTATUS_FAILED(ConsoleAllocateConsole(&fakeConnectInfo));

    CommandHistory::s_Allocate(fakeTitle, (HANDLE)pProcessHandle);

    gci.UnlockConsole();

    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT RunConhost()
{
    Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().hInstance = wil::GetModuleInstanceHandle();

    // passing stdin/stdout lets us drive this like conpty (!!) and test the VT renderer (!!)
    // but for now we want to drive it like conhost
    ConsoleArguments args({}, nullptr, nullptr);

    HRESULT hr = args.ParseCommandline();
    if (SUCCEEDED(hr))
    {
        hr = StartNullConsole(&args);
    }

    return hr;
}

#ifdef FUZZING_BUILD
extern "C" __declspec(dllexport) int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/)
#else
int main(int /*argc*/, char** /*argv*/)
#endif
{
    RETURN_IF_FAILED(RunConhost());
    return 0;
}

extern "C" __declspec(dllexport) int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    auto& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();

    const auto u16String{ til::u8u16(std::string_view{ reinterpret_cast<const char*>(data), size }) };
    SHORT scrollY{};
    size_t sizeInBytes{ u16String.size() * 2 };
    gci.LockConsole();
    auto u = wil::scope_exit([&]() { gci.UnlockConsole(); });
    (void)WriteCharsLegacy(gci.GetActiveOutputBuffer(),
                           u16String.data(),
                           u16String.data(),
                           u16String.data(),
                           &sizeInBytes,
                           nullptr,
                           0,
                           WC_PRINTABLE_CONTROL_CHARS | WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE,
                           &scrollY);
    return 0;
}
