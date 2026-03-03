using ModelContextProtocol.Server;
using System.ComponentModel;

/// <summary>
/// MCP tools for reading and writing Windows Terminal settings.json.
/// </summary>
[McpServerToolType]
internal class SettingsTools
{
    [McpServerTool, Description("Lists which Windows Terminal release channels are installed on this machine.")]
    public static string ListInstalledChannels()
    {
        var installed = Enum.GetValues<TerminalRelease>()
            .Where(r => r.IsInstalled())
            .Select(r => $"{r} ({r.GetSettingsJsonPath()})");

        var results = installed.ToList();
        if (results.Count == 0)
        {
            return "No Windows Terminal installations found.";
        }

        var defaultChannel = TerminalReleaseExtensions.DetectDefaultChannel();
        return $"Installed channels:\n{string.Join("\n", results)}\n\nDefault channel: {defaultChannel}";
    }

    [McpServerTool, Description("Returns the path to the settings directory (LocalState) for a Windows Terminal release channel. Other files like state.json and fragments are also stored here.")]
    public static string GetSettingsDirectory(
        [Description("The release channel. If not specified, the most-preview installed channel is used.")] TerminalRelease? release = null)
    {
        release ??= TerminalReleaseExtensions.DetectDefaultChannel();
        if (release is null)
        {
            return "No Windows Terminal installations found.";
        }

        var directory = Path.GetDirectoryName(release.Value.GetSettingsJsonPath());
        if (directory is null || !Directory.Exists(directory))
        {
            return $"Settings directory not found for {release}. Is this Terminal release installed?";
        }

        var files = Directory.GetFiles(directory)
            .Select(Path.GetFileName);

        return $"Settings directory for {release}: {directory}\n\nContents:\n{string.Join("\n", files)}";
    }

    [McpServerTool, Description("Reads the contents of a Windows Terminal settings.json file.")]
    public static string ReadSettings(
        [Description("The release channel to read settings from. If not specified, the most-preview installed channel is used.")] TerminalRelease? release = null)
    {
        release ??= TerminalReleaseExtensions.DetectDefaultChannel();
        if (release is null)
        {
            return "No Windows Terminal installations found.";
        }

        var path = release.Value.GetSettingsJsonPath();
        if (!File.Exists(path))
        {
            return $"Settings file not found at: {path}";
        }

        return File.ReadAllText(path);
    }

    [McpServerTool, Description("Writes new contents to a Windows Terminal settings.json file. Use with caution — this overwrites the entire file.")]
    public static string WriteSettings(
        [Description("The full JSON content to write to settings.json")] string settingsJson,
        [Description("The release channel to write settings to. If not specified, the most-preview installed channel is used.")] TerminalRelease? release = null)
    {
        release ??= TerminalReleaseExtensions.DetectDefaultChannel();
        if (release is null)
        {
            return "No Windows Terminal installations found.";
        }

        var path = release.Value.GetSettingsJsonPath();
        var directory = Path.GetDirectoryName(path);
        if (directory is not null && !Directory.Exists(directory))
        {
            return $"Settings directory not found: {directory}. Is this Terminal release installed?";
        }

        // Back up the existing file before overwriting
        if (File.Exists(path))
        {
            File.Copy(path, path + ".bak", overwrite: true);
        }

        File.WriteAllText(path, settingsJson);
        return $"Settings written to: {path}";
    }
}
