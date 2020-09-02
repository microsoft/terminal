#include "pch.h"

#include "CTerminalHandoff.h"

static NewHandoff _pfnHandoff = nullptr;

static DWORD g_cTerminalHandoffRegistration = 0;

// Routine Description:
// - Called back when COM says there is nothing left for our server to do and we can tear down.
void _releaseNotifier() noexcept
{
    CTerminalHandoff::StopListening();
}

HRESULT CTerminalHandoff::StartListening(NewHandoff pfnHandoff)
{
    // has to be because the rest of TerminalConnection is apartment threaded already
    // also is it really necessary if the rest of it already is?
    RETURN_IF_FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

    // We could probably hold this in a static...
    auto classFactory = Make<SimpleClassFactory<CTerminalHandoff>>();
    RETURN_IF_NULL_ALLOC(classFactory);

    ComPtr<IUnknown> unk;
    RETURN_IF_FAILED(classFactory.As(&unk));

    RETURN_IF_FAILED(CoRegisterClassObject(__uuidof(CTerminalHandoff), unk.Get(), CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &g_cTerminalHandoffRegistration));

    _pfnHandoff = pfnHandoff;

    return S_OK;
}

HRESULT CTerminalHandoff::StopListening()
{
    _pfnHandoff = nullptr;

    if (g_cTerminalHandoffRegistration)
    {
        RETURN_IF_FAILED(CoRevokeClassObject(g_cTerminalHandoffRegistration));
        g_cTerminalHandoffRegistration = 0;
    }

    CoUninitialize();

    return S_OK;
}

// Routine Description:
// - Helper to duplicate a handle to ourselves so we can keep holding onto it
//   after the caller frees the original one.
// Arguments:
// - in - Handle to duplicate
// - out - Where to place the duplicated value
// Return Value:
// - S_OK or Win32 error from `::DuplicateHandle`
HRESULT _duplicateHandle(const HANDLE in, HANDLE& out)
{
    RETURN_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(), in, GetCurrentProcess(), &out, 0, FALSE, DUPLICATE_SAME_ACCESS));
    return S_OK;
}

HRESULT CTerminalHandoff::EstablishHandoff(HANDLE in, HANDLE out, HANDLE signal)
{
    // Duplicate the handles from what we received.
    // The contract with COM specifies that any HANDLEs we receive from the caller belong
    // to the caller and will be freed when we leave the scope of this method.
    // Making our own duplicate copy ensures they hang around in our lifetime.
    RETURN_IF_FAILED(_duplicateHandle(in, in));
    RETURN_IF_FAILED(_duplicateHandle(out, out));
    RETURN_IF_FAILED(_duplicateHandle(signal, signal));

    // Call registered handler from when we started listening.
    return _pfnHandoff(in, out, signal);

    //return S_OK;
}

HRESULT CTerminalHandoff::DoNothing()
{
    return S_FALSE;
}
