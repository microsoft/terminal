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

// Thinking of adding a new VerifyOutputTraits for a new type? MAKE SURE that
// you include this header (or at least the relevant definition) before _every_
// Verify for that type.
//
// From a thread on the matter in 2018:
// > my best guess is that one of your cpp files uses a COORD in a Verify macro
// > without including consoletaeftemplates.hpp. This caused the
// > VerifyOutputTraits<COORD> symbol to be used with the default
// > implementation. In other of your cpp files, you did include
// > consoletaeftemplates.hpp properly and they would have compiled the actual
// > specialization from consoletaeftemplates.hpp into the corresponding obj
// > file for that cpp file. When the test DLL was linked, the linker picks one
// > of the multiple definitions available from the different obj files for
// > VerifyOutputTraits<COORD>. The linker happened to pick the one from the cpp
// > file where consoletaeftemplates.hpp was not included properly. I’ve
// > encountered a similar situation before and it was baffling because the
// > compiled code was obviously doing different behavior than what the source
// > code said. This can happen when you violate the one-definition rule.

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
