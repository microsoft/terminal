#include "pch.h"
#include "AppState.h"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

////////////////////////////////////////////////////////////////////////////////
// This seems like a hack, but it works.
//
// This class factory works so that there's only ever one instance of a Monarch
// per-process. Once the first monarch is created, we'll stash it in g_weak.
// Future callers who try to instantiate a Monarch will get the one that's
// already been made.
//
// I'm sure there's a better way to do this with WRL, but I'm not familiar
// enough with WRL to know for sure.

winrt::weak_ref<MonarchPeasantSample::implementation::Monarch> g_weak{ nullptr };

struct MonarchFactory : implements<MonarchFactory, IClassFactory>
{
    MonarchFactory() = default;

    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        if (!g_weak)
        {
            // Create a new Monarch instance
            auto strong = make_self<MonarchPeasantSample::implementation::Monarch>();

            g_weak = (*strong).get_weak();
            return strong.as(iid, result);
        }
        else
        {
            // We already instantiated one Monarch, let's just return that one!
            auto strong = g_weak.get();
            return strong.as(iid, result);
        }
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }
};
////////////////////////////////////////////////////////////////////////////////

// Function Description:
// - Register the Monarch object with COM. This allows other processes to create
//   Monarch's in our process space with CoCreateInstance and the Monarch_clsid.
DWORD registerAsMonarch()
{
    DWORD registrationHostClass{};
    check_hresult(CoRegisterClassObject(Monarch_clsid,
                                        make<MonarchFactory>().get(),
                                        CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationHostClass));
    return registrationHostClass;
}

// Function Description:
// - Called when the old monarch dies. Create a new connection to the new
//   monarch. This might be us! If we're the new monarch, then update the
//   Monarch to know which Peasant it came from. Otherwise, tell the new monarch
//   that we exist.
void electNewMonarch(AppState& state)
{
    state.monarch = AppState::instantiateMonarch();
    bool isMonarch = state.areWeTheKing(true);

    printf("LONG LIVE THE %sKING\x1b[m\n", isMonarch ? "\x1b[33m" : "");

    if (isMonarch)
    {
        state.remindKingWhoTheyAre(state.peasant);
    }
    else
    {
        // Add us to the new monarch
        state.monarch.AddPeasant(state.peasant);
    }
}

void appLoop(AppState& state)
{
    auto dwRegistration = registerAsMonarch();
    // IMPORTANT! Tear down the registration as soon as we exit. If we're not a
    // real peasant window (the monarch passed our commandline to someone else),
    // then the monarch dies, we don't want our registration becoming the active
    // monarch!
    auto cleanup = wil::scope_exit([&]() {
        check_hresult(CoRevokeClassObject(dwRegistration));
    });

    // Tricky - first, we have to ask the monarch to handle the commandline.
    // They will tell us if we need to create a peasant.
    state.createMonarch();
    // processCommandline will return true if we should exit early.
    if (state.processCommandline())
    {
        return;
    }

    bool isMonarch = state.areWeTheKing(true);
    bool exitRequested = false;

    // (monarch|peasant)AppLoop will return when they've run to completion. If
    // they return true, then just exit the application (the user might have
    // pressed 'q' to exit). If the peasant returns false, then it detected the
    // monarch died. Attempt to elect a new one.
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

int main(int argc, char** argv)
{
    AppState state;
    state.initializeState();

    // Collect up all the commandline arguments
    printf("args:[");
    for (auto& elem : wil::make_range(argv, argc))
    {
        printf("%s, ", elem);
        // This is obviously a bad way of converting args to a vector of
        // hstrings, but it'll do for now.
        state.args.emplace_back(winrt::to_hstring(elem));
    }
    printf("]\n");

    try
    {
        appLoop(state);
    }
    catch (const hresult_error& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }

    printf("We've left the app. Press any key to close.\n");
    const auto ch = _getch();
    ch;
    printf("Exiting client\n");
}
