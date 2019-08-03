#include "pch.h"
#include "Terminal.Plugin.AzureCloudShell.h"

#include "AzureConnection.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace winrt::Microsoft::Terminal::TerminalConnection;

static constexpr GUID AzureConnectionType = { 0xd9fcfdfa, 0xa479, 0x412c, { 0x83, 0xb7, 0xc5, 0x64, 0xe, 0x61, 0xcd, 0x62 } };

struct AzureCloudShellTerminalFactory : implements<AzureCloudShellTerminalFactory, ITerminalConnectionFactory>
{
    hstring Name()
    {
        return L"Azure Cloud Shell";
    }

    hstring CmdLine()
    {
        return L"azure";
    }

    Uri IconUri()
    {
        return nullptr;
    }

    guid ConnectionType()
    {
        return guid(AzureConnectionType);
    }

    ITerminalConnection Create(TerminalConnectionStartupInfo startupInfo)
    {
        return make<AzureConnection>(startupInfo.InitialRows(), startupInfo.InitialColumns());
    }
};

struct SingleTerminalProvider : implements<SingleTerminalProvider, ITerminalConnectionProvider>
{
    SingleTerminalProvider(ITerminalConnectionFactory factory) :
        _factory(factory)
    {
    }

    ITerminalConnectionFactory GetFactory(guid id)
    {
        if (id == _factory.ConnectionType())
        {
            return _factory;
        }

        return nullptr;
    }

    com_array<ITerminalConnectionFactory> GetFactories()
    {
        return com_array<ITerminalConnectionFactory>(1, _factory);
    }

private:
    ITerminalConnectionFactory _factory;
};

ITerminalConnectionProvider GetConnectionProvider()
{
    return make<SingleTerminalProvider>(make<AzureCloudShellTerminalFactory>());
}
