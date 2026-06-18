/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CursorAnimationState.h

Abstract:
- Per-pane cursor smear animation state. Tracks previous cursor position,
  drives exponential ease-out interpolation with TUI-compatible deferred
  jump detection. Used by BackendD3D to render cursor trail effects.

Usage:
  CursorAnimationState state;
  state.smearEnabled = true;
  
  // Each frame, call onPreRender() with the current cursor rect.
  // It returns true if the smear animation is active.
  if (state.onPreRender(cursorRect, nowQpc, qpcFreq))
  {
      // Use state.animX, state.animY for smear rendering
      DrawSmear(state.animX, state.animY, cursorRect);
  }

--*/

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

struct CursorAnimationState
{
    // Configuration (set once when settings change)
    bool smearEnabled = false;
    float animLength = 0.15f;
    float trailSize = 1.0f;

    // Animated smear position (pixel coordinates)
    float animX = 0.0f;
    float animY = 0.0f;

    // Whether the animation is currently active
    bool active = false;

    // Must be called every frame before rendering.
    // Returns true if the smear animation is active (smear should be drawn).
    // `cursorRect` is the current cursor rectangle in cell coordinates.
    // `nowQpc` is the current QPC value. `qpcFreq` is the QPC frequency.
    bool onPreRender(int cursorLeft, int cursorTop, uint64_t nowQpc, uint64_t qpcFreq,
                     float cellWidth, float cellHeight) noexcept
    {
        if (!smearEnabled)
        {
            active = false;
            return false;
        }

        // Target position in pixels (center of cell).
        const float targetX = (static_cast<float>(cursorLeft) + 0.5f) * cellWidth;
        const float targetY = (static_cast<float>(cursorTop) + 0.5f) * cellHeight;

        // On first valid frame, just initialize.
        if (_lastLeft < 0)
        {
            animX = targetX;
            animY = targetY;
            _lastLeft = cursorLeft;
            _lastTop = cursorTop;
            active = false;
            return false;
        }

        // Detect cursor movement.
        const int dx = cursorLeft - _lastLeft;
        const int dy = cursorTop - _lastTop;
        const int dist = std::abs(dx) + std::abs(dy);

        if (dist > 1)
        {
            // Multi-cell jump: arm the smear.
            _armTime = nowQpc;
            _lastArmX = animX;
            _lastArmY = animY;
            active = true;

            // Compute the target pixel position for the animation.
            _armTargetX = targetX;
            _armTargetY = targetY;

            // If we were in a deferred hold, resolve it.
            _deferred = false;
        }
        else if (dist == 1)
        {
            // Single-cell movement: snap immediately.
            animX = targetX;
            animY = targetY;
            active = false;
        }
        // else: no movement, continue animating if active.

        // Advance the animation.
        if (active)
        {
            const double elapsed = static_cast<double>(nowQpc - _armTime) /
                                    static_cast<double>(qpcFreq);
            const float t = std::min(static_cast<float>(elapsed) /
                                     std::max(animLength, 0.016f), 1.0f);

            // Ease-out: slow down as we approach the target.
            const float eased = 1.0f - std::exp(-5.0f * t);

            animX = _lastArmX + (_armTargetX - _lastArmX) * eased;
            animY = _lastArmY + (_armTargetY - _lastArmY) * eased;

            // Snap when close enough.
            if (t >= 1.0f ||
                (std::abs(animX - _armTargetX) < 0.5f &&
                 std::abs(animY - _armTargetY) < 0.5f))
            {
                animX = _armTargetX;
                animY = _armTargetY;
                active = false;
            }
        }

        // Save for next frame.
        _lastLeft = cursorLeft;
        _lastTop = cursorTop;

        return active;
    }

    // Resets all state (e.g., when cursor is hidden or settings change).
    void reset() noexcept
    {
        _lastLeft = -1;
        _lastTop = -1;
        active = false;
        _deferred = false;
    }

private:
    int _lastLeft = -1;
    int _lastTop = -1;
    uint64_t _armTime = 0;
    float _lastArmX = 0.0f;
    float _lastArmY = 0.0f;
    float _armTargetX = 0.0f;
    float _armTargetY = 0.0f;
    bool _deferred = false;
};
