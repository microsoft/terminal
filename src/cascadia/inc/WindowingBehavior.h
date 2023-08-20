/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/

#pragma once
inline constexpr int32_t WindowingBehaviorUseCurrent{ 0 };
inline constexpr int32_t WindowingBehaviorUseNew{ -1 };
inline constexpr int32_t WindowingBehaviorUseExisting{ -2 };
inline constexpr int32_t WindowingBehaviorUseAnyExisting{ -3 };
inline constexpr int32_t WindowingBehaviorUseName{ -4 };
inline constexpr int32_t WindowingBehaviorUseNone{ -5 };

inline constexpr std::wstring_view QuakeWindowName{ L"_quake" };
