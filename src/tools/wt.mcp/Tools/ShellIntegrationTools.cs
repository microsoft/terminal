using ModelContextProtocol.Server;
using System.ComponentModel;
using System.Text;
using System.Text.Json.Nodes;

/// <summary>
/// MCP tools for checking and configuring shell integration with Windows Terminal.
/// Shell integration uses OSC 133 (FinalTerm) escape sequences to let the terminal
/// understand prompt boundaries, command status, and working directory.
/// </summary>
[McpServerToolType]
internal class ShellIntegrationTools
{
    [McpServerTool, Description("""
        Reports the current state of shell integration for Windows Terminal.
        Checks both terminal-side settings (autoMarkPrompts, showMarksOnScrollbar)
        and shell-side configuration (PowerShell profile, Oh My Posh shell_integration).
        Use the results to determine what needs to be enabled.
        For terminal-side settings, use PreviewSettingsChange/ApplySettingsChange to enable them.
        For shell-side changes, use GetShellIntegrationSnippet to get the code to add.
        IMPORTANT: Display the FULL output to the user — do not summarize or collapse individual
        settings or actions. Each item should be shown explicitly for discoverability.
        """)]
    public static string GetShellIntegrationStatus(
        [Description("The release channel. If not specified, the most-preview installed channel is used.")] TerminalRelease? release = null)
    {
        var resolved = SettingsHelper.ResolveRelease(release);
        var sb = new StringBuilder();

        // Terminal-side checks
        sb.AppendLine("=== Terminal Settings ===");

        if (resolved is null)
        {
            sb.AppendLine("  No Windows Terminal installations found.");
        }
        else
        {
            var (doc, error) = SettingsHelper.LoadSettings(resolved.Value);
            if (error is not null)
            {
                sb.AppendLine($"  Could not read settings: {error}");
            }
            else
            {
                var defaults = doc!["profiles"]?["defaults"];

                var autoMark = defaults?["autoMarkPrompts"]?.GetValue<bool>() ?? false;
                sb.AppendLine($"  autoMarkPrompts: {(autoMark ? "✓ enabled" : "✗ disabled")}");
                if (!autoMark)
                {
                    sb.AppendLine("    → Enable with: patch [{\"op\":\"add\",\"path\":\"/profiles/defaults/autoMarkPrompts\",\"value\":true}]");
                }

                var showMarks = defaults?["showMarksOnScrollbar"]?.GetValue<bool>() ?? false;
                sb.AppendLine($"  showMarksOnScrollbar: {(showMarks ? "✓ enabled" : "✗ disabled")}");
                if (!showMarks)
                {
                    sb.AppendLine("    → Enable with: patch [{\"op\":\"add\",\"path\":\"/profiles/defaults/showMarksOnScrollbar\",\"value\":true}]");
                }

                // Build a lookup of action id → keys from the keybindings array
                var keybindingsMap = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                if (doc!["keybindings"] is JsonArray keybindings)
                {
                    foreach (var kb in keybindings)
                    {
                        var id = kb?["id"]?.GetValue<string>();
                        var keys = kb?["keys"]?.GetValue<string>();
                        if (id is not null && keys is not null)
                        {
                            keybindingsMap[id] = keys;
                        }
                    }
                }

                // Check each shell-integration-related action individually
                sb.AppendLine();
                sb.AppendLine("  Shell integration actions:");

                var shellIntegrationActions = new (string ActionName, string? Direction)[]
                {
                    ("scrollToMark", "previous"),
                    ("scrollToMark", "next"),
                    ("selectCommand", null),
                    ("selectOutput", null),
                };

                if (doc!["actions"] is JsonArray actions)
                {
                    foreach (var (actionName, direction) in shellIntegrationActions)
                    {
                        var label = direction is not null ? $"{actionName} ({direction})" : actionName;
                        var found = false;
                        string? actionId = null;

                        foreach (var action in actions)
                        {
                            var cmd = action?["command"];
                            string? actionStr = null;
                            string? dirStr = null;

                            if (cmd is JsonValue)
                            {
                                actionStr = cmd.GetValue<string>();
                            }
                            else if (cmd is JsonObject cmdObj)
                            {
                                actionStr = cmdObj["action"]?.GetValue<string>();
                                dirStr = cmdObj["direction"]?.GetValue<string>();
                            }

                            if (actionStr is not null &&
                                actionStr.Equals(actionName, StringComparison.OrdinalIgnoreCase) &&
                                (direction is null || (dirStr is not null && dirStr.Equals(direction, StringComparison.OrdinalIgnoreCase))))
                            {
                                found = true;
                                actionId = action?["id"]?.GetValue<string>();
                                break;
                            }
                        }

                        if (found)
                        {
                            var keyInfo = actionId is not null && keybindingsMap.TryGetValue(actionId, out var keys)
                                ? $" → bound to: {keys}"
                                : " (no keybinding)";
                            sb.AppendLine($"    ✓ {label}{keyInfo}");
                        }
                        else
                        {
                            sb.AppendLine($"    ✗ {label}: not found");
                        }
                    }
                }
                else
                {
                    sb.AppendLine("    (no actions array found)");
                }
            }

            sb.AppendLine($"  Channel: {resolved}");
        }

        sb.AppendLine();

        // Shell-side checks
        sb.AppendLine("=== PowerShell ===");
        CheckPowerShellIntegration(sb);

        sb.AppendLine();
        sb.AppendLine("=== CMD / Clink ===");
        CheckClinkIntegration(sb);

        sb.AppendLine();
        sb.AppendLine("=== Bash (Windows / Git Bash) ===");
        CheckWindowsBashIntegration(sb);

        sb.AppendLine();
        sb.AppendLine("=== Zsh (Windows / MSYS2) ===");
        CheckWindowsZshIntegration(sb);

        sb.AppendLine();
        sb.AppendLine("=== WSL Distributions ===");
        CheckWslIntegration(sb);

        return sb.ToString().TrimEnd();
    }

    /// <summary>
    /// Checks the user's PowerShell profile for OMP or manual OSC 133 markers.
    /// </summary>
    private static void CheckPowerShellIntegration(StringBuilder sb)
    {
        var profilePath = OhMyPoshHelper.GetPowerShellProfilePath();
        if (profilePath is null || !File.Exists(profilePath))
        {
            sb.AppendLine($"  Profile: {profilePath ?? "unknown"}");
            sb.AppendLine("  ⚠ Profile does not exist yet");
            return;
        }

        var content = File.ReadAllText(profilePath);
        sb.AppendLine($"  Profile: {profilePath}");

        var hasOmp = content.Contains("oh-my-posh", StringComparison.OrdinalIgnoreCase);
        if (hasOmp)
        {
            sb.AppendLine("  Oh My Posh: ✓ detected in profile");
            CheckOmpShellIntegration(content, sb);
        }
        else
        {
            sb.AppendLine("  Oh My Posh: not detected");
            var hasMarkers = HasOsc133Markers(content, powershell: true);
            sb.AppendLine($"  Manual shell integration (OSC 133): {(hasMarkers ? "✓ likely present" : "✗ not detected")}");
            if (!hasMarkers)
            {
                sb.AppendLine("    → Use GetShellIntegrationSnippet('powershell') to get the setup code");
            }
        }
    }

    /// <summary>
    /// Checks if Clink is installed (provides automatic shell integration for CMD).
    /// </summary>
    private static void CheckClinkIntegration(StringBuilder sb)
    {
        var clinkPath = FindOnPath("clink");
        if (clinkPath is not null)
        {
            sb.AppendLine($"  Clink: ✓ installed ({clinkPath})");
            sb.AppendLine("  Shell integration: ✓ automatic (Clink provides FinalTerm sequences natively in Windows Terminal)");
        }
        else
        {
            sb.AppendLine("  Clink: ✗ not installed");
            sb.AppendLine("  Shell integration requires Clink v1.14.25+ for CMD");
            sb.AppendLine("    → Install Clink: https://chrisant996.github.io/clink/");
        }
    }

    /// <summary>
    /// Checks ~/.bashrc on Windows (Git Bash / MSYS2) for shell integration markers.
    /// </summary>
    private static void CheckWindowsBashIntegration(StringBuilder sb)
    {
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var bashrc = Path.Combine(home, ".bashrc");

        if (!File.Exists(bashrc))
        {
            sb.AppendLine($"  {bashrc}: not found");

            // Check if Git Bash is installed
            var gitBash = FindOnPath("bash");
            if (gitBash is not null)
            {
                sb.AppendLine($"  bash: ✓ found ({gitBash})");
                sb.AppendLine("    → Create ~/.bashrc with shell integration snippet");
                sb.AppendLine("    → Use GetShellIntegrationSnippet('bash') to get the setup code");
            }
            else
            {
                sb.AppendLine("  bash: not found on PATH (Git Bash or MSYS2 not detected)");
            }
            return;
        }

        var content = File.ReadAllText(bashrc);
        sb.AppendLine($"  {bashrc}: ✓ found");

        var hasMarkers = HasOsc133Markers(content, powershell: false);
        sb.AppendLine($"  Shell integration (OSC 133): {(hasMarkers ? "✓ likely present" : "✗ not detected")}");
        if (!hasMarkers)
        {
            sb.AppendLine("    → Use GetShellIntegrationSnippet('bash') to get the setup code");
        }
    }

    /// <summary>
    /// Checks ~/.zshrc on Windows (MSYS2 / Git) for shell integration markers.
    /// </summary>
    private static void CheckWindowsZshIntegration(StringBuilder sb)
    {
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var zshrc = Path.Combine(home, ".zshrc");

        if (!File.Exists(zshrc))
        {
            sb.AppendLine($"  {zshrc}: not found");

            var zshPath = FindOnPath("zsh");
            if (zshPath is not null)
            {
                sb.AppendLine($"  zsh: ✓ found ({zshPath})");
                sb.AppendLine("    → Create ~/.zshrc with shell integration snippet");
                sb.AppendLine("    → Use GetShellIntegrationSnippet('zsh') to get the setup code");
            }
            else
            {
                sb.AppendLine("  zsh: not found on PATH");
            }
            return;
        }

        var content = File.ReadAllText(zshrc);
        sb.AppendLine($"  {zshrc}: ✓ found");

        var hasMarkers = HasOsc133Markers(content, powershell: false);
        sb.AppendLine($"  Shell integration (OSC 133): {(hasMarkers ? "✓ likely present" : "✗ not detected")}");
        if (!hasMarkers)
        {
            sb.AppendLine("    → Use GetShellIntegrationSnippet('zsh') to get the setup code");
        }
    }

    /// <summary>
    /// Enumerates WSL distributions and checks their shell profiles for integration markers.
    /// </summary>
    private static void CheckWslIntegration(StringBuilder sb)
    {
        var distros = GetWslDistributions();
        if (distros is null)
        {
            sb.AppendLine("  WSL: not available (wsl.exe not found or returned an error)");
            return;
        }

        if (distros.Count == 0)
        {
            sb.AppendLine("  WSL: no distributions installed");
            return;
        }

        sb.AppendLine($"  Found {distros.Count} distribution(s):\n");

        foreach (var distro in distros)
        {
            sb.AppendLine($"  [{distro}]");

            // Check .bashrc inside the WSL distro
            var bashrcContent = ReadWslFile(distro, "~/.bashrc");
            var zshrcContent = ReadWslFile(distro, "~/.zshrc");

            if (bashrcContent is not null)
            {
                var hasBashMarkers = HasOsc133Markers(bashrcContent, powershell: false);
                sb.AppendLine($"    ~/.bashrc: ✓ found");
                sb.AppendLine($"    bash shell integration: {(hasBashMarkers ? "✓ likely present" : "✗ not detected")}");
            }
            else
            {
                sb.AppendLine("    ~/.bashrc: not found or not readable");
            }

            if (zshrcContent is not null)
            {
                var hasZshMarkers = HasOsc133Markers(zshrcContent, powershell: false);
                sb.AppendLine($"    ~/.zshrc: ✓ found");
                sb.AppendLine($"    zsh shell integration: {(hasZshMarkers ? "✓ likely present" : "✗ not detected")}");
            }

            // Check if OMP is present in either profile
            var ompInBash = bashrcContent?.Contains("oh-my-posh", StringComparison.OrdinalIgnoreCase) ?? false;
            var ompInZsh = zshrcContent?.Contains("oh-my-posh", StringComparison.OrdinalIgnoreCase) ?? false;
            if (ompInBash || ompInZsh)
            {
                var shell = ompInBash ? "bash" : "zsh";
                sb.AppendLine($"    Oh My Posh: ✓ detected in {shell} profile");
            }

            sb.AppendLine();
        }
    }

    /// <summary>
    /// Checks whether an OMP config has shell_integration enabled.
    /// </summary>
    private static void CheckOmpShellIntegration(string profileContent, StringBuilder sb)
    {
        var configPath = OhMyPoshHelper.ExtractConfigPath(profileContent);
        if (configPath is null || !File.Exists(configPath))
        {
            sb.AppendLine("  OMP config: could not locate or read");
            return;
        }

        if (!configPath.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
        {
            sb.AppendLine($"  OMP config: {configPath} (non-JSON — cannot auto-check shell_integration)");
            return;
        }

        try
        {
            var configJson = File.ReadAllText(configPath);
            var configDoc = JsonNode.Parse(configJson);
            var shellIntegration = configDoc?["shell_integration"]?.GetValue<bool>() ?? false;
            sb.AppendLine($"  OMP shell_integration: {(shellIntegration ? "✓ enabled" : "✗ disabled")}");
            if (!shellIntegration)
            {
                sb.AppendLine("    → Enable in OMP config: set \"shell_integration\": true");
                sb.AppendLine("    → Or use GetShellIntegrationSnippet('omp') for details");
            }
        }
        catch
        {
            sb.AppendLine("  OMP shell_integration: could not parse config");
        }
    }

    /// <summary>
    /// Checks if content contains OSC 133 shell integration markers.
    /// </summary>
    private static bool HasOsc133Markers(string content, bool powershell)
    {
        if (!content.Contains("133", StringComparison.Ordinal))
        {
            return false;
        }

        if (powershell)
        {
            // PowerShell escape sequences
            return content.Contains("`e]", StringComparison.OrdinalIgnoreCase) ||
                   content.Contains("\\x1b]", StringComparison.Ordinal) ||
                   content.Contains("\\e]", StringComparison.Ordinal) ||
                   content.Contains("\x1b]", StringComparison.Ordinal);
        }

        // Bash/Zsh escape sequences
        return content.Contains("\\e]", StringComparison.Ordinal) ||
               content.Contains("\\x1b]", StringComparison.Ordinal) ||
               content.Contains("\\033]", StringComparison.Ordinal) ||
               content.Contains("\x1b]", StringComparison.Ordinal);
    }

    /// <summary>
    /// Finds an executable on the PATH using where.exe.
    /// </summary>
    private static string? FindOnPath(string executable)
    {
        var result = ProcessHelper.Run("where.exe", executable);
        if (result is null || result.ExitCode != 0 || string.IsNullOrWhiteSpace(result.Stdout))
        {
            return null;
        }

        return result.Stdout.Split('\n', StringSplitOptions.RemoveEmptyEntries)[0].Trim();
    }

    /// <summary>
    /// Lists installed WSL distributions. Returns null if WSL is not available.
    /// </summary>
    private static List<string>? GetWslDistributions()
    {
        // WSL can be slow to enumerate — give it a generous timeout
        var result = ProcessHelper.Run("wsl.exe", "--list --quiet", timeoutMs: 10000, outputEncoding: System.Text.Encoding.Unicode);
        if (result is null || result.ExitCode != 0)
        {
            return null;
        }

        var distros = result.Stdout.Split('\n', StringSplitOptions.RemoveEmptyEntries)
            .Select(l => l.Trim().TrimEnd('\0', '\r'))
            .Where(l => !string.IsNullOrWhiteSpace(l))
            .ToList();

        return distros;
    }

    /// <summary>
    /// Reads a file from inside a WSL distribution. Returns null on failure.
    /// </summary>
    private static string? ReadWslFile(string distro, string filePath)
    {
        var result = ProcessHelper.Run("wsl.exe", $"-d {distro} cat {filePath}", timeoutMs: 10000);
        return result is not null && result.ExitCode == 0 ? result.Stdout : null;
    }

    [McpServerTool, Description("""
        Returns the shell integration code snippet for a given shell.
        The snippet adds OSC 133 (FinalTerm) escape sequences that let Windows Terminal
        understand prompt boundaries, track command status, and enable features like
        scrolling between commands and selecting command output.
        
        For Oh My Posh users, returns the simpler config change to enable shell_integration
        in the OMP config instead of the manual prompt function.
        
        Display the returned snippet to the user as a code block so they can copy it
        into their shell profile.
        """)]
    public static string GetShellIntegrationSnippet(
        [Description("The shell to generate a snippet for: 'powershell', 'bash', 'zsh', 'cmd', or 'omp' (Oh My Posh).")] string shell)
    {
        var sb = new StringBuilder();
        var shellLower = shell.Trim().ToLowerInvariant();

        switch (shellLower)
        {
            case "powershell" or "pwsh" or "ps" or "ps1":
                sb.AppendLine("Add the following to your PowerShell profile ($PROFILE):\n");
                sb.AppendLine(GetPowerShellSnippet());
                sb.AppendLine();
                sb.AppendLine($"Profile location: {OhMyPoshHelper.GetPowerShellProfilePath() ?? "$PROFILE"}");
                break;

            case "bash":
                sb.AppendLine("Add the following to your ~/.bashrc:\n");
                sb.AppendLine(GetBashSnippet());
                break;

            case "zsh":
                sb.AppendLine("Add the following to your ~/.zshrc:\n");
                sb.AppendLine(GetZshSnippet());
                break;

            case "cmd" or "clink":
                sb.AppendLine("Shell integration for CMD requires Clink v1.14.25 or later.");
                sb.AppendLine("Clink supports FinalTerm sequences natively.\n");
                sb.AppendLine("Install Clink: https://chrisant996.github.io/clink/");
                sb.AppendLine("Clink automatically provides shell integration when running in Windows Terminal.");
                break;

            case "omp" or "oh-my-posh":
                sb.AppendLine("Oh My Posh has built-in shell integration support.\n");
                sb.AppendLine("Add or set this in your Oh My Posh config file (JSON):\n");
                sb.AppendLine("  \"shell_integration\": true\n");
                sb.AppendLine("This is the recommended approach when using Oh My Posh.");
                sb.AppendLine("It wraps the prompt with FinalTerm OSC 133 sequences automatically.");

                var configPath = OhMyPoshHelper.DetectConfigFromProfile();
                if (configPath is not null)
                {
                    sb.AppendLine($"\nYour config file: {configPath}");
                }
                break;

            default:
                sb.AppendLine($"Unknown shell: \"{shell}\"");
                sb.AppendLine("Supported shells: powershell, bash, zsh, cmd (via Clink), omp (Oh My Posh)");
                break;
        }

        return sb.ToString().TrimEnd();
    }

    private const string ShellIntegrationMarker = "# Shell integration for Windows Terminal";

    [McpServerTool, Description("""
        Enables shell integration by writing the appropriate snippet to a shell profile.
        Handles PowerShell, bash, zsh (Windows or WSL), and Oh My Posh.
        Idempotent — will not add the snippet if shell integration is already detected.
        For OMP users on PowerShell, sets shell_integration: true in the OMP config instead.
        For WSL, specify the distribution name (e.g., "Ubuntu").
        """)]
    public static string EnableShellIntegration(
        [Description("The shell to enable integration for: 'powershell', 'bash', 'zsh', or 'omp' (Oh My Posh).")] string shell,
        [Description("WSL distribution name (e.g., 'Ubuntu'). Required for WSL bash/zsh. Omit for Windows shells.")] string? wslDistro = null)
    {
        var shellLower = shell.Trim().ToLowerInvariant();

        switch (shellLower)
        {
            case "powershell" or "pwsh" or "ps" or "ps1":
                return EnablePowerShellIntegration();

            case "bash":
                return wslDistro is not null
                    ? EnableWslShellIntegration(wslDistro, "bash", "~/.bashrc", GetBashSnippet())
                    : EnableWindowsShellIntegration("bash", ".bashrc", GetBashSnippet());

            case "zsh":
                return wslDistro is not null
                    ? EnableWslShellIntegration(wslDistro, "zsh", "~/.zshrc", GetZshSnippet())
                    : EnableWindowsShellIntegration("zsh", ".zshrc", GetZshSnippet());

            case "omp" or "oh-my-posh":
                return EnableOmpShellIntegration();

            default:
                return $"Unknown shell: \"{shell}\". Supported: powershell, bash, zsh, omp";
        }
    }

    /// <summary>
    /// Enables shell integration for PowerShell. If OMP is detected, delegates to OMP integration.
    /// Otherwise appends the PowerShell snippet to $PROFILE.
    /// </summary>
    private static string EnablePowerShellIntegration()
    {
        var profilePath = OhMyPoshHelper.GetPowerShellProfilePath();
        if (profilePath is null)
        {
            return "Could not determine PowerShell profile path.";
        }

        if (File.Exists(profilePath))
        {
            var content = File.ReadAllText(profilePath);
            if (content.Contains("oh-my-posh", StringComparison.OrdinalIgnoreCase))
            {
                return EnableOmpShellIntegration();
            }

            if (HasOsc133Markers(content, powershell: true))
            {
                return "Shell integration is already present in your PowerShell profile.";
            }
        }

        var snippet = "\n" + GetPowerShellSnippet() + "\n";
        File.AppendAllText(profilePath, snippet);
        return $"✓ Shell integration appended to: {profilePath}\nRestart your shell or run `. $PROFILE` to activate.";
    }

    /// <summary>
    /// Enables OMP shell_integration by patching the OMP config file.
    /// </summary>
    private static string EnableOmpShellIntegration()
    {
        var configPath = OhMyPoshHelper.DetectConfigFromProfile();
        if (configPath is null)
        {
            return "Could not detect Oh My Posh config path from your PowerShell profile.";
        }

        if (!configPath.EndsWith(".json", StringComparison.OrdinalIgnoreCase) &&
            !configPath.EndsWith(".omp.json", StringComparison.OrdinalIgnoreCase))
        {
            return $"OMP config is not JSON ({configPath}). Manually add shell_integration to your config.";
        }

        try
        {
            var json = File.ReadAllText(configPath);
            var doc = JsonNode.Parse(json);
            if (doc is null)
            {
                return "Failed to parse OMP config.";
            }

            var current = doc["shell_integration"]?.GetValue<bool>() ?? false;
            if (current)
            {
                return "✓ Oh My Posh shell_integration is already enabled.";
            }

            doc["shell_integration"] = true;
            File.Copy(configPath, configPath + ".bak", overwrite: true);
            File.WriteAllText(configPath, doc.ToJsonString(new System.Text.Json.JsonSerializerOptions { WriteIndented = true }));
            return $"✓ Enabled shell_integration in: {configPath}\nRestart your shell to activate.";
        }
        catch (Exception ex)
        {
            return $"Error updating OMP config: {ex.Message}";
        }
    }

    /// <summary>
    /// Enables shell integration for a Windows-side bash/zsh profile.
    /// </summary>
    private static string EnableWindowsShellIntegration(string shellName, string rcFileName, string snippet)
    {
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var rcPath = Path.Combine(home, rcFileName);

        if (File.Exists(rcPath))
        {
            var content = File.ReadAllText(rcPath);
            if (content.Contains(ShellIntegrationMarker, StringComparison.Ordinal))
            {
                return $"Shell integration is already present in {rcPath}.";
            }
        }

        File.AppendAllText(rcPath, "\n" + snippet + "\n");
        return $"✓ Shell integration appended to: {rcPath}\nRestart your {shellName} session to activate.";
    }

    /// <summary>
    /// Enables shell integration inside a WSL distribution by appending to the profile via wsl.exe.
    /// </summary>
    private static string EnableWslShellIntegration(string distro, string shellName, string rcFile, string snippet)
    {
        // Check if the distro exists
        var distros = GetWslDistributions();
        if (distros is null)
        {
            return "WSL is not available.";
        }
        if (!distros.Any(d => d.Equals(distro, StringComparison.OrdinalIgnoreCase)))
        {
            return $"WSL distribution \"{distro}\" not found. Available: {string.Join(", ", distros)}";
        }

        // Check if integration is already present
        var existing = ReadWslFile(distro, rcFile);
        if (existing is not null && existing.Contains(ShellIntegrationMarker, StringComparison.Ordinal))
        {
            return $"Shell integration is already present in {rcFile} on {distro}.";
        }

        // Write the snippet to a temp file on Windows with LF line endings,
        // then use wsl to append it to the profile
        var tempFile = Path.GetTempFileName();
        try
        {
            var content = "\n" + snippet + "\n";
            content = content.Replace("\r\n", "\n");
            File.WriteAllBytes(tempFile, System.Text.Encoding.UTF8.GetBytes(content));

            // Convert Windows path to WSL path
            var wslPathResult = ProcessHelper.Run("wsl.exe", $"-d {distro} wslpath \"{tempFile}\"", timeoutMs: 10000);
            if (wslPathResult is null || wslPathResult.ExitCode != 0)
            {
                return $"Failed to convert temp file path for WSL. Is the {distro} distribution running?";
            }

            var wslTempPath = wslPathResult.Stdout.Trim();

            // Append to the profile
            var appendResult = ProcessHelper.Run("wsl.exe", $"-d {distro} bash -c \"cat '{wslTempPath}' >> {rcFile}\"", timeoutMs: 10000);
            if (appendResult is null || appendResult.ExitCode != 0)
            {
                var err = appendResult?.Stderr ?? "unknown error";
                return $"Failed to append to {rcFile} on {distro}: {err}";
            }

            // Verify it was written
            var verify = ReadWslFile(distro, rcFile);
            if (verify is not null && verify.Contains(ShellIntegrationMarker, StringComparison.Ordinal))
            {
                return $"✓ Shell integration appended to {rcFile} on WSL/{distro}.\nRestart your {shellName} session to activate.";
            }

            return $"Append appeared to succeed but verification failed. Check {rcFile} on {distro} manually.";
        }
        finally
        {
            try { File.Delete(tempFile); } catch { }
        }
    }

    private static string GetPowerShellSnippet()
    {
        return """
            # Shell integration for Windows Terminal
            # Marks prompt boundaries and tracks command status using OSC 133 sequences
            $Global:__LastHistoryId = -1

            function Global:__Terminal-Get-LastExitCode {
                if ($? -eq $True) { return 0 }
                $LastHistoryEntry = $(Get-History -Count 1)
                $IsPowerShellError = $Error[0] -and $Error[0].InvocationInfo -and
                    $Error[0].InvocationInfo.HistoryId -eq $LastHistoryEntry.Id
                if ($IsPowerShellError) { return -1 }
                return 0
            }

            function Global:prompt {
                $LastExitCode = $(__Terminal-Get-LastExitCode)

                # FTCS_COMMAND_FINISHED — marks end of previous command output with exit code
                if ($Global:__LastHistoryId -ne -1) {
                    Write-Host "`e]133;D;$LastExitCode`a" -NoNewline
                }

                $loc = $executionContext.SessionState.Path.CurrentLocation
                $out = ""

                # OSC 9;9 — notify Terminal of current working directory
                $out += "`e]9;9;`"$($loc.Path)`"`a"

                # FTCS_PROMPT — marks start of prompt
                $out += "`e]133;A`a"

                # Your prompt content here (customize as desired)
                $out += "PS $loc$('>' * ($nestedPromptLevel + 1)) "

                # FTCS_COMMAND_START — marks end of prompt / start of user input
                $out += "`e]133;B`a"

                $Global:__LastHistoryId = (Get-History -Count 1).Id ?? -1
                return $out
            }
            """;
    }

    private static string GetBashSnippet()
    {
        return """
            # Shell integration for Windows Terminal
            # Marks prompt boundaries and tracks command status using OSC 133 sequences

            __terminal_prompt_start() {
                printf '\e]133;A\a'
            }

            __terminal_prompt_end() {
                printf '\e]133;B\a'
            }

            __terminal_preexec() {
                printf '\e]133;C\a'
            }

            __terminal_precmd() {
                local exit_code=$?
                printf '\e]133;D;%d\a' "$exit_code"
                printf '\e]9;9;"%s"\a' "$PWD"
            }

            PROMPT_COMMAND="__terminal_precmd;${PROMPT_COMMAND}"
            PS0="${PS0}\$(__terminal_preexec)"
            PS1="\$(__terminal_prompt_start)${PS1}\$(__terminal_prompt_end)"
            """;
    }

    private static string GetZshSnippet()
    {
        return """
            # Shell integration for Windows Terminal
            # Marks prompt boundaries and tracks command status using OSC 133 sequences

            __terminal_precmd() {
                printf '\e]133;D;%d\a' "$?"
                printf '\e]9;9;"%s"\a' "$PWD"
            }

            __terminal_preexec() {
                printf '\e]133;C\a'
            }

            precmd_functions+=(__terminal_precmd)
            preexec_functions+=(__terminal_preexec)

            # Wrap prompt with FTCS markers
            PS1=$'%{\e]133;A\a%}'$PS1$'%{\e]133;B\a%}'
            """;
    }
}
