#include "pch.h"
#include "SamplePeasant.h"

#include "Peasant.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::MonarchPeasantSample::implementation
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

    bool Peasant::ExecuteCommandline(winrt::array_view<const winrt::hstring> args, winrt::hstring currentDirectory)
    {
        auto argsProcessed = 0;
        std::wstring fullCmdline;
        for (const auto& arg : args)
        {
            fullCmdline += argsProcessed++ == 0 ? L"sample.exe" : arg;
            fullCmdline += L" ";
        }
        wprintf(L"\x1b[32mExecuted Commandline\x1b[m: \"");
        wprintf(fullCmdline.c_str());
        wprintf(L"\"\n");
        return true;
    }

    void Peasant::raiseActivatedEvent()
    {
        _WindowActivatedHandlers(*this, nullptr);
    }

}
