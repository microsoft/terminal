using ModelContextProtocol;
using ModelContextProtocol.Protocol;
using ModelContextProtocol.Server;
using System.ComponentModel;

/// <summary>
/// MCP resources that expose Windows Terminal settings.json files.
/// </summary>
[McpServerResourceType]
internal class SettingsResources
{
    private const string SchemaUrl = "https://raw.githubusercontent.com/microsoft/terminal/main/doc/cascadia/profiles.schema.json";
    private static readonly HttpClient s_httpClient = new();

    [McpServerResource(UriTemplate = "wt://settings/{release}", Name = "Windows Terminal Settings")]
    [Description("Returns the settings.json for a given Terminal release channel (Stable, Preview, Canary, or Dev).")]
    public static TextResourceContents GetSettings(
        [Description("The release channel: Stable, Preview, Canary, or Dev")] string release)
    {
        if (!Enum.TryParse<TerminalRelease>(release, ignoreCase: true, out var channel))
        {
            throw new McpException($"Unknown release channel: '{release}'. Use Stable, Preview, Canary, or Dev.");
        }

        var path = channel.GetSettingsJsonPath();
        if (!File.Exists(path))
        {
            throw new McpException($"Settings file not found at: {path}");
        }

        return new TextResourceContents
        {
            Uri = $"wt://settings/{release}",
            MimeType = "application/json",
            Text = File.ReadAllText(path)
        };
    }

    [McpServerResource(UriTemplate = "wt://schema", Name = "Windows Terminal Settings Schema")]
    [Description("Returns the JSON schema for Windows Terminal settings.json, fetched from the official GitHub repository.")]
    public static async Task<TextResourceContents> GetSettingsSchema()
    {
        try
        {
            var schema = await s_httpClient.GetStringAsync(SchemaUrl);
            return new TextResourceContents
            {
                Uri = "wt://schema",
                MimeType = "application/json",
                Text = schema
            };
        }
        catch (HttpRequestException ex)
        {
            throw new McpException($"Failed to fetch settings schema from {SchemaUrl}: {ex.Message}");
        }
    }
}
