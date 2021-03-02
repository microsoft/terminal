/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Toast.h
--*/

#pragma once
#include "pch.h"

class Toast : public std::enable_shared_from_this<Toast>
{
public:
    Toast(const winrt::Microsoft::UI::Xaml::Controls::TeachingTip& tip);
    void Open();

private:
    winrt::Microsoft::UI::Xaml::Controls::TeachingTip _tip;
    winrt::Windows::UI::Xaml::DispatcherTimer _timer;
};
