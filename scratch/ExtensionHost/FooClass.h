#pragma once

#include "FooClass.g.h"

namespace winrt::ExtensionHost::implementation
{
    struct FooClass : FooClassT<FooClass>
    {
        FooClass() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);

    private:
        int32_t _MyProperty{ 42 };
    };
}

namespace winrt::ExtensionHost::factory_implementation
{
    struct FooClass : FooClassT<FooClass, implementation::FooClass>
    {
    };
}
