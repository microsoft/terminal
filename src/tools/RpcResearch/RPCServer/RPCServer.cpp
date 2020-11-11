/* file: hellos.c */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string>
#include "hello_h.h"
#include "ScratchImpl.h"
#include <windows.h>
#include <memory>

int g_doCount = 22;
// IScratch* g_iScratch = nullptr;
Microsoft::WRL::ComPtr<ScratchImpl> g_scratch{ nullptr };

void CreateScratch()
{
    auto impl = Microsoft::WRL::Make<ScratchImpl>();
    // ScratchImpl* foo = Microsoft::WRL::make<ScratchImpl>();

    // g_iScratch = impl.Get();
    g_scratch = impl;
}

void HelloProc(const wchar_t* psz)
{
    printf("Hello: %ws\n", psz);
    g_doCount++;
    printf("The do count is: %d\n", g_doCount);

    if (g_scratch)
    {
        int count;
        g_scratch->MyCount(&count);
        printf("The scratch's do count is: %d\n", count);
        g_scratch->MyMethod();
        g_scratch->MyCount(&count);
        printf("Now the scratch's do count is: %d\n", count);
    }
    else
    {
        printf("Creating a new scratch object\n");
        CreateScratch();
    }
}
HRESULT MarshallTheThing(IStream* pStream)
{
    printf("MarshalTheThing\n");
    auto hr = pStream->Seek({ 0, 0 }, STREAM_SEEK_SET, nullptr);
    printf("Seek %d\n", hr);
    hr = CoMarshalInterface(pStream, __uuidof(IScratch), g_scratch.Get(), MSHCTX_LOCAL, nullptr, MSHLFLAGS_NORMAL);
    printf("CoMarshalInterface %d\n", hr);

    return hr;
}

void InternallyMarshalAThing()
{
    CoInitialize(nullptr);
    auto f = Microsoft::WRL::Make<Microsoft::WRL::SimpleClassFactory<ScratchImpl>>();
    DWORD registrationHostClass;
    CoRegisterClassObject(__uuidof(ScratchImpl), f.Get(),
                                        CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationHostClass);

    printf("InternallyMarshalAThing\n");

    Microsoft::WRL::ComPtr<IStream> stream2;
    auto hr = CreateStreamOnHGlobal(
        NULL,
        TRUE,
        &stream2);
    printf("CreateStreamOnHGlobal: %d\n", hr);

    hr = stream2->Seek({ 0, 0 }, STREAM_SEEK_SET, nullptr);
    printf("Seek: %d\n", hr);

    hr = CoMarshalInterface(stream2.Get(), __uuidof(ScratchImpl), g_scratch.Get(), MSHCTX_LOCAL, nullptr, MSHLFLAGS_NORMAL);
    printf("CoMarshalInterface: %d\n", hr);

    hr = stream2->Seek({ 0, 0 }, STREAM_SEEK_SET, nullptr);
    printf("Seek (2): %d\n", hr);

    STATSTG stat;
    ULARGE_INTEGER ulSize{};
    hr = stream2->Stat(&stat, STATFLAG_NONAME);
    printf("Stat: %d\n", hr);
    ulSize = stat.cbSize;
    size_t size = ulSize.QuadPart;
    printf("size was: %lu\n", size);
    auto buffer = std::make_unique<char[]>(size + 1);

    ULONG numRead{};
    // DebugBreak();
    hr = stream2->Read(buffer.get(), size + 1, &numRead);
    if (FAILED(hr) || size != numRead)
    {
        printf("Read failed %d", hr);
    }
    else
    {
        printf("Marshalled as: %s", buffer.get());
    }
}

int GetDoCount()
{
    return g_doCount;
}

void Shutdown()
{
    printf("Goodbye\n");

    RPC_STATUS status;
    status = RpcMgmtStopServerListening(NULL);

    if (status)
    {
        exit(status);
    }

    status = RpcServerUnregisterIf(NULL, NULL, FALSE);

    if (status)
    {
        exit(status);
    }
}

void main()
{

    RPC_STATUS status;
    std::wstring pszProtocolSequence{ L"ncacn_np" };
    char* pszSecurity = NULL;
    std::wstring pszEndpoint{ L"\\pipe\\hello" };
    unsigned int cMinCalls = 1;
    unsigned int fDontWait = FALSE;

    status = RpcServerUseProtseqEp(reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(pszProtocolSequence.data())),
                                   RPC_C_LISTEN_MAX_CALLS_DEFAULT,
                                   reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(pszEndpoint.data())),
                                   pszSecurity);

    if (status)
    {
        printf("RpcServerUseProtseqEp returned an error:%d\n", status);
        exit(status);
    }

    // status = RpcServerRegisterIf(hello_IfHandle,
    status = RpcServerRegisterIf(hello_v1_0_s_ifspec,
                                 NULL,
                                 NULL);

    if (status)
    {
        printf("RpcServerRegisterIf returned an error:%d\n", status);
        exit(status);
    }

    status = RpcServerListen(cMinCalls,
                             RPC_C_LISTEN_MAX_CALLS_DEFAULT,
                             fDontWait);

    if (status)
    {
        printf("RpcServerListen returned an error:%d\n", status);
        exit(status);
    }
    else
    {
        printf("RpcServerListen returned 0\n");
    }
}

/******************************************************/
/*         MIDL allocate and free                     */
/******************************************************/

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len)
{
    return (malloc(len));
}

void __RPC_USER midl_user_free(void __RPC_FAR* ptr)
{
    free(ptr);
}
