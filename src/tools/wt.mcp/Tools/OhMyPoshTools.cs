using Json.Patch;
using ModelContextProtocol.Server;
using System.ComponentModel;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

/// <summary>
/// MCP tools for managing Oh My Posh prompt theme engine configuration.
/// Oh My Posh is a cross-platform prompt engine that customizes your shell prompt
/// with themes, segments, and blocks.
/// </summary>
[McpServerToolType]
internal class OhMyPoshTools
{
    [McpServerTool, Description("""
        Checks if Oh My Posh is installed and returns status information including:
        version, executable path, themes directory, detected config file path,
        and whether it's referenced in the user's PowerShell profile.
        """)]
    public static string GetOhMyPoshStatus()
    {
        var sb = new StringBuilder();

        var exePath = OhMyPoshHelper.FindExecutable();
        if (exePath is null)
        {
            sb.AppendLine("Oh My Posh is not installed (not found on PATH).");
            sb.AppendLine();
            sb.AppendLine("Install with: winget install JanDeDobbeleer.OhMyPosh --source winget");
            return sb.ToString().TrimEnd();
        }

        sb.Append($"Oh My Posh is installed: {exePath}");

        var version = OhMyPoshHelper.GetVersion();
        if (version is not null)
        {
            sb.Append($" (v{version})");
        }

        sb.AppendLine();

        var themesDir = OhMyPoshHelper.GetThemesDirectory();
        if (themesDir is not null)
        {
            var themeCount = OhMyPoshHelper.ListThemes().Count;
            sb.AppendLine($"Themes directory: {themesDir} ({themeCount} themes)");
        }
        else
        {
            sb.AppendLine("Themes directory: not found");
        }

        var configPath = OhMyPoshHelper.DetectConfigFromProfile();
        if (configPath is not null)
        {
            sb.AppendLine($"Config file (from profile): {configPath}");
            if (File.Exists(configPath))
            {
                sb.AppendLine("  ✓ Config file exists");
            }
            else
            {
                sb.AppendLine("  ⚠ Config file not found on disk");
            }
        }
        else
        {
            sb.AppendLine("Config file: not detected in PowerShell profile");
        }

        var profilePath = OhMyPoshHelper.GetPowerShellProfilePath();
        if (profilePath is not null)
        {
            sb.AppendLine($"PowerShell profile: {profilePath}");
            sb.AppendLine(File.Exists(profilePath)
                ? "  ✓ Profile exists"
                : "  ⚠ Profile does not exist yet");
        }

        return sb.ToString().TrimEnd();
    }

    [McpServerTool, Description("""
        Lists available Oh My Posh themes from the themes directory.
        These are built-in themes that ship with Oh My Posh and can be used as-is
        or as a starting point for custom configurations.
        """)]
    public static string ListOhMyPoshThemes()
    {
        var themes = OhMyPoshHelper.ListThemes();
        if (themes.Count == 0)
        {
            var themesDir = OhMyPoshHelper.GetThemesDirectory();
            return themesDir is null
                ? "Oh My Posh themes directory not found. Is Oh My Posh installed?"
                : $"No themes found in: {themesDir}";
        }

        var sb = new StringBuilder();
        sb.AppendLine($"Available themes ({themes.Count}):\n");
        foreach (var (name, fullPath) in themes)
        {
            sb.AppendLine($"  {name}");
        }

        sb.AppendLine();
        sb.AppendLine($"Themes directory: {OhMyPoshHelper.GetThemesDirectory()}");
        sb.AppendLine("Visual previews: https://ohmyposh.dev/docs/themes");
        sb.AppendLine("Preview in terminal: oh-my-posh print primary --config <theme-path>");
        sb.AppendLine("Use ReadOhMyPoshConfig with a theme name to view its configuration.");

        return sb.ToString().TrimEnd();
    }

    [McpServerTool, Description("""
        Reads an Oh My Posh configuration file (JSON, YAML, or TOML).
        If no path is given, auto-detects the current config from the user's PowerShell profile.
        Can also read a built-in theme by name (e.g. "agnoster", "paradox").
        """)]
    public static string ReadOhMyPoshConfig(
        [Description("Path to the config file, or a theme name. If omitted, auto-detects from the user's PowerShell profile.")] string? path = null)
    {
        var resolvedPath = ResolveConfigPath(path);
        if (resolvedPath is null)
        {
            return path is null
                ? "Could not auto-detect Oh My Posh config. No config reference found in the PowerShell profile. Provide a path explicitly."
                : $"Config file not found: {path}";
        }

        if (!File.Exists(resolvedPath))
        {
            return $"Config file not found at: {resolvedPath}";
        }

        var content = File.ReadAllText(resolvedPath);

        // Try to pretty-print if it's JSON
        if (resolvedPath.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
        {
            try
            {
                var doc = JsonNode.Parse(content);
                if (doc is not null)
                {
                    content = doc.ToJsonString(new JsonSerializerOptions { WriteIndented = true });
                }
            }
            catch
            {
                // Return as-is
            }
        }

        return $"Config: {resolvedPath}\n\n{content}";
    }

    [McpServerTool, Description("""
        Previews a JSON Patch (RFC 6902) against an Oh My Posh config file WITHOUT writing any changes.
        Returns a unified diff showing exactly what would change.
        Always call this before ApplyOhMyPoshConfigChange so the user can review the diff.
        IMPORTANT: You MUST display the returned diff inside a ```diff fenced code block.
        Only works with JSON config files (.omp.json).
        """)]
    public static string PreviewOhMyPoshConfigChange(
        [Description("A JSON Patch document (RFC 6902): an array of operations to apply")] string patchJson,
        [Description("Path to the config file. If omitted, auto-detects from the user's PowerShell profile.")] string? path = null)
    {
        var resolvedPath = ResolveConfigPath(path);
        if (resolvedPath is null)
        {
            return path is null
                ? "Could not auto-detect Oh My Posh config. Provide a path explicitly."
                : $"Config file not found: {path}";
        }

        if (!resolvedPath.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
        {
            return $"JSON Patch only works with JSON config files. This file is: {resolvedPath}";
        }

        if (!File.Exists(resolvedPath))
        {
            return $"Config file not found at: {resolvedPath}";
        }

        JsonPatch patch;
        try
        {
            patch = JsonSerializer.Deserialize<JsonPatch>(patchJson)
                ?? throw new JsonException("Patch deserialized to null.");
        }
        catch (JsonException ex)
        {
            return $"Invalid JSON Patch: {ex.Message}";
        }

        var original = File.ReadAllText(resolvedPath);
        var doc = JsonNode.Parse(original);
        if (doc is null)
        {
            return "Failed to parse current config file.";
        }

        var writeOptions = new JsonSerializerOptions { WriteIndented = true };
        var before = doc.ToJsonString(writeOptions);

        var result = patch.Apply(doc);
        if (!result.IsSuccess)
        {
            return $"Patch failed: {result.Error}";
        }

        var after = result.Result!.ToJsonString(writeOptions);
        var label = Path.GetFileName(resolvedPath);
        var diff = SettingsHelper.UnifiedDiff(before, after, label);

        if (string.IsNullOrEmpty(diff))
        {
            return "No changes — the patch produces identical output.";
        }

        return diff;
    }

    [McpServerTool, Description("""
        Applies a JSON Patch (RFC 6902) to an Oh My Posh config file and writes the result.
        IMPORTANT: Always call PreviewOhMyPoshConfigChange first and show the diff to the user.
        After showing the diff, call this tool immediately — do not ask for separate user confirmation.
        The client will show its own confirmation dialog for approval.
        Only works with JSON config files (.omp.json).
        """)]
    public static string ApplyOhMyPoshConfigChange(
        [Description("A JSON Patch document (RFC 6902): an array of operations to apply")] string patchJson,
        [Description("Path to the config file. If omitted, auto-detects from the user's PowerShell profile.")] string? path = null)
    {
        var resolvedPath = ResolveConfigPath(path);
        if (resolvedPath is null)
        {
            return path is null
                ? "Could not auto-detect Oh My Posh config. Provide a path explicitly."
                : $"Config file not found: {path}";
        }

        if (!resolvedPath.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
        {
            return $"JSON Patch only works with JSON config files. This file is: {resolvedPath}";
        }

        if (!File.Exists(resolvedPath))
        {
            return $"Config file not found at: {resolvedPath}";
        }

        JsonPatch patch;
        try
        {
            patch = JsonSerializer.Deserialize<JsonPatch>(patchJson)
                ?? throw new JsonException("Patch deserialized to null.");
        }
        catch (JsonException ex)
        {
            return $"Invalid JSON Patch: {ex.Message}";
        }

        var original = File.ReadAllText(resolvedPath);
        var doc = JsonNode.Parse(original);
        if (doc is null)
        {
            return "Failed to parse current config file.";
        }

        var result = patch.Apply(doc);
        if (!result.IsSuccess)
        {
            return $"Patch failed: {result.Error}";
        }

        var writeOptions = new JsonSerializerOptions { WriteIndented = true };
        var patched = result.Result!.ToJsonString(writeOptions);

        // Back up before writing
        File.Copy(resolvedPath, resolvedPath + ".bak", overwrite: true);
        File.WriteAllText(resolvedPath, patched);

        return $"Config updated: {resolvedPath}\n\nRestart your shell or run `oh-my-posh init pwsh | Invoke-Expression` to see changes.";
    }

    [McpServerTool, Description("""
        Sets the Oh My Posh theme by updating the user's PowerShell profile.
        Accepts a theme name (from built-in themes) or a full path to a config file.
        Shows what would change in the profile before applying.
        """)]
    public static string SetOhMyPoshTheme(
        [Description("Theme name (e.g. 'agnoster', 'paradox') or full path to a config file.")] string theme,
        [Description("If true, only preview the change without writing. Default is false.")] bool previewOnly = false)
    {
        var profilePath = OhMyPoshHelper.GetPowerShellProfilePath();
        if (profilePath is null)
        {
            return "Could not determine PowerShell profile path.";
        }

        // Resolve theme name to a path if it's not already a path
        var themePath = theme;
        if (!Path.IsPathRooted(theme) && !theme.Contains('.'))
        {
            // Looks like a theme name — resolve from themes directory
            var themes = OhMyPoshHelper.ListThemes();
            var match = themes.FirstOrDefault(t => t.Name.Equals(theme, StringComparison.OrdinalIgnoreCase));
            if (match != default)
            {
                themePath = match.FullPath;
            }
            else
            {
                return $"Theme \"{theme}\" not found in the themes directory. Use ListOhMyPoshThemes to see available themes.";
            }
        }

        // Use $env:POSH_THEMES_PATH for paths in the themes directory
        var themesDir = OhMyPoshHelper.GetThemesDirectory();
        var initLine = themesDir is not null && themePath.StartsWith(themesDir, StringComparison.OrdinalIgnoreCase)
            ? $"oh-my-posh init pwsh --config \"$env:POSH_THEMES_PATH\\{Path.GetFileName(themePath)}\" | Invoke-Expression"
            : $"oh-my-posh init pwsh --config '{themePath}' | Invoke-Expression";

        string before;
        string after;

        if (File.Exists(profilePath))
        {
            before = File.ReadAllText(profilePath);

            // Try to replace the existing oh-my-posh init line
            var lines = before.Split('\n').ToList();
            var replaced = false;
            for (var i = 0; i < lines.Count; i++)
            {
                if (lines[i].TrimStart().StartsWith("oh-my-posh", StringComparison.OrdinalIgnoreCase) &&
                    lines[i].Contains("init", StringComparison.OrdinalIgnoreCase))
                {
                    lines[i] = initLine;
                    replaced = true;
                    break;
                }
            }

            if (!replaced)
            {
                lines.Add(initLine);
            }

            after = string.Join('\n', lines);
        }
        else
        {
            before = "";
            after = initLine + "\n";
        }

        var diff = SettingsHelper.UnifiedDiff(before, after, Path.GetFileName(profilePath));

        if (previewOnly || string.IsNullOrEmpty(diff))
        {
            if (string.IsNullOrEmpty(diff))
            {
                return "No changes needed — the profile already uses this theme.";
            }

            return $"Preview of changes to {profilePath}:\n\n{diff}";
        }

        // Write the profile
        if (File.Exists(profilePath))
        {
            File.Copy(profilePath, profilePath + ".bak", overwrite: true);
        }
        else
        {
            Directory.CreateDirectory(Path.GetDirectoryName(profilePath)!);
        }

        File.WriteAllText(profilePath, after);
        return $"PowerShell profile updated: {profilePath}\n\nRestart your shell to see the new theme.";
    }

    /// <summary>
    /// Resolves a config path from a user-provided value (which may be null, a theme name, or a full path).
    /// </summary>
    private static string? ResolveConfigPath(string? path)
    {
        if (path is null)
        {
            // Auto-detect from profile
            return OhMyPoshHelper.DetectConfigFromProfile();
        }

        // If it's a full path, use as-is
        if (Path.IsPathRooted(path))
        {
            return path;
        }

        // Check if it's a theme name
        if (!path.Contains('.'))
        {
            var themes = OhMyPoshHelper.ListThemes();
            var match = themes.FirstOrDefault(t => t.Name.Equals(path, StringComparison.OrdinalIgnoreCase));
            if (match != default)
            {
                return match.FullPath;
            }
        }

        // Treat as a relative path
        var resolved = Path.GetFullPath(path);
        return File.Exists(resolved) ? resolved : null;
    }
}
