// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// This includes support libraries from the CRT, STL, WIL, and GSL
#define BLOCK_TIL // We want to include it later, after DX.
#include "LibraryIncludes.h"

#include <windows.h>
#include <winmeta.h>

#include "../host/conddkrefs.h"
#include <condrv.h>

#include <cmath>

#include <algorithm>
#include <exception>
#include <numeric>
#include <typeinfo>
#include <stdexcept>

#include <dcomp.h>

#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>

#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1_2.h>
#include <d2d1_3.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

// Re-include TIL at the bottom to gain DX superpowers.
#include "til.h"

#pragma hdrstop
