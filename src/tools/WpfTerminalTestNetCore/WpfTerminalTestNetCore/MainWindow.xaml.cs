using Microsoft.Terminal.Wpf;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace WpfTerminalTestNetCore
{
    public class PseudoConsoleConnection : Microsoft.Terminal.Wpf.ITerminalConnection
    {
        private String _command;
        private int _initialRows = 25, _initialCols = 80;
        PseudoConsole _pty = null;
        PseudoConsolePipe _inPipe = null;
        PseudoConsolePipe _outPipe = null;
        StreamWriter _inputWriter = null;
        Process _process;

        public PseudoConsoleConnection(String command, int columns, int rows)
        {
            _command = command;
            _initialCols = columns;
            _initialRows = rows;
        }

        public event EventHandler<TerminalOutputEventArgs> TerminalOutput;

        public void Close()
        {
            _pty.Dispose();
        }

        public void Resize(uint rows, uint columns)
        {
            _pty?.Resize((short)columns, (short)rows);
        }

        public void Start()
        {
            _inPipe = new PseudoConsolePipe();
            _outPipe = new PseudoConsolePipe();
            _inputWriter = new StreamWriter(new FileStream(_inPipe.WriteSide, FileAccess.Write));
            _pty = PseudoConsole.Create(_inPipe.ReadSide, _outPipe.WriteSide, (short)_initialCols, (short)_initialRows);
            _process = ProcessFactory.Start(_command, PseudoConsole.PseudoConsoleThreadAttribute, _pty.Handle);
            Task.Run(async () =>
            {
                var outReader = new FileStream(_outPipe.ReadSide, FileAccess.Read);
                var bytes = new byte[1024];
                var chars = new char[Encoding.UTF8.GetMaxCharCount(bytes.Length)];
                var decoder = Encoding.UTF8.GetDecoder();
                while (_pty != null)
                {
                    int read = await outReader.ReadAsync(bytes);
                    if (read == 0) break;
                    var decoded = decoder.GetChars(bytes, 0, read, chars, 0, false);
                    TerminalOutput.Invoke(this, new TerminalOutputEventArgs(new String(chars, 0, decoded)));
                }
            });
        }

        public void WriteInput(string data)
        {
            _inputWriter.Write(data);
            _inputWriter.Flush();
        }
    }
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            Terminal.Loaded += Terminal_Loaded;
        }

        private void Terminal_Loaded(object sender, RoutedEventArgs e)
        {
            var theme = new TerminalTheme
            {
                DefaultBackground = 0x0c0c0c,
                DefaultForeground = 0xcccccc,
                CursorStyle = CursorStyle.BlinkingBar,
                ColorTable = new uint[] { 0x0C0C0C, 0x1F0FC5, 0x0EA113, 0x009CC1, 0xDA3700, 0x981788, 0xDD963A, 0xCCCCCC, 0x767676, 0x5648E7, 0x0CC616, 0xA5F1F9, 0xFF783B, 0x9E00B4, 0xD6D661, 0xF2F2F2 },
            };
            Terminal.Connection = new PseudoConsoleConnection("c:\\windows\\system32\\windowspowershell\\v1.0\\powershell.exe", Terminal.Columns, Terminal.Rows);
            Terminal.SetTheme(theme, "Cascadia Code", 12);
            Terminal.Focus();
        }
    }
}
