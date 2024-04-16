#include "precomp.h"
#include "lib/QuickSelectAlphabet.h"

namespace Microsoft::Console::Render
{
    QuickSelectAlphabet::QuickSelectAlphabet()
    {
        _quickSelectAlphabet = { L'A', L'S', L'D', L'F', L'Q', L'W', L'E', L'R', L'Z', L'X', L'C', L'V', L'J', L'K', L'L', L'M', L'I', L'U', L'O', L'P', L'G', L'H', L'T', L'Y', L'B', L'N' };
        for (int16_t i = 0; i < _quickSelectAlphabet.size(); ++i)
        {
            _quickSelectAlphabetMap[_quickSelectAlphabet[i]] = i;
        }
    }

    bool QuickSelectAlphabet::Enabled() const
    {
        return _enabled;
    }

    void QuickSelectAlphabet::Enabled(bool val)
    {
        _enabled = val;
    }

    void QuickSelectAlphabet::AppendChar(wchar_t* ch)
    {
        _chars += ch;
    }

    void QuickSelectAlphabet::RemoveChar()
    {
        if (!_chars.empty())
        {
            _chars.pop_back();
        }
    }

    void QuickSelectAlphabet::ClearChars()
    {
        _chars.clear();
    }

    std::vector<QuickSelectSelection> QuickSelectAlphabet::GetQuickSelectChars(int32_t number) const noexcept
    try
    {
        auto result = std::vector<QuickSelectSelection>();
        result.reserve(number);

        int columns = 1;
        while (std::pow(_quickSelectAlphabet.size(), columns) < number)
        {
            columns++;
        }

        std::vector<int> indices(columns, 0);

        for (auto j = 0; j < number; j++)
        {
            bool allMatching = true;
            std::vector<Microsoft::Console::Render::QuickSelectChar> chs;
            for (int i = 0; i < indices.size(); i++)
            {
                auto idx = indices[i];
                auto ch = Microsoft::Console::Render::QuickSelectChar{};
                ch.val = _quickSelectAlphabet[idx];
                if (i < _chars.size())
                {
                    if (_quickSelectAlphabet[idx] != _chars[i])
                    {
                        allMatching = false;
                        //We are going to throw this away anyways
                        break;
                    }

                    ch.isMatch = true;
                }
                else
                {
                    ch.isMatch = false;
                }
                chs.emplace_back(ch);
            }

            auto isCurrentMatch = false;
            if ((_chars.size() == 0 || chs.size() >= _chars.size()) &&
                allMatching)
            {
                isCurrentMatch = true;
            }

            auto toAdd = Microsoft::Console::Render::QuickSelectSelection{};
            toAdd.isCurrentMatch = isCurrentMatch;

            for (auto ch : chs)
            {
                toAdd.chars.emplace_back(ch);
            }

            result.emplace_back(toAdd);

            for (int k = columns - 1; k >= 0; --k)
            {
                indices[k]++;
                if (indices[k] < _quickSelectAlphabet.size())
                {
                    break; // No carry over, break the loop
                }
                indices[k] = 0; // Carry over to the previous column
                if (j == 0)
                {
                    // If we exceed the alphabet * alphabet.  Give up and return empty result.
                    return {};
                }
            }
        }

        return result;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return {};
    }

    bool QuickSelectAlphabet::AllCharsSet(int32_t number) const
    {
        int columns = 1;
        while (std::pow(_quickSelectAlphabet.size(), columns) < number)
        {
            columns++;
        }

        auto result = _chars.size() == columns;
        return result;
    }

    int32_t QuickSelectAlphabet::GetIndexForChars()
    {
        int16_t selectionIndex = 0;
        int16_t power = static_cast<int16_t>(_chars.size() - 1);
        for (int16_t i = 0; i < _chars.size(); i++)
        {
            auto ch = _chars[i];
            auto index = _quickSelectAlphabetMap[ch];
            selectionIndex += index * static_cast<int16_t>(std::pow(_quickSelectAlphabet.size(), power--));
        }

        return selectionIndex;
    }
}
