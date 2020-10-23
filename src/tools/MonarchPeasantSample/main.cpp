#include "pch.h"
#include <conio.h>
#include "Monarch.h"
#include "Peasant.h"
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
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_ENABLE_AAA,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_ACTIVATE_AAA_AS_IU,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_FROM_DEFAULT_CONTEXT,
                                        // CLSCTX_LOCAL_SERVER | CLSCTX_ENABLE_CLOAKING,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationHostClass));
}

winrt::MonarchPeasantSample::Monarch instantiateAMonarch()
{
    auto monarch = create_instance<winrt::MonarchPeasantSample::Monarch>(Monarch_clsid, CLSCTX_LOCAL_SERVER);
    return monarch;
}

void areWeTheKing(const winrt::MonarchPeasantSample::Monarch& monarch)
{
    auto kingPID = monarch.GetPID();
    auto ourPID = GetCurrentProcessId();
    if (ourPID == kingPID)
    {
        printf("We're the king - our PID is %d\n", ourPID);
    }
    else
    {
        printf("we're a lowly peasant - the king is %d\n", kingPID);
    }
}

void printPeasants(const winrt::MonarchPeasantSample::Monarch& monarch)
{
    printf("This is unimplemented\n");
}

void app()
{
    registerAsMonarch();
    auto monarch = instantiateAMonarch();
    areWeTheKing(monarch);

    bool exitRequested = false;
    while (!exitRequested)
    {
        printf("-----------------------------\n");
        printf("input a command (l, q): ");
        const auto ch = _getch();

        if (ch == 'l')
        {
            printPeasants(monarch);
        }
        else if (ch == 'q')
        {
            exitRequested = true;
        }
    }
}

int main(int argc, char** argv)
{
    auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    init_apartment();

    try
    {
        app();
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }

    printf("Exiting client\n");
}
