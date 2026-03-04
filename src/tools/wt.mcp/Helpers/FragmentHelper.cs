using System.Text.Json;
using System.Text.Json.Nodes;

/// <summary>
/// Shared helpers for Windows Terminal fragment extension paths and I/O.
/// </summary>
internal static class FragmentHelper
{
    private static readonly JsonSerializerOptions s_writeOptions = new() { WriteIndented = true };

    /// <summary>
    /// Returns the user-scope fragments directory:
    /// %LOCALAPPDATA%\Microsoft\Windows Terminal\Fragments
    /// </summary>
    public static string GetUserFragmentsRoot()
    {
        var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(localAppData, "Microsoft", "Windows Terminal", "Fragments");
    }

    /// <summary>
    /// Returns the machine-scope fragments directory:
    /// %PROGRAMDATA%\Microsoft\Windows Terminal\Fragments
    /// </summary>
    public static string GetMachineFragmentsRoot()
    {
        var programData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
        return Path.Combine(programData, "Microsoft", "Windows Terminal", "Fragments");
    }

    /// <summary>
    /// Returns the full path to a specific fragment file.
    /// </summary>
    public static string GetFragmentPath(string appName, string fileName)
    {
        if (!fileName.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
        {
            fileName += ".json";
        }

        return Path.Combine(GetUserFragmentsRoot(), appName, fileName);
    }

    /// <summary>
    /// Finds a fragment file by searching user-scope first, then machine-scope.
    /// Returns the full path if found, or null if not found in either location.
    /// </summary>
    public static string? FindFragmentPath(string appName, string fileName)
    {
        if (!fileName.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
        {
            fileName += ".json";
        }

        var userPath = Path.Combine(GetUserFragmentsRoot(), appName, fileName);
        if (File.Exists(userPath))
        {
            return userPath;
        }

        var machinePath = Path.Combine(GetMachineFragmentsRoot(), appName, fileName);
        if (File.Exists(machinePath))
        {
            return machinePath;
        }

        return null;
    }

    /// <summary>
    /// Lists all installed fragments from both user and machine scopes.
    /// Returns (scope, appName, fileName, fullPath) tuples.
    /// </summary>
    public static List<(string Scope, string AppName, string FileName, string FullPath)> ListAllFragments()
    {
        var results = new List<(string, string, string, string)>();

        ScanFragmentRoot(GetUserFragmentsRoot(), "user", results);
        ScanFragmentRoot(GetMachineFragmentsRoot(), "machine", results);

        return results;
    }

    /// <summary>
    /// Reads a fragment file and returns its content as a pretty-printed JSON string.
    /// Returns (content, error).
    /// </summary>
    public static (string? Content, string? Error) ReadFragment(string appName, string fileName)
    {
        var path = FindFragmentPath(appName, fileName);
        if (path is null)
        {
            return (null, $"Fragment not found for {appName}/{fileName} in user or machine scope.");
        }

        return ReadFragmentAt(path);
    }

    /// <summary>
    /// Reads all fragment files for a given app name across both scopes.
    /// Returns a list of (fileName, fullPath, content) tuples.
    /// </summary>
    public static List<(string FileName, string FullPath, string Content)> ReadAllFragmentsForApp(string appName)
    {
        var results = new List<(string, string, string)>();
        var roots = new[] { GetUserFragmentsRoot(), GetMachineFragmentsRoot() };

        foreach (var root in roots)
        {
            var appDir = Path.Combine(root, appName);
            if (!Directory.Exists(appDir))
            {
                continue;
            }

            foreach (var file in Directory.GetFiles(appDir, "*.json"))
            {
                var (content, _) = ReadFragmentAt(file);
                if (content is not null)
                {
                    results.Add((Path.GetFileName(file), file, content));
                }
            }
        }

        return results;
    }

    private static (string? Content, string? Error) ReadFragmentAt(string path)
    {
        var json = File.ReadAllText(path);

        // Pretty-print for consistent output
        try
        {
            var doc = JsonNode.Parse(json);
            if (doc is not null)
            {
                return (doc.ToJsonString(s_writeOptions), null);
            }
        }
        catch
        {
            // Fall through and return raw content
        }

        return (json, null);
    }

    /// <summary>
    /// Writes a fragment file, creating the directory if needed.
    /// </summary>
    public static string WriteFragment(string appName, string fileName, string content)
    {
        var path = GetFragmentPath(appName, fileName);
        var directory = Path.GetDirectoryName(path)!;

        Directory.CreateDirectory(directory);

        // Back up if overwriting
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
            // Return as-is if parsing fails
        }

        return json;
    }

    private static void ScanFragmentRoot(
        string root,
        string scope,
        List<(string Scope, string AppName, string FileName, string FullPath)> results)
    {
        if (!Directory.Exists(root))
        {
            return;
        }

        foreach (var appDir in Directory.GetDirectories(root))
        {
            var appName = Path.GetFileName(appDir);
            foreach (var file in Directory.GetFiles(appDir, "*.json"))
            {
                results.Add((scope, appName, Path.GetFileName(file), file));
            }
        }
    }
}
