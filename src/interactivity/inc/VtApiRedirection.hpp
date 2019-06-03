/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VtApiRedirection.h

Abstract:
- Redefines several input-related API's that are not available on OneCore such
  that they be redirected through the ServiceLocator via the IInputServices
  interface.
- This ensures that all calls to these API's are executed as normal when the
  console is running on big Windows, but that they are also redirected to the
  Console IO Server when it is running on a OneCore system, where the OneCore
  implementations live.

Author:
- HeGatta Apr.25.2017
--*/

#pragma once

#define MapVirtualKeyW(x, y) VTRedirMapVirtualKeyW(x, y)
#define VkKeyScanW(x) VTRedirVkKeyScanW(x)
#define GetKeyState(x) VTRedirGetKeyState(x)

UINT VTRedirMapVirtualKeyW(_In_ UINT uCode, _In_ UINT uMapType);
SHORT VTRedirVkKeyScanW(_In_ WCHAR ch);
SHORT VTRedirGetKeyState(_In_ int nVirtKey);
