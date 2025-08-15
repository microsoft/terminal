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
        struct Writer
        {
            Writer() = default;
            Writer(VtIo* io) noexcept;

            ~Writer() noexcept;

            Writer(const Writer&) = delete;
            Writer& operator=(const Writer&) = delete;
            Writer(Writer&& other) = delete;
            Writer& operator=(Writer&& other) = delete;

            explicit operator bool() const noexcept;

            void Submit();

            void BackupCursor() const;
            void WriteUTF8(std::string_view str) const;
            void WriteUTF16(std::wstring_view str) const;
            void WriteUTF16TranslateCRLF(std::wstring_view str) const;
            void WriteUTF16StripControlChars(std::wstring_view str) const;
            void WriteUCS2(wchar_t ch) const;
            void WriteCUP(til::point position) const;
            void WriteDECTCEM(bool enabled) const;
            void WriteSGR1006(bool enabled) const;
            void WriteDECAWM(bool enabled) const;
            void WriteASB(bool enabled) const;
            void WriteWindowVisibility(bool visible) const;
            void WriteWindowTitle(std::wstring_view title) const;
            void WriteAttributes(const TextAttribute& attributes) const;
            void WriteInfos(til::point target, std::span<const CHAR_INFO> infos) const;
            void WriteScreenInfo(SCREEN_INFORMATION& newContext, til::size oldSize) const;

        private:
            VtIo* _io = nullptr;
        };

        friend struct Writer;

        static void FormatAttributes(std::string& target, const TextAttribute& attributes);
        static void FormatAttributes(std::wstring& target, const TextAttribute& attributes);
        static wchar_t SanitizeUCS2(wchar_t ch);

        [[nodiscard]] HRESULT Initialize(const ConsoleArguments* const pArgs);
        bool IsUsingVt() const;
        [[nodiscard]] HRESULT StartIfNeeded();
        void Shutdown() noexcept;

        void SetDeviceAttributes(til::enumset<DeviceAttribute, uint64_t> attributes) noexcept;
        til::enumset<DeviceAttribute, uint64_t> GetDeviceAttributes() const noexcept;
        void SendCloseEvent();
        void CreatePseudoWindow();

    private:
        enum class State : uint8_t
        {
            Uninitialized,
            Initialized,
            Starting,
            StartupFailed,
            Running,
        };

        [[nodiscard]] HRESULT _Initialize(const HANDLE InHandle, const HANDLE OutHandle, _In_opt_ const HANDLE SignalHandle);

        void _uncork();
        void _flushNow();

        // After CreateIoHandlers is called, these will be invalid.
        wil::unique_hfile _hInput;
        wil::unique_hfile _hOutput;
        // After Initialize is called, this will be invalid.
        wil::unique_hfile _hSignal;

        std::unique_ptr<Microsoft::Console::VtInputThread> _pVtInputThread;
        std::unique_ptr<Microsoft::Console::PtySignalInputThread> _pPtySignalInputThread;
        til::enumset<DeviceAttribute, uint64_t> _deviceAttributes;

        // We use two buffers: A front and a back buffer. The front buffer is the one we're currently
        // sending to the terminal (it's being "presented" = it's on the "front" & "visible").
        // The back buffer is the one we're concurrently writing to.
        std::string _front;
        std::string _back;
        OVERLAPPED* _overlapped = nullptr;
        OVERLAPPED _overlappedBuf{};
        wil::unique_event _overlappedEvent;
        bool _overlappedPending = false;
        bool _writerRestoreCursor = false;
        bool _writerTainted = false;

        State _state = State::Uninitialized;
        bool _lookingForCursorPosition = false;
        bool _closeEventSent = false;
        int _corked = 0;

#ifdef UNIT_TESTING
        friend class VtIoTests;
#endif
    };
}
