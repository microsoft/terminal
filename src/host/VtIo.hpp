// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "..\inc\VtIoModes.hpp"
#include "..\inc\ITerminalOwner.hpp"
#include "..\renderer\vt\vtrenderer.hpp"
#include "VtInputThread.hpp"
#include "PtySignalInputThread.hpp"

class ConsoleArguments;

namespace Microsoft::Console::VirtualTerminal
{
    class VtIo : public Microsoft::Console::ITerminalOwner
    {
    public:
        VtIo();
        virtual ~VtIo() override = default;

        [[nodiscard]] HRESULT Initialize(const ConsoleArguments* const pArgs);

        [[nodiscard]] HRESULT CreateAndStartSignalThread() noexcept;
        [[nodiscard]] HRESULT CreateIoHandlers() noexcept;

        bool IsUsingVt() const;

        [[nodiscard]] HRESULT StartIfNeeded();

        [[nodiscard]] static HRESULT ParseIoMode(const std::wstring& VtMode, _Out_ VtIoMode& ioMode);

        [[nodiscard]] HRESULT SuppressResizeRepaint();
        [[nodiscard]] HRESULT SetCursorPosition(const COORD coordCursor);

        void CloseInput() override;
        void CloseOutput() override;

        void BeginResize();
        void EndResize();

    private:
        // After CreateIoHandlers is called, these will be invalid.
        wil::unique_hfile _hInput;
        wil::unique_hfile _hOutput;
        // After CreateAndStartSignalThread is called, this will be invalid.
        wil::unique_hfile _hSignal;
        VtIoMode _IoMode;

        bool _initialized;
        bool _objectsCreated;

        bool _lookingForCursorPosition;
        std::mutex _shutdownLock;

        std::unique_ptr<Microsoft::Console::Render::VtEngine> _pVtRenderEngine;
        std::unique_ptr<Microsoft::Console::VtInputThread> _pVtInputThread;
        std::unique_ptr<Microsoft::Console::PtySignalInputThread> _pPtySignalInputThread;

        [[nodiscard]] HRESULT _Initialize(const HANDLE InHandle, const HANDLE OutHandle, const std::wstring& VtMode, _In_opt_ const HANDLE SignalHandle);

        void _ShutdownIfNeeded();

#ifdef UNIT_TESTING
        friend class VtIoTests;
#endif
    };
}
