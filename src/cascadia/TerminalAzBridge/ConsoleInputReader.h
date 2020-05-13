/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    ConsoleInputReader.h

Abstract:

    This file contains a class whose sole purpose is to
    abstract away a bunch of details you usually need to
    know to read VT from a console input handle.

--*/

class ConsoleInputReader
{
public:
    explicit ConsoleInputReader(HANDLE handle);
    void SetWindowSizeChangedCallback(std::function<void()> callback);
    std::optional<std::wstring_view> Read();

private:
    static constexpr size_t BufferSize{ 128 };

    HANDLE _handle;
    std::wstring _convertedString;
    std::vector<INPUT_RECORD> _buffer;
    std::optional<wchar_t> _highSurrogate;
    std::function<void()> _windowSizeChangedCallback;
};
