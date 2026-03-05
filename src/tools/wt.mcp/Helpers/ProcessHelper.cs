using System.Diagnostics;
using System.Text;

/// <summary>
/// Runs subprocesses with a timeout and async I/O to avoid deadlocks.
/// All MCP server subprocess calls should use this helper.
/// </summary>
internal static class ProcessHelper
{
    /// <summary>
    /// Runs a process and returns its stdout, stderr, and exit code.
    /// Returns null if the process fails to start, times out, or throws.
    /// Reads stdout and stderr asynchronously to avoid buffer deadlocks.
    /// </summary>
    public static ProcessResult? Run(
        string fileName,
        string arguments,
        int timeoutMs = 5000,
        Encoding? outputEncoding = null)
    {
        try
        {
            var psi = new ProcessStartInfo(fileName, arguments)
            {
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };

            if (outputEncoding is not null)
            {
                psi.StandardOutputEncoding = outputEncoding;
            }

            using var proc = Process.Start(psi);
            if (proc is null)
            {
                return null;
            }

            // Read both streams asynchronously to avoid deadlock and allow
            // WaitForExit to enforce the timeout before streams block us
            var stdoutTask = proc.StandardOutput.ReadToEndAsync();
            var stderrTask = proc.StandardError.ReadToEndAsync();

            if (!proc.WaitForExit(timeoutMs))
            {
                // Process exceeded timeout — kill it
                try
                {
                    proc.Kill(entireProcessTree: true);
                }
                catch
                {
                    // Best effort
                }

                return null;
            }

            // Process exited in time — collect output
            var stdout = stdoutTask.GetAwaiter().GetResult();
            var stderr = stderrTask.GetAwaiter().GetResult();

            return new ProcessResult(stdout, stderr, proc.ExitCode);
        }
        catch
        {
            return null;
        }
    }

    internal record ProcessResult(string Stdout, string Stderr, int ExitCode);
}
