// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsCard.h"
#include "SettingsCard.g.cpp"
#include "SettingsCardAutomationPeer.g.cpp"
#include "StyleExtensions.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Automation;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty SettingsCard::_HeaderProperty{ nullptr };
    DependencyProperty SettingsCard::_DescriptionProperty{ nullptr };
    DependencyProperty SettingsCard::_HeaderIconProperty{ nullptr };
    DependencyProperty SettingsCard::_ActionIconProperty{ nullptr };
    DependencyProperty SettingsCard::_ActionIconToolTipProperty{ nullptr };
    DependencyProperty SettingsCard::_IsClickEnabledProperty{ nullptr };
    DependencyProperty SettingsCard::_IsActionIconVisibleProperty{ nullptr };
    DependencyProperty SettingsCard::_ContentAlignmentProperty{ nullptr };

    static constexpr std::wstring_view NormalState{ L"Normal" };
    static constexpr std::wstring_view PointerOverState{ L"PointerOver" };
    static constexpr std::wstring_view PressedState{ L"Pressed" };
    static constexpr std::wstring_view DisabledState{ L"Disabled" };

    static constexpr std::wstring_view BitmapHeaderIconEnabledState{ L"BitmapHeaderIconEnabled" };
    static constexpr std::wstring_view BitmapHeaderIconDisabledState{ L"BitmapHeaderIconDisabled" };

    static constexpr std::wstring_view RightState{ L"Right" };
    static constexpr std::wstring_view LeftState{ L"Left" };
    static constexpr std::wstring_view VerticalState{ L"Vertical" };

    static constexpr std::wstring_view NoContentSpacingState{ L"NoContentSpacing" };
    static constexpr std::wstring_view ContentSpacingState{ L"ContentSpacing" };

    static constexpr std::wstring_view ContentAlignmentStatesGroup{ L"ContentAlignmentStates" };

    static constexpr std::wstring_view ActionIconPresenterHolder{ L"PART_ActionIconPresenterHolder" };
    static constexpr std::wstring_view HeaderPresenter{ L"PART_HeaderPresenter" };
    static constexpr std::wstring_view DescriptionPresenter{ L"PART_DescriptionPresenter" };
    static constexpr std::wstring_view HeaderIconPresenterHolder{ L"PART_HeaderIconPresenterHolder" };
    static constexpr std::wstring_view ContentPresenterPart{ L"PART_ContentPresenter" };
    static constexpr std::wstring_view RootGridPart{ L"PART_RootGrid" };

    static constexpr double SettingsCardVerticalHeaderContentSpacing{ 8.0 };

    // Returns true if the given object is null, or is a string that is empty.
    // Non-string non-null objects (e.g. a TextBlock) are considered "non-empty".
    static bool _isNullOrEmpty(const winrt::Windows::Foundation::IInspectable& obj)
    {
        if (!obj)
        {
            return true;
        }
        if (const auto pv{ obj.try_as<IPropertyValue>() }; pv && pv.Type() == PropertyType::String)
        {
            return unbox_value_or<hstring>(obj, hstring{}).empty();
        }
        return false;
    }

    SettingsCard::SettingsCard()
    {
        _InitializeProperties();
    }

    void SettingsCard::_InitializeProperties()
    {
        if (!_HeaderProperty)
        {
            _HeaderProperty = DependencyProperty::Register(
                L"Header",
                xaml_typename<IInspectable>(),
                xaml_typename<Editor::SettingsCard>(),
                PropertyMetadata{ nullptr, PropertyChangedCallback{ &SettingsCard::_OnHeaderChanged } });
        }
        if (!_DescriptionProperty)
        {
            _DescriptionProperty = DependencyProperty::Register(
                L"Description",
                xaml_typename<IInspectable>(),
                xaml_typename<Editor::SettingsCard>(),
                PropertyMetadata{ nullptr, PropertyChangedCallback{ &SettingsCard::_OnDescriptionChanged } });
        }
        if (!_HeaderIconProperty)
        {
            _HeaderIconProperty = DependencyProperty::Register(
                L"HeaderIcon",
                xaml_typename<IconElement>(),
                xaml_typename<Editor::SettingsCard>(),
                PropertyMetadata{ nullptr, PropertyChangedCallback{ &SettingsCard::_OnHeaderIconChanged } });
        }
        if (!_ActionIconProperty)
        {
            // No metadata default value — a fresh FontIcon is allocated per
            // card in OnApplyTemplate when the user hasn't set ActionIcon
            // explicitly. We cannot use a default FontIcon here because
            // UIElements have a single-parent constraint (one instance can't
            // be shared across cards) and a default string was rendering
            // through ContentPresenter -> TextBlock, whose line-height
            // padding made the Viewbox-scaled glyph visibly smaller than
            // WCT's SymbolIcon default.
            _ActionIconProperty = DependencyProperty::Register(
                L"ActionIcon",
                xaml_typename<IInspectable>(),
                xaml_typename<Editor::SettingsCard>(),
                PropertyMetadata{ nullptr });
        }
        if (!_ActionIconToolTipProperty)
        {
            _ActionIconToolTipProperty = DependencyProperty::Register(
                L"ActionIconToolTip",
                xaml_typename<hstring>(),
                xaml_typename<Editor::SettingsCard>(),
                PropertyMetadata{ box_value(hstring{}) });
        }
        if (!_IsClickEnabledProperty)
        {
            _IsClickEnabledProperty = DependencyProperty::Register(
                L"IsClickEnabled",
                xaml_typename<bool>(),
                xaml_typename<Editor::SettingsCard>(),
                PropertyMetadata{ box_value(false), PropertyChangedCallback{ &SettingsCard::_OnIsClickEnabledChanged } });
        }
        if (!_IsActionIconVisibleProperty)
        {
            _IsActionIconVisibleProperty = DependencyProperty::Register(
                L"IsActionIconVisible",
                xaml_typename<bool>(),
                xaml_typename<Editor::SettingsCard>(),
                PropertyMetadata{ box_value(true), PropertyChangedCallback{ &SettingsCard::_OnIsActionIconVisibleChanged } });
        }
        if (!_ContentAlignmentProperty)
        {
            _ContentAlignmentProperty = DependencyProperty::Register(
                L"ContentAlignment",
                xaml_typename<Editor::SettingsCardContentAlignment>(),
                xaml_typename<Editor::SettingsCard>(),
                PropertyMetadata{ box_value(Editor::SettingsCardContentAlignment::Right), PropertyChangedCallback{ &SettingsCard::_OnContentAlignmentChanged } });
        }
    }

    AutomationPeer SettingsCard::OnCreateAutomationPeer()
    {
        return winrt::make<implementation::SettingsCardAutomationPeer>(*this);
    }

    // Pointer overrides: gate the ButtonBase click pipeline on IsClickEnabled.
    // When IsClickEnabled=false, we do NOT call the base method, so the event
    // is left unhandled and bubbles up the visual tree. This is what lets the
    // SettingsExpander header (a ToggleButton hosting a SettingsCard with
    // IsClickEnabled=false) toggle on a click anywhere across the header row,
    // not just on the chevron. Mirrors the Community Toolkit's SettingsCard.cs.
    void SettingsCard::OnPointerPressed(const winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs& e)
    {
        if (IsClickEnabled())
        {
            base_type::OnPointerPressed(e);
            _GoToCommonState(PressedState, true);
        }
    }

    void SettingsCard::OnPointerReleased(const winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs& e)
    {
        if (IsClickEnabled())
        {
            base_type::OnPointerReleased(e);
            _GoToCommonState(NormalState, true);
        }
    }

    void SettingsCard::OnApplyTemplate()
    {
        // Match WCT's SettingsCard.OnApplyTemplate() which calls base first so the
        // template parts are fully wired up before any of our post-template setup
        // (visibility, visual states, click interaction) runs.
        base_type::OnApplyTemplate();

        // Inject the shared implicit ToggleSwitch / Slider / ComboBox / TextBox
        // dictionary so child controls in our Content slot get the Windows 11
        // settings-page defaults without an explicit Style attribute. Bypasses
        // the WCT Setter.Value-with-inline-ResourceDictionary pattern, which
        // crashes WinUI 2 XAML load in this codebase.
        StyleExtensions::EnsureImplicitStylesMergedInto(*this);

        // Drop any handlers from a previous template.
        _isEnabledChangedRevoker.revoke();
        _contentAlignmentStatesChangedRevoker.revoke();
        _DisableButtonInteraction();
        if (_contentChangedToken != 0)
        {
            UnregisterPropertyChangedCallback(ContentControl::ContentProperty(), _contentChangedToken);
            _contentChangedToken = 0;
        }

        _UpdateActionIconVisibility();
        _UpdateHeaderVisibility();
        _UpdateDescriptionVisibility();
        _UpdateHeaderIconVisibility();
        // Initial visual states.
        _CheckInitialVisualState();
        _CheckHeaderIconState();
        _SetAccessibleContentName();

        // Watch for Content changing later (we may need to refresh the AutomationProperties.Name on it).
        _contentChangedToken = RegisterPropertyChangedCallback(ContentControl::ContentProperty(), [weakThis = get_weak()](auto&&, auto&&) {
            if (const auto strongThis = weakThis.get())
            {
                strongThis->_SetAccessibleContentName();
            }
        });

        // Apply click-interaction state.
        if (IsClickEnabled())
        {
            _EnableButtonInteraction();
        }

        _isEnabledChangedRevoker = IsEnabledChanged(winrt::auto_revoke, [weakThis = get_weak()](auto&&, auto&&) {
            if (const auto strongThis = weakThis.get())
            {
                strongThis->_GoToCommonState(strongThis->IsEnabled() ? NormalState : DisabledState, true);
                strongThis->_CheckHeaderIconState();
            }
        });
    }

    void SettingsCard::_CheckInitialVisualState()
    {
        VisualStateManager::GoToState(*this, IsEnabled() ? hstring{ NormalState } : hstring{ DisabledState }, true);
        _UpdateContentAlignmentState();

        // Subscribe to ContentAlignmentStates so we can drive ContentSpacingStates
        // whenever the alignment shifts to a stacked layout (Vertical, RightWrapped*).
        if (const auto child{ GetTemplateChild(hstring{ ContentAlignmentStatesGroup }) })
        {
            if (const auto group{ child.try_as<VisualStateGroup>() })
            {
                _CheckVerticalSpacingState(group.CurrentState());
                _contentAlignmentStatesChangedRevoker = group.CurrentStateChanged(winrt::auto_revoke, [weakThis = get_weak()](auto&&, const VisualStateChangedEventArgs& args) {
                    if (const auto strongThis = weakThis.get())
                    {
                        strongThis->_CheckVerticalSpacingState(args.NewState());
                    }
                });
            }
        }
    }

    void SettingsCard::_CheckHeaderIconState()
    {
        // The Disabled common state recolors text/glyph foregrounds via the brush, but a
        // BitmapIcon is an image and won't pick up the disabled brush. Lower its opacity
        // instead, via the BitmapHeaderIconStates group. Mirrors the toolkit's
        // SettingsCard.cs::CheckHeaderIconState.
        if (HeaderIcon().try_as<BitmapIcon>())
        {
            VisualStateManager::GoToState(*this,
                                          hstring{ IsEnabled() ? BitmapHeaderIconEnabledState : BitmapHeaderIconDisabledState },
                                          true);
        }
        else
        {
            // Reset to the enabled state when a non-bitmap icon (or none) is present so the
            // opacity setter doesn't stick around from a previous bitmap icon.
            VisualStateManager::GoToState(*this, hstring{ BitmapHeaderIconEnabledState }, true);
        }
    }

    void SettingsCard::_CheckVerticalSpacingState(const VisualState& state)
    {
        // Add row spacing whenever the content sits below the header (Vertical or RightWrapped*)
        // AND there's both Content and (Header or Description) to space apart.
        //
        // For the Vertical case we read ContentAlignment() directly rather than
        // state.Name(): our Left/Vertical visual states are empty placeholders
        // (the actual layout work happens in _UpdateContentAlignmentState via
        // direct C++ property writes) and an empty <VisualState> may not have
        // its name register as CurrentState when GoToState fires in WinUI 2 —
        // leaving stackedLayout incorrectly false. RightWrapped*, in contrast,
        // are size-triggered visual states with real setters and ControlSizeTrigger,
        // so their CurrentState is reliably observed.
        const auto stateName{ state ? state.Name() : hstring{} };
        const bool stackedLayout =
            ContentAlignment() == Editor::SettingsCardContentAlignment::Vertical ||
            stateName == L"RightWrapped" ||
            stateName == L"RightWrappedNoIcon";

        const bool hasContent{ static_cast<bool>(Content()) };
        const bool hasHeaderOrDescription = !_isNullOrEmpty(Header()) || !_isNullOrEmpty(Description());

        const bool shouldSpace = stackedLayout && hasContent && hasHeaderOrDescription;

        VisualStateManager::GoToState(*this,
                                      hstring{ shouldSpace ? ContentSpacingState : NoContentSpacingState },
                                      true);

        // Belt-and-suspenders for the WinUI 2 quirk: VisualState.Setters activated
        // by C++ GoToState don't reliably engage. The ContentSpacing setter
        // targets PART_RootGrid.RowSpacing; set it directly here as well so the
        // Vertical / RightWrapped stacked-layout cards actually get the breathing
        // room between header and content that WCT shows.
        if (const auto child{ GetTemplateChild(hstring{ RootGridPart }) })
        {
            if (const auto rootGrid{ child.try_as<Grid>() })
            {
                rootGrid.RowSpacing(shouldSpace ? SettingsCardVerticalHeaderContentSpacing : 0.0);
            }
        }
    }

    void SettingsCard::_SetAccessibleContentName()
    {
        // If Header is a string and the inner Content lacks an AutomationProperties.Name, propagate the header
        // into the content so screen readers can announce something meaningful when focus lands there.
        const auto headerObj{ Header() };
        if (!headerObj)
        {
            return;
        }
        const auto headerString{ unbox_value_or<hstring>(headerObj, hstring{}) };
        if (headerString.empty())
        {
            return;
        }

        const auto contentObj{ Content() };
        if (const auto element{ contentObj.try_as<UIElement>() })
        {
            if (Automation::AutomationProperties::GetName(element).empty())
            {
                // Don't override ButtonBase content (would clobber its own name) or plain text blocks.
                if (!element.try_as<ButtonBase>() && !element.try_as<TextBlock>())
                {
                    Automation::AutomationProperties::SetName(element, headerString);
                }
            }
        }
    }

    void SettingsCard::_EnableButtonInteraction()
    {
        if (_interactionEnabled)
        {
            return;
        }
        _interactionEnabled = true;

        IsTabStop(true);
        _pointerEnteredRevoker = PointerEntered(winrt::auto_revoke, [weakThis = get_weak()](auto&&, auto&&) {
            if (const auto strongThis = weakThis.get())
            {
                strongThis->_GoToCommonState(PointerOverState, true);
            }
        });
        _pointerExitedRevoker = PointerExited(winrt::auto_revoke, [weakThis = get_weak()](auto&&, auto&&) {
            if (const auto strongThis = weakThis.get())
            {
                strongThis->_GoToCommonState(NormalState, true);
            }
        });
        _pointerCaptureLostRevoker = PointerCaptureLost(winrt::auto_revoke, [weakThis = get_weak()](auto&&, auto&&) {
            if (const auto strongThis = weakThis.get())
            {
                strongThis->_GoToCommonState(NormalState, true);
            }
        });
        _pointerCanceledRevoker = PointerCanceled(winrt::auto_revoke, [weakThis = get_weak()](auto&&, auto&&) {
            if (const auto strongThis = weakThis.get())
            {
                strongThis->_GoToCommonState(NormalState, true);
            }
        });
        _previewKeyDownRevoker = PreviewKeyDown(winrt::auto_revoke, [weakThis = get_weak()](auto&&, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e) {
            const auto strongThis = weakThis.get();
            if (!strongThis)
            {
                return;
            }
            const auto key = e.Key();
            if (key == Windows::System::VirtualKey::Enter || key == Windows::System::VirtualKey::Space || key == Windows::System::VirtualKey::GamepadA)
            {
                const auto focused{ strongThis->_GetFocusedElement() };
                if (focused && focused.try_as<Editor::SettingsCard>() == strongThis.as<Editor::SettingsCard>())
                {
                    strongThis->_GoToCommonState(PressedState, true);
                }
            }
        });
        _previewKeyUpRevoker = PreviewKeyUp(winrt::auto_revoke, [weakThis = get_weak()](auto&&, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e) {
            const auto strongThis = weakThis.get();
            if (!strongThis)
            {
                return;
            }
            const auto key = e.Key();
            if (key == Windows::System::VirtualKey::Enter || key == Windows::System::VirtualKey::Space || key == Windows::System::VirtualKey::GamepadA)
            {
                strongThis->_GoToCommonState(NormalState, true);
            }
        });
    }

    void SettingsCard::_DisableButtonInteraction()
    {
        _interactionEnabled = false;
        IsTabStop(false);
        _pointerEnteredRevoker.revoke();
        _pointerExitedRevoker.revoke();
        _pointerCaptureLostRevoker.revoke();
        _pointerCanceledRevoker.revoke();
        _previewKeyDownRevoker.revoke();
        _previewKeyUpRevoker.revoke();
    }

    void SettingsCard::_GoToCommonState(const std::wstring_view& state, bool useTransitions)
    {
        VisualStateManager::GoToState(*this, hstring{ state }, useTransitions);
    }

    FrameworkElement SettingsCard::_GetFocusedElement()
    {
        if (const auto root{ XamlRoot() })
        {
            return FocusManager::GetFocusedElement(root).try_as<FrameworkElement>();
        }
        return FocusManager::GetFocusedElement().try_as<FrameworkElement>();
    }

    void SettingsCard::_UpdateActionIconVisibility()
    {
        if (const auto child{ GetTemplateChild(hstring{ ActionIconPresenterHolder }) })
        {
            if (const auto frameworkChild{ child.try_as<FrameworkElement>() })
            {
                frameworkChild.Visibility((IsClickEnabled() && IsActionIconVisible()) ? Visibility::Visible : Visibility::Collapsed);
            }
        }
    }

    void SettingsCard::_UpdateHeaderVisibility()
    {
        if (const auto child{ GetTemplateChild(hstring{ HeaderPresenter }) })
        {
            if (const auto frameworkChild{ child.try_as<FrameworkElement>() })
            {
                frameworkChild.Visibility(_isNullOrEmpty(Header()) ? Visibility::Collapsed : Visibility::Visible);
            }
        }
    }

    void SettingsCard::_UpdateDescriptionVisibility()
    {
        if (const auto child{ GetTemplateChild(hstring{ DescriptionPresenter }) })
        {
            if (const auto frameworkChild{ child.try_as<FrameworkElement>() })
            {
                frameworkChild.Visibility(_isNullOrEmpty(Description()) ? Visibility::Collapsed : Visibility::Visible);
            }
        }
    }

    void SettingsCard::_UpdateHeaderIconVisibility()
    {
        if (const auto child{ GetTemplateChild(hstring{ HeaderIconPresenterHolder }) })
        {
            if (const auto frameworkChild{ child.try_as<FrameworkElement>() })
            {
                frameworkChild.Visibility(HeaderIcon() ? Visibility::Visible : Visibility::Collapsed);
            }
        }
    }

    void SettingsCard::_UpdateContentAlignmentState()
    {
        // Map ContentAlignment to the visual state name *and* the corresponding
        // direct layout values. We drive both: GoToState keeps the visual state
        // machine in sync (so _CheckVerticalSpacingState sees the right CurrentState),
        // but we *also* apply the layout properties directly to PART_ContentPresenter
        // because in WinUI 2, attached-property changes (Grid.Row/Grid.Column) and
        // HorizontalAlignment changes inside VisualState.Setters or Storyboards
        // don't reliably engage when activated by programmatic VisualStateManager::GoToState
        // — only by XAML StateTriggers (which is what WCT uses via tk:IsEqualStateTrigger).
        // Until/unless an equivalent state trigger gets ported, the direct C++ set is
        // the load-bearing path for Left/Vertical.
        std::wstring_view state{ RightState };
        int contentRow{ 0 };
        int contentColumn{ 2 };
        auto contentHA{ HorizontalAlignment::Right };

        const auto alignment = ContentAlignment();
        switch (alignment)
        {
        case Editor::SettingsCardContentAlignment::Left:
            state = LeftState;
            contentRow = 1;
            contentColumn = 1;
            contentHA = HorizontalAlignment::Left;
            break;
        case Editor::SettingsCardContentAlignment::Vertical:
            state = VerticalState;
            contentRow = 1;
            contentColumn = 1;
            contentHA = HorizontalAlignment::Stretch;
            break;
        default:
            break;
        }

        VisualStateManager::GoToState(*this, hstring{ state }, true);

        if (const auto child{ GetTemplateChild(hstring{ ContentPresenterPart }) })
        {
            if (const auto contentPresenter{ child.try_as<FrameworkElement>() })
            {
                Grid::SetRow(contentPresenter, contentRow);
                Grid::SetColumn(contentPresenter, contentColumn);
                contentPresenter.HorizontalAlignment(contentHA);

                // Vertical mirrors WCT's TemplatedParent Binding setter: propagate
                // the card's own HorizontalContentAlignment to the inner ContentPresenter
                // so e.g. HorizontalContentAlignment="Left" + ContentAlignment="Vertical"
                // actually left-aligns the inner content.
                if (const auto cp{ contentPresenter.try_as<ContentPresenter>() })
                {
                    cp.HorizontalContentAlignment(HorizontalContentAlignment());
                }
            }
        }

        // Left collapses the HeaderIcon column regardless of HeaderIcon content.
        if (const auto child{ GetTemplateChild(hstring{ HeaderIconPresenterHolder }) })
        {
            if (const auto headerIconHolder{ child.try_as<FrameworkElement>() })
            {
                if (alignment == Editor::SettingsCardContentAlignment::Left)
                {
                    headerIconHolder.Visibility(Visibility::Collapsed);
                }
                else
                {
                    headerIconHolder.Visibility(HeaderIcon() ? Visibility::Visible : Visibility::Collapsed);
                }
            }
        }
    }

    void SettingsCard::_OnHeaderChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto obj{ d.try_as<Editor::SettingsCard>() };
        const auto self = get_self<SettingsCard>(obj);
        self->_UpdateHeaderVisibility();
        self->_SetAccessibleContentName();
    }

    void SettingsCard::_OnDescriptionChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto obj{ d.try_as<Editor::SettingsCard>() };
        get_self<SettingsCard>(obj)->_UpdateDescriptionVisibility();
    }

    void SettingsCard::_OnHeaderIconChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto obj{ d.try_as<Editor::SettingsCard>() };
        const auto self = get_self<SettingsCard>(obj);
        self->_UpdateHeaderIconVisibility();
        // HeaderIcon type may have flipped between BitmapIcon and other icon types — re-evaluate
        // the BitmapHeaderIcon visual state so the disabled-opacity setter is applied (or cleared).
        self->_CheckHeaderIconState();
    }

    void SettingsCard::_OnIsClickEnabledChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto obj{ d.try_as<Editor::SettingsCard>() };
        const auto self = get_self<SettingsCard>(obj);
        self->_UpdateActionIconVisibility();
        if (self->IsClickEnabled())
        {
            self->_EnableButtonInteraction();
        }
        else
        {
            self->_DisableButtonInteraction();
        }
    }

    void SettingsCard::_OnIsActionIconVisibleChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto obj{ d.try_as<Editor::SettingsCard>() };
        get_self<SettingsCard>(obj)->_UpdateActionIconVisibility();
    }

    void SettingsCard::_OnContentAlignmentChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto obj{ d.try_as<Editor::SettingsCard>() };
        get_self<SettingsCard>(obj)->_UpdateContentAlignmentState();
    }

    SettingsCardAutomationPeer::SettingsCardAutomationPeer(const Editor::SettingsCard& owner) :
        SettingsCardAutomationPeerT<SettingsCardAutomationPeer>(owner)
    {
    }

    AutomationControlType SettingsCardAutomationPeer::GetAutomationControlTypeCore() const
    {
        if (const auto card{ Owner().try_as<Editor::SettingsCard>() })
        {
            if (card.IsClickEnabled())
            {
                return AutomationControlType::Button;
            }
        }
        return AutomationControlType::Group;
    }

    hstring SettingsCardAutomationPeer::GetClassNameCore() const
    {
        return hstring{ L"SettingsCard" };
    }

    hstring SettingsCardAutomationPeer::GetNameCore() const
    {
        if (const auto card{ Owner().try_as<Editor::SettingsCard>() })
        {
            if (card.IsClickEnabled())
            {
                if (const auto manualName{ AutomationProperties::GetName(card) }; !manualName.empty())
                {
                    return manualName;
                }
                if (const auto headerString{ unbox_value_or<hstring>(card.Header(), hstring{}) }; !headerString.empty())
                {
                    return headerString;
                }
            }
            // Not clickable, or no header text: fall back to AutomationProperties.Name (matching
            // FrameworkElementAutomationPeer's default behavior).
            return AutomationProperties::GetName(card);
        }
        return {};
    }

    winrt::Windows::Foundation::IInspectable SettingsCardAutomationPeer::GetPatternCore(PatternInterface patternInterface) const
    {
        if (patternInterface == PatternInterface::Invoke)
        {
            if (const auto card{ Owner().try_as<Editor::SettingsCard>() })
            {
                if (card.IsClickEnabled())
                {
                    // Only provide Invoke pattern if the card is clickable.
                    return *this;
                }
            }
            return nullptr;
        }
        // ButtonBaseAutomationPeer only provides Invoke; everything else returns null.
        return nullptr;
    }
}
