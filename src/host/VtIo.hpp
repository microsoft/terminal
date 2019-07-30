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
    // fwdecl so we can use it as the return type for BeginResize
    class VtIoResizeLock;

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

        VtIoResizeLock BeginResize();

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
        void _EndResize();

        friend class VtIoResizeLock;

#ifdef UNIT_TESTING
        friend class VtIoTests;
#endif
    };

    // VtIoResizeLock is a helper object to act as a RAII wrapper for
    // VtIo::BeginResize. When the VtIoResizeLock goes out of scope, it'll call
    // VtIo::_EndResize to end the resize operation.
    class VtIoResizeLock
    {
    public:
        VtIoResizeLock() = default;
        ~VtIoResizeLock()
        {
            if (_vtIo)
            {
                _vtIo->_EndResize();
            }
        };

    private:
        VtIo* _vtIo{ nullptr };
        VtIoResizeLock(VtIo* vtio) :
            _vtIo{ vtio } {};
        friend class VtIo;
    };
}
