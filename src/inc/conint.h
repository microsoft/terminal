/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    conint.h

Abstract:

    This module defines an abstraction for calling certain OS support
    functionality that we can't make completely public due to obligations
    we have with our partner teams about disclosure of their API surface.

Author:

    miniksa 17-Apr-2019

Revision History:

--*/

#pragma once

namespace Microsoft::Console::Internal
{
    namespace ProcessPolicy
    {
        [[nodiscard]] HRESULT CheckAppModelPolicy(const HANDLE hToken,
                                                  bool& fIsWrongWayBlocked) noexcept;

        [[nodiscard]] HRESULT CheckIntegrityLevelPolicy(const HANDLE hOtherToken,
                                                        bool& fIsWrongWayBlocked) noexcept;
    }

    namespace EdpPolicy
    {
        void AuditClipboard(const std::wstring_view destinationName) noexcept;
    }

}
