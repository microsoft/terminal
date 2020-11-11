/* file: hellos.c */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string>
#include "hello_h.h"
#include <windows.h>

int g_doCount = 22;

void HelloProc(const wchar_t* psz)
{
    printf("Hello: %ws\n", psz);
    g_doCount++;
    printf("The do count is: %d\n", g_doCount);
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
