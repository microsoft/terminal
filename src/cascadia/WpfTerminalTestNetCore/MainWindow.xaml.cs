// <copyright file="MainWindow.xaml.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

using Microsoft.Terminal.Wpf;
using System;
using System.Windows;

namespace WpfTerminalTestNetCore
{
    public class EchoConnection : Microsoft.Terminal.Wpf.ITerminalConnection
    {
        const string vtClear = "\x1b[H\x1b[2J\x1b[3J";
        const string ST = "\x1b\\";
        const string CSI = "\x1b[";
        const string OSC = "\x1b]";
        const string funPreamble =
            CSI + "38;5;118mT" + CSI + "0m" + CSI + "38;5;154mh" + CSI + "0m" + CSI + "38;5;154ma" + CSI + "0m" + CSI + "38;5;154mn" + CSI + "0m" + CSI + "38;5;148mk" + CSI + "0m" + CSI + "38;5;184ms" + CSI + "0m" + CSI + "38;5;184m " + CSI + "0m" + CSI + "38;5;184mf" + CSI + "0m" + CSI + "38;5;178mo" + CSI + "0m" + CSI + "38;5;214mr" + CSI + "0m" + CSI + "38;5;214m " + CSI + "0m" + CSI + "38;5;214mv" + CSI + "0m" + CSI + "38;5;208mi" + CSI + "0m" + CSI + "38;5;208ms" + CSI + "0m" + CSI + "38;5;208mi" + CSI + "0m" + CSI + "38;5;203mt" + CSI + "0m" + CSI + "38;5;203mi" + CSI + "0m" + CSI + "38;5;203mn" + CSI + "0m" + CSI + "38;5;203mg" + CSI + "0m" + CSI + "38;5;198m " + CSI + "0m" + CSI + "38;5;198mm" + CSI + "0m" + CSI + "38;5;198my" + CSI + "0m" + CSI + "38;5;199m " + CSI + "0m" + CSI + "38;5;199mw" + CSI + "0m" + CSI + "38;5;199me" + CSI + "0m" + CSI + "38;5;163mb" + CSI + "0m" + CSI + "38;5;164ms" + CSI + "0m" + CSI + "38;5;164mi" + CSI + "0m" + CSI + "38;5;164mt" + CSI + "0m" + CSI + "38;5;128me" + CSI + "0m" + CSI + "38;5;129m!" + CSI + "0m\r\n" +
            "\r\n" +
            "This is my cool website about terminals\r\n" +
            "Here is a list of cool terminals\r\n" +
            "* Windows Terminal (https://github.com/microsoft/terminal)\r\n" +
            "\r\n" +
            CSI + "5m[UNDER CONSTRUCTION]" + CSI + "m\r\n" +
            "\r\n" +
            "You are visitor number " + CSI + "53;4;92m00002" + CSI + "m to my website.\r\n" +
            "\r\n" +
            CSI + "38;5;244m" + CSI + "53m This is part of the Cool Terminals webring! " + CSI + "55m\r\n" +
            "\r\n" +
            " Visit other sites in the web ring...        \r\n" +
            "\r\n" +
            "       " + CSI + "94m" + OSC + "8;id=prev;https://terminal.site/?previous=1" + ST + "Previous" + OSC + "8;;" + ST + "" + CSI + "m     " + CSI + "94m" + OSC + "8;id=rand;https://terminal.site/?random=1" + ST + "Random" + OSC + "8;;" + ST + "" + CSI + "m     " + CSI + "94m" + OSC + "8;id=next;https://terminal.site/?next=1" + ST + "Next" + OSC + "8;;" + ST + "" + CSI + "m \r\n" +
            "       " + CSI + "94m" + OSC + "8;id=prev;https://terminal.site/?previous=1" + ST + "  Site  " + OSC + "8;;" + ST + "" + CSI + "m                " + CSI + "94m" + OSC + "8;id=next;https://terminal.site/?next=1" + ST + "Site" + OSC + "8;;" + ST + "" + CSI + "m \r\n" +
            CSI + "4;38;5;244m                                             " + CSI + "m\r\n";

        public event EventHandler<TerminalOutputEventArgs> TerminalOutput;

        public void Resize(uint rows, uint columns)
        {
            return;
        }

        private void _preamble()
        {
            _print(vtClear);
            _print(funPreamble);
            _print("^A: prt esc ^B: sgrmouse ^C: win32im ^D: again!\r\n");
            return;
        }
        public void Start()
        {
            _preamble();
        }

        private bool _escapeMode;
        private bool _mouseMode;
        private bool _win32InputMode;

        public void WriteInput(string data)
        {
            if (data.Length == 0)
            {
                return;
            }

            if (data[0] == '\x01') // ^A
            {
                _escapeMode = !_escapeMode;
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"Printable ESC mode: {_escapeMode}\r\n"));
            }
            else if (data[0] == '\x02') // ^B
            {
                _mouseMode = !_mouseMode;
                var decSet = _mouseMode ? "h" : "l";
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"\x1b[?1003{decSet}\x1b[?1006{decSet}"));
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"SGR Mouse mode (1003, 1006): {_mouseMode}\r\n"));
            }
            else if ((data[0] == '\x03') ||
                     (data == "\x1b[67;46;3;1;8;1_")) // ^C
            {
                _win32InputMode = !_win32InputMode;
                var decSet = _win32InputMode ? "h" : "l";
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"\x1b[?9001{decSet}"));
                TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"Win32 input mode: {_win32InputMode}\r\n"));

                // If escape mode isn't currently enabled, turn it on now.
                if (_win32InputMode && !_escapeMode)
                {
                    _escapeMode = true;
                    TerminalOutput.Invoke(this, new TerminalOutputEventArgs($"Printable ESC mode: {_escapeMode}\r\n"));
                }
            }
            else if (data[0] == '\x04')
            {
                _preamble();
            }
            else
            {
                // Echo back to the terminal, but make backspace/newline work properly.
                var str = data.Replace("\r", "\r\n").Replace("\x7f", "\x08 \x08");
                _print(str);
            }
        }

        private void _print(string str)
        {
            if (_escapeMode)
            {
                str = str.Replace("\x1b", "\u241b");
            }
            TerminalOutput.Invoke(this, new TerminalOutputEventArgs(str));
        }

        public void Close()
        {
            return;
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
                DefaultSelectionBackground = 0xcccccc,
                CursorStyle = CursorStyle.BlinkingBar,
                // This is Campbell.
                ColorTable = new uint[] { 0x0C0C0C, 0x1F0FC5, 0x0EA113, 0x009CC1, 0xDA3700, 0x981788, 0xDD963A, 0xCCCCCC, 0x767676, 0x5648E7, 0x0CC616, 0xA5F1F9, 0xFF783B, 0x9E00B4, 0xD6D661, 0xF2F2F2 },
            };

            Terminal.Connection = new EchoConnection();
            Terminal.SetTheme(theme, "Cascadia Code", 12);
            Terminal.Focus();
        }
    }
}
