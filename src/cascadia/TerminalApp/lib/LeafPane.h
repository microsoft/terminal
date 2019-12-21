#pragma once
#include "Pane.h"
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"
#include <winrt/Microsoft.Terminal.TerminalControl.h>

enum class Borders : int
{
    None = 0x0,
    Top = 0x1,
    Bottom = 0x2,
    Left = 0x4,
    Right = 0x8
};
DEFINE_ENUM_FLAG_OPERATORS(Borders);

class LeafPane : public Pane, public std::enable_shared_from_this<LeafPane>
{
public:
    struct SplitResult;

    LeafPane(const GUID& profile,
             const winrt::Microsoft::Terminal::TerminalControl::TermControl& control,
             const bool lastFocused = false);
    ~LeafPane() override;

    void Relayout() override;
    void ClearActive() override;
    void UpdateSettings(const winrt::Microsoft::Terminal::Settings::TerminalSettings& settings,
                        const GUID& profile);
    void ResizeContent(const winrt::Windows::Foundation::Size& newSize) override;
    std::shared_ptr<LeafPane> FindActivePane() override;
    bool CanSplit(winrt::TerminalApp::SplitState splitType);
    SplitResult Split(winrt::TerminalApp::SplitState splitType,
                                      const GUID& profile,
                                      const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);
    winrt::Microsoft::Terminal::TerminalControl::TermControl GetTerminalControl();
    bool WasLastActive() const noexcept;
    void SetActive();
    GUID GetProfile();
    void OnNeightbourClosed(std::shared_ptr<LeafPane> closedNeightbour);
    void Close();

    struct SplitResult
    {
        std::shared_ptr<ParentPane> newParent;
        std::shared_ptr<LeafPane> firstChild;
        std::shared_ptr<LeafPane> secondChild;
    };

    friend class Pane;

    WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
    DECLARE_EVENT(Splitted, _SplittedHandlers, winrt::delegate<std::shared_ptr<ParentPane>>);
    DECLARE_EVENT(GotFocus, _GotFocusHandlers, winrt::delegate<std::shared_ptr<LeafPane>>);
private:
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_focusedBorderBrush;
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_unfocusedBorderBrush;

    Borders _borders{ Borders::None };
    bool _lastActive{ false };
    winrt::Windows::UI::Xaml::Controls::Border _border{};
    winrt::Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };

    GUID _profile;
    winrt::event_token _connectionStateChangedToken{ 0 };
    winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;

    void _UpdateBorders();
    void _UpdateVisuals();
    winrt::fire_and_forget _ControlConnectionStateChangedHandler(
        const winrt::Microsoft::Terminal::TerminalControl::TermControl& sender,
        const winrt::Windows::Foundation::IInspectable& /*args*/);
    void _ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                 winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

    static void _SetupResources();

    std::shared_ptr<LeafPane> _FindFirstLeaf() override;
    SnapSizeResult _CalcSnappedDimension(const bool widthOrHeight, const float dimension) const override;
    void _AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const override;
    winrt::Windows::Foundation::Size _GetMinSize() const override;
};
