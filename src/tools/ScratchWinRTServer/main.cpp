#include "pch.h"
#include <winrt/ScratchWinRTServer.h>
using namespace winrt;
using namespace Windows::Foundation;

struct ScratchStringable : implements<ScratchStringable, IStringable, winrt::ScratchWinRTServer::IScratchInterface>
{
    hstring ToString()
    {
        return L"Hello from server, ScratchStringable";
    }
    hstring DoTheThing()
    {
        return L"Zhu Li! Do the thing!";
    }
};
struct Stringable : implements<Stringable, IStringable>
{
    hstring ToString()
    {
        return L"Hello from server, Stringable";
    }
};

struct Factory : implements<Factory, IClassFactory>
{
    HRESULT __stdcall CreateInstance(
        IUnknown* outer,
        GUID const& iid,
        void** result) noexcept final
    {
        *result = nullptr;

        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        // return make<Stringable>().as(iid, result);
        return make<ScratchStringable>().as(iid, result);
        // auto f = winrt::make_self<winrt::ScratchWinRTServer::ScratchClass>();
        // return f.as(iid, result);
        //return make<winrt::ScratchWinRTServer::ScratchClass>().as(iid, result);
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }
};

static constexpr GUID server_clsid // DAA16D7F-EF66-4FC9-B6F2-3E5B6D924576
    {
        0xdaa16d7f,
        0xef66,
        0x4fc9,
        { 0xb6, 0xf2, 0x3e, 0x5b, 0x6d, 0x92, 0x45, 0x76 }
    };

int main()
{
    init_apartment();

    DWORD registration{};

    check_hresult(CoRegisterClassObject(
        server_clsid,
        make<Factory>().get(),
        CLSCTX_LOCAL_SERVER,
        REGCLS_MULTIPLEUSE,
        &registration));

    printf("%d\n", registration);
    puts("Press Enter me when you're done serving.");
    getchar();
}
