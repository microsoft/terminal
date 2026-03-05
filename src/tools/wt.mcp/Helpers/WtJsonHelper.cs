using System.Text.Json;
using System.Text.Json.Nodes;

/// <summary>
/// Helpers for finding, reading, and validating wt.json snippet files.
/// wt.json files add per-directory snippets to Windows Terminal's suggestions pane.
/// </summary>
internal static class WtJsonHelper
{
    private static readonly JsonSerializerOptions s_writeOptions = new() { WriteIndented = true };

    /// <summary>
    /// Searches for a wt.json file starting from the given directory and walking
    /// up parent directories (same lookup behavior as Windows Terminal).
    /// Returns the full path if found, null otherwise.
    /// </summary>
    public static string? FindWtJson(string? startDirectory = null)
    {
        var dir = startDirectory ?? Directory.GetCurrentDirectory();

        while (!string.IsNullOrEmpty(dir))
        {
            var candidate = Path.Combine(dir, ".wt.json");
            if (File.Exists(candidate))
            {
                return candidate;
            }

            var parent = Directory.GetParent(dir)?.FullName;
            if (parent == dir)
            {
                break;
            }
            dir = parent;
        }

        return null;
    }

    /// <summary>
    /// Reads a wt.json file and returns its pretty-printed content.
    /// Returns (content, error).
    /// </summary>
    public static (string? Content, string? Error) ReadWtJson(string path)
    {
        if (!File.Exists(path))
        {
            return (null, $"File not found: {path}");
        }

        try
        {
            var json = File.ReadAllText(path);
            var doc = JsonNode.Parse(json);
            if (doc is not null)
            {
                return (doc.ToJsonString(s_writeOptions), null);
            }

            return (json, null);
        }
        catch (JsonException ex)
        {
            return (null, $"Failed to parse wt.json: {ex.Message}");
        }
    }

    /// <summary>
    /// Validates the structure of a wt.json file.
    /// Returns a list of validation errors (empty = valid).
    /// </summary>
    public static List<string> Validate(string json)
    {
        var errors = new List<string>();

        JsonNode? doc;
        try
        {
            doc = JsonNode.Parse(json);
        }
        catch (JsonException ex)
        {
            errors.Add($"Invalid JSON: {ex.Message}");
            return errors;
        }

        if (doc is not JsonObject root)
        {
            errors.Add("wt.json must be a JSON object.");
            return errors;
        }

        // Must have $version
        if (!root.ContainsKey("$version"))
        {
            errors.Add("Missing required \"$version\" property.");
        }
        else if (root["$version"] is not JsonValue)
        {
            errors.Add("\"$version\" must be a string value.");
        }

        // Must have snippets array
        if (!root.ContainsKey("snippets"))
        {
            errors.Add("Missing required \"snippets\" array.");
        }
        else if (root["snippets"] is not JsonArray snippets)
        {
            errors.Add("\"snippets\" must be an array.");
        }
        else
        {
            for (var i = 0; i < snippets.Count; i++)
            {
                if (snippets[i] is not JsonObject snippet)
                {
                    errors.Add($"snippets[{i}]: expected an object.");
                    continue;
                }

                if (!snippet.ContainsKey("name"))
                {
                    errors.Add($"snippets[{i}]: missing required \"name\" property.");
                }

                if (!snippet.ContainsKey("input"))
                {
                    errors.Add($"snippets[{i}]: missing required \"input\" property.");
                }
            }
        }

        return errors;
    }

    /// <summary>
    /// Writes a wt.json file, creating a backup if one already exists.
    /// </summary>
    public static string WriteWtJson(string path, string content)
    {
        if (File.Exists(path))
        {
            File.Copy(path, path + ".bak", overwrite: true);
        }

        File.WriteAllText(path, content);
        return path;
    }

    /// <summary>
    /// Pretty-prints a JSON string.
    /// </summary>
    public static string PrettyPrint(string json)
    {
        try
        {
            var doc = JsonNode.Parse(json);
            if (doc is not null)
            {
                return doc.ToJsonString(s_writeOptions);
            }
        }
        catch
        {
            // Return as-is
        }

        return json;
    }
}
