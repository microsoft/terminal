/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IControlAccessibilityInfo.h

Abstract:
- This serves as the interface defining all information known by the control
  hosting the terminal renderer that is needed for the UI Automation Tree.

Author(s):
- Zoey Riordan (zorio) Feb-2020
--*/

#pragma once

#include <wtypes.h>

namespace Microsoft::Console::Types
{
    class IControlAccessibilityInfo
    {
    public:
        virtual ~IControlAccessibilityInfo() = 0;

        virtual COORD GetFontSize() const = 0;
        virtual RECT GetBounds() const = 0;
        virtual RECT GetPadding() const = 0;
        virtual double GetScaleFactor() const = 0;
        virtual void ChangeViewport(const SMALL_RECT NewWindow) = 0;
        virtual HRESULT GetHostUiaProvider(IRawElementProviderSimple** provider) = 0;

    protected:
        IControlAccessibilityInfo() = default;
        IControlAccessibilityInfo(const IControlAccessibilityInfo&) = default;
        IControlAccessibilityInfo(IControlAccessibilityInfo&&) = default;
        IControlAccessibilityInfo& operator=(const IControlAccessibilityInfo&) = default;
        IControlAccessibilityInfo& operator=(IControlAccessibilityInfo&&) = default;
    };

    inline IControlAccessibilityInfo::~IControlAccessibilityInfo() {}
}