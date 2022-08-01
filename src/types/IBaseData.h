/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IUiaData.hpp

Abstract:
- This serves as the interface defining all information needed for the UI Automation Tree and the Renderer

Author(s):
- Carlos Zamora (CaZamor) Aug-2019
--*/

#pragma once

#include <til/ticket_lock.h>
#include "../renderer/inc/FontInfo.hpp"
#include "inc/viewport.hpp"

class TextBuffer;

namespace Microsoft::Console::Types
{
    class IBaseData
    {
    public:
        virtual ~IBaseData() = 0;

    protected:
        IBaseData() = default;
        IBaseData(const IBaseData&) = default;
        IBaseData(IBaseData&&) = default;
        IBaseData& operator=(const IBaseData&) = default;
        IBaseData& operator=(IBaseData&&) = default;

    public:
        virtual Microsoft::Console::Types::Viewport GetViewport() noexcept = 0;
        virtual til::point GetTextBufferEndPosition() const noexcept = 0;
        virtual const TextBuffer& GetTextBuffer() const noexcept = 0;
        virtual const FontInfo& GetFontInfo() const noexcept = 0;

        virtual std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept = 0;

        virtual [[nodiscard]] std::unique_lock<til::ticket_lock> LockConsole() noexcept = 0;
    };

    // See docs/virtual-dtors.md for an explanation of why this is weird.
    inline IBaseData::~IBaseData() {}
}
