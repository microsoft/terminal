/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Abstract:
- This header simply contains all the namespaces of the "legacy" dynamic profile
  generators. These generators were built-in to the code before we had a proper
  concept of a dynamic profile source. As such, these profiles will exist in
  user's settings without a `source` attribute, and we'll need to make sure to
  handle layering them specially.

Author(s):
- Mike Griese - August 2019

--*/

#pragma once

static constexpr std::wstring_view WslGeneratorNamespace{ L"Windows.Terminal.Wsl" };
static constexpr std::wstring_view AzureGeneratorNamespace{ L"Windows.Terminal.Azure" };
static constexpr std::wstring_view TelnetGeneratorNamespace{ L"Windows.Terminal.Telnet" };
static constexpr std::wstring_view PowershellCoreGeneratorNamespace{ L"Windows.Terminal.PowershellCore" };
