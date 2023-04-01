using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using WEX.Logging.Interop;
using WEX.TestExecution.Markup;

namespace WindowsTerminal.UIA.Tests.Common
{
    public static class PgoManager
    {
        private static string TrimExtension(string str)
        {
            if (str.EndsWith(".exe") || str.EndsWith(".dll"))
            {
                return str.Substring(0, str.Length - 4);
            }
            return str;
        }

        public static void PgoSweepIfInstrumented(TestContext context, string assemblyName)
        {
#if PGO_INSTRUMENT
            string pgcFileName = context.TestName;
            Log.Comment($"Running pgosweep on '{assemblyName}' for test: {pgcFileName}");
            try
            {
                var startInfo = new ProcessStartInfo() {
                    FileName = Path.GetFullPath(Path.Combine(context.TestDeploymentDir, "pgosweep.exe")),
                    Arguments = $"{assemblyName} {TrimExtension(assemblyName)}!{pgcFileName}.pgc",
                    UseShellExecute = false,
                    RedirectStandardOutput = true
                };
                using (var process = Process.Start(startInfo))
                {
                    var output = new StringBuilder();
                    while (!process.HasExited)
                    {
                        Log.Comment(process.StandardOutput.ReadToEnd());
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Comment("Failed trying to pgosweep. " + ex.ToString());
                throw;
            }
#endif
        }
    }
}
