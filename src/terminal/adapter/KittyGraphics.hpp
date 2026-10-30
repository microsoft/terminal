/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- KittyGraphics.hpp

Abstract:
- This class handles the kitty graphics protocol, which transmits images with
  APC sequences of the form ESC _ G <control data> ; <payload> ESC \.
--*/

#pragma once

#include "../../buffer/out/GraphicsImage.hpp"

#include <bitset>

class TextBuffer;

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDispatch;
    class Page;

    class KittyGraphics
    {
    public:
        using StringHandler = std::function<bool(const wchar_t)>;

        KittyGraphics(AdaptDispatch& dispatcher) noexcept;
        StringHandler Command();
        void Reset() noexcept;

    private:
        struct Control
        {
            std::array<int64_t, 128> values{};
            std::bitset<128> present;
            std::bitset<128> isChar;

            bool Has(char key) const noexcept;
            int64_t Get(char key, int64_t defaultValue = 0) const noexcept;
            uint32_t GetUnsigned(char key) const noexcept;
            int32_t GetSigned(char key) const noexcept;
            wchar_t GetChar(char key, wchar_t defaultValue) const noexcept;
        };

        struct Error
        {
            std::wstring_view code;
            std::wstring_view message;
        };

        enum class ParseState
        {
            Key,
            Value,
            Payload,
            Invalid
        };

        bool _parse(wchar_t ch);
        bool _parseControl(wchar_t ch) noexcept;
        bool _commitValue() noexcept;
        void _decodeBase64(wchar_t ch);
        void _execute();

        void _transmit(const Control& control, std::vector<uint8_t> data);
        void _put(const Control& control);
        void _delete(const Control& control);
        void _featureQuery(const Control& control) const;
        void _abortLoading() noexcept;

        std::shared_ptr<GraphicsImage> _load(const Control& control, std::vector<uint8_t> data, Error& error) const;
        bool _place(const Control& control, const std::shared_ptr<const GraphicsImage>& image, Error& error);
        void _placeVirtual(const Control& control, const std::shared_ptr<const GraphicsImage>& image);
        bool _erase(TextBuffer& buffer, const std::function<bool(const GraphicsRowRef&)>& predicate, bool freeImages);
        std::shared_ptr<const GraphicsImage> _findImage(const Control& control) const;
        GraphicsStore& _store() const;
        til::size _cellSize() const noexcept;

        void _respond(const Control& control, uint32_t id, const Error* error, std::wstring_view extraKeys = {}) const;

        AdaptDispatch& _dispatcher;

        ParseState _state = ParseState::Key;
        Control _control;
        char _key = 0;
        std::wstring _value;
        uint32_t _base64Accumulator = 0;
        int _base64Bits = 0;
        std::vector<uint8_t> _payload;
        bool _payloadTooLarge = false;

        bool _loading = false;
        Control _loadingControl;
        std::vector<uint8_t> _loadingData;
    };
}
