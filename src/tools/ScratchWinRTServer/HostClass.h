#pragma once

#include "HostClass.g.h"
// #include "IMyComInterface.h"
#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"

namespace winrt::ScratchWinRTServer::implementation
{
    struct HostClass : HostClassT<HostClass /*, IMyComInterface*/>
    {
        HostClass(const winrt::guid& g);
        ~HostClass();
        void DoTheThing();

        int DoCount();
        winrt::guid Id();

        // HRESULT __stdcall Call() override;

        void Attach(Windows::UI::Xaml::Controls::SwapChainPanel panel);
        void BeginRendering();

        void RenderEngineSwapChainChanged();

        void ThisIsInsane(uint64_t swapchainHandle);

    private:
        int _DoCount{ 0 };
        winrt::guid _id;
        wil::unique_handle _hSwapchain{ INVALID_HANDLE_VALUE };
        Windows::UI::Xaml::Controls::SwapChainPanel _panel{ nullptr };

        bool _initializedTerminal{ false };
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _connection;
        std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal;
        FontInfoDesired _desiredFont;
        FontInfo _actualFont;

        std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer;
        std::unique_ptr<::Microsoft::Console::Render::DxEngine> _renderEngine;
        // winrt::Microsoft::Terminal::Settings::IControlSettings _settings{ nullptr };
        // void _AttachDxgiSwapChainToXaml(HANDLE swapChainHandle);

        bool _InitializeTerminal();
    };
}

namespace winrt::ScratchWinRTServer::factory_implementation
{
    struct HostClass : HostClassT<HostClass, implementation::HostClass>
    {
    };
}
