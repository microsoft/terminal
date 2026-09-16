// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#define BLOCK_TIL
// This includes support libraries from the CRT, STL, WIL, and GSL
#include <LibraryIncludes.h>

#include <wil/cppwinrt.h>
#include <Unknwn.h>
#include <hstring.h>

#include <json/json.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Microsoft.Terminal.Core.h>
#include <winrt/Microsoft.Terminal.Control.h>
#include <winrt/Microsoft.Terminal.Settings.Model.h>

// Manually include til after we include Windows.Foundation to give it winrt superpowers
#include "til.h"
#include <til/u8u16convert.h>

#include <iostream>
