// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - ParentPane.h
//
// Abstract:
// - Panes are an abstraction by which the terminal can display multiple terminal
//   instances simultaneously in a single terminal window. While tabs allow for
//   a single terminal window to have many terminal sessions running
//   simultaneously within a single window, only one tab can be visible at a
//   time. Panes, on the other hand, allow a user to have many different
//   terminal sessions visible to the user within the context of a single window
//   at the same time. This can enable greater productivity from the user, as
//   they can see the output of one terminal window while working in another.
// - See doc/cascadia/Panes.md for a detailed description.
// - Panes can be one of 2 types, parent or leaf. A parent pane contains 2 other panes
//   (each of which could itself be a parent or could be a leaf). A leaf pane contains
//   a terminal control.
//
// Author(s):
// - Mike Griese (zadjii-msft) 16-May-2019
// - Pankaj Bhojwani February-2021

#pragma once

#include "inc/cppwinrt_utils.h"
#include "LeafPane.h"

#include "ParentPane.g.h"

namespace winrt::TerminalApp::implementation
{
    struct ParentPane : ParentPaneT<ParentPane>
    {
    public:
        ParentPane(TerminalApp::LeafPane firstChild, TerminalApp::LeafPane secondChild, winrt::Microsoft::Terminal::Settings::Model::SplitState splitState, float splitPosition);
        winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();
        void FocusPane(uint32_t id);
        void FocusFirstChild();
        bool HasFocusedChild();

        bool ContainsReadOnly();

        void InitializeChildren();

        void Shutdown();
        void ClearActive();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult& settings,
                            const GUID& profile);
        void ResizeContent(const winrt::Windows::Foundation::Size& newSize);
        void Relayout();
        IPane GetActivePane() const;
        bool ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);
        bool NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction);
        float CalcSnappedDimensionSingle(const bool widthOrHeight, const float dimension) const;
        uint32_t GetLeafPaneCount() const noexcept;
        winrt::Windows::Foundation::Size GetMinSize() const;

        void Maximize(IPane paneToZoom);
        void Restore(IPane paneToUnzoom);

        winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Settings::Model::SplitState> PreCalculateAutoSplit(const IPane target,
                                                                                                                              const winrt::Windows::Foundation::Size parentSize) const;
        winrt::Windows::Foundation::IReference<bool> PreCalculateCanSplit(const IPane target,
                                                                          winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                                                          const float splitSize,
                                                                          const winrt::Windows::Foundation::Size availableSpace) const;

        IPane FindFirstLeaf();

        void PropagateToLeavesOnEdge(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& edge, std::function<void(TerminalApp::LeafPane)> action);
        SnapSizeResult CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        TYPED_EVENT(PaneTypeChanged, IPane, IPane);

    private:
        struct SnapChildrenSizeResult;
        struct LayoutSizeNode;
        static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_unfocusedBorderBrush;

        IPane _firstChild;
        IPane _secondChild;
        winrt::Microsoft::Terminal::Settings::Model::SplitState _splitState{ winrt::Microsoft::Terminal::Settings::Model::SplitState::None };
        float _desiredSplitPosition;
        winrt::event_token _firstClosedToken{ 0 };
        winrt::event_token _secondClosedToken{ 0 };
        winrt::event_token _firstTypeChangedToken{ 0 };
        winrt::event_token _secondTypeChangedToken{ 0 };

        bool _firstLayoutUpdated{ false };
        bool _secondLayoutUpdated{ false };
        winrt::Microsoft::Terminal::Control::TermControl::LayoutUpdated_revoker _firstLayoutRevoker;
        winrt::Microsoft::Terminal::Control::TermControl::LayoutUpdated_revoker _secondLayoutRevoker;

        void _CreateRowColDefinitions();
        bool _Resize(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);
        bool _NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction);

        void _ChildrenLayoutUpdatedHelper(const bool isFirstChild);
        void _SetupEntranceAnimation();
        void _CloseChild(const bool closeFirst);
        winrt::fire_and_forget _CloseChildRoutine(const bool closeFirst);

        void _SetupChildEventHandlers(const bool isFirstChild);
        void _RemoveAllChildEventHandlers(const bool isFirstChild);

        void _OnChildSplitOrCollapse(const bool isFirstChild, IPane newChild);

        void _GridLayoutHelper() const noexcept;

        std::pair<float, float> _CalcChildrenSizes(const float fullSize) const;
        SnapChildrenSizeResult _CalcSnappedChildrenSizes(const bool widthOrHeight, const float fullSize) const;
        LayoutSizeNode _CreateMinSizeTree(const bool widthOrHeight) const;
        float _ClampSplitPosition(const bool widthOrHeight, const float requestedValue, const float totalSize) const;
        void _AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const;

        void _SetupResources();

        struct SnapChildrenSizeResult
        {
            std::pair<float, float> lower;
            std::pair<float, float> higher;
        };

        // Helper structure that builds a (roughly) binary tree corresponding
        // to the pane tree. Used for laying out panes with snapped sizes.
        struct LayoutSizeNode
        {
            float size;
            bool isMinimumSize;
            std::unique_ptr<LayoutSizeNode> firstChild;
            std::unique_ptr<LayoutSizeNode> secondChild;

            // These two fields hold next possible snapped values of firstChild and
            // secondChild. Although that could be calculated from these fields themselves,
            // it would be wasteful as we have to know these values more often than for
            // simple increment. Hence we cache that here.
            std::unique_ptr<LayoutSizeNode> nextFirstChild;
            std::unique_ptr<LayoutSizeNode> nextSecondChild;

            explicit LayoutSizeNode(const float minSize);
            LayoutSizeNode(const LayoutSizeNode& other);

            LayoutSizeNode& operator=(const LayoutSizeNode& other);

        private:
            void _AssignChildNode(std::unique_ptr<LayoutSizeNode>& nodeField, const LayoutSizeNode* const newNode);
        };

        // Function Description:
        // - Returns true if the given direction can be used with the given split
        //   type.
        // - This is used for pane resizing (which will need a pane separator
        //   that's perpendicular to the direction to be able to move the separator
        //   in that direction).
        // - Also used for moving focus between panes, which again happens _across_ a separator.
        // Arguments:
        // - direction: The Direction to compare
        // - splitType: The winrt::TerminalApp::SplitState to compare
        // Return Value:
        // - true iff the direction is perpendicular to the splitType. False for
        //   winrt::TerminalApp::SplitState::None.
        template<typename T>
        static constexpr bool DirectionMatchesSplit(const T& direction,
                                                    const winrt::Microsoft::Terminal::Settings::Model::SplitState& splitType)
        {
            if (splitType == winrt::Microsoft::Terminal::Settings::Model::SplitState::None)
            {
                return false;
            }
            else if (splitType == winrt::Microsoft::Terminal::Settings::Model::SplitState::Horizontal)
            {
                return direction == T::Up ||
                       direction == T::Down;
            }
            else if (splitType == winrt::Microsoft::Terminal::Settings::Model::SplitState::Vertical)
            {
                return direction == T::Left ||
                       direction == T::Right;
            }
            return false;
        }
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ParentPane);
}
