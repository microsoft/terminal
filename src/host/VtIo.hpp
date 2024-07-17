// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../renderer/vt/vtrenderer.hpp"
#include "VtInputThread.hpp"
#include "PtySignalInputThread.hpp"

class ConsoleArguments;

namespace Microsoft::Console::Render
{
    class VtEngine;
}

namespace Microsoft::Console::VirtualTerminal
{
    class VtIo
    {
    public:
        static void FormatAttributes(std::string& target, const TextAttribute& attributes);
        static void FormatAttributes(std::wstring& target, const TextAttribute& attributes);

        VtIo();

        [[nodiscard]] HRESULT Initialize(const ConsoleArguments* const pArgs);

        [[nodiscard]] HRESULT CreateAndStartSignalThread() noexcept;
        [[nodiscard]] HRESULT CreateIoHandlers() noexcept;

        bool IsUsingVt() const;

        [[nodiscard]] HRESULT StartIfNeeded();

        [[nodiscard]] HRESULT SuppressResizeRepaint();
        [[nodiscard]] HRESULT SetCursorPosition(const til::point coordCursor);
        [[nodiscard]] HRESULT SwitchScreenBuffer(const bool useAltBuffer);
        void SendCloseEvent();

        void CloseInput();
        void CloseOutput();

        void CorkRenderer(bool corked) const noexcept;

#ifdef UNIT_TESTING
        void EnableConptyModeForTests(std::unique_ptr<Microsoft::Console::Render::VtEngine> vtRenderEngine, const bool resizeQuirk = false);
#endif

        bool IsResizeQuirkEnabled() const;

        [[nodiscard]] HRESULT ManuallyClearScrollback() const noexcept;
        [[nodiscard]] HRESULT RequestMouseMode(bool enable) const noexcept;

        void CreatePseudoWindow();
        void SetWindowVisibility(bool showOrHide) noexcept;

    private:
        // After CreateIoHandlers is called, these will be invalid.
        wil::unique_hfile _hInput;
        wil::unique_hfile _hOutput;
        // After CreateAndStartSignalThread is called, this will be invalid.
        wil::unique_hfile _hSignal;

        bool _initialized;

        bool _lookingForCursorPosition;

        bool _resizeQuirk{ false };
        bool _closeEventSent{ false };

        std::unique_ptr<Microsoft::Console::Render::VtEngine> _pVtRenderEngine;
        std::unique_ptr<Microsoft::Console::VtInputThread> _pVtInputThread;
        std::unique_ptr<Microsoft::Console::PtySignalInputThread> _pPtySignalInputThread;

        [[nodiscard]] HRESULT _Initialize(const HANDLE InHandle, const HANDLE OutHandle, _In_opt_ const HANDLE SignalHandle);

#ifdef UNIT_TESTING
        friend class VtIoTests;
#endif
    };
}
