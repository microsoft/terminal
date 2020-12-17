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
    Peasant::Peasant() :
        _ourPID{ GetCurrentProcessId() }
    {
    }
    Peasant::Peasant(const uint64_t testPID) :
        _ourPID{ testPID }
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
        return _ourPID;
    }

    bool Peasant::ExecuteCommandline(const Remoting::CommandlineArgs& args)
    {
        if (_initialArgs == nullptr)
        {
            _initialArgs = args;
        }

        _ExecuteCommandlineRequestedHandlers(*this, args);

        return true;
    }

    Remoting::CommandlineArgs Peasant::InitialArgs()
    {
        return _initialArgs;
    }

}
