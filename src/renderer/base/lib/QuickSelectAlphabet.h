#pragma once
#include <map>

namespace Microsoft::Console::Render
{
    struct QuickSelectChar
    {
        bool isMatch = false;
        wchar_t val;
    };

    struct QuickSelectSelection
    {
        bool isCurrentMatch;
        std::vector<QuickSelectChar> chars;
        Microsoft::Console::Types::Viewport selection;
    };

    struct QuickSelectState
    {
        std::map<til::CoordType, std::vector<QuickSelectSelection>> selectionMap;
    };
    class QuickSelectAlphabet
    {
        bool _enabled = false;
        std::vector<wchar_t> _quickSelectAlphabet;
        std::unordered_map<wchar_t, int16_t> _quickSelectAlphabetMap;
        std::wstring _chars;

    public:
        QuickSelectAlphabet();
        bool Enabled() const;
        void Enabled(bool val);
        void AppendChar(wchar_t* ch);
        void RemoveChar();
        void ClearChars();
        std::vector<Microsoft::Console::Render::QuickSelectSelection> GetQuickSelectChars(int32_t number) const noexcept;
        bool AllCharsSet(int32_t number) const;
        int32_t GetIndexForChars();
    };
}
