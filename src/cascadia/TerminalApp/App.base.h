#pragma once

namespace winrt::TerminalApp::implementation
{
    template<typename D, typename... I>
    struct App_baseWithProvider : public App_base<D, WUXMarkup::IXamlMetadataProvider>
    {
        using IXamlType = WUXMarkup::IXamlType;

        IXamlType GetXamlType(const WUX::Interop::TypeName& type)
        {
            return _appProvider.GetXamlType(type);
        }

        IXamlType GetXamlType(const ::winrt::hstring& fullName)
        {
            return _appProvider.GetXamlType(fullName);
        }

        ::winrt::com_array<WUXMarkup::XmlnsDefinition> GetXmlnsDefinitions()
        {
            return _appProvider.GetXmlnsDefinitions();
        }

    private:
        bool _contentLoaded{ false };
        MTApp::XamlMetaDataProvider _appProvider;
    };

    template<typename D, typename... I>
    using AppT2 = App_baseWithProvider<D, I...>;
}
