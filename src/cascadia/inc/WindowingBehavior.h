/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/

#pragma once
constexpr int32_t WindowingBehaviorUseCurrent{ 0 };
constexpr int32_t WindowingBehaviorUseNew{ -1 };
constexpr int32_t WindowingBehaviorUseExisting{ -2 };
constexpr int32_t WindowingBehaviorUseAnyExisting{ -3 };
constexpr int32_t WindowingBehaviorUseName{ -4 };
constexpr int32_t WindowingBehaviorUseNone{ -5 };

static constexpr std::wstring_view QuakeWindowName{ L"_quake" };
