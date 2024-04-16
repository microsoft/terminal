#pragma once
#include "../../renderer/base/Renderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../renderer/base/lib/QuickSelectAlphabet.h"
#include "search.h"

class Search;

namespace Microsoft::Console::Render
{
    class Renderer;
}

class QuickSelectHandler
{
private:
    std::shared_ptr<Microsoft::Terminal::Core::Terminal> _terminal;
    bool _copyMode = false;
    std::shared_ptr<Microsoft::Console::Render::QuickSelectAlphabet> _quickSelectAlphabet;

public:
    QuickSelectHandler(
        const std::shared_ptr<Microsoft::Terminal::Core::Terminal>& terminal,
        const std::shared_ptr<Microsoft::Console::Render::QuickSelectAlphabet>& quickSelectAlphabet);
    void EnterQuickSelectMode(std::wstring_view text,
                              bool copyMode,
                              Search& searcher,
                              Microsoft::Console::Render::Renderer* renderer);
    bool Enabled() const;
    void HandleChar(uint32_t vkey, Microsoft::Console::Render::Renderer* renderer) const;
};
