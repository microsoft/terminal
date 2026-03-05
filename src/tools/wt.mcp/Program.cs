using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

public static class Program
{
    public static async Task Main(string[] args)
    {
        var builder = Host.CreateApplicationBuilder(args);

        // Configure all logs to go to stderr (stdout is used for the MCP protocol messages).
        builder.Logging.AddConsole(o => o.LogToStandardErrorThreshold = LogLevel.Trace);

        // Add the MCP services: the transport to use (stdio) and the tools to register.
        builder.Services
            .AddMcpServer()
            .WithStdioServerTransport()
            .WithTools<SettingsTools>()
            .WithTools<FragmentTools>()
            .WithTools<OhMyPoshTools>()
            .WithTools<ShellIntegrationTools>()
            .WithTools<SnippetTools>();

        await builder.Build().RunAsync();
    }
}
