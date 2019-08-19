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
        virtual ~IUiaData() = 0;

        virtual const bool IsSelectionActive() const = 0;
        virtual void ClearSelection() = 0;
        virtual void SelectNewRegion(const COORD coordStart, const COORD coordEnd) = 0;

        // TODO GitHub #605: Search functionality
        // For now, just adding it here to make UiaTextRange easier to create (Accessibility)
        // We should actually abstract this out better once Windows Terminal has Search
        virtual HRESULT SearchForText(_In_ BSTR text,
                                      _In_ BOOL searchBackward,
                                      _In_ BOOL ignoreCase,
                                      _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal,
                                      unsigned int _start,
                                      unsigned int _end,
                                      std::function<unsigned int(IUiaData*, const COORD)> _coordToEndpoint,
                                      std::function<COORD(IUiaData*, const unsigned int)> _endpointToCoord,
                                      std::function<IFACEMETHODIMP(ITextRangeProvider**)> Clone) = 0;
    };

    // See docs/virtual-dtors.md for an explanation of why this is weird.
    inline IUiaData::~IUiaData() {}
}
