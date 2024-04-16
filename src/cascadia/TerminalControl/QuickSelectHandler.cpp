#include "pch.h"
#include "QuickSelectHandler.h"

#include "ControlInteractivity.h"
#include "../../buffer/out/search.h"

QuickSelectHandler::QuickSelectHandler(
    const std::shared_ptr<Microsoft::Terminal::Core::Terminal>& terminal,
    const std::shared_ptr<Microsoft::Console::Render::QuickSelectAlphabet>& quickSelectAlphabet)
{
    _terminal = terminal;
    _quickSelectAlphabet = quickSelectAlphabet;
}

void QuickSelectHandler::EnterQuickSelectMode(
    std::wstring_view text,
    bool copyMode,
    Search& searcher,
    Microsoft::Console::Render::Renderer* renderer)
{
    _quickSelectAlphabet->Enabled(true);
    _copyMode = copyMode;
    searcher.QuickSelectRegex(*_terminal, text, true);
    searcher.HighlightResults();
    renderer->TriggerSelection();
}

bool QuickSelectHandler::Enabled() const
{
    return _quickSelectAlphabet->Enabled();
}

void QuickSelectHandler::HandleChar(const uint32_t vkey, Microsoft::Console::Render::Renderer* renderer) const
{
    if (vkey == VK_ESCAPE)
    {
        _quickSelectAlphabet->Enabled(false);
        _quickSelectAlphabet->ClearChars();
        _terminal->ClearSelection();
        renderer->TriggerSelection();
        return;
    }

    if (vkey == VK_BACK)
    {
        _quickSelectAlphabet->RemoveChar();
        renderer->TriggerSelection();
        return;
    }

    wchar_t vkeyText[2] = { 0 };
    BYTE keyboardState[256];
    if (!GetKeyboardState(keyboardState))
    {
        return;
    }

    keyboardState[VK_SHIFT] = 0x80;
    ToUnicode(vkey, MapVirtualKey(vkey, MAPVK_VK_TO_VSC), keyboardState, vkeyText, 2, 0);

    _quickSelectAlphabet->AppendChar(vkeyText);

    if (_quickSelectAlphabet->AllCharsSet(_terminal->NumberOfVisibleSearchSelections()))
    {
        const auto index = _quickSelectAlphabet->GetIndexForChars();
        const auto quickSelectResult = _terminal->GetViewportSelectionAtIndex(index);
        if (quickSelectResult.has_value())
        {
            const auto startPoint = std::get<0>(quickSelectResult.value());
            const auto endPoint = std::get<1>(quickSelectResult.value());

            if (!_copyMode)
            {
                _quickSelectAlphabet->Enabled(false);
                _quickSelectAlphabet->ClearChars();
                _terminal->ClearSelection();
                _terminal->SelectNewRegion(til::point{ startPoint.x, startPoint.y }, til::point{ startPoint.x, startPoint.y});
                if (_terminal->SelectionMode() != Microsoft::Terminal::Core::Terminal::SelectionInteractionMode::Mark)
                {
                    _terminal->ToggleMarkMode();
                }

                renderer->TriggerSelection();
            }
            else
            {
                const auto req = TextBuffer::CopyRequest::FromConfig(_terminal->GetTextBuffer(), startPoint, endPoint, true, false, false);
                const auto text = _terminal->GetTextBuffer().GetPlainText(req);
                _terminal->CopyToClipboard(text);

                std::thread hideTimerThread([this, renderer]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    {
                        auto lock = _terminal->LockForWriting();
                        _quickSelectAlphabet->Enabled(false);
                        _quickSelectAlphabet->ClearChars();
                        _terminal->ClearSelection();
                        //This isn't technically safe.  There is a slight chance that the renderer is deleted
                        //I think the fix is to make the renderer a shared pointer but I am not ready to mess with change core terminal stuff
                        renderer->TriggerSelection();
                        renderer->TriggerRedrawAll();
                        renderer->NotifyPaintFrame();
                    }
                });
                hideTimerThread.detach();
            }
        }
    }
    renderer->TriggerRedrawAll();
    renderer->NotifyPaintFrame();
}
