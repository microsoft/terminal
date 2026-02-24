//-----------------------------------------------------------
// Copyright (c) Microsoft Corporation. All Rights Reserved.
//-----------------------------------------------------------
#pragma once

#include "UnitTestApp.g.h"

// NOTE: 03-Jan-2020
// This class is horrifyingly defined in CX, _NOT_ CppWinrt. It was largely
// taken straight from the TAEF sample code. However, it does _work_, and it's
// not about to be changed ever, so it's not worth the effort to try and port it
// to cppwinrt at this time.

namespace TestHostApp
{
    /// <summary>
    /// Provides application-specific behavior to supplement the default Application class.
    /// </summary>
    ref class App sealed
    {
    protected:
        virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs ^ e) override;

        internal :
            App();
    };
}
