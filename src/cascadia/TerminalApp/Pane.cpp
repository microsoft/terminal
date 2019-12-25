// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"
#include "Profile.h"
#include "CascadiaSettings.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::TerminalApp;
using namespace TerminalApp;

Pane::Pane()
{
}

// Method Description:
// - Get the root UIElement of this pane. There may be a single TermControl as a
//   child, or an entire tree of grids and panes as children of this element.
// Arguments:
// - <none>
// Return Value:
// - the Grid acting as the root of this pane.
Controls::Grid Pane::GetRootElement() const
{
    return _root;
}

// Method Description:
// - Adjusts given dimension (width or height) so that all descendant terminals
//   align with their character grids as close as possible. Snaps to closes match
//   (either upward or downward). Also makes sure to fit in minimal sizes of the panes.
// Arguments:
// - widthOrHeight: if true operates on width, otherwise on height
// - dimension: a dimension (width or height) to snap
// Return Value:
// - A value corresponding to the next closest snap size for this Pane, either upward or downward
float Pane::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
{
    const auto [lower, higher] = _CalcSnappedDimension(widthOrHeight, dimension);
    return dimension - lower < higher - dimension ? lower : higher;
}

// Method Description:
// - Builds a tree of LayoutSizeNode that matches the tree of panes. Each node
//   has minimum size that the corresponding pane can have.
// Arguments:
// - widthOrHeight: if true operates on width, otherwise on height
// Return Value:
// - Root node of built tree that matches this pane.
Pane::LayoutSizeNode Pane::_CreateMinSizeTree(const bool widthOrHeight) const
{
    const auto size = _GetMinSize();
    return LayoutSizeNode(widthOrHeight ? size.Width : size.Height);
}
