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

        [[nodiscard]] NTSTATUS Connect() noexcept;
        VOID ServiceInputPipe();

        [[nodiscard]] NTSTATUS RequestGetDisplaySize(_Inout_ PCD_IO_DISPLAY_SIZE pCdDisplaySize) const noexcept;
        [[nodiscard]] NTSTATUS RequestGetFontSize(_Inout_ PCD_IO_FONT_SIZE pCdFontSize) const noexcept;
        [[nodiscard]] NTSTATUS RequestSetCursor(_In_ const CD_IO_CURSOR_INFORMATION* const pCdCursorInformation) const noexcept;
        [[nodiscard]] NTSTATUS RequestUpdateDisplay(_In_ til::CoordType RowIndex) const noexcept;

        [[nodiscard]] NTSTATUS RequestMapVirtualKey(_In_ UINT uCode, _In_ UINT uMapType, _Out_ UINT* puReturnValue) noexcept;
        [[nodiscard]] NTSTATUS RequestVkKeyScan(_In_ WCHAR wCharacter, _Out_ SHORT* psReturnValue) noexcept;
        [[nodiscard]] NTSTATUS RequestGetKeyState(_In_ int iVirtualKey, _Out_ SHORT* psReturnValue) noexcept;
        [[nodiscard]] USHORT GetDisplayMode() const noexcept;

        PVOID GetSharedViewBase() const noexcept;

        VOID CleanupForHeadless(const NTSTATUS status);

        UINT ConIoMapVirtualKeyW(UINT uCode, UINT uMapType) noexcept;
        SHORT ConIoVkKeyScanW(WCHAR ch) noexcept;
        SHORT ConIoGetKeyState(int nVirtKey) noexcept;

        [[nodiscard]] NTSTATUS InitializeBgfx();
        [[nodiscard]] NTSTATUS InitializeWddmCon();

        Render::WddmConEngine* pWddmConEngine = nullptr;

    private:
        [[nodiscard]] NTSTATUS EnsureConnection() noexcept;
        [[nodiscard]] NTSTATUS SendRequestReceiveReply(PCIS_MSG Message) const noexcept;

        VOID HandleFocusEvent(const CIS_EVENT* const FocusEvent);

        Render::BgfxEngine* _bgfxEngine = nullptr;

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
