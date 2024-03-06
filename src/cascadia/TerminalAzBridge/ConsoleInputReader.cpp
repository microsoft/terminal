// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ConsoleInputReader.h"
#include "unicode.hpp"

ConsoleInputReader::ConsoleInputReader(HANDLE handle) :
    _handle(handle)
{
    _buffer.resize(BufferSize);
    _convertedString.reserve(BufferSize);
}

void ConsoleInputReader::SetWindowSizeChangedCallback(std::function<void()> callback)
{
    _windowSizeChangedCallback = std::move(callback);
}

std::optional<std::wstring_view> ConsoleInputReader::Read()
{
    DWORD readCount{ 0 };

    _convertedString.clear();
    while (_convertedString.empty())
    {
        _buffer.resize(BufferSize);
        auto succeeded =
            ReadConsoleInputW(_handle, _buffer.data(), gsl::narrow_cast<DWORD>(_buffer.size()), &readCount);
        if (!succeeded)
        {
            return std::nullopt;
        }

        _buffer.resize(readCount);
        for (auto it = _buffer.begin(); it != _buffer.end(); ++it)
        {
            if (it->EventType == WINDOW_BUFFER_SIZE_EVENT && _windowSizeChangedCallback)
            {
                _windowSizeChangedCallback();
            }
            else if (it->EventType == KEY_EVENT)
            {
                const auto& keyEvent = it->Event.KeyEvent;
                if (keyEvent.bKeyDown || (!keyEvent.bKeyDown && keyEvent.wVirtualKeyCode == VK_MENU))
                {
                    // Got a high surrogate at the end of the buffer
                    if (IS_HIGH_SURROGATE(keyEvent.uChar.UnicodeChar))
                    {
                        _highSurrogate.emplace(keyEvent.uChar.UnicodeChar);
                        continue; // we've consumed it -- only dispatch it if we get a low
                    }

                    if (IS_LOW_SURROGATE(keyEvent.uChar.UnicodeChar))
                    {
                        // No matter what we do, we want to destructively consume the high surrogate
                        if (const auto oldHighSurrogate{ std::exchange(_highSurrogate, std::nullopt) })
                        {
                            _convertedString.push_back(*_highSurrogate);
                        }
                        else
                        {
                            // If we get a low without a high surrogate, we've done everything we can.
                            // This is an illegal state.
                            _convertedString.push_back(UNICODE_REPLACEMENT);
                            continue; // onto the next event
                        }
                    }

                    // (\0 with a scancode is probably a modifier key, not a VT input key)
                    if (keyEvent.uChar.UnicodeChar != L'\0' || keyEvent.wVirtualScanCode == 0)
                    {
                        if (_highSurrogate) // non-destructive: we don't want to set it to nullopt needlessly for every character
                        {
                            // If we get a high surrogate *here*, we didn't find a low surrogate.
                            // This state is also illegal.
                            _convertedString.push_back(UNICODE_REPLACEMENT);
                            _highSurrogate.reset();
                        }
                        _convertedString.push_back(keyEvent.uChar.UnicodeChar);
                    }
                }
            }
        }
    }
    return _convertedString;
}
