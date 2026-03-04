using Json.Patch;
using ModelContextProtocol.Server;
using System.ComponentModel;
using System.Text.Json;
using System.Text.Json.Nodes;

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

    [McpServerTool, Description("""
        Previews a JSON Patch (RFC 6902) against Windows Terminal settings.json WITHOUT writing any changes.
        Returns a unified diff showing exactly what would change.
        Always call this before ApplySettingsChange so the user can review the diff.
        Display the returned diff to the user in your response as a code block so they can review it.
        The patch is an array of operations, each with "op", "path", and optionally "value" or "from".
        Supported ops: "add", "remove", "replace", "move", "copy", "test".
        Example: [{"op": "replace", "path": "/theme", "value": "dark"}]
        """)]
    public static string PreviewSettingsChange(
        [Description("A JSON Patch document (RFC 6902): an array of operations to apply")] string patchJson,
        [Description("The release channel. If not specified, the most-preview installed channel is used.")] TerminalRelease? release = null)
    {
        var resolved = SettingsHelper.ResolveRelease(release);
        if (resolved is null)
        {
            return "No Windows Terminal installations found.";
        }

        var (before, patched, error) = SettingsHelper.ApplyPatch(resolved.Value, patchJson);
        if (error is not null)
        {
            return error;
        }

        var diff = SettingsHelper.UnifiedDiff(before!, patched!, $"settings.json ({resolved})");
        if (string.IsNullOrEmpty(diff))
        {
            return "No changes — the patch produces identical output.";
        }

        return diff;
    }

    [McpServerTool, Description("""
        Applies a JSON Patch (RFC 6902) to a Windows Terminal settings.json file and writes the result.
        IMPORTANT: Always call PreviewSettingsChange first and show the diff to the user before calling this tool.
        After showing the diff, call this tool immediately — do not ask for separate user confirmation.
        The client will show its own confirmation dialog for approval.
        """)]
    public static string ApplySettingsChange(
        [Description("A JSON Patch document (RFC 6902): an array of operations to apply")] string patchJson,
        [Description("The release channel. If not specified, the most-preview installed channel is used.")] TerminalRelease? release = null)
    {
        var resolved = SettingsHelper.ResolveRelease(release);
        if (resolved is null)
        {
            return "No Windows Terminal installations found.";
        }

        var (before, patched, error) = SettingsHelper.ApplyPatch(resolved.Value, patchJson);
        if (error is not null)
        {
            return error;
        }

        var path = resolved.Value.GetSettingsJsonPath();

        // Back up before writing
        File.Copy(path, path + ".bak", overwrite: true);

        var writeOptions = new JsonSerializerOptions { WriteIndented = true };
        File.WriteAllText(path, patched!);

        return $"Settings updated. Written to: {path}";
    }
}
