/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IUiaData.hpp

Abstract:
- This serves as the interface defining all information needed for the UI Automation Tree

Author(s):
- Carlos Zamora (CaZamor) Aug-2019
--*/

#pragma once

#include "IBaseData.h"

struct ITextRangeProvider;

namespace Microsoft::Console::Types
{
    class IUiaData : public IBaseData
    {
    public:
        ~IUiaData() = 0;

    protected:
        IUiaData() = default;
        IUiaData(const IUiaData&) = default;
        IUiaData(IUiaData&&) = default;
        IUiaData& operator=(const IUiaData&) = default;
        IUiaData& operator=(IUiaData&&) = default;

    public:
        virtual const bool IsSelectionActive() const = 0;
        virtual void ClearSelection() = 0;
        virtual void SelectNewRegion(const COORD coordStart, const COORD coordEnd) = 0;
        virtual const COORD GetSelectionAnchor() const = 0;
        virtual void ColorSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd, const TextAttribute attr) = 0;
    };

    // See docs/virtual-dtors.md for an explanation of why this is weird.
    inline IUiaData::~IUiaData() {}
}
