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
        MouseInput(const WriteInputEvents pfnWriteEvents);
        ~MouseInput();

        bool HandleMouse(const COORD coordMousePosition,
                         const unsigned int uiButton,
                         const short sModifierKeystate,
                         const short sWheelDelta);

        void SetUtf8ExtendedMode(const bool fEnable);
        void SetSGRExtendedMode(const bool fEnable);

        void EnableDefaultTracking(const bool fEnable);
        void EnableButtonEventTracking(const bool fEnable);
        void EnableAnyEventTracking(const bool fEnable);

        void EnableAlternateScroll(const bool fEnable);
        void UseAlternateScreenBuffer();
        void UseMainScreenBuffer();

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

        ExtendedMode _ExtendedMode = ExtendedMode::None;
        TrackingMode _TrackingMode = TrackingMode::None;

        bool _fAlternateScroll = false;
        bool _fInAlternateBuffer = false;

        COORD _coordLastPos;
        unsigned int _lastButton;

        void _SendInputSequence(_In_reads_(cchLength) const wchar_t* const pwszSequence, const size_t cchLength) const;
        bool _GenerateDefaultSequence(const COORD coordMousePosition,
                                      const unsigned int uiButton,
                                      const bool fIsHover,
                                      const short sModifierKeystate,
                                      const short sWheelDelta,
                                      _Outptr_result_buffer_(*pcchLength) wchar_t** const ppwchSequence,
                                      _Out_ size_t* const pcchLength) const;
        bool _GenerateUtf8Sequence(const COORD coordMousePosition,
                                   const unsigned int uiButton,
                                   const bool fIsHover,
                                   const short sModifierKeystate,
                                   const short sWheelDelta,
                                   _Outptr_result_buffer_(*pcchLength) wchar_t** const ppwchSequence,
                                   _Out_ size_t* const pcchLength) const;
        bool _GenerateSGRSequence(const COORD coordMousePosition,
                                  const unsigned int uiButton,
                                  const bool isDown,
                                  const bool fIsHover,
                                  const short sModifierKeystate,
                                  const short sWheelDelta,
                                  _Outptr_result_buffer_(*pcchLength) wchar_t** const ppwchSequence,
                                  _Out_ size_t* const pcchLength) const;

        bool _ShouldSendAlternateScroll(_In_ unsigned int uiButton, _In_ short sScrollDelta) const;
        bool _SendAlternateScroll(_In_ short sScrollDelta) const;

        static int s_WindowsButtonToXEncoding(const unsigned int uiButton,
                                              const bool fIsHover,
                                              const short sModifierKeystate,
                                              const short sWheelDelta);

        static int s_WindowsButtonToSGREncoding(const unsigned int uiButton,
                                                const bool fIsHover,
                                                const short sModifierKeystate,
                                                const short sWheelDelta);

        static bool s_IsButtonDown(const unsigned int uiButton);
        static bool s_IsButtonMsg(const unsigned int uiButton);
        static bool s_IsHoverMsg(const unsigned int uiButton);
        static COORD s_WinToVTCoord(const COORD coordWinCoordinate);
        static short s_EncodeDefaultCoordinate(const short sCoordinateValue);
        static unsigned int s_GetPressedButton();
    };
}
