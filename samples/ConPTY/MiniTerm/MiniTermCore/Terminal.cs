using Microsoft.Win32.SafeHandles;
using System.Text;
using Windows.Win32;

namespace MiniTermCore
{
    /// <summary>
    /// The UI of the terminal. It's just a normal console window, but we're managing the input/output.
    /// In a "real" project this could be some other UI.
    /// </summary>
    internal static class Terminal
    {
        private const string CtrlC_Command = "\x3";

        internal enum CtrlTypes : uint
        {
            CTRL_C_EVENT = 0,
            CTRL_BREAK_EVENT,
            CTRL_CLOSE_EVENT,
            CTRL_LOGOFF_EVENT = 5,
            CTRL_SHUTDOWN_EVENT
        }

        /// <summary>
        /// Start the pseudoconsole and run the process as shown in 
        /// https://docs.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session#creating-the-pseudoconsole
        /// </summary>
        /// <param name="command">the command to run, e.g. cmd.exe</param>
        public static void Run(string command)
        {
            using var inputPipe = new PseudoConsolePipe();
            using var outputPipe = new PseudoConsolePipe();
            using var pseudoConsole = PseudoConsole.Create(inputPipe.ReadSide, outputPipe.WriteSide, (short)Console.WindowWidth, (short)Console.WindowHeight);
            using var process = System.Diagnostics.Process.Start(command) ?? throw new Exception();

            // copy all pseudoconsole output to stdout
            Task.Run(() => CopyPipeToOutput(outputPipe.ReadSide));
            // prompt for stdin input and send the result to the pseudoconsole
            Task.Run(() => CopyInputToPipe(inputPipe.WriteSide));
            // free resources in case the console is ungracefully closed (e.g. by the 'x' in the window titlebar)
            OnClose(() => DisposeResources(process, pseudoConsole, outputPipe, inputPipe));

            process.WaitForExit();
        }

        /// <summary>
        /// Reads terminal input and copies it to the PseudoConsole
        /// </summary>
        /// <param name="inputWriteSide">the "write" side of the pseudo console input pipe</param>
        private static void CopyInputToPipe(SafeFileHandle inputWriteSide)
        {
            using var stream = new FileStream(inputWriteSide, FileAccess.Write);
            ForwardCtrlC(stream);

            while (true)
            {
                if (!Console.KeyAvailable) continue;

                // send input character-by-character to the pipe
                char key = Console.ReadKey(intercept: true).KeyChar;
                stream.WriteByte((byte)key);
                stream.Flush();
            }
        }

        /// <summary>
        /// Don't let ctrl-c kill the terminal, it should be sent to the process in the terminal.
        /// </summary>
        private static void ForwardCtrlC(FileStream stream)
        {
            Console.CancelKeyPress += (sender, e) =>
            {
                e.Cancel = true;
                stream.Write(Encoding.UTF8.GetBytes(CtrlC_Command));
                stream.Flush();
            };
        }

        /// <summary>
        /// Reads PseudoConsole output and copies it to the terminal's standard out.
        /// </summary>
        /// <param name="outputReadSide">the "read" side of the pseudo console output pipe</param>
        private static void CopyPipeToOutput(SafeFileHandle outputReadSide)
        {
            using var terminalOutput = Console.OpenStandardOutput();
            using var pseudoConsoleOutput = new FileStream(outputReadSide, FileAccess.Read);
            pseudoConsoleOutput.CopyTo(terminalOutput);
        }

        /// <summary>
        /// Set a callback for when the terminal is closed (e.g. via the "X" window decoration button).
        /// Intended for resource cleanup logic.
        /// </summary>
        private static void OnClose(Action handler)
        {
            PInvoke.SetConsoleCtrlHandler(eventType =>
            {
                if (eventType == (uint)CtrlTypes.CTRL_CLOSE_EVENT)
                {
                    handler();
                }
                return false;
            }, true);
        }

        private static void DisposeResources(params IDisposable[] disposables)
        {
            foreach (var disposable in disposables)
            {
                disposable.Dispose();
            }
        }
    }
}
