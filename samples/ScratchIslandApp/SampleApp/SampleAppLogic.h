// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "SampleAppLogic.g.h"
#include "MyPage.h"
#include "../../../src/cascadia/inc/cppwinrt_utils.h"

namespace winrt::SampleApp::implementation
{
    struct SampleAppLogic : SampleAppLogicT<SampleAppLogic>
    {
    public:
        static SampleAppLogic* Current() noexcept;

        SampleAppLogic();
        ~SampleAppLogic() = default;

        void Create();

        Windows::UI::Xaml::UIElement GetRoot() noexcept;

        winrt::hstring Title();

    private:
        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the SampleAppLogic.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in _ApplyTheme. The root currently is _root.
        winrt::com_ptr<MyPage> _root{ nullptr };
    };
}

namespace winrt::SampleApp::factory_implementation
{
    struct SampleAppLogic : SampleAppLogicT<SampleAppLogic, implementation::SampleAppLogic>
    {
    };
}
