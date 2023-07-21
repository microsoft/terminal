/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- ProposeCommandlineResult.h

Abstract:
- This is a helper class for encapsulating the result of a
  Monarch::ProposeCommandline call. The monarch will be telling the new process
  whether it should create a new window or not. If the value of
  ShouldCreateWindow is false, that implies that some other window process was
  given the commandline for handling, and the caller should just exit.
- If ShouldCreateWindow is true, the Id property may or may not contain an ID
  that the new window should use as its ID.

--*/

#pragma once

#include "ProposeCommandlineResult.g.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct ProposeCommandlineResult : public ProposeCommandlineResultT<ProposeCommandlineResult>
    {
    public:
        ProposeCommandlineResult(const Remoting::ProposeCommandlineResult& other) :
            _Id{ other.Id() },
            _WindowName{ other.WindowName() },
            _ShouldCreateWindow{ other.ShouldCreateWindow() } {};

        WINRT_PROPERTY(Windows::Foundation::IReference<uint64_t>, Id);
        WINRT_PROPERTY(winrt::hstring, WindowName);
        WINRT_PROPERTY(bool, ShouldCreateWindow, true);

    public:
        ProposeCommandlineResult(bool shouldCreateWindow) :
            _ShouldCreateWindow{ shouldCreateWindow } {};
    };
}
