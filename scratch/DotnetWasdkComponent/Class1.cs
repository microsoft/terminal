using WinRT.Interop;
using WinRT;
using SampleExtensions;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace DotnetWasdkComponent
{
    public sealed class Class1 : IExtension
    {        
        public int MySharpProperty { get; set; }

        public Windows.UI.Xaml.FrameworkElement PaneContent => throw new NotImplementedException();

        public Class1()
        {
            MySharpProperty = 11;
        }

        public event TypedEventHandler<object, SendInputArgs> SendInputRequested;

        public int DoTheThing()
        {
            throw new NotImplementedException();
        }
    }
}
