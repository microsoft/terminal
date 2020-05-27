/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- OpenTerminalHere.h

Abstract:
- This is the class that implements our Explorer Context Menu item. By
  implementing IExplorerCommand, we can provide an entry to the context menu to
  allow the user to open the Terminal in the current working directory.
- Importantly, we need to make sure to declare the GUID of this implementation
  class explicitly, so we can refer to it in our manifest, and use it to create
  instances of this class when the shell asks for one.
- This is defined as a WRL type, so that we can use WRL's CoCreatableClass magic
  to create the class factory and module management for us. See more details in
  dllmain.cpp.

Author(s):
- Mike Griese - May 2020

--*/
#pragma once

#include <conattrs.hpp>
#include "../../cascadia/inc/cppwinrt_utils.h"

using namespace Microsoft::WRL;

struct __declspec(uuid("9f156763-7844-4dc4-b2b1-901f640f5155"))
    OpenTerminalHere : public RuntimeClass<RuntimeClassFlags<ClassicCom | InhibitFtmBase>, IExplorerCommand>
{
#pragma region IExplorerCommand
    HRESULT Invoke(IShellItemArray* psiItemArray,
                   IBindCtx* pBindContext);
    HRESULT GetToolTip(IShellItemArray* psiItemArray,
                       LPWSTR* ppszInfoTip);
    HRESULT GetTitle(IShellItemArray* psiItemArray,
                     LPWSTR* ppszName);
    HRESULT GetState(IShellItemArray* psiItemArray,
                     BOOL fOkToBeSlow,
                     EXPCMDSTATE* pCmdState);
    HRESULT GetIcon(IShellItemArray* psiItemArray,
                    LPWSTR* ppszIcon);
    HRESULT GetFlags(EXPCMDFLAGS* pFlags);
    HRESULT GetCanonicalName(GUID* pguidCommandName);
    HRESULT EnumSubCommands(IEnumExplorerCommand** ppEnum);
#pragma endregion
};

CoCreatableClass(OpenTerminalHere);
