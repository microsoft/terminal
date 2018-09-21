using Microsoft.Win32.SafeHandles;
using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using static MiniTerm.Native.ConsoleApi;

namespace MiniTerm
{
    /// <summary>
    /// The UI of the terminal. It's just a normal console window, but we're managing the input/output.
    /// In a "real" project this could be some other UI.
    /// </summary>
    internal sealed class Terminal
    {
        private const string ExitCommand = "exit\r";
        private const string CtrlC_Command = "\x3";

        public Terminal()
        {
            EnableVirtualTerminalSequenceProcessing();
        }

        /// <summary>
        /// Newer versions of the windows console support interpreting virtual terminal sequences, we just have to opt-in
        /// </summary>
        private static void EnableVirtualTerminalSequenceProcessing()
        {
            var hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (!GetConsoleMode(hStdOut, out uint outConsoleMode))
            {
                throw new InvalidOperationException("Could not get console mode");
            }

            outConsoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
            if (!SetConsoleMode(hStdOut, outConsoleMode))
            {
                throw new InvalidOperationException("Could not enable virtual terminal processing");
            }
        }

        /// <summary>
        /// Start the psuedoconsole and run the process as shown in 
        /// https://docs.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session#creating-the-pseudoconsole
        /// </summary>
        /// <param name="command">the command to run, e.g. cmd.exe</param>
        public void Run(string command)
        {
            using (var inputPipe = new PseudoConsolePipe())
            using (var outputPipe = new PseudoConsolePipe())
            using (var pseudoConsole = PseudoConsole.Create(inputPipe.ReadSide, outputPipe.WriteSide, (short)Console.WindowWidth, (short)Console.WindowHeight))
            using (var process = ProcessFactory.Start(command, PseudoConsole.PseudoConsoleThreadAttribute, pseudoConsole.Handle))
            {
                // copy all pseudoconsole output to stdout
                Task.Run(() => CopyPipeToOutput(outputPipe.ReadSide));
                // prompt for stdin input and send the result to the pseudoconsole
                Task.Run(() => CopyInputToPipe(inputPipe.WriteSide));
                // free resources in case the console is ungracefully closed (e.g. by the 'x' in the window titlebar)
                OnClose(() => DisposeResources(process, pseudoConsole, outputPipe, inputPipe));

                WaitForExit(process).WaitOne(Timeout.Infinite);
            }
        }

        /// <summary>
        /// Reads terminal input and copies it to the PseudoConsole
        /// </summary>
        /// <param name="inputWriteSide">the "write" side of the pseudo console input pipe</param>
        private static void CopyInputToPipe(SafeFileHandle inputWriteSide)
        {
            using (var writer = new StreamWriter(new FileStream(inputWriteSide, FileAccess.Write)))
            {
                ForwardCtrlC(writer);
                writer.AutoFlush = true;
                writer.WriteLine(@"cd \");

                while (true)
                {
                    // send input character-by-character to the pipe
                    char key = Console.ReadKey(intercept: true).KeyChar;
                    writer.Write(key);
                }
            }
        }

        /// <summary>
        /// Don't let ctrl-c kill the terminal, it should be sent to the process in the terminal.
        /// </summary>
        private static void ForwardCtrlC(StreamWriter writer)
        {
            Console.CancelKeyPress += (sender, e) =>
            {
                e.Cancel = true;
                writer.Write(CtrlC_Command);
            };
        }

        /// <summary>
        /// Reads PseudoConsole output and copies it to the terminal's standard out.
        /// </summary>
        /// <param name="outputReadSide">the "read" side of the pseudo console output pipe</param>
        private static void CopyPipeToOutput(SafeFileHandle outputReadSide)
        {
            using (var terminalOutput = Console.OpenStandardOutput())
            using (var pseudoConsoleOutput = new FileStream(outputReadSide, FileAccess.Read))
            {
                pseudoConsoleOutput.CopyTo(terminalOutput);
            }
        }

        /// <summary>
        /// Get an AutoResetEvent that signals when the process exits
        /// </summary>
        private static AutoResetEvent WaitForExit(Process process) =>
            new AutoResetEvent(false)
            {
                SafeWaitHandle = new SafeWaitHandle(process.ProcessInfo.hProcess, ownsHandle: false)
            };

        /// <summary>
        /// Set a callback for when the terminal is closed (e.g. via the "X" window decoration button).
        /// Intended for resource cleanup logic.
        /// </summary>
        private static void OnClose(Action handler)
        {
            SetConsoleCtrlHandler(eventType =>
            {
                if(eventType == CtrlTypes.CTRL_CLOSE_EVENT)
                {
                    handler();
                }
                return false;
            }, true);
        }

        private void DisposeResources(params IDisposable[] disposables)
        {
            foreach (var disposable in disposables)
            {
                disposable.Dispose();
            }
        }
    }
}
