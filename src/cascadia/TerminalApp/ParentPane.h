// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"
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

        void UpdateSettings(const winrt::TerminalApp::TerminalSettings& settings,
                            const GUID& profile);
        void ResizeContent(const winrt::Windows::Foundation::Size& newSize);
        void Relayout();
        IPane GetActivePane();
        bool ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);
        bool NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction);
        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
        int GetLeafPaneCount() const noexcept;
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
        SnapSizeResult _CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        TYPED_EVENT(PaneTypeChanged, IPane, IPane);

    private:
        struct SnapChildrenSizeResult;
        struct LayoutSizeNode;

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
        winrt::Microsoft::Terminal::TerminalControl::TermControl::LayoutUpdated_revoker _firstLayoutRevoker;
        winrt::Microsoft::Terminal::TerminalControl::TermControl::LayoutUpdated_revoker _secondLayoutRevoker;

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

        std::function<void(winrt::Windows::UI::Xaml::FrameworkElement const&, int32_t)> _GetGridSetColOrRowFunc() const noexcept;

        std::pair<float, float> _CalcChildrenSizes(const float fullSize) const;
        SnapChildrenSizeResult _CalcSnappedChildrenSizes(const bool widthOrHeight, const float fullSize) const;
        LayoutSizeNode _CreateMinSizeTree(const bool widthOrHeight) const;
        float _ClampSplitPosition(const bool widthOrHeight, const float requestedValue, const float totalSize) const;
        void AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const;

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
