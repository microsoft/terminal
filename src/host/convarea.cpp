// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "_output.h"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using Microsoft::Console::Interactivity::ServiceLocator;

bool IsValidSmallRect(_In_ PSMALL_RECT const Rect)
{
    return (Rect->Right >= Rect->Left && Rect->Bottom >= Rect->Top);
}

void WriteConvRegionToScreen(const SCREEN_INFORMATION& ScreenInfo,
                             const Viewport& convRegion)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (!ScreenInfo.IsActiveScreenBuffer())
    {
        return;
    }

    ConsoleImeInfo* const pIme = &gci.ConsoleIme;

    for (unsigned int i = 0; i < pIme->ConvAreaCompStr.size(); ++i)
    {
        const auto& ConvAreaInfo = pIme->ConvAreaCompStr[i];

        if (!ConvAreaInfo.IsHidden())
        {
            const auto currentViewport = ScreenInfo.GetViewport().ToInclusive();
            const auto areaInfo = ConvAreaInfo.GetAreaBufferInfo();

            // Do clipping region
            SMALL_RECT Region;
            Region.Left = currentViewport.Left + areaInfo.rcViewCaWindow.Left + areaInfo.coordConView.X;
            Region.Right = Region.Left + (areaInfo.rcViewCaWindow.Right - areaInfo.rcViewCaWindow.Left);
            Region.Top = currentViewport.Top + areaInfo.rcViewCaWindow.Top + areaInfo.coordConView.Y;
            Region.Bottom = Region.Top + (areaInfo.rcViewCaWindow.Bottom - areaInfo.rcViewCaWindow.Top);

            SMALL_RECT ClippedRegion;
            ClippedRegion.Left = std::max(Region.Left, currentViewport.Left);
            ClippedRegion.Top = std::max(Region.Top, currentViewport.Top);
            ClippedRegion.Right = std::min(Region.Right, currentViewport.Right);
            ClippedRegion.Bottom = std::min(Region.Bottom, currentViewport.Bottom);

            if (IsValidSmallRect(&ClippedRegion))
            {
                Region = ClippedRegion;
                ClippedRegion.Left = std::max(Region.Left, convRegion.Left());
                ClippedRegion.Top = std::max(Region.Top, convRegion.Top());
                ClippedRegion.Right = std::min(Region.Right, convRegion.RightInclusive());
                ClippedRegion.Bottom = std::min(Region.Bottom, convRegion.BottomInclusive());
                if (IsValidSmallRect(&ClippedRegion))
                {
                    // if we have a renderer, we need to update.
                    // we've already confirmed (above with an early return) that we're on conversion areas that are a part of the active (visible/rendered) screen
                    // so send invalidates to those regions such that we're queried for data on the next frame and repainted.
                    if (ServiceLocator::LocateGlobals().pRender != nullptr)
                    {
                        // convert inclusive rectangle to exclusive rectangle
                        SMALL_RECT srExclusive = ClippedRegion;
                        srExclusive.Right++;
                        srExclusive.Bottom++;

                        ServiceLocator::LocateGlobals().pRender->TriggerRedraw(Viewport::FromExclusive(srExclusive));
                    }
                }
            }
        }
    }
}

[[nodiscard]] HRESULT ConsoleImeResizeCompStrView()
{
    try
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        ConsoleImeInfo* const pIme = &gci.ConsoleIme;
        pIme->RedrawCompMessage();
    }
    CATCH_RETURN();

    return S_OK;
}

[[nodiscard]] HRESULT ConsoleImeResizeCompStrScreenBuffer(const COORD coordNewScreenSize)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    ConsoleImeInfo* const pIme = &gci.ConsoleIme;

    return pIme->ResizeAllAreas(coordNewScreenSize);
}

[[nodiscard]] HRESULT ImeStartComposition()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole();
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    gci.pInputBuffer->fInComposition = true;
    return S_OK;
}

[[nodiscard]] HRESULT ImeEndComposition()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole();
    auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

    gci.pInputBuffer->fInComposition = false;
    return S_OK;
}

[[nodiscard]] HRESULT ImeComposeData(std::wstring_view text,
                                     std::basic_string_view<BYTE> attributes,
                                     std::basic_string_view<WORD> colorArray)
{
    try
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.LockConsole();
        auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

        ConsoleImeInfo* const pIme = &gci.ConsoleIme;
        pIme->WriteCompMessage(text, attributes, colorArray);
    }
    CATCH_RETURN();
    return S_OK;
}

[[nodiscard]] HRESULT ImeClearComposeData()
{
    try
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.LockConsole();
        auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

        ConsoleImeInfo* const pIme = &gci.ConsoleIme;
        pIme->ClearAllAreas();
    }
    CATCH_RETURN();
    return S_OK;
}

[[nodiscard]] HRESULT ImeComposeResult(std::wstring_view text)
{
    try
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.LockConsole();
        auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });

        ConsoleImeInfo* const pIme = &gci.ConsoleIme;
        pIme->WriteResultMessage(text);
    }
    CATCH_RETURN();
    return S_OK;
}
