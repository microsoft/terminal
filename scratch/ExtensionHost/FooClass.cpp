#include "pch.h"
#include "FooClass.h"
#include "FooClass.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::ExtensionHost::implementation
{
    int32_t FooClass::MyProperty()
    {
        return _MyProperty;
    }

    void FooClass::MyProperty(int32_t value)
    {
        _MyProperty = value;
    }
}
