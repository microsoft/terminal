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

bool areWeTheKing(const winrt::MonarchPeasantSample::Monarch& monarch, const bool logPIDs = false)
{
    auto kingPID = monarch.GetPID();
    auto ourPID = GetCurrentProcessId();
    if (logPIDs)
    {
        if (ourPID == kingPID)
        {
            printf("We're the king - our PID is %d\n", ourPID);
        }
        else
        {
            printf("we're a lowly peasant - the king is %d\n", kingPID);
        }
    }
    return (ourPID == kingPID);
}

MonarchPeasantSample::IPeasant getOurPeasant(const winrt::MonarchPeasantSample::Monarch& monarch)
{
    if (areWeTheKing(monarch))
    {
        auto iPeasant = monarch.try_as<winrt::MonarchPeasantSample::IPeasant>();
    }
    else
    {
        auto peasant = winrt::make_self<MonarchPeasantSample::implementation::Peasant>();
        auto ourID = monarch.AddPeasant(*peasant);
        printf("The monarch assigned us the ID %d\n", ourID);
        return *peasant;
    }
}

void printPeasants(const winrt::MonarchPeasantSample::Monarch& monarch)
{
    printf("This is unimplemented\n");
}

void monarchAppLoop(const winrt::MonarchPeasantSample::Monarch& monarch,
                    const winrt::MonarchPeasantSample::IPeasant& peasant)
{
    bool exitRequested = false;
    printf("Press `l` to list peasants, `q` to quit\n");

    while (!exitRequested)
    {
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
void peasantAppLoop(const winrt::MonarchPeasantSample::Monarch& monarch,
                    const winrt::MonarchPeasantSample::IPeasant& peasant)
{
    bool exitRequested = false;
    printf("Press `q` to quit\n");

    while (!exitRequested)
    {
        const auto ch = _getch();
        if (ch == 'q')
        {
            exitRequested = true;
        }
    }
}

void app()
{
    registerAsMonarch();
    auto monarch = instantiateAMonarch();
    const bool isMonarch = areWeTheKing(monarch, true);
    auto peasant = getOurPeasant(monarch);

    if (isMonarch)
    {
        monarchAppLoop(monarch, peasant);
    }
    else
    {
        peasantAppLoop(monarch, peasant);
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
