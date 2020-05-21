/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Foo.h

Abstract:
Author(s):
- Mike Griese - May 2020

--*/
#pragma once

#include <conattrs.hpp>
#include "../../cascadia/inc/cppwinrt_utils.h"

struct __declspec(uuid("9f156763-7844-4dc4-bbb1-901f640f5155"))
    OpenTerminalHere : winrt::implements<OpenTerminalHere, IExplorerCommand>
{
    HRESULT Invoke(IShellItemArray* psiItemArray,
                   IBindCtx* pbc);
    HRESULT GetToolTip(IShellItemArray* psiItemArray,
                       LPWSTR* ppszInfotip);
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
};
