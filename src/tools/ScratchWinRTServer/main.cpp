#include "pch.h"
#include <winrt/ScratchWinRTServer.h>
#include "ScratchClass.h"
#include "HostClass.h"
using namespace winrt;
// using namespace winrt::Windows::Foundation;

struct ScratchStringable : implements<ScratchStringable, winrt::Windows::Foundation::IStringable, winrt::Windows::Foundation::IClosable, winrt::ScratchWinRTServer::IScratchInterface>
{
    hstring ToString()
    {
        return L"Hello from server, ScratchStringable";
    }
    hstring DoTheThing()
    {
        return L"Zhu Li! Do the thing!";
    }
    void Close() { printf("Closed ScratchStringable\n"); }
};
struct MyStringable : implements<MyStringable, winrt::Windows::Foundation::IStringable>
{
    hstring ToString()
    {
        return L"Hello from server, MyStringable";
    }
};

struct MyStringableFactory : implements<MyStringableFactory, IClassFactory>
{
    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }
        printf("Created MyStringable\n");
        return make<MyStringable>().as(iid, result);
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }
};

struct ScratchStringableFactory : implements<ScratchStringableFactory, IClassFactory>
{
    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }
        printf("Created ScratchStringable\n");
        return make<ScratchStringable>().as(iid, result);
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }
};
struct ScratchClassFactory : implements<ScratchClassFactory, IClassFactory>
{
    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }
        printf("\x1b[90mSERVER: Created ScratchClass\x1b[m\n");
        return make<ScratchWinRTServer::implementation::ScratchClass>().as(iid, result);
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }
};

std::mutex m;
std::condition_variable cv;
bool dtored = false;
winrt::weak_ref<ScratchWinRTServer::implementation::HostClass> g_weak{ nullptr };

struct HostClassFactory : implements<HostClassFactory, IClassFactory>
{
    HostClassFactory(winrt::guid g) :
        _guid{ g } {};

    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        if (!g_weak)
        {
            auto strong = make_self<ScratchWinRTServer::implementation::HostClass>(_guid);

            g_weak = (*strong).get_weak();
            return strong.as(iid, result);
        }
        else
        {
            auto strong = g_weak.get();
            return strong.as(iid, result);
        }
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }

private:
    winrt::guid _guid;
};

// DAA16D7F-EF66-4FC9-B6F2-3E5B6D924576
static constexpr GUID MyStringable_clsid{
    0xdaa16d7f,
    0xef66,
    0x4fc9,
    { 0xb6, 0xf2, 0x3e, 0x5b, 0x6d, 0x92, 0x45, 0x76 }
};

// EAA16D7F-EF66-4FC9-B6F2-3E5B6D924576
static constexpr GUID ScratchStringable_clsid{
    0xeaa16d7f,
    0xef66,
    0x4fc9,
    { 0xb6, 0xf2, 0x3e, 0x5b, 0x6d, 0x92, 0x45, 0x76 }
};
// FAA16D7F-EF66-4FC9-B6F2-3E5B6D924576
static constexpr GUID ScratchClass_clsid{
    0xfaa16d7f,
    0xef66,
    0x4fc9,
    { 0xb6, 0xf2, 0x3e, 0x5b, 0x6d, 0x92, 0x45, 0x76 }
};

int main(int argc, char** argv)
{
    auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    printf("\x1b[90mSERVER: args:[");
    if (argc > 0)
    {
        for (int i = 0; i < argc; i++)
        {
            printf("%s,", argv[i]);
        }
    }
    printf("]\x1b[m\n");

    winrt::guid guidFromCmdline{};

    if (argc > 1)
    {
        std::string guidString{ argv[1] };
        auto canConvert = guidString.length() == 38 && guidString.front() == '{' && guidString.back() == '}';
        if (canConvert)
        {
            std::wstring wideGuidStr{ til::u8u16(guidString) };
            printf("\x1b[90mSERVER: Found GUID:%ls\x1b[m\n", wideGuidStr.c_str());
            GUID result{};
            THROW_IF_FAILED(IIDFromString(wideGuidStr.c_str(), &result));
            guidFromCmdline = result;
        }
    }
    if (guidFromCmdline == winrt::guid{})
    {
        printf("did not recieve GUID, early returning.");
        return -1;
    }

    init_apartment();

    DWORD registrationHostClass{};
    check_hresult(CoRegisterClassObject(guidFromCmdline,
                                        make<HostClassFactory>(guidFromCmdline).get(),
                                        CLSCTX_LOCAL_SERVER,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_ENABLE_AAA,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_ACTIVATE_AAA_AS_IU,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_FROM_DEFAULT_CONTEXT,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_ENABLE_CLOAKING,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationHostClass));
    printf("%d\n", registrationHostClass);

    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return dtored; });

    // printf("\x1b[90mSERVER: Press Enter me when you're done serving.\x1b[m");
    // getchar();
    printf("\x1b[90mSERVER: exiting %s\n\x1b[m", argv[1]);
}
