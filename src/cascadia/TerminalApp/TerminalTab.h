// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "Pane.h"
#include "ColorPickupFlyout.h"
#include "TabBase.h"
#include "TerminalTab.g.h"

static constexpr double HeaderRenameBoxWidthDefault{ 165 };
static constexpr double HeaderRenameBoxWidthTitleLength{ std::numeric_limits<double>::infinity() };

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

namespace winrt::TerminalApp::implementation
{
    struct TerminalTab : TerminalTabT<TerminalTab, TabBase>
    {
    public:
        TerminalTab(std::shared_ptr<Pane> rootPane);

        // Called after construction to perform the necessary setup, which relies on weak_ptr
        void Initialize();

        MTControl::TermControl GetActiveTerminalControl() const;
        MTSM::Profile GetFocusedProfile() const noexcept;

        void Focus(WUX::FocusState focusState) override;

        winrt::fire_and_forget Scroll(const int delta);

        std::shared_ptr<Pane> DetachRoot();
        std::shared_ptr<Pane> DetachPane();
        void AttachPane(std::shared_ptr<Pane> pane);

        void AttachColorPicker(MTApp::ColorPickupFlyout& colorPicker);

        void SplitPane(MTSM::SplitDirection splitType,
                       const float splitSize,
                       std::shared_ptr<Pane> newPane);

        void ToggleSplitOrientation();
        winrt::fire_and_forget UpdateIcon(const winrt::hstring iconPath);
        winrt::fire_and_forget HideIcon(const bool hide);

        winrt::fire_and_forget ShowBellIndicator(const bool show);
        winrt::fire_and_forget ActivateBellIndicatorTimer();

        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
        std::optional<MTSM::SplitDirection> PreCalculateCanSplit(MTSM::SplitDirection splitType,
                                                                 const float splitSize,
                                                                 WF::Size availableSpace) const;

        void ResizePane(const MTSM::ResizeDirection& direction);
        bool NavigateFocus(const MTSM::FocusDirection& direction);
        bool SwapPane(const MTSM::FocusDirection& direction);
        bool FocusPane(const uint32_t id);

        void UpdateSettings();
        winrt::fire_and_forget UpdateTitle();

        void Shutdown() override;
        void ClosePane();

        void SetTabText(winrt::hstring title);
        winrt::hstring GetTabText() const;
        void ResetTabText();
        void ActivateTabRenamer();

        virtual std::optional<winrt::Windows::UI::Color> GetTabColor() override;
        void SetRuntimeTabColor(const winrt::Windows::UI::Color& color);
        void ResetRuntimeTabColor();
        void RequestColorPicker();

        void UpdateZoom(std::shared_ptr<Pane> newFocus);
        void ToggleZoom();
        bool IsZoomed();
        void EnterZoom();
        void ExitZoom();

        std::vector<MTSM::ActionAndArgs> BuildStartupActions() const override;

        int GetLeafPaneCount() const noexcept;

        void TogglePaneReadOnly();
        std::shared_ptr<Pane> GetActivePane() const;
        MTApp::TaskbarState GetCombinedTaskbarState() const;

        std::shared_ptr<Pane> GetRootPane() const { return _rootPane; }

        MTApp::TerminalTabStatus TabStatus()
        {
            return _tabStatus;
        }

        WINRT_CALLBACK(ActivePaneChanged, winrt::delegate<>);
        WINRT_CALLBACK(TabRaiseVisualBell, winrt::delegate<>);
        WINRT_CALLBACK(DuplicateRequested, winrt::delegate<>);
        WINRT_CALLBACK(SplitTabRequested, winrt::delegate<>);
        WINRT_CALLBACK(FindRequested, winrt::delegate<>);
        WINRT_CALLBACK(ExportTabRequested, winrt::delegate<>);
        WINRT_CALLBACK(ColorPickerRequested, winrt::delegate<>);
        TYPED_EVENT(TaskbarProgressChanged, IInspectable, IInspectable);

    private:
        std::shared_ptr<Pane> _rootPane{ nullptr };
        std::shared_ptr<Pane> _activePane{ nullptr };
        std::shared_ptr<Pane> _zoomedPane{ nullptr };

        winrt::hstring _lastIconPath{};
        std::optional<winrt::Windows::UI::Color> _runtimeTabColor{};
        MTApp::TabHeaderControl _headerControl{};
        MTApp::TerminalTabStatus _tabStatus{};

        MTApp::ColorPickupFlyout _tabColorPickup{ nullptr };
        winrt::event_token _colorSelectedToken;
        winrt::event_token _colorClearedToken;
        winrt::event_token _pickerClosedToken;

        struct ControlEventTokens
        {
            winrt::event_token titleToken;
            winrt::event_token colorToken;
            winrt::event_token taskbarToken;
            winrt::event_token readOnlyToken;
            winrt::event_token focusToken;
        };
        std::unordered_map<uint32_t, ControlEventTokens> _controlEvents;

        winrt::event_token _rootClosedToken{};

        std::vector<uint32_t> _mruPanes;
        uint32_t _nextPaneId{ 0 };

        bool _receivedKeyDown{ false };
        bool _iconHidden{ false };
        bool _changingActivePane{ false };

        winrt::hstring _runtimeTabText{};
        bool _inRename{ false };
        WUXC::TextBox::LayoutUpdated_revoker _tabRenameBoxLayoutUpdatedRevoker;

        MTApp::ShortcutActionDispatch _dispatch;

        void _Setup();

        std::optional<WUX::DispatcherTimer> _bellIndicatorTimer;
        void _BellIndicatorTimerTick(const WF::IInspectable& sender, const WF::IInspectable& e);

        void _MakeTabViewItem() override;

        winrt::fire_and_forget _UpdateHeaderControlMaxWidth();

        void _CreateContextMenu() override;
        virtual winrt::hstring _CreateToolTipTitle() override;

        void _DetachEventHandlersFromControl(const uint32_t paneId, const MTControl::TermControl& control);
        void _AttachEventHandlersToControl(const uint32_t paneId, const MTControl::TermControl& control);
        void _AttachEventHandlersToPane(std::shared_ptr<Pane> pane);

        void _UpdateActivePane(std::shared_ptr<Pane> pane);

        winrt::hstring _GetActiveTitle() const;

        void _RecalculateAndApplyReadOnly();

        void _UpdateProgressState();

        void _DuplicateTab();

        virtual WUXMedia::Brush _BackgroundBrush() override;

        friend class ::TerminalAppLocalTests::TabTests;
    };
}
