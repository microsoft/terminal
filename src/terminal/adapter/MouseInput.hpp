/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- MouseInput.hpp

Abstract:
- This serves as an adapter between mouse input from a user and the virtual terminal sequences that are
  typically emitted by an xterm-compatible console.

Author(s):
- Mike Griese (migrie) 01-Aug-2016
--*/
#pragma once

#include "../../types/inc/IInputEvent.hpp"

#include <deque>
#include <memory>

namespace Microsoft::Console::VirtualTerminal
{
    typedef void (*WriteInputEvents)(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& events);

    class MouseInput sealed
    {
    public:
        MouseInput(const WriteInputEvents pfnWriteEvents) noexcept;

        bool HandleMouse(const COORD position,
                         const unsigned int button,
                         const short modifierKeyState,
                         const short delta);

        void SetUtf8ExtendedMode(const bool enable) noexcept;
        void SetSGRExtendedMode(const bool enable) noexcept;

        void EnableDefaultTracking(const bool enable) noexcept;
        void EnableButtonEventTracking(const bool enable) noexcept;
        void EnableAnyEventTracking(const bool enable) noexcept;

        void EnableAlternateScroll(const bool enable) noexcept;
        void UseAlternateScreenBuffer() noexcept;
        void UseMainScreenBuffer() noexcept;

        enum class ExtendedMode : unsigned int
        {
            None,
            Utf8,
            Sgr,
            Urxvt
        };

        enum class TrackingMode : unsigned int
        {
            None,
            Default,
            ButtonEvent,
            AnyEvent
        };

    private:
        static const int s_MaxDefaultCoordinate = 94;

        WriteInputEvents _pfnWriteEvents;

        ExtendedMode _extendedMode = ExtendedMode::None;
        TrackingMode _trackingMode = TrackingMode::None;

        bool _alternateScroll = false;
        bool _inAlternateBuffer = false;

        COORD _lastPos;
        unsigned int _lastButton;

        void _SendInputSequence(const std::wstring_view sequence) const noexcept;
        static std::wstring _GenerateDefaultSequence(const COORD position,
                                                     const unsigned int button,
                                                     const bool isHover,
                                                     const short modifierKeyState,
                                                     const short delta);
        static std::wstring _GenerateUtf8Sequence(const COORD position,
                                                  const unsigned int button,
                                                  const bool isHover,
                                                  const short modifierKeyState,
                                                  const short delta);
        static std::wstring _GenerateSGRSequence(const COORD position,
                                                 const unsigned int button,
                                                 const bool isDown,
                                                 const bool isHover,
                                                 const short modifierKeyState,
                                                 const short delta);

        bool _ShouldSendAlternateScroll(const unsigned int button, const short delta) const noexcept;
        bool _SendAlternateScroll(const short delta) const noexcept;

        static unsigned int s_GetPressedButton() noexcept;
    };
}
