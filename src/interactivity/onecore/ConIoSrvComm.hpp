/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConIoSrvComm.hpp

Abstract:
- OneCore implementation of the IConIoSrvComm interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include <condrv.h>

#include "ConIoSrv.h"

#include "BgfxEngine.hpp"
#include "../../renderer/wddmcon/WddmConRenderer.hpp"

#pragma hdrstop

namespace Microsoft::Console::Interactivity::OneCore
{
    class ConIoSrvComm final
    {
    public:
        ConIoSrvComm() noexcept;
        ~ConIoSrvComm();

        static ConIoSrvComm* GetConIoSrvComm();

        [[nodiscard]] NTSTATUS Connect();
        VOID ServiceInputPipe();

        [[nodiscard]] NTSTATUS RequestGetDisplaySize(_Inout_ PCD_IO_DISPLAY_SIZE pCdDisplaySize) const;
        [[nodiscard]] NTSTATUS RequestGetFontSize(_Inout_ PCD_IO_FONT_SIZE pCdFontSize) const;
        [[nodiscard]] NTSTATUS RequestSetCursor(_In_ const CD_IO_CURSOR_INFORMATION* const pCdCursorInformation) const;
        [[nodiscard]] NTSTATUS RequestUpdateDisplay(_In_ SHORT RowIndex) const;

        [[nodiscard]] USHORT GetDisplayMode() const noexcept;

        PVOID GetSharedViewBase() const noexcept;

        VOID CleanupForHeadless(const NTSTATUS status);

        [[nodiscard]] NTSTATUS InitializeBgfx();
        [[nodiscard]] NTSTATUS InitializeWddmCon();

        std::unique_ptr<Render::WddmConEngine> pWddmConEngine;

    private:
        [[nodiscard]] NTSTATUS EnsureConnection();
        [[nodiscard]] NTSTATUS SendRequestReceiveReply(PCIS_MSG Message) const;

        VOID HandleFocusEvent(const CIS_EVENT* const FocusEvent);

        std::unique_ptr<Render::BgfxEngine> _bgfxEngine;

        HANDLE _inputPipeThreadHandle;

        HANDLE _pipeReadHandle;
        HANDLE _pipeWriteHandle;

        HANDLE _alpcClientCommunicationPort;
        SIZE_T _alpcSharedViewSize;
        PVOID _alpcSharedViewBase;

        USHORT _displayMode;

        bool _fIsInputInitialized;
    };
}
