/* file: helloc.c */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "hello_h.h"
#include "IScratch_h.h"
#include <windows.h>

#include <wrl.h>

void mainLoop()
{
    const wchar_t* pszString = L"hello, world";

    // Do the logic for the app in here, once RPC has already been set up.

    printf("The server said the DoCount is %d\n", GetDoCount());

    HelloProc(pszString);
    HelloProc(L"A different string");

    printf("Now, the DoCount is %d\n", GetDoCount());

    // This Shutdown RPC call will stop the server process
    // Shutdown();
    InternallyMarshalAThing();
    
    Microsoft::WRL::ComPtr<IStream> stream2;
    auto hr = CreateStreamOnHGlobal(
        NULL,
        TRUE,
        &stream2);
    printf("CreateStreamOnHGlobal: %d\n", hr);

    hr = stream2->Seek({ 0, 0 }, STREAM_SEEK_SET, nullptr);
    printf("Seek: %d\n", hr);

    hr = MarshallTheThing(stream2.Get());

    printf("MarshallTheThing: %d\n", hr);
    Microsoft::WRL::ComPtr<IScratch> otherScratch;
    
    hr = CoUnmarshalInterface(stream2.Get(), __uuidof(IScratch), (void**)otherScratch.GetAddressOf());
    printf("CoUnmarshalInterface: %d\n", hr);
    if (otherScratch)
    {
        printf("Successfully got a IScratch\n");
    }
    else
    {
        printf("Failed to get a IScratch\n");
    }
}

void main()
{
    RPC_STATUS status;
    wchar_t* pszUuid = NULL;
    const wchar_t* pszProtocolSequence = L"ncacn_np";
    wchar_t* pszNetworkAddress = NULL;
    const wchar_t* pszEndpoint = L"\\pipe\\hello";
    wchar_t* pszOptions = NULL;
    wchar_t* pszStringBinding = NULL;
    unsigned long ulCode;

    status = RpcStringBindingCompose(reinterpret_cast<RPC_WSTR>(pszUuid),
                                     reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(pszProtocolSequence)),
                                     reinterpret_cast<RPC_WSTR>(pszNetworkAddress),
                                     reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(pszEndpoint)),
                                     reinterpret_cast<RPC_WSTR>(pszOptions),
                                     reinterpret_cast<RPC_WSTR*>(&pszStringBinding));
    if (status)
    {
        printf("RpcStringBindingCompose failed with %d\n", status);
        exit(status);
    }

    // status = RpcBindingFromStringBinding(reinterpret_cast<RPC_WSTR>(pszStringBinding), &hello_ClientIfHandle);
    // status = RpcBindingFromStringBinding(reinterpret_cast<RPC_WSTR>(pszStringBinding), &hello_v1_0_c_ifspec);
    status = RpcBindingFromStringBinding(reinterpret_cast<RPC_WSTR>(pszStringBinding), &hello_IfHandle);

    if (status)
    {
        printf("RpcBindingFromStringBinding failed with %d\n", status);
        exit(status);
    }

    RpcTryExcept
    {
        mainLoop();
    }
    RpcExcept(1)
    {
        ulCode = RpcExceptionCode();
        printf("Runtime reported exception 0x%lx = %ld\n", ulCode, ulCode);
    }
    RpcEndExcept;

    status = RpcStringFree(reinterpret_cast<RPC_WSTR*>(&pszStringBinding));
    if (status)
    {
        printf("RpcStringFree failed with %d\n", status);
        exit(status);
    }

    status = RpcBindingFree(&hello_IfHandle);

    if (status)
    {
        printf("RpcBindingFree failed with %d\n", status);
        exit(status);
    }

    exit(0);
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
