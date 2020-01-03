//-----------------------------------------------------------
// Copyright (c) Microsoft Corporation. All Rights Reserved.
//-----------------------------------------------------------
#pragma once

#include "UnitTestApp.g.h"

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
