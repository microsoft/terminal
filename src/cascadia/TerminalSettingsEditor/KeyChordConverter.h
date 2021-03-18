// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChordToStringConverter.g.h"
#include "KeyChordToVisibilityConverter.g.h"
#include "../inc/cppwinrt_utils.h"

DECLARE_CONVERTER(winrt::Microsoft::Terminal::Settings::Editor, KeyChordToStringConverter);
DECLARE_CONVERTER(winrt::Microsoft::Terminal::Settings::Editor, KeyChordToVisibilityConverter);
