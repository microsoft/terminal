/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ProcessPolicy.h

Abstract:
- This file defines a policy framework that applies to attached client applications
  to restrict or enforce certain behavior depending on the client app type.

Author:
- Michael Niksa (miniksa) 06-Oct-2017

--*/

#pragma once

class ConsoleProcessPolicy
{
public:
    ~ConsoleProcessPolicy();

    static ConsoleProcessPolicy s_CreateInstance(const HANDLE hProcess);

    bool CanReadOutputBuffer() const;
    bool CanWriteInputBuffer() const;

private:
    ConsoleProcessPolicy(const bool fCanReadOutputBuffer,
                         const bool fCanWriteInputBuffer);

    const bool _fCanReadOutputBuffer;
    const bool _fCanWriteInputBuffer;
};
