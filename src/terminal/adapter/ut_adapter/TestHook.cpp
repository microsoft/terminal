#include "precomp.h"
#include "TestHook.h"

using namespace TestHook;

thread_local HKL g_keyboardLayout;

extern "C" HKL TestHook_TerminalInput_KeyboardLayout()
{
    return g_keyboardLayout;
}

static bool isPreloadedLayout(const wchar_t* klid) noexcept
{
    wil::unique_hkey preloadKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Keyboard Layout\\Preload", 0, KEY_READ, preloadKey.addressof()) != ERROR_SUCCESS)
    {
        return false;
    }

    wil::unique_hkey substitutesKey;
    RegOpenKeyExW(HKEY_CURRENT_USER, L"Keyboard Layout\\Substitutes", 0, KEY_READ, substitutesKey.addressof());

    wchar_t idx[16];
    wchar_t layoutId[KL_NAMELENGTH];

    for (DWORD i = 0;; i++)
    {
        DWORD idxLen = ARRAYSIZE(idx);
        DWORD layoutIdSize = sizeof(layoutId);
        if (RegEnumValueW(preloadKey.get(), i, idx, &idxLen, nullptr, nullptr, reinterpret_cast<BYTE*>(layoutId), &layoutIdSize) != ERROR_SUCCESS)
        {
            break;
        }

        // Preload contains base language IDs (e.g. "0000040c").
        // The actual layout ID (e.g. "0001040c") may only appear in the Substitutes key.
        if (substitutesKey)
        {
            wchar_t substitute[KL_NAMELENGTH];
            DWORD substituteSize = sizeof(substitute);
            if (RegGetValueW(substitutesKey.get(), nullptr, layoutId, RRF_RT_REG_SZ, nullptr, substitute, &substituteSize) == ERROR_SUCCESS)
            {
                memcpy(layoutId, substitute, sizeof(layoutId));
            }
        }

        if (wcscmp(layoutId, klid) == 0)
        {
            return true;
        }
    }

    return false;
}

void LayoutGuard::_destroy() const noexcept
{
    if (g_keyboardLayout == _layout)
    {
        g_keyboardLayout = nullptr;
    }
    if (_needsUnload)
    {
        UnloadKeyboardLayout(_layout);
    }
}

LayoutGuard::LayoutGuard(HKL layout, bool needsUnload) noexcept :
    _layout{ layout },
    _needsUnload{ needsUnload }
{
}

LayoutGuard::~LayoutGuard()
{
    _destroy();
}

LayoutGuard::LayoutGuard(LayoutGuard&& other) noexcept :
    _layout{ std::exchange(other._layout, nullptr) },
    _needsUnload{ std::exchange(other._needsUnload, false) }
{
}

LayoutGuard& LayoutGuard::operator=(LayoutGuard&& other) noexcept
{
    if (this != &other)
    {
        _destroy();
        _layout = std::exchange(other._layout, nullptr);
        _needsUnload = std::exchange(other._needsUnload, false);
    }
    return *this;
}

LayoutGuard::operator bool() const noexcept
{
    return _layout != nullptr;
}

LayoutGuard::operator HKL() const noexcept
{
    return _layout;
}

LayoutGuard TestHook::SetTerminalInputKeyboardLayout(const wchar_t* klid)
{
    THROW_HR_IF_MSG(E_UNEXPECTED, g_keyboardLayout != nullptr, "Nested layout test overrides are not supported");

    // Check if the layout is installed. LoadKeyboardLayoutW silently returns the
    // current active layout if the requested one is missing.
    const auto keyPath = fmt::format(FMT_COMPILE(L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\{}"), klid);
    wil::unique_hkey key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ, key.addressof()) != ERROR_SUCCESS)
    {
        return {};
    }

    const auto layout = LoadKeyboardLayoutW(klid, KLF_NOTELLSHELL);
    THROW_LAST_ERROR_IF_NULL(layout);

    g_keyboardLayout = layout;

    // Unload the layout if it's not one of the user's layouts.
    // GetKeyboardLayoutList is unreliable for this purpose, as the keyboard layout API mutates global OS state.
    // If a process crashes or exits early without calling UnloadKeyboardLayout all future processes will get it
    // returned in their GetKeyboardLayoutList calls. Shell could fix it but alas. So we peek into the registry.
    const auto needsUnload = !isPreloadedLayout(klid);

    return { layout, needsUnload };
}
