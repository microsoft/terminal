// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "AccessibilityNotifier.hpp"

using namespace Microsoft::Console::Interactivity::OneCore;

void AccessibilityNotifier::NotifyConsoleCaretEvent(_In_ const til::rect& /*rectangle*/) noexcept
{
}

void AccessibilityNotifier::NotifyConsoleCaretEvent(_In_ ConsoleCaretEventFlags /*flags*/, _In_ LONG /*position*/) noexcept
{
}

void AccessibilityNotifier::NotifyConsoleUpdateScrollEvent(_In_ LONG /*x*/, _In_ LONG /*y*/) noexcept
{
}

void AccessibilityNotifier::NotifyConsoleUpdateSimpleEvent(_In_ LONG /*start*/, _In_ LONG /*charAndAttribute*/) noexcept
{
}

void AccessibilityNotifier::NotifyConsoleUpdateRegionEvent(_In_ LONG /*startXY*/, _In_ LONG /*endXY*/) noexcept
{
}

void AccessibilityNotifier::NotifyConsoleLayoutEvent() noexcept
{
}

void AccessibilityNotifier::NotifyConsoleStartApplicationEvent(_In_ DWORD /*processId*/) noexcept
{
}

void AccessibilityNotifier::NotifyConsoleEndApplicationEvent(_In_ DWORD /*processId*/) noexcept
{
}
