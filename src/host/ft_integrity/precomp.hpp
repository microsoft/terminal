// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Windows
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <sddl.h>

// WRL
#include <wrl.h>

// WEX
#include <WexTestClass.h>
#define LOG_OUTPUT(fmt, ...) WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))

// wil
#include <wil/result.h>
#include <wil/tokenhelpers.h>

// STL
#include <thread>
#include <fstream>
#include <memory>
#include <cstring>

// AppModel TestHelper
#include <AppModelTestHelper.h>
