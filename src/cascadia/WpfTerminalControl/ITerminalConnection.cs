using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WpfTerminalControl
{
    public class TerminalOutputEventArgs: EventArgs
    {
        public TerminalOutputEventArgs(string data)
        {
            Data = data;
        }

        public string Data { get; }
    }

    public interface ITerminalConnection
    {
        event EventHandler<TerminalOutputEventArgs> TerminalOutput;
        event EventHandler TerminalDisconnected;

        void Start();
        void WriteInput(string data);
        void Resize(uint rows, uint columns);
        void Close();
    };
}
