using System.ComponentModel;

/// <summary>
/// Represents the different release channels of Windows Terminal.
/// </summary>
internal enum TerminalRelease
{
    [Description("Windows Terminal (Stable)")]
    Stable,

    [Description("Windows Terminal Preview")]
    Preview,

    [Description("Windows Terminal Canary")]
    Canary,

    [Description("Windows Terminal Dev")]
    Dev
}

internal static class TerminalReleaseExtensions
{
    /// <summary>
    /// Returns the path to settings.json for the given release channel.
    /// </summary>
    public static string GetSettingsJsonPath(this TerminalRelease release)
    {
        var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);

        return release switch
        {
            TerminalRelease.Stable => Path.Combine(localAppData, @"Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json"),
            TerminalRelease.Preview => Path.Combine(localAppData, @"Packages\Microsoft.WindowsTerminalPreview_8wekyb3d8bbwe\LocalState\settings.json"),
            TerminalRelease.Canary => Path.Combine(localAppData, @"Packages\Microsoft.WindowsTerminalCanary_8wekyb3d8bbwe\LocalState\settings.json"),
            TerminalRelease.Dev => Path.Combine(localAppData, @"Packages\Microsoft.WindowsTerminalDev_8wekyb3d8bbwe\LocalState\settings.json"),
            _ => throw new ArgumentOutOfRangeException(nameof(release))
        };
    }

    /// <summary>
    /// Returns true if the given release channel has a settings.json on disk.
    /// </summary>
    public static bool IsInstalled(this TerminalRelease release)
    {
        return File.Exists(release.GetSettingsJsonPath());
    }

    /// <summary>
    /// Returns the most-preview installed channel.
    /// Delegates to <see cref="SettingsHelper.ResolveRelease"/>.
    /// </summary>
    public static TerminalRelease? DetectDefaultChannel()
    {
        return SettingsHelper.ResolveRelease();
    }
}
