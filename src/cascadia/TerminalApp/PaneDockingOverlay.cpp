// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneDockingOverlay.h"
#include <LibraryResources.h>
#include <winrt/Windows.UI.ViewManagement.h>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::UI::Xaml::Media::Animation;
using Direction = winrt::Microsoft::Terminal::Settings::Model::SplitDirection;

namespace
{
    constexpr Direction directions[]{ Direction::Up, Direction::Left, Direction::Right, Direction::Down };

    Brush resourceBrush(const wchar_t* key)
    {
        return Application::Current().Resources().Lookup(winrt::box_value(key)).as<Brush>();
    }

    void place(const FrameworkElement& element, const Rect bounds)
    {
        Canvas::SetLeft(element, bounds.X);
        Canvas::SetTop(element, bounds.Y);
        element.Width(bounds.Width);
        element.Height(bounds.Height);
    }
}

PaneDockingOverlay::PaneDockingOverlay()
{
    _root.AllowDrop(true);
    _root.Background(SolidColorBrush{ Colors::Transparent() });
    _root.Children().Append(_canvas);
    _preview.IsHitTestVisible(false);
    _preview.BorderThickness(ThicknessHelper::FromUniformLength(2));
    _preview.Visibility(Visibility::Collapsed);
    _canvas.Children().Append(_preview);
    Hide();
}

void PaneDockingOverlay::Show(const std::shared_ptr<Pane>& destination, const std::shared_ptr<Pane>& incoming)
{
    if (_destination.lock() != destination || _incoming.lock() != incoming)
    {
        ClearPreview();
        _zones.clear();
    }
    _destination = destination;
    _incoming = incoming;
    _root.Visibility(Visibility::Visible);
    _root.IsHitTestVisible(true);
    _Refresh();
}

void PaneDockingOverlay::_Refresh()
{
    auto destination = _destination.lock();
    auto incoming = _incoming.lock();
    if (!destination || !incoming)
    {
        Hide();
        return;
    }

    std::vector<Zone> zones;
    for (const auto& pane : destination->GetLeafPanes())
    {
        const auto element = pane->GetRootElement();
        if (element.ActualWidth() <= 0 || element.ActualHeight() <= 0)
        {
            continue;
        }
        const auto bounds = element.TransformToVisual(_root).TransformBounds(
            Rect{ 0, 0, static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight()) });
        for (const auto direction : directions)
        {
            const auto layout = pane->CalculateDockLayout(incoming, direction);
            auto preview = layout ? layout->incoming : Rect{};
            preview.X += bounds.X;
            preview.Y += bounds.Y;
            zones.push_back({ { pane, direction, preview }, ZoneBounds(bounds, direction), layout.has_value(), nullptr });
        }
    }

    const auto same = zones.size() == _zones.size() && std::equal(zones.begin(), zones.end(), _zones.begin(), [](const auto& a, const auto& b) {
                          return a.hit.pane == b.hit.pane && a.hit.direction == b.hit.direction &&
                                 a.hit.preview == b.hit.preview && a.bounds == b.bounds && a.enabled == b.enabled;
                      });
    if (same)
    {
        return;
    }

    ClearPreview();
    _canvas.Children().Clear();
    _preview.Background(resourceBrush(L"SystemControlHighlightAccentBrush"));
    _preview.BorderBrush(resourceBrush(L"SystemControlForegroundBaseHighBrush"));
    _canvas.Children().Append(_preview);
    for (auto& zone : zones)
    {
        Border visual;
        visual.Background(resourceBrush(L"SystemControlBackgroundChromeMediumLowBrush"));
        visual.BorderBrush(resourceBrush(L"SystemControlForegroundBaseHighBrush"));
        visual.BorderThickness(ThicknessHelper::FromUniformLength(1));
        visual.Opacity(zone.enabled ? 1 : 0.35);
        TextBlock glyph;
        glyph.FontFamily(FontFamily{ L"Segoe MDL2 Assets" });
        glyph.FontSize(14);
        glyph.HorizontalAlignment(HorizontalAlignment::Center);
        glyph.VerticalAlignment(VerticalAlignment::Center);
        glyph.Foreground(resourceBrush(L"SystemControlForegroundBaseHighBrush"));
        winrt::hstring name;
        switch (zone.hit.direction)
        {
        case Direction::Up:
            glyph.Text(L"\xE74A");
            name = RS_(L"DockPaneTop");
            break;
        case Direction::Left:
            glyph.Text(L"\xE72B");
            name = RS_(L"DockPaneLeft");
            break;
        case Direction::Right:
            glyph.Text(L"\xE72A");
            name = RS_(L"DockPaneRight");
            break;
        case Direction::Down:
            glyph.Text(L"\xE74B");
            name = RS_(L"DockPaneBottom");
            break;
        default:
            break;
        }
        Automation::AutomationProperties::SetName(visual, name);
        ToolTipService::SetToolTip(visual, winrt::box_value(name));
        visual.Child(glyph);
        place(visual, zone.bounds);
        _canvas.Children().Append(visual);
        zone.visual = visual;
    }
    _zones = std::move(zones);
}

std::optional<PaneDockingOverlay::Hit> PaneDockingOverlay::DragOver(const Point point)
{
    _Refresh();
    for (const auto& zone : _zones)
    {
        if (zone.enabled && Contains(zone.bounds, point))
        {
            _SetPreview(zone.hit);
            return zone.hit;
        }
    }
    ClearPreview();
    return std::nullopt;
}

void PaneDockingOverlay::_SetPreview(const Hit& hit)
{
    if (_hovered && _hovered->pane == hit.pane && _hovered->direction == hit.direction && _hovered->preview == hit.preview)
    {
        return;
    }
    ClearPreview();
    _hovered = hit;
    for (const auto& zone : _zones)
    {
        if (zone.hit.pane == hit.pane && zone.hit.direction == hit.direction)
        {
            zone.visual.BorderThickness(ThicknessHelper::FromUniformLength(3));
        }
    }
    place(_preview, hit.preview);
    _preview.Visibility(Visibility::Visible);
    _preview.Opacity(0.35);
    if (winrt::Windows::UI::ViewManagement::UISettings{}.AnimationsEnabled())
    {
        DoubleAnimation fade;
        fade.From(0.0);
        fade.To(0.35);
        fade.Duration(DurationHelper::FromTimeSpan(std::chrono::milliseconds{ 120 }));
        Storyboard::SetTarget(fade, _preview);
        Storyboard::SetTargetProperty(fade, L"Opacity");
        _animation.Children().Clear();
        _animation.Children().Append(fade);
        _animation.Begin();
    }
}

void PaneDockingOverlay::ClearPreview()
{
    _animation.Stop();
    _preview.Visibility(Visibility::Collapsed);
    _hovered.reset();
    for (const auto& zone : _zones)
    {
        zone.visual.BorderThickness(ThicknessHelper::FromUniformLength(1));
    }
}

void PaneDockingOverlay::Hide()
{
    ClearPreview();
    _root.Visibility(Visibility::Collapsed);
    _root.IsHitTestVisible(false);
    _destination.reset();
    _incoming.reset();
    _zones.clear();
    _canvas.Children().Clear();
}
