#include "pch.h"
#include "Class.h"
#include "Class.g.cpp"

namespace winrt::ExtensionComponent::implementation
{
    int32_t Class::MyProperty()
    {
        return 99;
    }

    void Class::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }
}
