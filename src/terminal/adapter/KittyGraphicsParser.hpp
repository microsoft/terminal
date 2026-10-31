/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- KittyGraphicsParser.hpp

Abstract:
- Parses Kitty Graphics Protocol commands, which arrive as APC sequences
  ("ESC _ G <key=value,...> ; <base64 payload> ESC \"), and applies them to
  the active buffer's KittyImages store. Modeled on SixelParser: driven by
  a per-character handler returned to the state machine, with the sequence
  ended by a synthetic ESC.
- Supported: transmission of RGB, RGBA and PNG data over the direct
  medium (t=d), chunked transmission (m=1/m=0), client and terminal
  assigned image ids (i=/I=), direct placements with source-rectangle
  cropping and cell-rectangle scaling (x,y,w,h,c,r,X,Y,z), cursor movement
  policies (C=), Unicode placeholder virtual placements (U=1), deletion
  (d=), quiet responses (q=), and query actions (a=q) including feature
  queries (e=).
- Not supported, answered with ENOSYS when asked: file and shared memory
  transmission (t=f/t/s), compression (o=z), indexed color formats (f=8,
  f=16), animation (a=f/a/c), and relative placements (P=/Q=/H=/V=).
--*/

#pragma once

#include "til.h"
#include "DispatchTypes.hpp"

#include <optional>
#include <string>

class KittyImages;

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDispatch;
    class StateMachine;

    class KittyGraphicsParser
    {
    public:
        using StringHandler = std::function<bool(wchar_t)>;

        KittyGraphicsParser(AdaptDispatch& dispatcher, const StateMachine& stateMachine) noexcept;

        // Returns the character handler for one APC sequence.
        StringHandler BeginSequence();

    private:
        // The parsed command, mirroring the control keys of the protocol.
        // A nullopt member means the key was absent.
        struct Command
        {
            wchar_t action = 0; // a= (0 = absent, e.g. on continuation chunks)
            wchar_t deleteAction = 0; // d=
            wchar_t transmissionType = L'd'; // t=
            wchar_t compressed = 0; // o=
            wchar_t reportSupported = 0; // e=
            std::optional<uint32_t> format; // f=
            std::optional<uint32_t> more; // m=
            std::optional<uint32_t> id; // i=
            std::optional<uint32_t> imageNumber; // I=
            std::optional<uint32_t> placementId; // p=
            std::optional<uint32_t> quiet; // q=
            std::optional<uint32_t> width; // w=
            std::optional<uint32_t> height; // h=
            std::optional<uint32_t> xOffset; // x=
            std::optional<uint32_t> yOffset; // y=
            std::optional<uint32_t> dataHeight; // v=
            std::optional<uint32_t> dataWidth; // s=
            std::optional<uint32_t> numCells; // c=
            std::optional<uint32_t> numLines; // r=
            std::optional<uint32_t> cellXOffset; // X=
            std::optional<uint32_t> cellYOffset; // Y=
            std::optional<int32_t> zIndex; // z=
            std::optional<uint32_t> cursorMovement; // C=
            std::optional<uint32_t> unicodePlacement; // U=
            std::optional<uint32_t> usageHints; // N=
            std::string payloadBase64;
        };

        bool _handleChar(wchar_t ch);
        void _finalizeCommand();
        // Folds the current key=value pair into the command. Returns false
        // (and rejects the sequence) if the value does not fit.
        bool _foldKeyValue();
        void _applyCommand();

        void _transmit();
        void _finishTransmission();
        void _place();
        void _placeVirtual(const Command& cmd);
        bool _placeDirect(const Command& cmd, uint32_t imageId);
        void _query();
        void _queryFeatures();
        void _delete();

        void _respond(bool ok, const wchar_t* message = L"OK") const;
        std::wstring _responsePrefix() const;
        bool _quietSuppress(bool ok) const noexcept;
        const wchar_t* _validateTransmit();
        KittyImages& _images() const;
        uint32_t _assignImageId() const;

        AdaptDispatch& _dispatcher;
        const StateMachine& _stateMachine;

        enum class ParseState
        {
            ExpectG,
            Key,
            Equal,
            Value,
            FlagValue,
            Payload,
            Rejected
        };
        ParseState _state = ParseState::ExpectG;
        wchar_t _currentKey = 0;
        bool _currentKeyIsNegative = false;
        std::wstring _currentValue;
        Command _cmd;

        // A multi-chunk transmission accumulates here. Only the m and q
        // keys are meaningful on continuation chunks; the rest of the
        // command comes from the first chunk.
        bool _pendingValid = false;
        Command _pending;
        size_t _pendingBase64Bytes = 0;

        // The id the terminal assigned for this command, when the client
        // did not supply one. Echoed in responses.
        std::optional<uint32_t> _assignedId;
    };
}
