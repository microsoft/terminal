#include "pch.h"

#include "CTerminalHandoff.h"

static NewHandoff _pfnHandoff = nullptr;

// Routine Description:
// - Called back when COM says there is nothing left for our server to do and we can tear down.
void _releaseNotifier() noexcept
{
    CTerminalHandoff::StopListening();
}

HRESULT CTerminalHandoff::StartListening(NewHandoff pfnHandoff)
{
    RETURN_IF_FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    RETURN_IF_FAILED(Module<OutOfProc>::Create(&_releaseNotifier).RegisterObjects());

    _pfnHandoff = pfnHandoff;

    return S_OK;
}

HRESULT CTerminalHandoff::StopListening()
{
    _pfnHandoff = nullptr;

    RETURN_IF_FAILED(Module<OutOfProc>::Create(&_releaseNotifier).UnregisterObjects());

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
}
