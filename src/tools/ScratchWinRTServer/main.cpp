#include "pch.h"
#include <winrt/ScratchWinRTServer.h>
using namespace winrt;
using namespace Windows::Foundation;

struct ScratchStringable : implements<ScratchStringable, IStringable, IClosable, winrt::ScratchWinRTServer::IScratchInterface>
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
struct MyStringable : implements<MyStringable, IStringable>
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

int main()
{
    init_apartment();

    DWORD registrationMyStringable{};
    check_hresult(CoRegisterClassObject(MyStringable_clsid,
                                        make<MyStringableFactory>().get(),
                                        CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationMyStringable));

    printf("%d\n", registrationMyStringable);

    DWORD registrationScratchStringable{};
    check_hresult(CoRegisterClassObject(ScratchStringable_clsid,
                                        make<ScratchStringableFactory>().get(),
                                        CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationMyStringable));

    printf("%d\n", registrationScratchStringable);
    puts("Press Enter me when you're done serving.");
    getchar();
}
