// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "pch.h"
#include "common.h"

#include "TabView.h"
#include "TabViewItem.g.h"
#include "TabViewItem.properties.h"
#include "TabViewItemAutomationPeer.h"
#include "TabViewItemTemplateSettings.h"
#include "TabViewTrace.h"

class TabViewItem :
    public ReferenceTracker<TabViewItem, winrt::implementation::TabViewItemT>,
    public TabViewItemProperties
{
public:
    TabViewItem();

    // IFrameworkElement
    void OnApplyTemplate();
    void OnLoaded(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args);
    // IUIElement
    winrt::AutomationPeer OnCreateAutomationPeer();

    void OnIsClosablePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnHeaderPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);
    void OnIconSourcePropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args);

    void OnPointerEntered(winrt::PointerRoutedEventArgs const& args);
    void OnPointerExited(winrt::PointerRoutedEventArgs const& args);
    void OnPointerPressed(winrt::PointerRoutedEventArgs const& args);
    void OnPointerReleased(winrt::PointerRoutedEventArgs const& args);
    void OnPointerCanceled(winrt::PointerRoutedEventArgs const& args);
    void OnPointerCaptureLost(winrt::PointerRoutedEventArgs const& args);

    // Control
    void OnKeyDown(winrt::KeyRoutedEventArgs const& e);

    void RaiseRequestClose(TabViewTabCloseRequestedEventArgs const& args);
    void OnTabViewWidthModeChanged(winrt::TabViewWidthMode const& mode);
    void OnCloseButtonOverlayModeChanged(winrt::TabViewCloseButtonOverlayMode const& mode);

    winrt::TabView GetParentTabView();
    void SetParentTabView(winrt::TabView const& tabView);

    void StartBringTabIntoView();

    winrt::Button GetCloseButton() { return m_closeButton.get(); }

private:
    void UpdateCloseButton();
    void UpdateForeground();
    void RequestClose();
    void OnHeaderChanged();
    void OnIconSourceChanged();
    void UpdateWidthModeVisualState();

    void OnSizeChanged(const winrt::IInspectable&, const winrt::SizeChangedEventArgs& args);
    void UpdateTabGeometry();

    void OnCloseButtonClick(const winrt::IInspectable& sender, const winrt::RoutedEventArgs& args);

    void OnIsSelectedPropertyChanged(const winrt::DependencyObject& sender, const winrt::DependencyProperty& args);
    void OnForegroundPropertyChanged(const winrt::DependencyObject&, const winrt::DependencyProperty&);

    void OnTabDragStarting(const winrt::IInspectable& sender, const winrt::TabViewTabDragStartingEventArgs& args);
    void OnTabDragCompleted(const winrt::IInspectable& sender, const winrt::TabViewTabDragCompletedEventArgs& args);

    void HideLeftAdjacentTabSeparator();
    void RestoreLeftAdjacentTabSeparatorVisibility();

    void UpdateShadow();
    void UpdateSelectedBackgroundPathTranslateTransform();

    bool m_hasPointerCapture = false;
    bool m_isMiddlePointerButtonPressed = false;
    bool m_isDragging = false;
    bool m_isPointerOver = false;
    bool m_firstTimeSettingToolTip{ true };

    tracker_ref<winrt::Path> m_selectedBackgroundPath{ this };
    tracker_ref<winrt::Button> m_closeButton{ this };
    tracker_ref<winrt::ToolTip> m_toolTip{ this };
    tracker_ref<winrt::ContentPresenter> m_headerContentPresenter{ this };
    winrt::TabViewWidthMode m_tabViewWidthMode{ winrt::TabViewWidthMode::Equal };
    winrt::TabViewCloseButtonOverlayMode m_closeButtonOverlayMode{ winrt::TabViewCloseButtonOverlayMode::Auto };

    winrt::FrameworkElement::SizeChanged_revoker m_selectedBackgroundPathSizeChangedRevoker{};
    winrt::ButtonBase::Click_revoker m_closeButtonClickRevoker{};
    winrt::TabView::TabDragStarting_revoker m_tabDragStartingRevoker{};
    winrt::TabView::TabDragCompleted_revoker m_tabDragCompletedRevoker{};

    winrt::IInspectable m_shadow{ nullptr };

    winrt::weak_ref<winrt::TabView> m_parentTabView{ nullptr };
};
