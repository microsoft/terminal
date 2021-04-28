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
        TerminalTab(const GUID& profile, const winrt::Microsoft::Terminal::Control::TermControl& control);

        // Called after construction to perform the necessary setup, which relies on weak_ptr
        void Initialize(const winrt::Microsoft::Terminal::Control::TermControl& control);

        winrt::Microsoft::Terminal::Control::TermControl GetActiveTerminalControl() const;
        std::optional<GUID> GetFocusedProfile() const noexcept;

        void Focus(winrt::Windows::UI::Xaml::FocusState focusState) override;

        winrt::fire_and_forget Scroll(const int delta);

        void SplitPane(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                       const float splitSize,
                       const GUID& profile,
                       winrt::Microsoft::Terminal::Control::TermControl& control);

        winrt::fire_and_forget UpdateIcon(const winrt::hstring iconPath);
        winrt::fire_and_forget HideIcon(const bool hide);

        winrt::fire_and_forget ShowBellIndicator(const bool show);
        winrt::fire_and_forget ActivateBellIndicatorTimer();

        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
        winrt::Microsoft::Terminal::Settings::Model::SplitState PreCalculateAutoSplit(winrt::Windows::Foundation::Size rootSize) const;
        bool PreCalculateCanSplit(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                  const float splitSize,
                                  winrt::Windows::Foundation::Size availableSpace) const;

        void ResizeContent(const winrt::Windows::Foundation::Size& newSize);
        void ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);
        void NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction);

        void UpdateSettings(const Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult& settings, const GUID& profile);
        winrt::fire_and_forget UpdateTitle();

        void Shutdown() override;
        void ClosePane();

        void SetTabText(winrt::hstring title);
        winrt::hstring GetTabText() const;
        void ResetTabText();
        void ActivateTabRenamer();

        std::optional<winrt::Windows::UI::Color> GetTabColor();

        void SetRuntimeTabColor(const winrt::Windows::UI::Color& color);
        void ResetRuntimeTabColor();
        void ActivateColorPicker();

        void ToggleZoom();
        bool IsZoomed();
        void EnterZoom();
        void ExitZoom();

        int GetLeafPaneCount() const noexcept;

        void TogglePaneReadOnly();
        std::shared_ptr<Pane> GetActivePane() const;

        winrt::TerminalApp::TerminalTabStatus TabStatus()
        {
            return _tabStatus;
        }

        DECLARE_EVENT(ActivePaneChanged, _ActivePaneChangedHandlers, winrt::delegate<>);
        DECLARE_EVENT(ColorSelected, _colorSelected, winrt::delegate<winrt::Windows::UI::Color>);
        DECLARE_EVENT(ColorCleared, _colorCleared, winrt::delegate<>);
        DECLARE_EVENT(TabRaiseVisualBell, _TabRaiseVisualBellHandlers, winrt::delegate<>);
        DECLARE_EVENT(DuplicateRequested, _DuplicateRequestedHandlers, winrt::delegate<>);
        TYPED_EVENT(TaskbarProgressChanged, IInspectable, IInspectable);

    private:
        std::shared_ptr<Pane> _rootPane{ nullptr };
        std::shared_ptr<Pane> _activePane{ nullptr };
        std::shared_ptr<Pane> _zoomedPane{ nullptr };
        winrt::hstring _lastIconPath{};
        winrt::TerminalApp::ColorPickupFlyout _tabColorPickup{};
        std::optional<winrt::Windows::UI::Color> _themeTabColor{};
        std::optional<winrt::Windows::UI::Color> _runtimeTabColor{};
        winrt::TerminalApp::TabHeaderControl _headerControl{};
        winrt::TerminalApp::TerminalTabStatus _tabStatus{};

        std::vector<uint16_t> _mruPanes;
        uint16_t _nextPaneId{ 0 };

        bool _receivedKeyDown{ false };
        bool _iconHidden{ false };

        winrt::hstring _runtimeTabText{};
        bool _inRename{ false };
        winrt::Windows::UI::Xaml::Controls::TextBox::LayoutUpdated_revoker _tabRenameBoxLayoutUpdatedRevoker;

        winrt::TerminalApp::ShortcutActionDispatch _dispatch;

        std::optional<Windows::UI::Xaml::DispatcherTimer> _bellIndicatorTimer;
        void _BellIndicatorTimerTick(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& e);

        void _MakeTabViewItem() override;

        winrt::fire_and_forget _UpdateHeaderControlMaxWidth();

        void _CreateContextMenu() override;
        virtual winrt::hstring _CreateToolTipTitle() override;

        void _RefreshVisualState();

        void _BindEventHandlers(const winrt::Microsoft::Terminal::Control::TermControl& control) noexcept;

        void _AttachEventHandlersToControl(const winrt::Microsoft::Terminal::Control::TermControl& control);
        void _AttachEventHandlersToPane(std::shared_ptr<Pane> pane);

        void _UpdateActivePane(std::shared_ptr<Pane> pane);

        winrt::hstring _GetActiveTitle() const;

        void _RecalculateAndApplyTabColor();
        void _ApplyTabColor(const winrt::Windows::UI::Color& color);
        void _ClearTabBackgroundColor();

        void _RecalculateAndApplyReadOnly();

        void _UpdateProgressState();

        void _DuplicateTab();

        friend class ::TerminalAppLocalTests::TabTests;
    };
}
