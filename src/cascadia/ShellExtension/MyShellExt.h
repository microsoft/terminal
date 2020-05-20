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

struct __declspec(uuid("8eb80de0-e1ff-442c-956a-c5f2b54ca274")) IMyShellExt : ::IUnknown
{
    virtual HRESULT __stdcall Call() = 0;
};

struct __declspec(uuid("9f156763-7844-4dc4-bbb1-901f640f5155"))
    MyShellExt : winrt::implements<MyShellExt, IExplorerCommand>
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
