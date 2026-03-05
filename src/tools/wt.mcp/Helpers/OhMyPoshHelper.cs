using System.Text.RegularExpressions;

/// <summary>
/// Helpers for detecting and interacting with Oh My Posh installations.
/// </summary>
internal static partial class OhMyPoshHelper
{
    // Well-known install locations for Oh My Posh on Windows
    private static readonly string[] s_knownPaths =
    [
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "oh-my-posh", "bin", "oh-my-posh.exe"),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "oh-my-posh", "bin", "oh-my-posh.exe"),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "oh-my-posh", "bin", "oh-my-posh.exe"),
    ];

    /// <summary>
    /// Finds the oh-my-posh executable. Checks well-known install locations first,
    /// then falls back to where.exe PATH lookup. The MCP server process may not have
    /// the same PATH as the user's interactive shell.
    /// </summary>
    public static string? FindExecutable()
    {
        // Check well-known locations first (most reliable — PATH may differ for this process)
        foreach (var knownPath in s_knownPaths)
        {
            if (File.Exists(knownPath))
            {
                return knownPath;
            }
        }

        // Fall back to where.exe PATH search
        var result = ProcessHelper.Run("where.exe", "oh-my-posh");
        if (result is null || result.ExitCode != 0 || string.IsNullOrWhiteSpace(result.Stdout))
        {
            return null;
        }

        var firstLine = result.Stdout.Split('\n', StringSplitOptions.RemoveEmptyEntries)[0].Trim();
        return File.Exists(firstLine) ? firstLine : null;
    }

    /// <summary>
    /// Gets the Oh My Posh version string, or null if not installed.
    /// Uses the resolved executable path rather than relying on PATH.
    /// </summary>
    public static string? GetVersion()
    {
        var exePath = FindExecutable();
        if (exePath is null)
        {
            return null;
        }

        try
        {
            var info = System.Diagnostics.FileVersionInfo.GetVersionInfo(exePath);
            var version = (info.ProductVersion ?? info.FileVersion)?.Trim('\r', '\n', '\0', ' ');
            return string.IsNullOrEmpty(version) ? null : version;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    /// Returns the themes directory path. Checks POSH_THEMES_PATH env var first,
    /// then derives from executable location, then checks the default install path.
    /// </summary>
    public static string? GetThemesDirectory()
    {
        var envPath = Environment.GetEnvironmentVariable("POSH_THEMES_PATH");
        if (!string.IsNullOrEmpty(envPath) && Directory.Exists(envPath))
        {
            return envPath;
        }

        // Derive from executable location: .../oh-my-posh/bin/oh-my-posh.exe → .../oh-my-posh/themes
        var exePath = FindExecutable();
        if (exePath is not null)
        {
            var binDir = Path.GetDirectoryName(exePath);
            if (binDir is not null)
            {
                var themesFromExe = Path.Combine(Path.GetDirectoryName(binDir)!, "themes");
                if (Directory.Exists(themesFromExe))
                {
                    return themesFromExe;
                }
            }
        }

        // Default location: %LOCALAPPDATA%\Programs\oh-my-posh\themes
        var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var defaultPath = Path.Combine(localAppData, "Programs", "oh-my-posh", "themes");
        if (Directory.Exists(defaultPath))
        {
            return defaultPath;
        }

        return null;
    }

    /// <summary>
    /// Lists all available theme files in the themes directory.
    /// Returns (name, fullPath) tuples.
    /// </summary>
    public static List<(string Name, string FullPath)> ListThemes()
    {
        var themesDir = GetThemesDirectory();
        if (themesDir is null)
        {
            return [];
        }

        var themes = new List<(string, string)>();
        foreach (var ext in new[] { "*.omp.json", "*.omp.yaml", "*.omp.toml" })
        {
            foreach (var file in Directory.GetFiles(themesDir, ext))
            {
                var name = Path.GetFileName(file);
                // Strip the .omp.{ext} suffix for a clean name
                var cleanName = name;
                if (cleanName.Contains(".omp."))
                {
                    cleanName = cleanName[..cleanName.IndexOf(".omp.")];
                }
                themes.Add((cleanName, file));
            }
        }

        themes.Sort((a, b) => string.Compare(a.Item1, b.Item1, StringComparison.OrdinalIgnoreCase));
        return themes;
    }

    /// <summary>
    /// Attempts to find the user's current OMP config path by scanning the PowerShell profile.
    /// Returns the config file path if found, null otherwise.
    /// </summary>
    public static string? DetectConfigFromProfile()
    {
        var profilePath = GetPowerShellProfilePath();
        if (profilePath is null || !File.Exists(profilePath))
        {
            return null;
        }

        var content = File.ReadAllText(profilePath);
        return ExtractConfigPath(content);
    }

    /// <summary>
    /// Extracts the OMP config path from shell profile content.
    /// Looks for patterns like: oh-my-posh init pwsh --config 'path'
    /// </summary>
    internal static string? ExtractConfigPath(string profileContent)
    {
        // Match: oh-my-posh init <shell> --config <path>
        // The path may be quoted with single quotes, double quotes, or unquoted
        // Match new syntax first, then fall back to legacy
        var match = ConfigPathRegex().Match(profileContent);
        if (!match.Success)
        {
            match = LegacyConfigPathRegex().Match(profileContent);
        }

        if (match.Success)
        {
            var configPath = match.Groups["path"].Value.Trim('\'', '"');

            // Expand environment variables
            configPath = Environment.ExpandEnvironmentVariables(configPath);

            // Expand ~ to user profile
            if (configPath.StartsWith('~'))
            {
                var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                configPath = Path.Combine(home, configPath[1..].TrimStart('/', '\\'));
            }

            // Expand $env: variables
            configPath = ExpandPowerShellEnvVars(configPath);

            return configPath;
        }

        return null;
    }

    /// <summary>
    /// Returns the PowerShell profile path ($PROFILE equivalent).
    /// </summary>
    public static string? GetPowerShellProfilePath()
    {
        var docs = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        if (string.IsNullOrEmpty(docs))
        {
            return null;
        }

        // Check PowerShell 7+ first, then Windows PowerShell
        var pwsh7 = Path.Combine(docs, "PowerShell", "Microsoft.PowerShell_profile.ps1");
        if (File.Exists(pwsh7))
        {
            return pwsh7;
        }

        var pwsh5 = Path.Combine(docs, "WindowsPowerShell", "Microsoft.PowerShell_profile.ps1");
        if (File.Exists(pwsh5))
        {
            return pwsh5;
        }

        // Return the PowerShell 7 path as the default even if it doesn't exist yet
        return pwsh7;
    }

    private static string ExpandPowerShellEnvVars(string path)
    {
        // Expand $env:VAR_NAME patterns
        return PsEnvVarRegex().Replace(path, match =>
        {
            var varName = match.Groups[1].Value;
            return Environment.GetEnvironmentVariable(varName) ?? match.Value;
        });
    }

    [GeneratedRegex(@"\$env:(\w+)", RegexOptions.IgnoreCase)]
    private static partial Regex PsEnvVarRegex();

    // New syntax: oh-my-posh init pwsh --config <path>
    [GeneratedRegex(@"oh-my-posh\s+init\s+\w+\s+--config\s+(?<path>'[^']+'|""[^""]+""|[^\s|;]+)", RegexOptions.IgnoreCase)]
    private static partial Regex ConfigPathRegex();

    // Old syntax: oh-my-posh --init --shell pwsh --config <path>
    [GeneratedRegex(@"oh-my-posh\s+--init\s+--shell\s+\w+\s+--config\s+(?<path>'[^']+'|""[^""]+""|[^\s|;]+)", RegexOptions.IgnoreCase)]
    private static partial Regex LegacyConfigPathRegex();
}
