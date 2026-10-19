// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Pane.h"
#include <algorithm>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>

// Presentation only: the caller owns the drag payload and commits pane moves.
class PaneDockingOverlay
{
public:
    struct Hit
    {
        std::shared_ptr<Pane> pane;
        winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction;
        winrt::Windows::Foundation::Rect preview;
    };

    PaneDockingOverlay();
    winrt::Windows::UI::Xaml::Controls::Grid Root() const { return _root; }
    void Show(const std::shared_ptr<Pane>& destination, const std::shared_ptr<Pane>& incoming);
    std::optional<Hit> DragOver(winrt::Windows::Foundation::Point point);
    void ClearPreview();
    void Hide();

    static constexpr winrt::Windows::Foundation::Rect ZoneBounds(const winrt::Windows::Foundation::Rect bounds, const winrt::Microsoft::Terminal::Settings::Model::SplitDirection direction) noexcept
    {
        using Direction = winrt::Microsoft::Terminal::Settings::Model::SplitDirection;
        const auto cell = std::min({ 28.0f, bounds.Width / 3.0f, bounds.Height / 3.0f });
        auto x = bounds.X + bounds.Width / 2 - cell / 2;
        auto y = bounds.Y + bounds.Height / 2 - cell / 2;
        switch (direction)
        {
        case Direction::Up:
            y -= cell;
            break;
        case Direction::Left:
            x -= cell;
            break;
        case Direction::Right:
            x += cell;
            break;
        case Direction::Down:
            y += cell;
            break;
        default:
            return {};
        }
        return { x, y, cell, cell };
    }

    static constexpr bool Contains(const winrt::Windows::Foundation::Rect bounds, const winrt::Windows::Foundation::Point point) noexcept
    {
        return point.X >= bounds.X && point.X < bounds.X + bounds.Width &&
               point.Y >= bounds.Y && point.Y < bounds.Y + bounds.Height;
    }

private:
    struct Zone
    {
        Hit hit;
        winrt::Windows::Foundation::Rect bounds;
        bool enabled;
        winrt::Windows::UI::Xaml::Controls::Border visual{ nullptr };
    };

    winrt::Windows::UI::Xaml::Controls::Grid _root;
    winrt::Windows::UI::Xaml::Controls::Canvas _canvas;
    winrt::Windows::UI::Xaml::Controls::Border _preview;
    winrt::Windows::UI::Xaml::Media::Animation::Storyboard _animation;
    std::weak_ptr<Pane> _destination;
    std::weak_ptr<Pane> _incoming;
    std::vector<Zone> _zones;
    std::optional<Hit> _hovered;

    void _Refresh();
    void _SetPreview(const Hit& hit);
};
