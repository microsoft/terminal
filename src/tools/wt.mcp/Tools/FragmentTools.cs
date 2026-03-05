using ModelContextProtocol.Server;
using System.ComponentModel;
using System.Text;

/// <summary>
/// MCP tools for managing Windows Terminal fragment extensions.
/// Fragments are portable, shareable settings components that can add
/// profiles, color schemes, and actions without modifying settings.json.
/// </summary>
[McpServerToolType]
internal class FragmentTools
{
    private const string RestartNotice =
        "\n\n⚠ Windows Terminal must be fully restarted for fragment changes to take effect.";
    [McpServerTool, Description("""
        Lists installed Windows Terminal fragment extensions.
        Fragments are found in user-scope (%LOCALAPPDATA%\Microsoft\Windows Terminal\Fragments)
        and machine-scope (%PROGRAMDATA%\Microsoft\Windows Terminal\Fragments) directories.
        If appName is provided, only lists fragments for that app.
        """)]
    public static string ListFragments(
        [Description("Optional app name filter. If provided, only lists fragments for this app.")] string? appName = null)
    {
        var fragments = FragmentHelper.ListAllFragments();

        if (appName is not null)
        {
            fragments = fragments.Where(f => f.AppName.Equals(appName, StringComparison.OrdinalIgnoreCase)).ToList();
        }

        if (fragments.Count == 0)
        {
            var filter = appName is not null ? $" for \"{appName}\"" : "";
            return $"No fragments found{filter}.\n\nUser fragments directory: {FragmentHelper.GetUserFragmentsRoot()}\nMachine fragments directory: {FragmentHelper.GetMachineFragmentsRoot()}";
        }

        var sb = new StringBuilder();
        sb.AppendLine($"Found {fragments.Count} fragment(s):\n");

        var grouped = fragments.GroupBy(f => f.Scope);
        foreach (var group in grouped)
        {
            sb.AppendLine($"[{group.Key}]");
            foreach (var (_, app, file, fullPath) in group)
            {
                sb.AppendLine($"  {app}/{file}  ({fullPath})");
            }
            sb.AppendLine();
        }

        return sb.ToString().TrimEnd();
    }

    [McpServerTool, Description("""
        Reads the contents of fragment extension files for an app.
        If fileName is provided, reads that specific file. If omitted, reads all fragment files for the app.
        """)]
    public static string ReadFragment(
        [Description("The app/extension name (folder name under the Fragments directory)")] string appName,
        [Description("The fragment file name (e.g. \"profiles.json\"). If omitted, reads all files for this app.")] string? fileName = null)
    {
        if (fileName is not null)
        {
            var (content, error) = FragmentHelper.ReadFragment(appName, fileName);
            if (error is not null)
            {
                return error;
            }

            var path = FragmentHelper.FindFragmentPath(appName, fileName) ?? FragmentHelper.GetFragmentPath(appName, fileName);
            return $"Fragment: {appName}/{fileName}\nPath: {path}\n\n{content}";
        }

        // Read all fragments for this app
        var fragments = FragmentHelper.ReadAllFragmentsForApp(appName);
        if (fragments.Count == 0)
        {
            return $"No fragments found for \"{appName}\" in user or machine scope.";
        }

        var sb = new StringBuilder();
        sb.AppendLine($"Found {fragments.Count} fragment(s) for \"{appName}\":\n");
        foreach (var (file, fullPath, content) in fragments)
        {
            sb.AppendLine($"--- {appName}/{file} ({fullPath}) ---");
            sb.AppendLine(content);
            sb.AppendLine();
        }

        return sb.ToString().TrimEnd();
    }

    [McpServerTool, Description("""
        Previews creating a new fragment extension WITHOUT writing any changes.
        Returns the formatted fragment content and validation results.
        Always call this before CreateFragment so the user can review the content.
        Display the returned preview to the user in your response so they can review it.
        Fragments support: profiles (with "guid" for new or "updates" for modifying existing),
        schemes (color schemes with a "name"), and actions (with "command", no "keys"/keybindings).
        """)]
    public static string PreviewCreateFragment(
        [Description("The app/extension name (folder name under the Fragments directory)")] string appName,
        [Description("The fragment file name (e.g. \"profiles.json\")")] string fileName,
        [Description("The full JSON content of the fragment")] string fragmentJson)
    {
        var prettyJson = FragmentHelper.PrettyPrint(fragmentJson);
        var path = FragmentHelper.GetFragmentPath(appName, fileName);
        var exists = File.Exists(path);

        var validationErrors = SchemaValidator.ValidateFragment(prettyJson);

        var sb = new StringBuilder();
        sb.AppendLine(exists
            ? $"⚠ File already exists and will be overwritten: {path}"
            : $"Will create: {path}");
        sb.AppendLine();
        sb.AppendLine(prettyJson);

        if (validationErrors.Count > 0)
        {
            sb.AppendLine();
            sb.AppendLine("⚠ Validation issues:");
            foreach (var error in validationErrors)
            {
                sb.AppendLine($"  ⚠ {error}");
            }
        }
        else
        {
            sb.AppendLine();
            sb.AppendLine("✓ Validation passed");
        }

        return sb.ToString().TrimEnd();
    }

    [McpServerTool, Description("""
        Creates a new fragment extension file.
        IMPORTANT: Always call PreviewCreateFragment first and show the preview to the user.
        After showing the preview, call this tool immediately — do not ask for separate user confirmation.
        The client will show its own confirmation dialog for approval.
        """)]
    public static string CreateFragment(
        [Description("The app/extension name (folder name under the Fragments directory)")] string appName,
        [Description("The fragment file name (e.g. \"profiles.json\")")] string fileName,
        [Description("The full JSON content of the fragment")] string fragmentJson)
    {
        var prettyJson = FragmentHelper.PrettyPrint(fragmentJson);

        // Validate before writing
        var validationErrors = SchemaValidator.ValidateFragment(prettyJson);
        if (validationErrors.Count > 0)
        {
            var errors = string.Join("\n", validationErrors.Select(e => $"  ✗ {e}"));
            return $"Fragment rejected — validation failed:\n{errors}";
        }

        var path = FragmentHelper.WriteFragment(appName, fileName, prettyJson);
        return $"Fragment created: {path}{RestartNotice}";
    }

    [McpServerTool, Description("""
        Previews changes to an existing fragment extension WITHOUT writing any changes.
        Returns a unified diff showing exactly what would change.
        Always call this before UpdateFragment so the user can review the diff.
        IMPORTANT: You MUST display the returned diff inside a ```diff fenced code block.
        """)]
    public static string PreviewUpdateFragment(
        [Description("The app/extension name (folder name under the Fragments directory)")] string appName,
        [Description("The fragment file name (e.g. \"profiles.json\")")] string fileName,
        [Description("The new full JSON content of the fragment")] string fragmentJson)
    {
        var path = FragmentHelper.FindFragmentPath(appName, fileName);
        if (path is null)
        {
            return $"Fragment not found for {appName}/{fileName}. Use PreviewCreateFragment/CreateFragment to create a new one.";
        }

        var (currentContent, error) = FragmentHelper.ReadFragment(appName, fileName);
        if (error is not null)
        {
            return error;
        }

        var prettyNew = FragmentHelper.PrettyPrint(fragmentJson);
        var validationErrors = SchemaValidator.ValidateFragment(prettyNew);

        var diff = SettingsHelper.UnifiedDiff(currentContent!, prettyNew, $"{appName}/{fileName}");

        if (string.IsNullOrEmpty(diff))
        {
            return "No changes — the new content is identical to the existing fragment.";
        }

        var sb = new StringBuilder();
        sb.Append(diff);

        if (validationErrors.Count > 0)
        {
            sb.AppendLine();
            sb.AppendLine("⚠ Validation issues:");
            foreach (var ve in validationErrors)
            {
                sb.AppendLine($"  ⚠ {ve}");
            }
        }
        else
        {
            sb.AppendLine();
            sb.AppendLine("✓ Validation passed");
        }

        return sb.ToString().TrimEnd();
    }

    [McpServerTool, Description("""
        Updates an existing fragment extension file with new content.
        IMPORTANT: Always call PreviewUpdateFragment first and show the diff to the user.
        After showing the diff, call this tool immediately — do not ask for separate user confirmation.
        The client will show its own confirmation dialog for approval.
        """)]
    public static string UpdateFragment(
        [Description("The app/extension name (folder name under the Fragments directory)")] string appName,
        [Description("The fragment file name (e.g. \"profiles.json\")")] string fileName,
        [Description("The new full JSON content of the fragment")] string fragmentJson)
    {
        var path = FragmentHelper.FindFragmentPath(appName, fileName);
        if (path is null)
        {
            return $"Fragment not found for {appName}/{fileName}. Use CreateFragment to create a new one.";
        }

        var prettyJson = FragmentHelper.PrettyPrint(fragmentJson);

        // Validate before writing
        var validationErrors = SchemaValidator.ValidateFragment(prettyJson);
        if (validationErrors.Count > 0)
        {
            var errors = string.Join("\n", validationErrors.Select(e => $"  ✗ {e}"));
            return $"Fragment update rejected — validation failed:\n{errors}";
        }

        FragmentHelper.WriteFragment(appName, fileName, prettyJson);
        return $"Fragment updated: {path}{RestartNotice}";
    }

    [McpServerTool, Description("""
        Deletes a fragment extension file, or all fragments for an app if no fileName is specified.
        This action is not reversible (though .bak files may exist from prior updates).
        """)]
    public static string DeleteFragment(
        [Description("The app/extension name (folder name under the Fragments directory)")] string appName,
        [Description("The fragment file name. If omitted, deletes all fragments for this app.")] string? fileName = null)
    {
        if (fileName is not null)
        {
            var path = FragmentHelper.FindFragmentPath(appName, fileName);
            if (path is null)
            {
                return $"Fragment not found for {appName}/{fileName}";
            }

            File.Delete(path);

            // Clean up the app directory if it's now empty
            var appDir = Path.GetDirectoryName(path);
            if (appDir is not null && Directory.Exists(appDir) && !Directory.EnumerateFileSystemEntries(appDir).Any())
            {
                Directory.Delete(appDir);
            }

            return $"Fragment deleted: {path}{RestartNotice}";
        }

        // Delete all fragments for this app across both scopes
        var deleted = new List<string>();
        var roots = new[] { FragmentHelper.GetUserFragmentsRoot(), FragmentHelper.GetMachineFragmentsRoot() };
        foreach (var root in roots)
        {
            var appDir = Path.Combine(root, appName);
            if (Directory.Exists(appDir))
            {
                foreach (var file in Directory.GetFiles(appDir, "*.json"))
                {
                    File.Delete(file);
                    deleted.Add(file);
                }

                if (!Directory.EnumerateFileSystemEntries(appDir).Any())
                {
                    Directory.Delete(appDir);
                }
            }
        }

        if (deleted.Count == 0)
        {
            return $"No fragments found for \"{appName}\"";
        }

        return $"Deleted {deleted.Count} fragment(s) for \"{appName}\":\n{string.Join("\n", deleted)}{RestartNotice}";
    }
}
