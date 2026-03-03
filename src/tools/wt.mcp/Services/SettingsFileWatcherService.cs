using ModelContextProtocol.Protocol;
using ModelContextProtocol.Server;
using System.Collections.Concurrent;

/// <summary>
/// Manages FileSystemWatchers for subscribed settings.json resources and sends
/// MCP resource-update notifications when files change on disk.
/// </summary>
internal sealed class SettingsFileWatcherService : IDisposable
{
    private readonly ConcurrentDictionary<string, FileSystemWatcher> _watchers = new();

    // Debounce window — FileSystemWatcher often fires multiple events per save
    private readonly ConcurrentDictionary<string, DateTime> _lastNotified = new();
    private static readonly TimeSpan DebounceInterval = TimeSpan.FromMilliseconds(500);

    public void Subscribe(string uri, McpServer server)
    {
        if (_watchers.ContainsKey(uri))
        {
            return;
        }

        // Parse release from URI like "wt://settings/Stable"
        var releaseName = uri.Replace("wt://settings/", "");
        if (!Enum.TryParse<TerminalRelease>(releaseName, ignoreCase: true, out var release))
            return;

        var path = release.GetSettingsJsonPath();
        var directory = Path.GetDirectoryName(path);
        var fileName = Path.GetFileName(path);

        if (directory is null || !Directory.Exists(directory))
        {
            return;
        }

        var watcher = new FileSystemWatcher(directory, fileName)
        {
            NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.Size,
            EnableRaisingEvents = true
        };

        watcher.Changed += async (_, _) =>
        {
            var now = DateTime.UtcNow;
            if (_lastNotified.TryGetValue(uri, out var last) && now - last < DebounceInterval)
            {
                return;
            }

            _lastNotified[uri] = now;

            try
            {
                await server.SendNotificationAsync(
                    NotificationMethods.ResourceUpdatedNotification,
                    new ResourceUpdatedNotificationParams { Uri = uri });
            }
            catch
            {
                // Best-effort notification; don't crash the watcher
            }
        };

        if (!_watchers.TryAdd(uri, watcher))
        {
            watcher.Dispose();
        }
    }

    public void Unsubscribe(string uri)
    {
        if (_watchers.TryRemove(uri, out var watcher))
        {
            watcher.Dispose();
        }

        _lastNotified.TryRemove(uri, out _);
    }

    public void Dispose()
    {
        foreach (var watcher in _watchers.Values)
            watcher.Dispose();

        _watchers.Clear();
        _lastNotified.Clear();
    }
}
