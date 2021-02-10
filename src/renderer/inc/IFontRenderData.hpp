/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IFontRenderData.hpp

Abstract:
- This serves as the interface defining all information needed to render a font.
--*/

#pragma once

namespace Microsoft::Console::Render
{
    template <class T>
    class IFontRenderData
    {
    public:
        [[nodiscard]] virtual T Regular() = 0;
        [[nodiscard]] virtual T Bold() = 0;
        [[nodiscard]] virtual T Italic() = 0;
    };
}
