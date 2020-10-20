// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "resource.h"

#include "winrt/Windows.ApplicationModel.h"

// clang-format off
enum class IconClass : uint8_t {
    None             = 0,
    Version_Pre      = 0b00000001,
    Version_Dev      = 0b00000010,
    Variant_HC       = 0b00000100,
    Variant_HC_White = 0b00001000,
};

DEFINE_ENUM_FLAG_OPERATORS(IconClass);

static constexpr std::array<std::pair<IconClass, int>, 9> s_iconClassMapping{
    std::pair{ IconClass::None,                                                              IDI_APPICON },
    std::pair{ IconClass::Version_Pre,                                                       IDI_APPICON_PRE },
    std::pair{ IconClass::Version_Dev,                                                       IDI_APPICON_DEV },

    // High Contrast
    std::pair{ IconClass::Variant_HC,                                                        IDI_APPICON_HC_B },
    std::pair{ IconClass::Variant_HC | IconClass::Variant_HC_White,                          IDI_APPICON_HC_W },
    std::pair{ IconClass::Version_Pre | IconClass::Variant_HC,                               IDI_APPICON_PRE_HC_B },
    std::pair{ IconClass::Version_Pre | IconClass::Variant_HC | IconClass::Variant_HC_White, IDI_APPICON_PRE_HC_W },
    std::pair{ IconClass::Version_Dev | IconClass::Variant_HC,                               IDI_APPICON_DEV_HC_B },
    std::pair{ IconClass::Version_Dev | IconClass::Variant_HC | IconClass::Variant_HC_White, IDI_APPICON_DEV_HC_W },
};
// clang-format on

static wchar_t* _GetActiveAppIcon()
{
    auto iconClass{ IconClass::None };
    try
    {
        const auto package{ winrt::Windows::ApplicationModel::Package::Current() };
        const auto id{ package.Id() };
        const auto name{ id.FullName() };
        const std::wstring_view nameView{ name };
        WI_UpdateFlag(iconClass, IconClass::Version_Pre, nameView.find(L"Preview") != std::wstring_view::npos);
        WI_UpdateFlag(iconClass, IconClass::Version_Dev, nameView.find(L"Dev") != std::wstring_view::npos);
    }
    catch (...)
    {
        // Just fall through and assume that we're un-badged.
    }

    HIGHCONTRASTW hcInfo{};
    hcInfo.cbSize = sizeof(hcInfo);

    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hcInfo), &hcInfo, 0))
    {
        if (WI_IsFlagSet(hcInfo.dwFlags, HCF_HIGHCONTRASTON))
        {
            WI_SetFlag(iconClass, IconClass::Variant_HC);

            if (hcInfo.lpszDefaultScheme)
            {
                const std::wstring_view theme{ hcInfo.lpszDefaultScheme };
                WI_UpdateFlag(iconClass, IconClass::Variant_HC_White, theme.find(L"White") != std::wstring_view::npos);
            }
        }
    }

    const auto found{ std::find_if(s_iconClassMapping.cbegin(), s_iconClassMapping.cend(), [&](auto&& entry) { return entry.first == iconClass; }) };
    const auto resource{ found != s_iconClassMapping.cend() ? found->second : IDI_APPICON };
    return MAKEINTRESOURCEW(resource);
}

void UpdateWindowIconForActiveMetrics(HWND window)
{
    auto iconResource{ _GetActiveAppIcon() };

    // These handles are loaded with LR_SHARED, so they are safe to "leak".
    HANDLE smallIcon{ LoadImageW(wil::GetModuleInstanceHandle(), iconResource, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED) };
    LOG_LAST_ERROR_IF_NULL(smallIcon);

    HANDLE largeIcon{ LoadImageW(wil::GetModuleInstanceHandle(), iconResource, IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED) };
    LOG_LAST_ERROR_IF_NULL(largeIcon);

    if (smallIcon)
    {
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }
    if (largeIcon)
    {
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
    }
}
