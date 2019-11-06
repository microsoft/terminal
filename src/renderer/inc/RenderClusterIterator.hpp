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

    protected:
        void _GenerateCluster();

        Cluster _cluster;
        TextAttribute _attr;
        TextBufferCellIterator& _cellIter;
        bool _exceeded{};
    };
}
