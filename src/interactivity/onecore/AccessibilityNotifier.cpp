// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "AccessibilityNotifier.hpp"

using namespace Microsoft::Console::Interactivity::OneCore;

void AccessibilityNotifier::NotifyConsoleCaretEvent(_In_ RECT /*rectangle*/)
{
}

void AccessibilityNotifier::NotifyConsoleCaretEvent(_In_ ConsoleCaretEventFlags /*flags*/, _In_ LONG /*position*/)
{
}

void AccessibilityNotifier::NotifyConsoleUpdateScrollEvent(_In_ LONG /*x*/, _In_ LONG /*y*/)
{
}

void AccessibilityNotifier::NotifyConsoleUpdateSimpleEvent(_In_ LONG /*start*/, _In_ LONG /*charAndAttribute*/)
{
}

void AccessibilityNotifier::NotifyConsoleUpdateRegionEvent(_In_ LONG /*startXY*/, _In_ LONG /*endXY*/)
{
}

void AccessibilityNotifier::NotifyConsoleLayoutEvent()
{
}

void AccessibilityNotifier::NotifyConsoleStartApplicationEvent(_In_ DWORD /*processId*/)
{
}

void AccessibilityNotifier::NotifyConsoleEndApplicationEvent(_In_ DWORD /*processId*/)
{
}
