using System;
using System.Collections.Generic;
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

namespace WpfTerminalControl
{
    /// <summary>
    /// Interaction logic for UserControl1.xaml
    /// </summary>
    public partial class TerminalControl : UserControl
    {
        private int accumulatedDelta = 0;

        public TerminalControl()
        {
            InitializeComponent();
            this.termContainer.TerminalScrolled += TermControl_TerminalScrolled;
            this.termContainer.UserScrolled += TermControl_UserScrolled;
            this.scrollbar.MouseWheel += Scrollbar_MouseWheel;
        }

        private void Scrollbar_MouseWheel(object sender, MouseWheelEventArgs e)
        {
            this.TermControl_UserScrolled(sender, e.Delta);
        }

        private void TermControl_UserScrolled(object sender, int delta)
        {
            var lineDelta = 120 / SystemParameters.WheelScrollLines;
            this.accumulatedDelta += delta;

            if (accumulatedDelta < lineDelta && accumulatedDelta > -lineDelta)
            {
                return;
            }

            Dispatcher.InvokeAsync(() =>
            {
                var lines = -this.accumulatedDelta / lineDelta;
                this.scrollbar.Value += lines;
                this.accumulatedDelta = 0;
            });
        }

        public ITerminalConnection Connection
        {
            set
            {
                this.termContainer.Connection = value;
            }
        }

        private void TermControl_TerminalScrolled(object sender, (int viewTop, int viewHeight, int bufferSize) e)
        {
            Dispatcher.InvokeAsync(() =>
            {

                this.scrollbar.Minimum = 0;
                this.scrollbar.Maximum = e.bufferSize - e.viewHeight;
                this.scrollbar.Value = e.viewTop;
                this.scrollbar.ViewportSize = e.viewHeight;
            });
        }

        private void Scrollbar_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            var viewTop = (int)this.scrollbar.Value;
            this.termContainer.UserScroll(viewTop);
        }
    }
}
