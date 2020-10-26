/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Abstract:
- This header stores our default namespace guid. This is used in the creation of
  default and in-box dynamic profiles. It also provides a helper function for
  creating a "default" profile. Prior to GH#754, this was used to create the
  cmd, powershell, wsl, pwsh, and azure profiles. Now, this helper is used for
  any of the in-box dynamic profile generators.

Author(s):
- Mike Griese - August 2019
-- */
#pragma once

#include "Profile.h"

winrt::Microsoft::Terminal::Settings::Model::Profile CreateDefaultProfile(const std::wstring_view name, const std::wstring_view source);
