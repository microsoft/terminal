#include "pch.h"
#include <winrt/ScratchWinRTServer.h>
#include "ScratchClass.h"
#include "HostClass.h"
using namespace winrt;
// using namespace winrt::Windows::Foundation;

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
        // Obviously this isn't right but it's good enough
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

    printf("\x1b[90mSERVER: exiting %s\n\x1b[m", argv[1]);
}
