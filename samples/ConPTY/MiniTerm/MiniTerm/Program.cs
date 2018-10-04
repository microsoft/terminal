using System;

namespace MiniTerm
{
    /// <summary>
    /// C# version of:
    /// https://blogs.msdn.microsoft.com/commandline/2018/08/02/windows-command-line-introducing-the-windows-pseudo-console-conpty/
    /// https://docs.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session
    ///
    /// System Requirements:
    /// As of September 2018, requires Windows 10 with the "Windows Insider Program" installed for Redstone 5.
    /// Also requires the Windows Insider Preview SDK: https://www.microsoft.com/en-us/software-download/windowsinsiderpreviewSDK
    /// </summary>
    /// <remarks>
    /// Basic design is:
    /// Terminal UI starts the PseudoConsole, and controls it using a pair of PseudoConsolePipes
    /// Terminal UI will run the Process (cmd.exe) and associate it with the PseudoConsole.
    /// </remarks>
    static class Program
    {
        static void Main(string[] args)
        {
            try
            {
                var terminal = new Terminal();
                terminal.Run("cmd.exe");
            }
            catch (InvalidOperationException e)
            {
                Console.Error.WriteLine(e.Message);
                throw;
            }
        }
    }
}
