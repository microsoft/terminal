// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "Pane.h"
#include "ColorPickupFlyout.h"
#include "TabBase.h"
#include "TerminalTab.g.h"

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

        winrt::Microsoft::Terminal::Control::TermControl GetActiveTerminalControl() const;
        winrt::Microsoft::Terminal::Settings::Model::Profile GetFocusedProfile() const noexcept;

        void Focus(winrt::Windows::UI::Xaml::FocusState focusState) override;

        void Scroll(const int delta);

        std::shared_ptr<Pane> DetachRoot();
        std::shared_ptr<Pane> DetachPane();
        void AttachPane(std::shared_ptr<Pane> pane);

        void AttachColorPicker(winrt::TerminalApp::ColorPickupFlyout& colorPicker);

        std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> SplitPane(winrt::Microsoft::Terminal::Settings::Model::SplitDirection splitType,
                                                                          const float splitSize,
                                                                          std::shared_ptr<Pane> newPane);

        void ToggleSplitOrientation();
        void UpdateIcon(const winrt::hstring iconPath, const winrt::Microsoft::Terminal::Settings::Model::IconStyle iconStyle);
        void HideIcon(const bool hide);

        void ShowBellIndicator(const bool show);
        void ActivateBellIndicatorTimer();

        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
        std::optional<winrt::Microsoft::Terminal::Settings::Model::SplitDirection> PreCalculateCanSplit(winrt::Microsoft::Terminal::Settings::Model::SplitDirection splitType,
                                                                                                        const float splitSize,
                                                                                                        winrt::Windows::Foundation::Size availableSpace) const;

        void ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);
        bool NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction);
        bool SwapPane(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction);
        bool FocusPane(const uint32_t id);

        void UpdateSettings();
        void UpdateTitle();

        void Shutdown() override;
        void ClosePane();

        void SetTabText(winrt::hstring title);
        winrt::hstring GetTabText() const;
        void ResetTabText();
        void ActivateTabRenamer();

        virtual std::optional<winrt::Windows::UI::Color> GetTabColor() override;
        void SetRuntimeTabColor(const winrt::Windows::UI::Color& color);
        void ResetRuntimeTabColor();

        void UpdateZoom(std::shared_ptr<Pane> newFocus);
        void ToggleZoom();
        bool IsZoomed();
        void EnterZoom();
        void ExitZoom();

        std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs> BuildStartupActions(const bool asContent = false) const override;

        int GetLeafPaneCount() const noexcept;

        void TogglePaneReadOnly();
        void SetPaneReadOnly(const bool readOnlyState);
        void ToggleBroadcastInput();

        std::shared_ptr<Pane> GetActivePane() const;
        winrt::TerminalApp::TaskbarState GetCombinedTaskbarState() const;

        std::shared_ptr<Pane> GetRootPane() const { return _rootPane; }

        winrt::TerminalApp::TerminalTabStatus TabStatus()
        {
            return _tabStatus;
        }

        til::event<winrt::delegate<>> ActivePaneChanged;
        til::event<winrt::delegate<>> TabRaiseVisualBell;
        til::typed_event<IInspectable, IInspectable> TaskbarProgressChanged;

    private:
        static constexpr double HeaderRenameBoxWidthDefault{ 165 };
        static constexpr double HeaderRenameBoxWidthTitleLength{ std::numeric_limits<double>::infinity() };

        std::shared_ptr<Pane> _rootPane{ nullptr };
        std::shared_ptr<Pane> _activePane{ nullptr };
        std::shared_ptr<Pane> _zoomedPane{ nullptr };

        Windows::UI::Xaml::Controls::MenuFlyoutItem _closePaneMenuItem;
        Windows::UI::Xaml::Controls::MenuFlyoutItem _restartConnectionMenuItem;

        winrt::Microsoft::Terminal::Settings::Model::IconStyle _lastIconStyle;
        winrt::hstring _lastIconPath{};
        std::optional<winrt::Windows::UI::Color> _runtimeTabColor{};
        winrt::TerminalApp::TabHeaderControl _headerControl{};
        winrt::TerminalApp::TerminalTabStatus _tabStatus{};

        winrt::TerminalApp::ColorPickupFlyout _tabColorPickup{ nullptr };
        winrt::event_token _colorSelectedToken;
        winrt::event_token _colorClearedToken;
        winrt::event_token _pickerClosedToken;

        struct ControlEventTokens
        {
            winrt::event_token titleToken;
            winrt::event_token colorToken;
            winrt::event_token taskbarToken;
            winrt::event_token stateToken;
            winrt::event_token readOnlyToken;
            winrt::event_token focusToken;

            winrt::Microsoft::Terminal::Control::TermControl::KeySent_revoker KeySent;
            winrt::Microsoft::Terminal::Control::TermControl::CharSent_revoker CharSent;
            winrt::Microsoft::Terminal::Control::TermControl::StringSent_revoker StringSent;
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
        winrt::Windows::UI::Xaml::Controls::TextBox::LayoutUpdated_revoker _tabRenameBoxLayoutUpdatedRevoker;

        void _Setup();

        SafeDispatcherTimer _bellIndicatorTimer;
        void _BellIndicatorTimerTick(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& e);

        void _MakeTabViewItem() override;

        void _UpdateHeaderControlMaxWidth();

        void _CreateContextMenu() override;
        virtual winrt::hstring _CreateToolTipTitle() override;

        void _DetachEventHandlersFromControl(const uint32_t paneId, const winrt::Microsoft::Terminal::Control::TermControl& control);
        void _AttachEventHandlersToControl(const uint32_t paneId, const winrt::Microsoft::Terminal::Control::TermControl& control);
        void _AttachEventHandlersToPane(std::shared_ptr<Pane> pane);

        void _UpdateActivePane(std::shared_ptr<Pane> pane);

        winrt::hstring _GetActiveTitle() const;

        void _RecalculateAndApplyReadOnly();

        void _UpdateProgressState();

        void _UpdateConnectionClosedState();
        void _RestartActivePaneConnection();

        void _DuplicateTab();

        virtual winrt::Windows::UI::Xaml::Media::Brush _BackgroundBrush() override;

        void _addBroadcastHandlers(const winrt::Microsoft::Terminal::Control::TermControl& control, ControlEventTokens& events);

        void _chooseColorClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _renameTabClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _duplicateTabClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _splitTabClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _closePaneClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _exportTextClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _moveTabToNewWindowClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _findClicked(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        friend class ::TerminalAppLocalTests::TabTests;
    };
}
