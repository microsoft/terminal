using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using ModelContextProtocol.Protocol;

public static class Program
{
    public static async Task Main(string[] args)
    {
        var builder = Host.CreateApplicationBuilder(args);

        // Configure all logs to go to stderr (stdout is used for the MCP protocol messages).
        builder.Logging.AddConsole(o => o.LogToStandardErrorThreshold = LogLevel.Trace);

        var watcherService = new SettingsFileWatcherService();
        builder.Services.AddSingleton(watcherService);

        // Add the MCP services: the transport to use (stdio) and the tools to register.
        builder.Services
            .AddMcpServer()
            .WithStdioServerTransport()
            .WithTools<SettingsTools>()
            .WithResources<SettingsResources>()
            .WithPrompts<SettingsPrompts>()
            .WithSubscribeToResourcesHandler(async (ctx, ct) =>
            {
                if (ctx.Params?.Uri is { } uri)
                {
                    watcherService.Subscribe(uri, ctx.Server);
                }
                return new EmptyResult();
            })
            .WithUnsubscribeFromResourcesHandler(async (ctx, ct) =>
            {
                if (ctx.Params?.Uri is { } uri)
                {
                    watcherService.Unsubscribe(uri);
                }
                return new EmptyResult();
            });

        await builder.Build().RunAsync();
    }
}
