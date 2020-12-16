#include "pch.h"
#include "Peasant.h"
#include "CommandlineArgs.h"
#include "Peasant.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    Peasant::Peasant()
    {
    }

    void Peasant::AssignID(uint64_t id)
    {
        _id = id;
    }
    uint64_t Peasant::GetID()
    {
        return _id;
    }

    uint64_t Peasant::GetPID()
    {
        return GetCurrentProcessId();
    }

    bool Peasant::ExecuteCommandline(const Remoting::CommandlineArgs& args)
    {
        if (_initialArgs == nullptr)
        {
            _initialArgs = args;
        }

        _ExecuteCommandlineRequestedHandlers(*this, args);

        // auto argsProcessed = 0;
        // std::wstring fullCmdline;
        // for (const auto& arg : args)
        // {
        //     fullCmdline += argsProcessed++ == 0 ? L"sample.exe" : arg;
        //     fullCmdline += L" ";
        // }
        // wprintf(L"\x1b[32mExecuted Commandline\x1b[m: \"");
        // wprintf(fullCmdline.c_str());
        // wprintf(L"\"\n");
        return true;
    }

    void Peasant::raiseActivatedEvent()
    {
        _WindowActivatedHandlers(*this, nullptr);
    }

    Remoting::CommandlineArgs Peasant::InitialArgs()
    {
        return _initialArgs;
    }

}
