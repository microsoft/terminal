#include "pch.h"
#include <conio.h>
#include "Monarch.h"
#include "Peasant.h"
#include "AppState.h"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

////////////////////////////////////////////////////////////////////////////////
std::mutex m;
std::condition_variable cv;
bool dtored = false;
winrt::weak_ref<MonarchPeasantSample::implementation::Monarch> g_weak{ nullptr };

struct MonarchFactory : implements<MonarchFactory, IClassFactory>
{
    MonarchFactory() :
        _guid{ Monarch_clsid } {};

    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        if (!g_weak)
        {
            auto strong = make_self<MonarchPeasantSample::implementation::Monarch>();

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
////////////////////////////////////////////////////////////////////////////////

void registerAsMonarch()
{
    DWORD registrationHostClass{};
    check_hresult(CoRegisterClassObject(Monarch_clsid,
                                        make<MonarchFactory>().get(),
                                        CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationHostClass));
}

void electNewMonarch(AppState& state)
{
    state._monarch = AppState::instantiateAMonarch();
    bool isMonarch = state.areWeTheKing(true);

    printf("LONG LIVE THE %sKING\x1b[m\n", isMonarch ? "\x1b[33m" : "");

    if (isMonarch)
    {
        state.remindKingWhoTheyAre(state._peasant);
    }
    else
    {
        // Add us to the new monarch
        state._monarch.AddPeasant(state._peasant);
    }
}

void appLoop(AppState& state)
{
    registerAsMonarch();
    bool exitRequested = false;

    state.createMonarchAndPeasant();

    bool isMonarch = state.areWeTheKing(true);

    while (!exitRequested)
    {
        if (isMonarch)
        {
            exitRequested = monarchAppLoop(state);
        }
        else
        {
            exitRequested = peasantAppLoop(state);
            if (!exitRequested)
            {
                electNewMonarch(state);
                isMonarch = state.areWeTheKing(false);
            }
        }
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    AppState state;
    state.initializeState();

    try
    {
        appLoop(state);
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }

    printf("We've left the app. Press any key to close.\n");
    const auto ch = _getch();
    ch;
    printf("Exiting client\n");
}
