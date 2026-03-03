using ModelContextProtocol.Protocol;
using ModelContextProtocol.Server;
using System.ComponentModel;

/// <summary>
/// MCP prompts for interacting with Windows Terminal settings.
/// </summary>
[McpServerPromptType]
internal class SettingsPrompts
{
    [McpServerPrompt, Description("Explains the current Windows Terminal settings.json as natural language.")]
    public static IEnumerable<PromptMessage> ExplainSettings(
        [Description("The Terminal release channel to explain settings for. If not specified, the most-preview installed channel is used.")] TerminalRelease? release = null)
    {
        release ??= TerminalReleaseExtensions.DetectDefaultChannel();
        if (release is null)
        {
            return [
                new PromptMessage
                {
                    Role = Role.User,
                    Content = new TextContentBlock { Text = "I want to understand my Windows Terminal settings, but no Windows Terminal installations were found." }
                }
            ];
        }

        var path = release.Value.GetSettingsJsonPath();
        if (!File.Exists(path))
        {
            return [
                new PromptMessage
                {
                    Role = Role.User,
                    Content = new TextContentBlock { Text = $"I want to understand my Windows Terminal settings, but no settings file was found at: {path}" }
                }
            ];
        }

        var settingsJson = File.ReadAllText(path);
        return [
            new PromptMessage
            {
                Role = Role.User,
                Content = new TextContentBlock
                {
                    Text = $"""
                        Explain the following Windows Terminal ({release}) settings.json in plain, natural language.
                        Describe what profiles are configured along with the newTabMenu and what the default profile is.
                        Describe any configured actions and key bindings.
                        Describe what themes are available and which one is currently active, as well as what is in it.
                        Describe any global settings that are configured too.
                        Identify anything notable or unusual about the configuration.

                        ```json
                        {settingsJson}
                        ```
                        """
                }
            }
        ];
    }
}
