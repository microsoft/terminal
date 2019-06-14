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
    public partial class UserControl1 : UserControl
    {
        public UserControl1()
        {
            InitializeComponent();
            this.termControl.TerminalScrolled += TermControl_TerminalScrolled;
        }

        public ITerminalConnection Connection
        {
            set
            {
                this.termControl.Connection = value;
            }
        }

        private void TermControl_TerminalScrolled(object sender, (int viewTop, int viewHeight, int bufferSize) e)
        {
            Dispatcher.InvokeAsync(() =>
            {

                this.scrollbar.Minimum = 0;
                this.scrollbar.Maximum = e.bufferSize;
                this.scrollbar.Value = e.viewTop + e.viewHeight;
                this.scrollbar.ViewportSize = e.viewHeight;
            });
        }

        private void Scrollbar_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {

        }
    }
}
