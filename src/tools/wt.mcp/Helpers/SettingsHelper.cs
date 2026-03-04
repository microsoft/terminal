using Json.Patch;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

/// <summary>
/// Shared helpers for reading and writing Windows Terminal settings.json
/// as a mutable JsonNode tree.
/// </summary>
internal static class SettingsHelper
{
    private static readonly JsonSerializerOptions s_writeOptions = new() { WriteIndented = true };

    // A fixed namespace UUID for generating deterministic profile GUIDs
    private static readonly Guid s_profileNamespace =
        Guid.Parse("7f4015d0-3f34-5137-9fdc-0c66a1866a30");

    /// <summary>
    /// Generates a deterministic GUID (UUID v5 / SHA-1) from the given name.
    /// Uses a fixed namespace so the same name always produces the same GUID.
    /// </summary>
    public static string GenerateDeterministicGuid(string name)
    {
        // RFC 4122 UUID v5: SHA-1 hash of (namespace bytes ++ name bytes)
        var nsBytes = s_profileNamespace.ToByteArray();

        // .NET stores the first 3 fields in little-endian; convert to big-endian (network order)
        Array.Reverse(nsBytes, 0, 4);
        Array.Reverse(nsBytes, 4, 2);
        Array.Reverse(nsBytes, 6, 2);

        var nameBytes = Encoding.UTF8.GetBytes(name);
        var input = new byte[nsBytes.Length + nameBytes.Length];
        Buffer.BlockCopy(nsBytes, 0, input, 0, nsBytes.Length);
        Buffer.BlockCopy(nameBytes, 0, input, nsBytes.Length, nameBytes.Length);

        var hash = SHA1.HashData(input);

        // Set version (5) and variant (RFC 4122)
        hash[6] = (byte)((hash[6] & 0x0F) | 0x50);
        hash[8] = (byte)((hash[8] & 0x3F) | 0x80);

        // Convert back to little-endian for .NET Guid constructor
        Array.Reverse(hash, 0, 4);
        Array.Reverse(hash, 4, 2);
        Array.Reverse(hash, 6, 2);

        var guid = new Guid(hash.AsSpan(0, 16));
        return $"{{{guid}}}";
    }

    /// <summary>
    /// Resolves the release channel to use. If a release is provided, returns it.
    /// Otherwise, returns the most-preview installed channel (Canary > Preview > Stable),
    /// excluding Dev unless no other channel is found. Returns null if nothing is installed.
    /// </summary>
    public static TerminalRelease? ResolveRelease(TerminalRelease? release = null)
    {
        if (release is not null)
        {
            return release;
        }

        if (TerminalRelease.Canary.IsInstalled())
        {
            return TerminalRelease.Canary;
        }
        if (TerminalRelease.Preview.IsInstalled())
        {
            return TerminalRelease.Preview;
        }
        if (TerminalRelease.Stable.IsInstalled())
        {
            return TerminalRelease.Stable;
        }

        return null;
    }

    /// <summary>
    /// Loads settings.json as a mutable JsonNode tree.
    /// Returns (doc, error) — if error is non-null, doc is null.
    /// </summary>
    public static (JsonNode? Doc, string? Error) LoadSettings(TerminalRelease release)
    {
        var path = release.GetSettingsJsonPath();
        if (!File.Exists(path))
        {
            return (null, $"Settings file not found at: {path}");
        }

        var json = File.ReadAllText(path);
        var doc = JsonNode.Parse(json);
        if (doc is null)
        {
            return (null, "Failed to parse settings.json.");
        }

        return (doc, null);
    }

    /// <summary>
    /// Saves the JsonNode tree back to settings.json, creating a .bak backup first.
    /// </summary>
    public static string SaveSettings(JsonNode doc, TerminalRelease release)
    {
        var path = release.GetSettingsJsonPath();

        if (File.Exists(path))
        {
            File.Copy(path, path + ".bak", overwrite: true);
        }

        File.WriteAllText(path, doc.ToJsonString(s_writeOptions));
        return path;
    }

    /// <summary>
    /// Parses and applies a JSON Patch (RFC 6902) to settings.json in memory.
    /// Returns (beforeJson, afterJson, error). If error is non-null, the other values are null.
    /// </summary>
    public static (string? Before, string? After, string? Error) ApplyPatch(TerminalRelease release, string patchJson)
    {
        var path = release.GetSettingsJsonPath();
        if (!File.Exists(path))
        {
            return (null, null, $"Settings file not found at: {path}");
        }

        JsonPatch patch;
        try
        {
            patch = JsonSerializer.Deserialize<JsonPatch>(patchJson)
                ?? throw new JsonException("Patch deserialized to null.");
        }
        catch (JsonException ex)
        {
            return (null, null, $"Invalid JSON Patch document: {ex.Message}");
        }

        var original = File.ReadAllText(path);
        var doc = JsonNode.Parse(original);
        if (doc is null)
        {
            return (null, null, "Failed to parse current settings.json.");
        }

        // Pretty-print the original for consistent diffing
        var before = doc.ToJsonString(s_writeOptions);

        var result = patch.Apply(doc);
        if (!result.IsSuccess)
        {
            return (null, null, $"Patch failed: {result.Error}");
        }

        var after = result.Result!.ToJsonString(s_writeOptions);
        return (before, after, null);
    }

    /// <summary>
    /// Produces a unified diff between two strings, with a configurable number of context lines.
    /// </summary>
    public static string UnifiedDiff(string before, string after, string label, int contextLines = 3)
    {
        var oldLines = before.Split('\n');
        var newLines = after.Split('\n');

        // Simple LCS-based diff
        var diff = new StringBuilder();
        diff.AppendLine($"--- a/{label}");
        diff.AppendLine($"+++ b/{label}");

        // Build edit script: match, delete, insert
        var edits = ComputeEdits(oldLines, newLines);

        // Group into hunks with context
        var hunks = GroupIntoHunks(edits, oldLines, newLines, contextLines);
        foreach (var hunk in hunks)
        {
            diff.Append(hunk);
        }

        return diff.ToString();
    }

    private enum EditKind { Equal, Delete, Insert }

    private record struct Edit(EditKind Kind, int OldIndex, int NewIndex);

    private static List<Edit> ComputeEdits(string[] oldLines, string[] newLines)
    {
        // Myers-like forward scan using a simple LCS approach
        var oldLen = oldLines.Length;
        var newLen = newLines.Length;

        // Build LCS table (optimized for typical settings files which are small)
        var dp = new int[oldLen + 1, newLen + 1];
        for (var i = oldLen - 1; i >= 0; i--)
        {
            for (var j = newLen - 1; j >= 0; j--)
            {
                if (oldLines[i] == newLines[j])
                {
                    dp[i, j] = dp[i + 1, j + 1] + 1;
                }
                else
                {
                    dp[i, j] = Math.Max(dp[i + 1, j], dp[i, j + 1]);
                }
            }
        }

        // Walk the table to produce edits
        var edits = new List<Edit>();
        int oi = 0, ni = 0;
        while (oi < oldLen && ni < newLen)
        {
            if (oldLines[oi] == newLines[ni])
            {
                edits.Add(new Edit(EditKind.Equal, oi, ni));
                oi++;
                ni++;
            }
            else if (dp[oi + 1, ni] >= dp[oi, ni + 1])
            {
                edits.Add(new Edit(EditKind.Delete, oi, -1));
                oi++;
            }
            else
            {
                edits.Add(new Edit(EditKind.Insert, -1, ni));
                ni++;
            }
        }

        while (oi < oldLen)
        {
            edits.Add(new Edit(EditKind.Delete, oi++, -1));
        }

        while (ni < newLen)
        {
            edits.Add(new Edit(EditKind.Insert, -1, ni++));
        }

        return edits;
    }

    private static List<string> GroupIntoHunks(List<Edit> edits, string[] oldLines, string[] newLines, int ctx)
    {
        var hunks = new List<string>();
        var changeIndices = new List<int>();

        for (var i = 0; i < edits.Count; i++)
        {
            if (edits[i].Kind != EditKind.Equal)
            {
                changeIndices.Add(i);
            }
        }

        if (changeIndices.Count == 0)
        {
            return hunks;
        }

        // Group nearby changes into hunks
        var groups = new List<(int Start, int End)>();
        var gStart = changeIndices[0];
        var gEnd = changeIndices[0];

        for (var i = 1; i < changeIndices.Count; i++)
        {
            if (changeIndices[i] - gEnd <= ctx * 2 + 1)
            {
                gEnd = changeIndices[i];
            }
            else
            {
                groups.Add((gStart, gEnd));
                gStart = changeIndices[i];
                gEnd = changeIndices[i];
            }
        }
        groups.Add((gStart, gEnd));

        foreach (var (start, end) in groups)
        {
            var hunkStart = Math.Max(0, start - ctx);
            var hunkEnd = Math.Min(edits.Count - 1, end + ctx);

            // Calculate line numbers for the hunk header
            int oldStart = 0, oldCount = 0, newStart = 0, newCount = 0;
            var firstOld = true;
            var firstNew = true;

            var body = new StringBuilder();
            for (var i = hunkStart; i <= hunkEnd; i++)
            {
                var edit = edits[i];
                switch (edit.Kind)
                {
                    case EditKind.Equal:
                        if (firstOld) { oldStart = edit.OldIndex + 1; firstOld = false; }
                        if (firstNew) { newStart = edit.NewIndex + 1; firstNew = false; }
                        oldCount++;
                        newCount++;
                        body.AppendLine($" {oldLines[edit.OldIndex]}");
                        break;
                    case EditKind.Delete:
                        if (firstOld) { oldStart = edit.OldIndex + 1; firstOld = false; }
                        oldCount++;
                        body.AppendLine($"-{oldLines[edit.OldIndex]}");
                        break;
                    case EditKind.Insert:
                        if (firstNew) { newStart = edit.NewIndex + 1; firstNew = false; }
                        newCount++;
                        body.AppendLine($"+{newLines[edit.NewIndex]}");
                        break;
                }
            }

            if (firstOld) { oldStart = 1; }
            if (firstNew) { newStart = 1; }

            hunks.Add($"@@ -{oldStart},{oldCount} +{newStart},{newCount} @@\n{body}");
        }

        return hunks;
    }
}
