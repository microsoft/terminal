constexpr std::wstring_view WtExe{ L"wt.exe" };
constexpr std::wstring_view WtdExe{ L"wtd.exe" };
constexpr std::wstring_view WindowsTerminalExe{ L"WindowsTerminal.exe" };
constexpr std::wstring_view LocalAppDataAppsPath{ L"%LOCALAPPDATA%\\Microsoft\\WindowsApps\\" };

_TIL_INLINEPREFIX bool IsPackaged()
{
    static const bool isPackaged = []() -> bool {
        try
        {
            const auto package = winrt::Windows::ApplicationModel::Package::Current();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }();
    return isPackaged;
}

// Function Description:
// - This is a helper to determine if we're running as a part of the Dev Build
//   Package or the release package. We'll need to return different text, icons,
//   and use different commandlines depending on which one the user requested.
// - Uses a C++11 "magic static" to make sure this is only computed once.
// - If we can't determine if it's the dev build or not, we'll default to true
// Arguments:
// - <none>
// Return Value:
// - true if we believe this extension is being run in the dev build package.
_TIL_INLINEPREFIX bool IsDevBuild()
{
    // use C++11 magic statics to make sure we only do this once.
    static const bool isDevBuild = []() -> bool {
        if (IsPackaged())
        {
            try
            {
                const auto package = winrt::Windows::ApplicationModel::Package::Current();
                const auto id = package.Id();
                const auto name = id.FullName();
                return til::starts_with(name, L"WindowsTerminalDev");
            }
            CATCH_LOG();
        }

        return true;
    }();
    return isDevBuild;
}

// Function Description:
// - Helper function for getting the path to the appropriate executable to use
//   for this instance of the shell extension. If we're running the dev build,
//   it should be a `wtd.exe`, but if we're preview or release, we want to make
//   sure to get the correct `wt.exe` that corresponds to _us_.
// - If we're unpackaged, this needs to get us `WindowsTerminal.exe`, because
//   the `wt*exe` alias won't have been installed for this install.
// Arguments:
// - <none>
// Return Value:
// - the full path to the exe, one of `wt.exe`, `wtd.exe`, or `WindowsTerminal.exe`.
_TIL_INLINEPREFIX const std::wstring& GetWtExePath()
{
    static const std::wstring exePath = []() -> std::wstring {
        // First, check a packaged location for the exe. If we've got a package
        // family name, that means we're one of the packaged Dev build, packaged
        // Release build, or packaged Preview build.
        //
        // If we're the preview or release build, there's no way of knowing if the
        // `wt.exe` on the %PATH% is us or not. Fortunately, _our_ execution alias
        // is located in "%LOCALAPPDATA%\Microsoft\WindowsApps\<our package family
        // name>", _always_, so we can use that to look up the exe easier.
        if (IsPackaged())
        {
            try
            {
                const auto package = winrt::Windows::ApplicationModel::Package::Current();
                const auto id = package.Id();
                const auto pfn = id.FamilyName();
                if (!pfn.empty())
                {
                    const std::filesystem::path windowsAppsPath{ wil::ExpandEnvironmentStringsW<std::wstring>(LocalAppDataAppsPath.data()) };
                    const std::filesystem::path wtPath = windowsAppsPath / std::wstring_view{ pfn } / (IsDevBuild() ? WtdExe : WtExe);
                    return wtPath;
                }
            }
            CATCH_LOG();
        }

        // If we're here, then we couldn't resolve our exe from the package. This
        // means we're running unpackaged. We should just use the
        // WindowsTerminal.exe that's sitting in the directory next to us.
        try
        {
            std::filesystem::path module = wil::GetModuleFileNameW<std::wstring>(nullptr);
            module.replace_filename(WindowsTerminalExe);
            return module;
        }
        CATCH_LOG();

        return std::wstring{ WtExe };
    }();
    return exePath;
}
