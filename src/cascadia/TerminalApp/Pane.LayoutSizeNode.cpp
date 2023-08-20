// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"

Pane::LayoutSizeNode::LayoutSizeNode(const float minSize) :
    size{ minSize },
    isMinimumSize{ true },
    firstChild{ nullptr },
    secondChild{ nullptr },
    nextFirstChild{ nullptr },
    nextSecondChild{ nullptr }
{
}

Pane::LayoutSizeNode::LayoutSizeNode(const LayoutSizeNode& other) :
    size{ other.size },
    isMinimumSize{ other.isMinimumSize },
    firstChild{ other.firstChild ? std::make_unique<LayoutSizeNode>(*other.firstChild) : nullptr },
    secondChild{ other.secondChild ? std::make_unique<LayoutSizeNode>(*other.secondChild) : nullptr },
    nextFirstChild{ other.nextFirstChild ? std::make_unique<LayoutSizeNode>(*other.nextFirstChild) : nullptr },
    nextSecondChild{ other.nextSecondChild ? std::make_unique<LayoutSizeNode>(*other.nextSecondChild) : nullptr }
{
}

// Method Description:
// - Makes sure that this node and all its descendants equal the supplied node.
//   This may be more efficient that copy construction since it will reuse its
//   allocated children.
// Arguments:
// - other: Node to take the values from.
// Return Value:
// - itself
Pane::LayoutSizeNode& Pane::LayoutSizeNode::operator=(const LayoutSizeNode& other)
{
    size = other.size;
    isMinimumSize = other.isMinimumSize;

    firstChild = other.firstChild ? std::make_unique<LayoutSizeNode>(*other.firstChild) : nullptr;
    secondChild = other.secondChild ? std::make_unique<LayoutSizeNode>(*other.secondChild) : nullptr;
    nextFirstChild = other.nextFirstChild ? std::make_unique<LayoutSizeNode>(*other.nextFirstChild) : nullptr;
    nextSecondChild = other.nextSecondChild ? std::make_unique<LayoutSizeNode>(*other.nextSecondChild) : nullptr;

    return *this;
}
