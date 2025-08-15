/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConsoleShimPolicy.h

Abstract:
- This is a helper class to identify if the client process is cmd.exe or
  powershell.exe. If it is, we might need to enable certain compatibility shims
  for them.
- For more info, see GH#3126

Author:
- Mike Griese (migrie) 29-Apr-2020

--*/

#pragma once

class ConsoleShimPolicy
{
public:
    ConsoleShimPolicy(const HANDLE hProcess);
    bool IsCmdExe() const noexcept;
    bool IsPowershellExe() const noexcept;

private:
    bool _isCmd{ false };
    bool _isPowershell{ false };
};
