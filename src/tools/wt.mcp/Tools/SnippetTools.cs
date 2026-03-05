using ModelContextProtocol.Server;
using System.ComponentModel;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

/// <summary>
/// MCP tools for managing snippets (sendInput commands) across all Windows Terminal locations.
/// Snippets can live in three places:
/// - wt.json files: per-directory snippets, best for project/folder-specific commands
/// - settings.json actions: global sendInput actions, available everywhere via command palette
/// - fragment extensions: portable sendInput actions grouped with other fragment settings
/// The AddSnippet tool handles all three locations via its "location" parameter.
/// </summary>
[McpServerToolType]
internal class SnippetTools
{
    // wt.json snippets are cached per-directory in Terminal's memory.
    // New directories load on first access, but edits to already-cached
    // directories require restarting Terminal.
    private const string WtJsonReloadNote =
        "Snippets appear in the Suggestions UI and Snippets Pane. " +
        "If this directory was already cached, restart Terminal to pick up the changes.";

    // settings.json changes are picked up automatically by Terminal's file watcher.
    private const string SettingsReloadNote =
        "Available in the Suggestions UI, Snippets Pane, and Command Palette. " +
        "Settings changes are picked up automatically.";
    [McpServerTool, Description("""
        Reads a wt.json snippet file. If no path is given, searches the current directory
        and parent directories (same lookup behavior as Windows Terminal).
        Returns the file content and its resolved location.
        """)]
    public static string ReadWtJson(
        [Description("Path to the wt.json file or the directory to search in. If omitted, searches from the current directory upward.")] string? path = null)
    {
        string resolvedPath;

        if (path is not null && File.Exists(path))
        {
            resolvedPath = Path.GetFullPath(path);
        }
        else
        {
            var searchDir = path is not null && Directory.Exists(path) ? path : null;
            var found = WtJsonHelper.FindWtJson(searchDir);
            if (found is null)
            {
                var searchFrom = searchDir ?? Directory.GetCurrentDirectory();
                return $"No .wt.json file found searching from: {searchFrom}";
            }
            resolvedPath = found;
        }

        var (content, error) = WtJsonHelper.ReadWtJson(resolvedPath);
        if (error is not null)
        {
            return error;
        }

        return $"File: {resolvedPath}\n\n{content}";
    }

    [McpServerTool, Description("""
        Previews creating a new wt.json snippet file WITHOUT writing any changes.
        Returns the formatted content and validation results.
        Always call this before CreateWtJson so the user can review the content.
        Display the returned preview to the user in your response so they can review it.
        wt.json files require "$version" (string) and "snippets" (array).
        Each snippet needs "name" and "input". Optional: "description" and "icon".
        Add \r at the end of "input" to auto-execute the command.
        """)]
    public static string PreviewCreateWtJson(
        [Description("The full JSON content of the wt.json file")] string wtJson,
        [Description("Directory to create the file in. Defaults to the current directory.")] string? directory = null)
    {
        var dir = directory ?? Directory.GetCurrentDirectory();
        var path = Path.Combine(dir, ".wt.json");
        var prettyJson = WtJsonHelper.PrettyPrint(wtJson);
        var exists = File.Exists(path);

        var validationErrors = WtJsonHelper.Validate(prettyJson);

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
        Creates a new wt.json snippet file.
        IMPORTANT: Always call PreviewCreateWtJson first and show the preview to the user.
        After showing the preview, call this tool immediately — do not ask for separate user confirmation.
        The client will show its own confirmation dialog for approval.
        """)]
    public static string CreateWtJson(
        [Description("The full JSON content of the wt.json file")] string wtJson,
        [Description("Directory to create the file in. Defaults to the current directory.")] string? directory = null)
    {
        var dir = directory ?? Directory.GetCurrentDirectory();
        var path = Path.Combine(dir, ".wt.json");
        var prettyJson = WtJsonHelper.PrettyPrint(wtJson);

        var validationErrors = WtJsonHelper.Validate(prettyJson);
        if (validationErrors.Count > 0)
        {
            var errors = string.Join("\n", validationErrors.Select(e => $"  ✗ {e}"));
            return $"wt.json rejected — validation failed:\n{errors}";
        }

        WtJsonHelper.WriteWtJson(path, prettyJson);
        return $"Created: {path}\n\n{WtJsonReloadNote}";
    }

    [McpServerTool, Description("""
        Previews changes to an existing wt.json file WITHOUT writing any changes.
        Returns a unified diff showing exactly what would change.
        Always call this before UpdateWtJson so the user can review the diff.
        IMPORTANT: You MUST display the returned diff inside a ```diff fenced code block.
        """)]
    public static string PreviewUpdateWtJson(
        [Description("The new full JSON content of the wt.json file")] string wtJson,
        [Description("Path to the wt.json file. If omitted, searches from the current directory upward.")] string? path = null)
    {
        var resolvedPath = ResolvePath(path);
        if (resolvedPath is null)
        {
            return path is null
                ? "No .wt.json file found. Use PreviewCreateWtJson/CreateWtJson to create a new one."
                : $"File not found: {path}";
        }

        var (currentContent, error) = WtJsonHelper.ReadWtJson(resolvedPath);
        if (error is not null)
        {
            return error;
        }

        var prettyNew = WtJsonHelper.PrettyPrint(wtJson);
        var validationErrors = WtJsonHelper.Validate(prettyNew);

        var diff = SettingsHelper.UnifiedDiff(currentContent!, prettyNew, Path.GetFileName(resolvedPath));

        if (string.IsNullOrEmpty(diff))
        {
            return "No changes — the new content is identical to the existing file.";
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
        Updates an existing wt.json file with new content.
        IMPORTANT: Always call PreviewUpdateWtJson first and show the diff to the user.
        After showing the diff, call this tool immediately — do not ask for separate user confirmation.
        The client will show its own confirmation dialog for approval.
        """)]
    public static string UpdateWtJson(
        [Description("The new full JSON content of the wt.json file")] string wtJson,
        [Description("Path to the wt.json file. If omitted, searches from the current directory upward.")] string? path = null)
    {
        var resolvedPath = ResolvePath(path);
        if (resolvedPath is null)
        {
            return path is null
                ? "No .wt.json file found. Use CreateWtJson to create a new one."
                : $"File not found: {path}";
        }

        var prettyJson = WtJsonHelper.PrettyPrint(wtJson);

        var validationErrors = WtJsonHelper.Validate(prettyJson);
        if (validationErrors.Count > 0)
        {
            var errors = string.Join("\n", validationErrors.Select(e => $"  ✗ {e}"));
            return $"Update rejected — validation failed:\n{errors}";
        }

        WtJsonHelper.WriteWtJson(resolvedPath, prettyJson);
        return $"Updated: {resolvedPath}\n\n{WtJsonReloadNote}";
    }

    [McpServerTool, Description("""
        Adds a single snippet to a wt.json file. If no wt.json exists in the target directory,
        creates a new one. If one exists, adds the snippet to the existing snippets array.
        Uses the preview/apply pattern: shows what would change, then applies.
        Add \r at the end of the input to auto-execute the command when selected.

        Snippets (also known as "sendInput actions") can live in three locations:
        - "wt.json" (default): Best for project/folder-specific commands. The snippet only appears
          when Terminal's working directory is at or below the wt.json file location.
        - "settings": Best for globally useful commands that should be available everywhere,
          regardless of the current directory. Adds a sendInput action to settings.json.
        - "fragment": Best for portable, grouped settings that belong to a fragment extension.
          Adds a sendInput action to the specified fragment. Requires appName and fileName.

        When the user asks to add a "snippet" or "sendInput action", consider which location
        is most appropriate based on the context. If unclear, prefer wt.json for project-specific
        commands and settings for general-purpose commands.
        """)]
    public static string AddSnippet(
        [Description("The command text to send to the shell. Add \\r at the end to auto-execute.")] string input,
        [Description("Display name for the snippet.")] string name,
        [Description("Optional description of what the snippet does.")] string? description = null,
        [Description("Optional icon (e.g. a Segoe Fluent icon character).")] string? icon = null,
        [Description("Where to add the snippet: 'wt.json' (default, project/folder-specific), 'settings' (global, in settings.json), or 'fragment' (in a fragment extension).")] string location = "wt.json",
        [Description("Path to the wt.json file or directory. Only used when location is 'wt.json'. If omitted, uses the current directory.")] string? path = null,
        [Description("Fragment app name. Required when location is 'fragment'.")] string? appName = null,
        [Description("Fragment file name. Required when location is 'fragment'.")] string? fileName = null,
        [Description("The release channel for settings.json. Only used when location is 'settings'.")] TerminalRelease? release = null,
        [Description("If true, only preview the change without writing. Default is false.")] bool previewOnly = false)
    {
        var locationLower = location.Trim().ToLowerInvariant();

        return locationLower switch
        {
            "wt.json" or "wtjson" or "wt" => AddSnippetToWtJson(input, name, description, icon, path, previewOnly),
            "settings" or "settings.json" => AddSnippetToSettings(input, name, description, icon, release, previewOnly),
            "fragment" or "fragments" => AddSnippetToFragment(input, name, description, icon, appName, fileName, previewOnly),
            _ => $"Unknown location: \"{location}\". Use 'wt.json', 'settings', or 'fragment'."
        };
    }

    private static string AddSnippetToWtJson(string input, string name, string? description, string? icon, string? path, bool previewOnly)
    {
        // Resolve to an existing file or default to CWD
        string resolvedPath;
        if (path is not null && File.Exists(path))
        {
            resolvedPath = Path.GetFullPath(path);
        }
        else if (path is not null && Directory.Exists(path))
        {
            resolvedPath = Path.Combine(Path.GetFullPath(path), ".wt.json");
        }
        else
        {
            var found = WtJsonHelper.FindWtJson(path);
            resolvedPath = found ?? Path.Combine(Directory.GetCurrentDirectory(), ".wt.json");
        }

        // Build the new snippet
        var snippet = new JsonObject { ["input"] = input, ["name"] = name };
        if (description is not null)
        {
            snippet["description"] = description;
        }
        if (icon is not null)
        {
            snippet["icon"] = icon;
        }

        JsonObject root;
        string before;

        if (File.Exists(resolvedPath))
        {
            var (content, error) = WtJsonHelper.ReadWtJson(resolvedPath);
            if (error is not null)
            {
                return error;
            }

            before = content!;
            root = JsonNode.Parse(before)?.AsObject() ?? new JsonObject();
        }
        else
        {
            before = "";
            root = new JsonObject
            {
                ["$version"] = "1.0.0",
                ["snippets"] = new JsonArray()
            };
        }

        // Add the snippet
        if (root["snippets"] is not JsonArray snippets)
        {
            snippets = new JsonArray();
            root["snippets"] = snippets;
        }
        snippets.Add(snippet);

        var writeOptions = new JsonSerializerOptions { WriteIndented = true };
        var after = root.ToJsonString(writeOptions);

        if (string.IsNullOrEmpty(before))
        {
            // New file
            if (previewOnly)
            {
                return $"Will create: {resolvedPath}\n\n{after}\n\n✓ Validation passed";
            }

            var validationErrors = WtJsonHelper.Validate(after);
            if (validationErrors.Count > 0)
            {
                var errors = string.Join("\n", validationErrors.Select(e => $"  ✗ {e}"));
                return $"Rejected — validation failed:\n{errors}";
            }

            WtJsonHelper.WriteWtJson(resolvedPath, after);
            return $"Created {resolvedPath} with snippet \"{name}\".\n\n{WtJsonReloadNote}";
        }

        // Existing file — show diff
        var diff = SettingsHelper.UnifiedDiff(before, after, Path.GetFileName(resolvedPath));

        if (previewOnly)
        {
            return $"Preview of changes to {resolvedPath}:\n\n{diff}";
        }

        var errors2 = WtJsonHelper.Validate(after);
        if (errors2.Count > 0)
        {
            var errorList = string.Join("\n", errors2.Select(e => $"  ✗ {e}"));
            return $"Rejected — validation failed:\n{errorList}";
        }

        WtJsonHelper.WriteWtJson(resolvedPath, after);
        return $"Added snippet \"{name}\" to {resolvedPath}.\n\n{WtJsonReloadNote}";
    }

    private static string AddSnippetToSettings(string input, string name, string? description, string? icon, TerminalRelease? release, bool previewOnly)
    {
        var resolved = SettingsHelper.ResolveRelease(release);
        if (resolved is null)
        {
            return "No Windows Terminal installations found.";
        }

        var (doc, error) = SettingsHelper.LoadSettings(resolved.Value);
        if (error is not null)
        {
            return error;
        }

        // Build a sendInput action
        var action = new JsonObject
        {
            ["command"] = new JsonObject
            {
                ["action"] = "sendInput",
                ["input"] = input
            },
            ["id"] = $"User.sendInput.{SanitizeId(name)}"
        };
        if (name is not null)
        {
            action["name"] = name;
        }
        if (description is not null)
        {
            action["description"] = description;
        }
        if (icon is not null)
        {
            action["icon"] = icon;
        }

        // Add to actions array
        if (doc!["actions"] is not JsonArray actions)
        {
            actions = new JsonArray();
            doc!["actions"] = actions;
        }
        actions.Add(action);

        var writeOptions = new JsonSerializerOptions { WriteIndented = true };
        var before = File.ReadAllText(resolved.Value.GetSettingsJsonPath());
        var after = doc.ToJsonString(writeOptions);
        var diff = SettingsHelper.UnifiedDiff(before, after, $"settings.json ({resolved})");

        if (previewOnly)
        {
            return $"Preview of changes to settings.json ({resolved}):\n\n{diff}";
        }

        var path = resolved.Value.GetSettingsJsonPath();
        File.Copy(path, path + ".bak", overwrite: true);
        File.WriteAllText(path, after);
        return $"Added sendInput action \"{name}\" to settings.json ({resolved}).\n\n{SettingsReloadNote}";
    }

    private static string AddSnippetToFragment(string input, string name, string? description, string? icon, string? appName, string? fileName, bool previewOnly)
    {
        if (string.IsNullOrWhiteSpace(appName))
        {
            return "appName is required when adding a snippet to a fragment extension.";
        }
        if (string.IsNullOrWhiteSpace(fileName))
        {
            return "fileName is required when adding a snippet to a fragment extension.";
        }

        // Build a sendInput action for the fragment
        var action = new JsonObject
        {
            ["command"] = new JsonObject
            {
                ["action"] = "sendInput",
                ["input"] = input
            },
            ["id"] = $"{SanitizeId(appName)}.sendInput.{SanitizeId(name)}"
        };
        if (name is not null)
        {
            action["name"] = name;
        }
        if (description is not null)
        {
            action["description"] = description;
        }
        if (icon is not null)
        {
            action["icon"] = icon;
        }

        // Load or create the fragment
        var fragPath = FragmentHelper.FindFragmentPath(appName, fileName);
        JsonObject root;
        string before;

        if (fragPath is not null)
        {
            var (content, error) = FragmentHelper.ReadFragment(appName, fileName);
            if (error is not null)
            {
                return error;
            }
            before = content!;
            root = JsonNode.Parse(before)?.AsObject() ?? new JsonObject();
        }
        else
        {
            before = "";
            root = new JsonObject();
        }

        // Add to actions array
        if (root["actions"] is not JsonArray actions)
        {
            actions = new JsonArray();
            root["actions"] = actions;
        }
        actions.Add(action);

        var writeOptions = new JsonSerializerOptions { WriteIndented = true };
        var after = root.ToJsonString(writeOptions);

        if (string.IsNullOrEmpty(before))
        {
            if (previewOnly)
            {
                return $"Will create fragment {appName}/{fileName}:\n\n{after}";
            }

            FragmentHelper.WriteFragment(appName, fileName, after);
            return $"Created fragment {appName}/{fileName} with sendInput action \"{name}\".";
        }

        var diff = SettingsHelper.UnifiedDiff(before, after, $"{appName}/{fileName}");

        if (previewOnly)
        {
            return $"Preview of changes to fragment {appName}/{fileName}:\n\n{diff}";
        }

        FragmentHelper.WriteFragment(appName, fileName, after);
        return $"Added sendInput action \"{name}\" to fragment {appName}/{fileName}.";
    }

    private static string SanitizeId(string value)
    {
        return new string(value.Where(c => char.IsLetterOrDigit(c) || c == '.' || c == '_' || c == '-').ToArray());
    }

    private static string? ResolvePath(string? path)
    {
        if (path is not null)
        {
            if (File.Exists(path))
            {
                return Path.GetFullPath(path);
            }
            return null;
        }

        return WtJsonHelper.FindWtJson();
    }
}
