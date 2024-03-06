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

// Magic names for magic windowing behaviors. These are reserved names, in place
// of window names. "new" can also be used in MoveTab / MovePane actions.
// * new: to use a new window, always
// * last: use the most recent window
inline constexpr std::string_view NewWindow{ "new" };
inline constexpr std::string_view MostRecentlyUsedWindow{ "last" };
