# Xaml Application for Win32

This project implements an Xaml application that is suited for Win32 projects.
A pseudo implementation of this object is:
```
namespace Microsoft.UI.Xaml.Markup
{
    interface IXamlMetadataProviderContainer
    {
        Windows.Foundation.Collections.IVector<Windows.UI.Xaml.Markup.IXamlMetadataProvider> Providers { get; };
    };

    class XamlApplication : Windows.UI.Xaml.Application, IXamlMetadataProviderContainer, IDisposable
    {
        XamlApplication()
        {
           WindowsXamlManager.InitializeForThread();
        }
    }
}
```

