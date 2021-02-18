/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- winrtTaefTemplates.hpp

Abstract:
- This module contains common TAEF templates for winrt-isms that don't otherwise
  have them. This is very similar to consoleTaefTemplates, but this one presumes
  that the winrt headers have already been included.

Author:
- Mike Griese 2021

--*/

#pragma once

namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<winrt::hstring>
    {
    public:
        static WEX::Common::NoThrowString ToString(const winrt::hstring& hstr)
        {
            return WEX::Common::NoThrowString().Format(L"%s", hstr.c_str());
        }
    };
}
