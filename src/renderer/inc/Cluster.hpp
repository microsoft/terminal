/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Cluster.hpp

Abstract:
- This serves as a structure to represent a single glyph cluster drawn on the screen
- This is required to enable N wchar_ts to consume M columns in the display
- Historically, the console only supported 1 wchar_t = 1 column or 1 wchar_t = 2 columns.

Author(s):
- Michael Niksa (MiNiksa) 25-Mar-2019
--*/

#pragma once

namespace Microsoft::Console::Render
{
    class Cluster
    {
    public:
        Cluster(const std::wstring_view text, const size_t columns);

        const wchar_t GetTextAsSingle() const noexcept;

        const std::wstring_view& GetText() const noexcept;

        const size_t GetColumns() const noexcept;

    private:
        // This is the UTF-16 string of characters that form a particular drawing cluster
        const std::wstring_view _text;

        // This is how many columns we're expecting this cluster to take in the display grid
        const size_t _columns;
    };
}
