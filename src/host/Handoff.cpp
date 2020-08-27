#include "precomp.h"

#include "Handoff.h"

#include "srvinit.h"

HRESULT Handoff::EstablishHandoff(HANDLE server,
                                  HANDLE inputEvent,
                                  PCCONSOLE_PORTABLE_ARGUMENTS args,
                                  PCCONSOLE_PORTABLE_ATTACH_MSG msg)
{
    ConsoleArguments consoleArgs;
    // consoleArgs = *args;
    CONSOLE_API_MSG apiMsg;
    // apiMsg = *msg;

    ConsoleEstablishHandoff(server, &consoleArgs, inputEvent, &apiMsg);

    UNREFERENCED_PARAMETER(server);
    UNREFERENCED_PARAMETER(inputEvent);
    UNREFERENCED_PARAMETER(args);
    UNREFERENCED_PARAMETER(msg);
    return S_FALSE;
}
