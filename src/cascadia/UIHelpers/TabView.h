// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "pch.h"
#include "common.h"

#include "TabView.g.h"
#include "TabView.properties.h"
#include "TabViewTabCloseRequestedEventArgs.g.h"
#include "TabViewTabDroppedOutsideEventArgs.g.h"
#include "TabViewTabDragStartingEventArgs.g.h"
#include "TabViewTabDragCompletedEventArgs.g.h"
#include "TabViewTrace.h"
#include "DispatcherHelper.h"

static constexpr double c_tabShadowDepth = 16.0;
static constexpr wstring_view c_tabViewShadowDepthName{ L"TabViewShadowDepth"sv };

class TabViewTabCloseRequestedEventArgs :
    public winrt::implementation::TabViewTabCloseRequestedEventArgsT<TabViewTabCloseRequestedEventArgs>
{
public:
    TabViewTabCloseRequestedEventArgs(winrt::IInspectable const& item, winrt::TabViewItem tab) :
        m_item(item), m_tab(tab) {}

    winrt::IInspectable Item() { return m_item; }
    winrt::TabViewItem Tab() { return m_tab; }

private:
    winrt::IInspectable m_item{};
    winrt::TabViewItem m_tab{};
};

class TabViewTabDroppedOutsideEventArgs :
    public winrt::implementation::TabViewTabDroppedOutsideEventArgsT<TabViewTabDroppedOutsideEventArgs>
{
public:
    TabViewTabDroppedOutsideEventArgs(winrt::IInspectable const& item, winrt::TabViewItem tab) :
        m_item(item), m_tab(tab) {}

    winrt::IInspectable Item() { return m_item; }
    winrt::TabViewItem Tab() { return m_tab; }

private:
    winrt::IInspectable m_item{};
    winrt::TabViewItem m_tab{};
};

class TabViewTabDragStartingEventArgs :
    public winrt::implementation::TabViewTabDragStartingEventArgsT<TabViewTabDragStartingEventArgs>
{
public:
    TabViewTabDragStartingEventArgs(winrt::DragItemsStartingEventArgs const& args, winrt::IInspectable const& item, winrt::TabViewItem tab) :
        m_args(args), m_item(item), m_tab(tab) {}

    bool Cancel() { return m_args.Cancel(); }
    void Cancel(bool value) { m_args.Cancel(value); }
    winrt::DataPackage Data() { return m_args.Data(); }
    winrt::IInspectable Item() { return m_item; }
    winrt::TabViewItem Tab() { return m_tab; }

private:
    winrt::DragItemsStartingEventArgs m_args{};
    winrt::IInspectable m_item{};
    winrt::TabViewItem m_tab{};
};

class TabViewTabDragCompletedEventArgs :
    public winrt::implementation::TabViewTabDragCompletedEventArgsT<TabViewTabDragCompletedEventArgs>
{
public:
    TabViewTabDragCompletedEventArgs(winrt::DragItemsCompletedEventArgs const& args, winrt::IInspectable const& item, winrt::TabViewItem tab) :
        m_args(args), m_item(item), m_tab(tab) {}

    winrt::DataPackageOperation DropResult() { return m_args.DropResult(); }
    winrt::IInspectable Item() { return m_item; }
    winrt::TabViewItem Tab() { return m_tab; }

private:
    winrt::DragItemsCompletedEventArgs m_args{ nullptr };
    winrt::IInspectable m_item{};
    winrt::TabViewItem m_tab{};
};

class TabView :
    public ReferenceTracker<TabView, winrt::implementation::TabViewT>,
    public TabViewProperties
{
public:
    TabView();
    ~TabView();

    // IFrameworkElement
    void OnApplyTemplate();
    winrt::Size MeasureOverride(winrt::Size const& availableSize);

    // IUIElement
    winrt::AutomationPeer OnCreateAutomationPeer();

    // From ListView
    winrt::DependencyObject ContainerFromItem(winrt::IInspectable const& item);
    winrt::DependencyObject ContainerFromIndex(int index);
    int IndexFromContainer(winrt::DependencyObject const& container);
    winrt::IInspectable ItemFromContainer(winrt::DependencyObject const& container);

    // Control
    void OnKeyDown(winrt::KeyRoutedEventArgs const& e);

    // Internal
    void OnCloseButtonOverlayModePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnTabItemsSourcePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnTabWidthModePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnSelectedIndexPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnSelectedItemPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnTabStripFooterPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);

    void OnItemsChanged(winrt::IInspectable const& item);
    void UpdateTabContent();

    void RequestCloseTab(winrt::TabViewItem const& item, bool updateTabWidths);

    winrt::UIElement GetShadowReceiver() { return m_shadowReceiver.get(); }

    winrt::hstring GetTabCloseButtonTooltipText() { return m_tabCloseButtonTooltipText; }
    void SetTabSeparatorOpacity(int index, int opacityValue);
    void SetTabSeparatorOpacity(int index);

    bool MoveFocus(bool moveForward);

private:
    void OnLoaded(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args);
    void OnScrollViewerLoaded(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args);
    void OnAddButtonClick(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args);
    void OnScrollDecreaseClick(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args);
    void OnScrollIncreaseClick(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args);
    void OnScrollViewerViewChanged(winrt::IInspectable const& sender, winrt::ScrollViewerViewChangedEventArgs const& args);
    void OnItemsPresenterSizeChanged(const winrt::IInspectable& sender, const winrt::SizeChangedEventArgs& args);

    void OnFooterSizeChanged(const winrt::DependencyObject& /*sender*/, const winrt::DependencyProperty& args);

    void OnListViewLoaded(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args);
    void OnTabStripPointerExited(const winrt::IInspectable& sender, const winrt::PointerRoutedEventArgs& args);
    void OnTabStripPointerEntered(const winrt::IInspectable& sender, const winrt::PointerRoutedEventArgs& args);
    void OnListViewSelectionChanged(const winrt::IInspectable& sender, const winrt::SelectionChangedEventArgs& args);

    void OnListViewDragItemsStarting(const winrt::IInspectable& sender, const winrt::DragItemsStartingEventArgs& args);
    void OnListViewDragItemsCompleted(const winrt::IInspectable& sender, const winrt::DragItemsCompletedEventArgs& args);
    void OnListViewDragOver(const winrt::IInspectable& sender, const winrt::DragEventArgs& args);
    void OnListViewDrop(const winrt::IInspectable& sender, const winrt::DragEventArgs& args);

    void OnCtrlF4Invoked(const winrt::KeyboardAccelerator& sender, const winrt::KeyboardAcceleratorInvokedEventArgs& args);
    void OnCtrlTabInvoked(const winrt::KeyboardAccelerator& sender, const winrt::KeyboardAcceleratorInvokedEventArgs& args);
    void OnCtrlShiftTabInvoked(const winrt::KeyboardAccelerator& sender, const winrt::KeyboardAcceleratorInvokedEventArgs& args);

    void OnAddButtonKeyDown(const winrt::IInspectable& sender, winrt::KeyRoutedEventArgs const& args);

    bool RequestCloseCurrentTab();
    bool MoveSelection(bool moveForward);
    void BringSelectedTabIntoView();

    void UpdateSelectedItem();
    void UpdateSelectedIndex();

    void UpdateTabWidths(bool shouldUpdateWidths = true, bool fillAllAvailableSpace = true);

    void UpdateScrollViewerDecreaseAndIncreaseButtonsViewState();
    void UpdateListViewItemContainerTransitions();

    void UnhookEventsAndClearFields();

    void OnListViewDraggingPropertyChanged(const winrt::DependencyObject& sender, const winrt::DependencyProperty& args);
    void OnListViewGettingFocus(const winrt::IInspectable& sender, const winrt::GettingFocusEventArgs& args);

    int GetItemCount();

    void UpdateBottomBorderLineVisualStates();
    void UpdateTabBottomBorderLineVisualStates();

    winrt::TabViewItem FindTabViewItemFromDragItem(const winrt::IInspectable& item);

    static bool IsFocusable(winrt::DependencyObject const& object, bool checkTabStop = false);

    bool m_updateTabWidthOnPointerLeave{ false };
    bool m_pointerInTabstrip{ false };

    tracker_ref<winrt::ColumnDefinition> m_leftContentColumn{ this };
    tracker_ref<winrt::ColumnDefinition> m_tabColumn{ this };
    tracker_ref<winrt::ColumnDefinition> m_addButtonColumn{ this };
    tracker_ref<winrt::ColumnDefinition> m_rightContentColumn{ this };

    tracker_ref<winrt::ListView> m_listView{ this };
    tracker_ref<winrt::ContentPresenter> m_tabContentPresenter{ this };
    tracker_ref<winrt::ContentPresenter> m_rightContentPresenter{ this };
    tracker_ref<winrt::Grid> m_tabContainerGrid{ this };
    tracker_ref<winrt::FxScrollViewer> m_scrollViewer{ this };
    tracker_ref<winrt::RepeatButton> m_scrollDecreaseButton{ this };
    tracker_ref<winrt::RepeatButton> m_scrollIncreaseButton{ this };
    tracker_ref<winrt::Button> m_addButton{ this };
    tracker_ref<winrt::ItemsPresenter> m_itemsPresenter{ this };

    tracker_ref<winrt::Grid> m_shadowReceiver{ this };

    PropertyChanged_revoker m_footerMinWidthProperyChangedRevoker{};
    PropertyChanged_revoker m_footerWidthProperyChangedRevoker{};

    winrt::ListView::Loaded_revoker m_listViewLoadedRevoker{};
    winrt::ListView::PointerExited_revoker m_tabStripPointerExitedRevoker{};
    winrt::ListView::PointerEntered_revoker m_tabStripPointerEnteredRevoker{};
    winrt::Selector::SelectionChanged_revoker m_listViewSelectionChangedRevoker{};
    winrt::UIElement::GettingFocus_revoker m_listViewGettingFocusRevoker{};

    PropertyChanged_revoker m_listViewCanReorderItemsPropertyChangedRevoker{};
    PropertyChanged_revoker m_listViewAllowDropPropertyChangedRevoker{};

    winrt::ListView::DragItemsStarting_revoker m_listViewDragItemsStartingRevoker{};
    winrt::ListView::DragItemsCompleted_revoker m_listViewDragItemsCompletedRevoker{};
    winrt::UIElement::DragOver_revoker m_listViewDragOverRevoker{};
    winrt::UIElement::Drop_revoker m_listViewDropRevoker{};

    winrt::FxScrollViewer::Loaded_revoker m_scrollViewerLoadedRevoker{};
    winrt::FxScrollViewer::ViewChanged_revoker m_scrollViewerViewChangedRevoker{};

    winrt::Button::Click_revoker m_addButtonClickRevoker{};

    winrt::RepeatButton::Click_revoker m_scrollDecreaseClickRevoker{};
    winrt::RepeatButton::Click_revoker m_scrollIncreaseClickRevoker{};

    winrt::Button::KeyDown_revoker m_addButtonKeyDownRevoker{};

    winrt::ItemsPresenter::SizeChanged_revoker m_itemsPresenterSizeChangedRevoker{};

    DispatcherHelper m_dispatcherHelper{ *this };

    winrt::hstring m_tabCloseButtonTooltipText{};

    winrt::Size m_previousAvailableSize{};

    bool m_isDragging{ false };
};
