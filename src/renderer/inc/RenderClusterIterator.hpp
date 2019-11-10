/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- RenderClusterIterator.hpp

Abstract:
- A Read-only iterator to extract cluster data for rendering while walking through text cells.
- This is done for performance reasons (avoid heap allocs and copies).

Author:
- Chester Liu (skyline75489) 10-Nov-2019

--*/

#pragma once

#include "../inc/Cluster.hpp"
#include "../../buffer/out/TextAttribute.hpp"

class TextBufferCellIterator;

namespace Microsoft::Console::Render
{
    class RenderClusterIterator
    {
    public:
        RenderClusterIterator(TextBufferCellIterator& cellIterator);

        operator bool() const noexcept;

        bool operator==(const RenderClusterIterator& it) const;
        bool operator!=(const RenderClusterIterator& it) const;

        RenderClusterIterator& operator+=(const ptrdiff_t& movement);
        RenderClusterIterator& operator-=(const ptrdiff_t& movement);
        RenderClusterIterator& operator++();
        RenderClusterIterator& operator--();
        RenderClusterIterator operator++(int);
        RenderClusterIterator operator--(int);
        RenderClusterIterator operator+(const ptrdiff_t& movement);
        RenderClusterIterator operator-(const ptrdiff_t& movement);

        const Cluster& operator*() const noexcept;
        const Cluster* operator->() const noexcept;

        ptrdiff_t GetClusterDistance(RenderClusterIterator other) const noexcept;

    protected:
        void _GenerateCluster();

        TextBufferCellIterator& _cellIt;
        Cluster _cluster;
        TextAttribute _attr;
        size_t _distance;
        bool _exceeded;
    };
}
