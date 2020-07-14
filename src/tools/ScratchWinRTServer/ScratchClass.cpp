#include "pch.h"
#include "ScratchClass.h"

#include "ScratchClass.g.cpp"
namespace winrt::ScratchWinRTServer::implementation
{
    ScratchClass::ScratchClass()
    {
        _MyButton.Content(winrt::box_value({ L"This is a button" }));
    }
    Windows::UI::Xaml::Controls::Button ScratchClass::MyButton()
    {
        return _MyButton;
    }

}
