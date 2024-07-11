// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "VtInputThread.hpp"
#include "PtySignalInputThread.hpp"

class ConsoleArguments;

namespace Microsoft::Console::VirtualTerminal
{
    class VtIo
    {
    public:
        struct CorkLock
        {
            CorkLock() = default;
            CorkLock(VtIo* io) noexcept;

            ~CorkLock() noexcept;

            CorkLock(const CorkLock&) = delete;
            CorkLock& operator=(const CorkLock&) = delete;
            CorkLock(CorkLock&& other) noexcept;
            CorkLock& operator=(CorkLock&& other) noexcept;

        private:
            VtIo* _io = nullptr;
        };

        struct CursorRestore
        {
            CursorRestore() = default;
            CursorRestore(VtIo* io, til::point position) noexcept;

            ~CursorRestore() noexcept;

            CursorRestore(const CursorRestore&) = delete;
            CursorRestore& operator=(const CursorRestore&) = delete;
            CursorRestore(CursorRestore&& other) noexcept;
            CursorRestore& operator=(CursorRestore&& other) noexcept;

        private:
            VtIo* _io = nullptr;
            til::point _position;
        };

        friend struct CorkLock;

        static bool IsControlCharacter(wchar_t wch) noexcept;
        static void FormatAttributes(std::string& target, WORD attributes);
        static void FormatAttributes(std::wstring& target, WORD attributes);

        [[nodiscard]] HRESULT Initialize(const ConsoleArguments* const pArgs);
        [[nodiscard]] HRESULT CreateAndStartSignalThread() noexcept;
        [[nodiscard]] HRESULT CreateIoHandlers() noexcept;

        bool IsUsingVt() const;

        [[nodiscard]] HRESULT StartIfNeeded();

        void SendCloseEvent();

        void CloseInput();
        void CloseOutput();

        void CreatePseudoWindow();

        CorkLock Cork() noexcept;
        bool BufferHasContent() const noexcept;

        void WriteFormat(auto&&... args)
        {
            fmt::format_to(std::back_inserter(_back), std::forward<decltype(args)>(args)...);
            _flush();
        }
        void WriteUTF8(std::string_view str);
        void WriteUTF16(std::wstring_view str);
        void WriteUTF16StripControlChars(std::wstring_view str);
        void WriteUCS2(wchar_t ch);
        void WriteCUP(til::point position);
        void WriteDECTCEM(bool enabled);
        void WriteSGR1006(bool enabled);
        void WriteDECAWM(bool enabled);
        void WriteLNM(bool enabled);
        void WriteASB(bool enabled);
        void WriteAttributes(WORD attributes);
        void WriteInfos(til::point target, std::span<const CHAR_INFO> infos);

    private:
        [[nodiscard]] HRESULT _Initialize(const HANDLE InHandle, const HANDLE OutHandle, _In_opt_ const HANDLE SignalHandle);

        void _uncork();
        void _flush();
        void _flushNow();

        // After CreateIoHandlers is called, these will be invalid.
        wil::unique_hfile _hInput;
        wil::unique_hfile _hOutput;
        // After CreateAndStartSignalThread is called, this will be invalid.
        wil::unique_hfile _hSignal;

        std::unique_ptr<Microsoft::Console::VtInputThread> _pVtInputThread;
        std::unique_ptr<Microsoft::Console::PtySignalInputThread> _pPtySignalInputThread;

        // We use two buffers: A front and a back buffer. The front buffer is the one we're currently
        // sending to the terminal (it's being "presented" = it's on the "front" & "visible").
        // The back buffer is the one we're concurrently writing to.
        std::string _front;
        std::string _back;
        OVERLAPPED* _overlapped = nullptr;
        OVERLAPPED _overlappedBuf{};
        wil::unique_event _overlappedEvent;
        bool _overlappedPending = false;

        bool _initialized = false;
        bool _lookingForCursorPosition = false;
        bool _closeEventSent = false;
        int _corked = 0;

#ifdef UNIT_TESTING
        friend class VtIoTests;
#endif
    };
}
