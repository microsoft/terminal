#pragma once

#include "ScratchClass.g.h"
namespace winrt::ScratchWinRTServer::implementation
{
    struct ScratchClass : public ScratchClassT<ScratchClass>
    {
        ScratchClass();
        hstring DoTheThing()
        {
            return L"Hello there";
        }

        Windows::UI::Xaml::Controls::Button MyButton();

    private:
        Windows::UI::Xaml::Controls::Button _MyButton;
    };
}

namespace winrt::ScratchWinRTServer::factory_implementation
{
    struct ScratchClass : ScratchClassT<ScratchClass, implementation::ScratchClass>
    {
    };
}
