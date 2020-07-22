#pragma once

#include "HostManager.g.h"
#include "HostAndProcess.h"

// 50dba6cd-4ddb-4b12-8363-5e06f5d0082c
static constexpr GUID HostManager_clsid{
    0x50dba6cd,
    0x4ddb,
    0x4b12,
    { 0x83, 0x63, 0x5e, 0x06, 0xf5, 0xd0, 0x08, 0x2c }
};

namespace winrt::ScratchIsland::implementation
{
    struct HostManager : public HostManagerT<HostManager>
    {
        HostManager();

        Windows::Foundation::Collections::IObservableVector<winrt::ScratchIsland::HostAndProcess> Hosts();
        winrt::ScratchIsland::HostAndProcess CreateHost();

    private:
        Windows::Foundation::Collections::IObservableVector<winrt::ScratchIsland::HostAndProcess> _hosts{ nullptr };
    };
}

namespace winrt::ScratchIsland::factory_implementation
{
    struct HostManager : HostManagerT<HostManager, implementation::HostManager>
    {
    };
}

// I bet all this could be a macro.
// I MORE be that this is all done by the factory_implementation stuff, isn't it...
struct HostManagerFactory : winrt::implements<HostManagerFactory, IClassFactory>
{
    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        return winrt::make<winrt::ScratchIsland::implementation::HostManager>().as(iid, result);
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }

    static void RegisterHostManager()
    {
        DWORD registrationHostManager{};

        winrt::check_hresult(CoRegisterClassObject(HostManager_clsid,
                                                   winrt::make<HostManagerFactory>().get(),
                                                   CLSCTX_LOCAL_SERVER,
                                                   REGCLS_MULTIPLEUSE,
                                                   &registrationHostManager));
        printf("registrationHostManager:%d\n", registrationHostManager);
    }
};
