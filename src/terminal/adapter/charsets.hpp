/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- charsets.hpp

Abstract:
- Defines translation tables for the various VT character sets used in the TerminalOutput class.
--*/

#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    template<wchar_t BaseChar, size_t Size>
    class CharSet
    {
    public:
        constexpr CharSet(const std::initializer_list<std::pair<wchar_t, wchar_t>> replacements)
        {
            for (auto i = L'\0'; i < _translationTable.size(); i++)
                _translationTable.at(i) = BaseChar + i;
            for (auto replacement : replacements)
                _translationTable.at(replacement.first - BaseChar) = replacement.second;
        }
        constexpr operator const std::wstring_view() const
        {
            return { _translationTable.data(), _translationTable.size() };
        }
        constexpr bool operator==(const std::wstring_view rhs) const
        {
            return _translationTable.data() == rhs.data();
        }

    private:
        std::array<wchar_t, Size> _translationTable = {};
    };

    template<wchar_t BaseChar, size_t Size>
    constexpr bool operator==(const std::wstring_view lhs, const CharSet<BaseChar, Size>& rhs)
    {
        return rhs == lhs;
    }

    // Note that the 94-character sets are deliberately defined with a size of
    // 95 to avoid having to test the lower bound. We just always leave the first
    // entry - which is not meant to be mapped - as a SPACE or NBSP, which is at
    // least visually equivalent to leaving it untranslated.

    typedef CharSet<L'\x20', 95> AsciiBasedCharSet;
    typedef CharSet<L'\xa0', 95> Latin1BasedCharSet94;
    typedef CharSet<L'\xa0', 96> Latin1BasedCharSet96;

    static constexpr auto Ascii = AsciiBasedCharSet{};
    static constexpr auto Latin1 = Latin1BasedCharSet96{};

#pragma warning(push)
#pragma warning(disable : 26483) // Suppress spurious "value is outside the bounds" warnings

    // https://en.wikipedia.org/wiki/ISO/IEC_8859-2
    static constexpr auto Latin2 = Latin1BasedCharSet96{
        { L'\xa1', L'\u0104' }, // Latin Capital Letter A With Ogonek
        { L'\xa2', L'\u02d8' }, // Breve
        { L'\xa3', L'\u0141' }, // Latin Capital Letter L With Stroke
        { L'\xa5', L'\u013d' }, // Latin Capital Letter L With Caron
        { L'\xa6', L'\u015a' }, // Latin Capital Letter S With Acute
        { L'\xa9', L'\u0160' }, // Latin Capital Letter S With Caron
        { L'\xaa', L'\u015e' }, // Latin Capital Letter S With Cedilla
        { L'\xab', L'\u0164' }, // Latin Capital Letter T With Caron
        { L'\xac', L'\u0179' }, // Latin Capital Letter Z With Acute
        { L'\xae', L'\u017d' }, // Latin Capital Letter Z With Caron
        { L'\xaf', L'\u017b' }, // Latin Capital Letter Z With Dot Above
        { L'\xb1', L'\u0105' }, // Latin Small Letter A With Ogonek
        { L'\xb2', L'\u02db' }, // Ogonek
        { L'\xb3', L'\u0142' }, // Latin Small Letter L With Stroke
        { L'\xb5', L'\u013e' }, // Latin Small Letter L With Caron
        { L'\xb6', L'\u015b' }, // Latin Small Letter S With Acute
        { L'\xb7', L'\u02c7' }, // Caron
        { L'\xb9', L'\u0161' }, // Latin Small Letter S With Caron
        { L'\xba', L'\u015f' }, // Latin Small Letter S With Cedilla
        { L'\xbb', L'\u0165' }, // Latin Small Letter T With Caron
        { L'\xbc', L'\u017a' }, // Latin Small Letter Z With Acute
        { L'\xbd', L'\u02dd' }, // Double Acute Accent
        { L'\xbe', L'\u017e' }, // Latin Small Letter Z With Caron
        { L'\xbf', L'\u017c' }, // Latin Small Letter Z With Dot Above
        { L'\xc0', L'\u0154' }, // Latin Capital Letter R With Acute
        { L'\xc3', L'\u0102' }, // Latin Capital Letter A With Breve
        { L'\xc5', L'\u0139' }, // Latin Capital Letter L With Acute
        { L'\xc6', L'\u0106' }, // Latin Capital Letter C With Acute
        { L'\xc8', L'\u010c' }, // Latin Capital Letter C With Caron
        { L'\xca', L'\u0118' }, // Latin Capital Letter E With Ogonek
        { L'\xcc', L'\u011a' }, // Latin Capital Letter E With Caron
        { L'\xcf', L'\u010e' }, // Latin Capital Letter D With Caron
        { L'\xd0', L'\u0110' }, // Latin Capital Letter D With Stroke
        { L'\xd1', L'\u0143' }, // Latin Capital Letter N With Acute
        { L'\xd2', L'\u0147' }, // Latin Capital Letter N With Caron
        { L'\xd5', L'\u0150' }, // Latin Capital Letter O With Double Acute
        { L'\xd8', L'\u0158' }, // Latin Capital Letter R With Caron
        { L'\xd9', L'\u016e' }, // Latin Capital Letter U With Ring Above
        { L'\xdb', L'\u0170' }, // Latin Capital Letter U With Double Acute
        { L'\xde', L'\u0162' }, // Latin Capital Letter T With Cedilla
        { L'\xe0', L'\u0155' }, // Latin Small Letter R With Acute
        { L'\xe3', L'\u0103' }, // Latin Small Letter A With Breve
        { L'\xe5', L'\u013a' }, // Latin Small Letter L With Acute
        { L'\xe6', L'\u0107' }, // Latin Small Letter C With Acute
        { L'\xe8', L'\u010d' }, // Latin Small Letter C With Caron
        { L'\xea', L'\u0119' }, // Latin Small Letter E With Ogonek
        { L'\xec', L'\u011b' }, // Latin Small Letter E With Caron
        { L'\xef', L'\u010f' }, // Latin Small Letter D With Caron
        { L'\xf0', L'\u0111' }, // Latin Small Letter D With Stroke
        { L'\xf1', L'\u0144' }, // Latin Small Letter N With Acute
        { L'\xf2', L'\u0148' }, // Latin Small Letter N With Caron
        { L'\xf5', L'\u0151' }, // Latin Small Letter O With Double Acute
        { L'\xf8', L'\u0159' }, // Latin Small Letter R With Caron
        { L'\xf9', L'\u016f' }, // Latin Small Letter U With Ring Above
        { L'\xfb', L'\u0171' }, // Latin Small Letter U With Double Acute
        { L'\xfe', L'\u0163' }, // Latin Small Letter T With Cedilla
        { L'\xff', L'\u02d9' }, // Dot Above
    };

    // https://en.wikipedia.org/wiki/ISO/IEC_8859-5
    static constexpr auto LatinCyrillic = Latin1BasedCharSet96{
        { L'\xa1', L'\u0401' }, // Cyrillic Capital Letter Io
        { L'\xa2', L'\u0402' }, // Cyrillic Capital Letter Dje
        { L'\xa3', L'\u0403' }, // Cyrillic Capital Letter Gje
        { L'\xa4', L'\u0404' }, // Cyrillic Capital Letter Ukrainian Ie
        { L'\xa5', L'\u0405' }, // Cyrillic Capital Letter Dze
        { L'\xa6', L'\u0406' }, // Cyrillic Capital Letter Byelorussian-Ukrainian I
        { L'\xa7', L'\u0407' }, // Cyrillic Capital Letter Yi
        { L'\xa8', L'\u0408' }, // Cyrillic Capital Letter Je
        { L'\xa9', L'\u0409' }, // Cyrillic Capital Letter Lje
        { L'\xaa', L'\u040a' }, // Cyrillic Capital Letter Nje
        { L'\xab', L'\u040b' }, // Cyrillic Capital Letter Tshe
        { L'\xac', L'\u040c' }, // Cyrillic Capital Letter Kje
        { L'\xae', L'\u040e' }, // Cyrillic Capital Letter Short U
        { L'\xaf', L'\u040f' }, // Cyrillic Capital Letter Dzhe
        { L'\xb0', L'\u0410' }, // Cyrillic Capital Letter A
        { L'\xb1', L'\u0411' }, // Cyrillic Capital Letter Be
        { L'\xb2', L'\u0412' }, // Cyrillic Capital Letter Ve
        { L'\xb3', L'\u0413' }, // Cyrillic Capital Letter Ghe
        { L'\xb4', L'\u0414' }, // Cyrillic Capital Letter De
        { L'\xb5', L'\u0415' }, // Cyrillic Capital Letter Ie
        { L'\xb6', L'\u0416' }, // Cyrillic Capital Letter Zhe
        { L'\xb7', L'\u0417' }, // Cyrillic Capital Letter Ze
        { L'\xb8', L'\u0418' }, // Cyrillic Capital Letter I
        { L'\xb9', L'\u0419' }, // Cyrillic Capital Letter Short I
        { L'\xba', L'\u041a' }, // Cyrillic Capital Letter Ka
        { L'\xbb', L'\u041b' }, // Cyrillic Capital Letter El
        { L'\xbc', L'\u041c' }, // Cyrillic Capital Letter Em
        { L'\xbd', L'\u041d' }, // Cyrillic Capital Letter En
        { L'\xbe', L'\u041e' }, // Cyrillic Capital Letter O
        { L'\xbf', L'\u041f' }, // Cyrillic Capital Letter Pe
        { L'\xc0', L'\u0420' }, // Cyrillic Capital Letter Er
        { L'\xc1', L'\u0421' }, // Cyrillic Capital Letter Es
        { L'\xc2', L'\u0422' }, // Cyrillic Capital Letter Te
        { L'\xc3', L'\u0423' }, // Cyrillic Capital Letter U
        { L'\xc4', L'\u0424' }, // Cyrillic Capital Letter Ef
        { L'\xc5', L'\u0425' }, // Cyrillic Capital Letter Ha
        { L'\xc6', L'\u0426' }, // Cyrillic Capital Letter Tse
        { L'\xc7', L'\u0427' }, // Cyrillic Capital Letter Che
        { L'\xc8', L'\u0428' }, // Cyrillic Capital Letter Sha
        { L'\xc9', L'\u0429' }, // Cyrillic Capital Letter Shcha
        { L'\xca', L'\u042a' }, // Cyrillic Capital Letter Hard Sign
        { L'\xcb', L'\u042b' }, // Cyrillic Capital Letter Yeru
        { L'\xcc', L'\u042c' }, // Cyrillic Capital Letter Soft Sign
        { L'\xcd', L'\u042d' }, // Cyrillic Capital Letter E
        { L'\xce', L'\u042e' }, // Cyrillic Capital Letter Yu
        { L'\xcf', L'\u042f' }, // Cyrillic Capital Letter Ya
        { L'\xd0', L'\u0430' }, // Cyrillic Small Letter A
        { L'\xd1', L'\u0431' }, // Cyrillic Small Letter Be
        { L'\xd2', L'\u0432' }, // Cyrillic Small Letter Ve
        { L'\xd3', L'\u0433' }, // Cyrillic Small Letter Ghe
        { L'\xd4', L'\u0434' }, // Cyrillic Small Letter De
        { L'\xd5', L'\u0435' }, // Cyrillic Small Letter Ie
        { L'\xd6', L'\u0436' }, // Cyrillic Small Letter Zhe
        { L'\xd7', L'\u0437' }, // Cyrillic Small Letter Ze
        { L'\xd8', L'\u0438' }, // Cyrillic Small Letter I
        { L'\xd9', L'\u0439' }, // Cyrillic Small Letter Short I
        { L'\xda', L'\u043a' }, // Cyrillic Small Letter Ka
        { L'\xdb', L'\u043b' }, // Cyrillic Small Letter El
        { L'\xdc', L'\u043c' }, // Cyrillic Small Letter Em
        { L'\xdd', L'\u043d' }, // Cyrillic Small Letter En
        { L'\xde', L'\u043e' }, // Cyrillic Small Letter O
        { L'\xdf', L'\u043f' }, // Cyrillic Small Letter Pe
        { L'\xe0', L'\u0440' }, // Cyrillic Small Letter Er
        { L'\xe1', L'\u0441' }, // Cyrillic Small Letter Es
        { L'\xe2', L'\u0442' }, // Cyrillic Small Letter Te
        { L'\xe3', L'\u0443' }, // Cyrillic Small Letter U
        { L'\xe4', L'\u0444' }, // Cyrillic Small Letter Ef
        { L'\xe5', L'\u0445' }, // Cyrillic Small Letter Ha
        { L'\xe6', L'\u0446' }, // Cyrillic Small Letter Tse
        { L'\xe7', L'\u0447' }, // Cyrillic Small Letter Che
        { L'\xe8', L'\u0448' }, // Cyrillic Small Letter Sha
        { L'\xe9', L'\u0449' }, // Cyrillic Small Letter Shcha
        { L'\xea', L'\u044a' }, // Cyrillic Small Letter Hard Sign
        { L'\xeb', L'\u044b' }, // Cyrillic Small Letter Yeru
        { L'\xec', L'\u044c' }, // Cyrillic Small Letter Soft Sign
        { L'\xed', L'\u044d' }, // Cyrillic Small Letter E
        { L'\xee', L'\u044e' }, // Cyrillic Small Letter Yu
        { L'\xef', L'\u044f' }, // Cyrillic Small Letter Ya
        { L'\xf0', L'\u2116' }, // Numero Sign
        { L'\xf1', L'\u0451' }, // Cyrillic Small Letter Io
        { L'\xf2', L'\u0452' }, // Cyrillic Small Letter Dje
        { L'\xf3', L'\u0453' }, // Cyrillic Small Letter Gje
        { L'\xf4', L'\u0454' }, // Cyrillic Small Letter Ukrainian Ie
        { L'\xf5', L'\u0455' }, // Cyrillic Small Letter Dze
        { L'\xf6', L'\u0456' }, // Cyrillic Small Letter Byelorussian-Ukrainian I
        { L'\xf7', L'\u0457' }, // Cyrillic Small Letter Yi
        { L'\xf8', L'\u0458' }, // Cyrillic Small Letter Je
        { L'\xf9', L'\u0459' }, // Cyrillic Small Letter Lje
        { L'\xfa', L'\u045a' }, // Cyrillic Small Letter Nje
        { L'\xfb', L'\u045b' }, // Cyrillic Small Letter Tshe
        { L'\xfc', L'\u045c' }, // Cyrillic Small Letter Kje
        { L'\xfd', L'\u00a7' }, // Section Sign
        { L'\xfe', L'\u045e' }, // Cyrillic Small Letter Short U
        { L'\xff', L'\u045f' }, // Cyrillic Small Letter Dzhe
    };

    // https://en.wikipedia.org/wiki/ISO/IEC_8859-7
    // Note that this is the 1987 version of the standard, and not the 2003
    // update, which has three additional characters.
    static constexpr auto LatinGreek = Latin1BasedCharSet96{
        { L'\xa1', L'\u2018' }, // Left Single Quotation Mark
        { L'\xa2', L'\u2019' }, // Right Single Quotation Mark
        { L'\xa4', L'\u2426' }, // Undefined
        { L'\xa5', L'\u2426' }, // Undefined
        { L'\xaa', L'\u2426' }, // Undefined
        { L'\xae', L'\u2426' }, // Undefined
        { L'\xaf', L'\u2015' }, // Horizontal Bar
        { L'\xb4', L'\u0384' }, // Greek Tonos
        { L'\xb5', L'\u0385' }, // Greek Dialytika Tonos
        { L'\xb6', L'\u0386' }, // Greek Capital Letter Alpha With Tonos
        { L'\xb8', L'\u0388' }, // Greek Capital Letter Epsilon With Tonos
        { L'\xb9', L'\u0389' }, // Greek Capital Letter Eta With Tonos
        { L'\xba', L'\u038a' }, // Greek Capital Letter Iota With Tonos
        { L'\xbc', L'\u038c' }, // Greek Capital Letter Omicron With Tonos
        { L'\xbe', L'\u038e' }, // Greek Capital Letter Upsilon With Tonos
        { L'\xbf', L'\u038f' }, // Greek Capital Letter Omega With Tonos
        { L'\xc0', L'\u0390' }, // Greek Small Letter Iota With Dialytika And Tonos
        { L'\xc1', L'\u0391' }, // Greek Capital Letter Alpha
        { L'\xc2', L'\u0392' }, // Greek Capital Letter Beta
        { L'\xc3', L'\u0393' }, // Greek Capital Letter Gamma
        { L'\xc4', L'\u0394' }, // Greek Capital Letter Delta
        { L'\xc5', L'\u0395' }, // Greek Capital Letter Epsilon
        { L'\xc6', L'\u0396' }, // Greek Capital Letter Zeta
        { L'\xc7', L'\u0397' }, // Greek Capital Letter Eta
        { L'\xc8', L'\u0398' }, // Greek Capital Letter Theta
        { L'\xc9', L'\u0399' }, // Greek Capital Letter Iota
        { L'\xca', L'\u039a' }, // Greek Capital Letter Kappa
        { L'\xcb', L'\u039b' }, // Greek Capital Letter Lamda
        { L'\xcc', L'\u039c' }, // Greek Capital Letter Mu
        { L'\xcd', L'\u039d' }, // Greek Capital Letter Nu
        { L'\xce', L'\u039e' }, // Greek Capital Letter Xi
        { L'\xcf', L'\u039f' }, // Greek Capital Letter Omicron
        { L'\xd0', L'\u03a0' }, // Greek Capital Letter Pi
        { L'\xd1', L'\u03a1' }, // Greek Capital Letter Rho
        { L'\xd2', L'\u2426' }, // Undefined
        { L'\xd3', L'\u03a3' }, // Greek Capital Letter Sigma
        { L'\xd4', L'\u03a4' }, // Greek Capital Letter Tau
        { L'\xd5', L'\u03a5' }, // Greek Capital Letter Upsilon
        { L'\xd6', L'\u03a6' }, // Greek Capital Letter Phi
        { L'\xd7', L'\u03a7' }, // Greek Capital Letter Chi
        { L'\xd8', L'\u03a8' }, // Greek Capital Letter Psi
        { L'\xd9', L'\u03a9' }, // Greek Capital Letter Omega
        { L'\xda', L'\u03aa' }, // Greek Capital Letter Iota With Dialytika
        { L'\xdb', L'\u03ab' }, // Greek Capital Letter Upsilon With Dialytika
        { L'\xdc', L'\u03ac' }, // Greek Small Letter Alpha With Tonos
        { L'\xdd', L'\u03ad' }, // Greek Small Letter Epsilon With Tonos
        { L'\xde', L'\u03ae' }, // Greek Small Letter Eta With Tonos
        { L'\xdf', L'\u03af' }, // Greek Small Letter Iota With Tonos
        { L'\xe0', L'\u03b0' }, // Greek Small Letter Upsilon With Dialytika And Tonos
        { L'\xe1', L'\u03b1' }, // Greek Small Letter Alpha
        { L'\xe2', L'\u03b2' }, // Greek Small Letter Beta
        { L'\xe3', L'\u03b3' }, // Greek Small Letter Gamma
        { L'\xe4', L'\u03b4' }, // Greek Small Letter Delta
        { L'\xe5', L'\u03b5' }, // Greek Small Letter Epsilon
        { L'\xe6', L'\u03b6' }, // Greek Small Letter Zeta
        { L'\xe7', L'\u03b7' }, // Greek Small Letter Eta
        { L'\xe8', L'\u03b8' }, // Greek Small Letter Theta
        { L'\xe9', L'\u03b9' }, // Greek Small Letter Iota
        { L'\xea', L'\u03ba' }, // Greek Small Letter Kappa
        { L'\xeb', L'\u03bb' }, // Greek Small Letter Lamda
        { L'\xec', L'\u03bc' }, // Greek Small Letter Mu
        { L'\xed', L'\u03bd' }, // Greek Small Letter Nu
        { L'\xee', L'\u03be' }, // Greek Small Letter Xi
        { L'\xef', L'\u03bf' }, // Greek Small Letter Omicron
        { L'\xf0', L'\u03c0' }, // Greek Small Letter Pi
        { L'\xf1', L'\u03c1' }, // Greek Small Letter Rho
        { L'\xf2', L'\u03c2' }, // Greek Small Letter Final Sigma
        { L'\xf3', L'\u03c3' }, // Greek Small Letter Sigma
        { L'\xf4', L'\u03c4' }, // Greek Small Letter Tau
        { L'\xf5', L'\u03c5' }, // Greek Small Letter Upsilon
        { L'\xf6', L'\u03c6' }, // Greek Small Letter Phi
        { L'\xf7', L'\u03c7' }, // Greek Small Letter Chi
        { L'\xf8', L'\u03c8' }, // Greek Small Letter Psi
        { L'\xf9', L'\u03c9' }, // Greek Small Letter Omega
        { L'\xfa', L'\u03ca' }, // Greek Small Letter Iota With Dialytika
        { L'\xfb', L'\u03cb' }, // Greek Small Letter Upsilon With Dialytika
        { L'\xfc', L'\u03cc' }, // Greek Small Letter Omicron With Tonos
        { L'\xfd', L'\u03cd' }, // Greek Small Letter Upsilon With Tonos
        { L'\xfe', L'\u03ce' }, // Greek Small Letter Omega With Tonos
        { L'\xff', L'\u2426' }, // Undefined
    };

    // https://en.wikipedia.org/wiki/ISO/IEC_8859-8
    static constexpr auto LatinHebrew = Latin1BasedCharSet96{
        { L'\xa1', L'\u2426' }, // Undefined
        { L'\xaa', L'\u00d7' }, // Multiplication Sign
        { L'\xba', L'\u00f7' }, // Division Sign
        { L'\xbf', L'\u2426' }, // Undefined
        { L'\xc0', L'\u2426' }, // Undefined
        { L'\xc1', L'\u2426' }, // Undefined
        { L'\xc2', L'\u2426' }, // Undefined
        { L'\xc3', L'\u2426' }, // Undefined
        { L'\xc4', L'\u2426' }, // Undefined
        { L'\xc5', L'\u2426' }, // Undefined
        { L'\xc6', L'\u2426' }, // Undefined
        { L'\xc7', L'\u2426' }, // Undefined
        { L'\xc8', L'\u2426' }, // Undefined
        { L'\xc9', L'\u2426' }, // Undefined
        { L'\xca', L'\u2426' }, // Undefined
        { L'\xcb', L'\u2426' }, // Undefined
        { L'\xcc', L'\u2426' }, // Undefined
        { L'\xcd', L'\u2426' }, // Undefined
        { L'\xce', L'\u2426' }, // Undefined
        { L'\xcf', L'\u2426' }, // Undefined
        { L'\xd0', L'\u2426' }, // Undefined
        { L'\xd1', L'\u2426' }, // Undefined
        { L'\xd2', L'\u2426' }, // Undefined
        { L'\xd3', L'\u2426' }, // Undefined
        { L'\xd4', L'\u2426' }, // Undefined
        { L'\xd5', L'\u2426' }, // Undefined
        { L'\xd6', L'\u2426' }, // Undefined
        { L'\xd7', L'\u2426' }, // Undefined
        { L'\xd8', L'\u2426' }, // Undefined
        { L'\xd9', L'\u2426' }, // Undefined
        { L'\xda', L'\u2426' }, // Undefined
        { L'\xdb', L'\u2426' }, // Undefined
        { L'\xdc', L'\u2426' }, // Undefined
        { L'\xdd', L'\u2426' }, // Undefined
        { L'\xde', L'\u2426' }, // Undefined
        { L'\xdf', L'\u2017' }, // Double Low Line
        { L'\xe0', L'\u05d0' }, // Hebrew Letter Alef
        { L'\xe1', L'\u05d1' }, // Hebrew Letter Bet
        { L'\xe2', L'\u05d2' }, // Hebrew Letter Gimel
        { L'\xe3', L'\u05d3' }, // Hebrew Letter Dalet
        { L'\xe4', L'\u05d4' }, // Hebrew Letter He
        { L'\xe5', L'\u05d5' }, // Hebrew Letter Vav
        { L'\xe6', L'\u05d6' }, // Hebrew Letter Zayin
        { L'\xe7', L'\u05d7' }, // Hebrew Letter Het
        { L'\xe8', L'\u05d8' }, // Hebrew Letter Tet
        { L'\xe9', L'\u05d9' }, // Hebrew Letter Yod
        { L'\xea', L'\u05da' }, // Hebrew Letter Final Kaf
        { L'\xeb', L'\u05db' }, // Hebrew Letter Kaf
        { L'\xec', L'\u05dc' }, // Hebrew Letter Lamed
        { L'\xed', L'\u05dd' }, // Hebrew Letter Final Mem
        { L'\xee', L'\u05de' }, // Hebrew Letter Mem
        { L'\xef', L'\u05df' }, // Hebrew Letter Final Nun
        { L'\xf0', L'\u05e0' }, // Hebrew Letter Nun
        { L'\xf1', L'\u05e1' }, // Hebrew Letter Samekh
        { L'\xf2', L'\u05e2' }, // Hebrew Letter Ayin
        { L'\xf3', L'\u05e3' }, // Hebrew Letter Final Pe
        { L'\xf4', L'\u05e4' }, // Hebrew Letter Pe
        { L'\xf5', L'\u05e5' }, // Hebrew Letter Final Tsadi
        { L'\xf6', L'\u05e6' }, // Hebrew Letter Tsadi
        { L'\xf7', L'\u05e7' }, // Hebrew Letter Qof
        { L'\xf8', L'\u05e8' }, // Hebrew Letter Resh
        { L'\xf9', L'\u05e9' }, // Hebrew Letter Shin
        { L'\xfa', L'\u05ea' }, // Hebrew Letter Tav
        { L'\xfb', L'\u2426' }, // Undefined
        { L'\xfc', L'\u2426' }, // Undefined
        { L'\xfd', L'\u200e' }, // Left-To-Right Mark
        { L'\xfe', L'\u200f' }, // Right-To-Left Mark
        { L'\xff', L'\u2426' }, // Undefined
    };

    // https://en.wikipedia.org/wiki/ISO/IEC_8859-9
    static constexpr auto Latin5 = Latin1BasedCharSet96{
        { L'\xd0', L'\u011e' }, // Latin Capital Letter G With Breve
        { L'\xdd', L'\u0130' }, // Latin Capital Letter I With Dot Above
        { L'\xde', L'\u015e' }, // Latin Capital Letter S With Cedilla
        { L'\xf0', L'\u011f' }, // Latin Small Letter G With Breve
        { L'\xfd', L'\u0131' }, // Latin Small Letter Dotless I
        { L'\xfe', L'\u015f' }, // Latin Small Letter S With Cedilla
    };

    // https://www.vt100.net/docs/vt220-rm/table2-3b.html
    static constexpr auto DecSupplemental = Latin1BasedCharSet94{
        { L'\xa4', L'\u2426' }, // Undefined
        { L'\xa6', L'\u2426' }, // Undefined
        { L'\xa8', L'\u00a4' }, // Currency Sign
        { L'\xac', L'\u2426' }, // Undefined
        { L'\xad', L'\u2426' }, // Undefined
        { L'\xae', L'\u2426' }, // Undefined
        { L'\xaf', L'\u2426' }, // Undefined
        { L'\xb4', L'\u2426' }, // Undefined
        { L'\xb8', L'\u2426' }, // Undefined
        { L'\xbe', L'\u2426' }, // Undefined
        { L'\xd0', L'\u2426' }, // Undefined
        { L'\xd7', L'\u0152' }, // Latin Capital Ligature Oe
        { L'\xdd', L'\u0178' }, // Latin Capital Letter Y With Diaeresis
        { L'\xde', L'\u2426' }, // Undefined
        { L'\xf0', L'\u2426' }, // Undefined
        { L'\xf7', L'\u0153' }, // Latin Small Ligature Oe
        { L'\xfd', L'\u00ff' }, // Latin Small Letter Y With Diaeresis
        { L'\xfe', L'\u2426' }, // Undefined
    };

    // https://www.vt100.net/docs/vt220-rm/table2-4.html
    static constexpr auto DecSpecialGraphics = AsciiBasedCharSet{
        { L'\x5f', L'\u0020' }, // Blank
        { L'\x60', L'\u2666' }, // Diamond (more commonly U+25C6, but U+2666 renders better for us)
        { L'\x61', L'\u2592' }, // Checkerboard
        { L'\x62', L'\u2409' }, // HT, SYMBOL FOR HORIZONTAL TABULATION
        { L'\x63', L'\u240c' }, // FF, SYMBOL FOR FORM FEED
        { L'\x64', L'\u240d' }, // CR, SYMBOL FOR CARRIAGE RETURN
        { L'\x65', L'\u240a' }, // LF, SYMBOL FOR LINE FEED
        { L'\x66', L'\u00b0' }, // Degree symbol
        { L'\x67', L'\u00b1' }, // Plus/minus
        { L'\x68', L'\u2424' }, // NL, SYMBOL FOR NEWLINE
        { L'\x69', L'\u240b' }, // VT, SYMBOL FOR VERTICAL TABULATION
        { L'\x6a', L'\u2518' }, // Lower-right corner
        { L'\x6b', L'\u2510' }, // Upper-right corner
        { L'\x6c', L'\u250c' }, // Upper-left corner
        { L'\x6d', L'\u2514' }, // Lower-left corner
        { L'\x6e', L'\u253c' }, // Crossing lines
        { L'\x6f', L'\u23ba' }, // Horizontal line - Scan 1
        { L'\x70', L'\u23bb' }, // Horizontal line - Scan 3
        { L'\x71', L'\u2500' }, // Horizontal line - Scan 5
        { L'\x72', L'\u23bc' }, // Horizontal line - Scan 7
        { L'\x73', L'\u23bd' }, // Horizontal line - Scan 9
        { L'\x74', L'\u251c' }, // Left "T"
        { L'\x75', L'\u2524' }, // Right "T"
        { L'\x76', L'\u2534' }, // Bottom "T"
        { L'\x77', L'\u252c' }, // Top "T"
        { L'\x78', L'\u2502' }, // | Vertical bar
        { L'\x79', L'\u2264' }, // Less than or equal to
        { L'\x7a', L'\u2265' }, // Greater than or equal to
        { L'\x7b', L'\u03c0' }, // Pi
        { L'\x7c', L'\u2260' }, // Not equal to
        { L'\x7d', L'\u00a3' }, // UK pound sign
        { L'\x7e', L'\u00b7' }, // Centered dot
    };

    // https://en.wikipedia.org/wiki/KOI-8
    // This is referred to as KOI-8 Cyrillic in the VT520/VT525 Video Terminal
    // Programmer Information manual (EK-VT520-RM.A01)
    static constexpr auto DecCyrillic = Latin1BasedCharSet94{
        { L'\xa1', L'\u2426' }, // Undefined
        { L'\xa2', L'\u2426' }, // Undefined
        { L'\xa3', L'\u2426' }, // Undefined
        { L'\xa4', L'\u2426' }, // Undefined
        { L'\xa5', L'\u2426' }, // Undefined
        { L'\xa6', L'\u2426' }, // Undefined
        { L'\xa7', L'\u2426' }, // Undefined
        { L'\xa8', L'\u2426' }, // Undefined
        { L'\xa9', L'\u2426' }, // Undefined
        { L'\xaa', L'\u2426' }, // Undefined
        { L'\xab', L'\u2426' }, // Undefined
        { L'\xac', L'\u2426' }, // Undefined
        { L'\xad', L'\u2426' }, // Undefined
        { L'\xae', L'\u2426' }, // Undefined
        { L'\xaf', L'\u2426' }, // Undefined
        { L'\xb0', L'\u2426' }, // Undefined
        { L'\xb1', L'\u2426' }, // Undefined
        { L'\xb2', L'\u2426' }, // Undefined
        { L'\xb3', L'\u2426' }, // Undefined
        { L'\xb4', L'\u2426' }, // Undefined
        { L'\xb5', L'\u2426' }, // Undefined
        { L'\xb6', L'\u2426' }, // Undefined
        { L'\xb7', L'\u2426' }, // Undefined
        { L'\xb8', L'\u2426' }, // Undefined
        { L'\xb9', L'\u2426' }, // Undefined
        { L'\xba', L'\u2426' }, // Undefined
        { L'\xbb', L'\u2426' }, // Undefined
        { L'\xbc', L'\u2426' }, // Undefined
        { L'\xbd', L'\u2426' }, // Undefined
        { L'\xbe', L'\u2426' }, // Undefined
        { L'\xbf', L'\u2426' }, // Undefined
        { L'\xc0', L'\u044e' }, // Cyrillic Small Letter Yu
        { L'\xc1', L'\u0430' }, // Cyrillic Small Letter A
        { L'\xc2', L'\u0431' }, // Cyrillic Small Letter Be
        { L'\xc3', L'\u0446' }, // Cyrillic Small Letter Tse
        { L'\xc4', L'\u0434' }, // Cyrillic Small Letter De
        { L'\xc5', L'\u0435' }, // Cyrillic Small Letter Ie
        { L'\xc6', L'\u0444' }, // Cyrillic Small Letter Ef
        { L'\xc7', L'\u0433' }, // Cyrillic Small Letter Ghe
        { L'\xc8', L'\u0445' }, // Cyrillic Small Letter Ha
        { L'\xc9', L'\u0438' }, // Cyrillic Small Letter I
        { L'\xca', L'\u0439' }, // Cyrillic Small Letter Short I
        { L'\xcb', L'\u043a' }, // Cyrillic Small Letter Ka
        { L'\xcc', L'\u043b' }, // Cyrillic Small Letter El
        { L'\xcd', L'\u043c' }, // Cyrillic Small Letter Em
        { L'\xce', L'\u043d' }, // Cyrillic Small Letter En
        { L'\xcf', L'\u043e' }, // Cyrillic Small Letter O
        { L'\xd0', L'\u043f' }, // Cyrillic Small Letter Pe
        { L'\xd1', L'\u044f' }, // Cyrillic Small Letter Ya
        { L'\xd2', L'\u0440' }, // Cyrillic Small Letter Er
        { L'\xd3', L'\u0441' }, // Cyrillic Small Letter Es
        { L'\xd4', L'\u0442' }, // Cyrillic Small Letter Te
        { L'\xd5', L'\u0443' }, // Cyrillic Small Letter U
        { L'\xd6', L'\u0436' }, // Cyrillic Small Letter Zhe
        { L'\xd7', L'\u0432' }, // Cyrillic Small Letter Ve
        { L'\xd8', L'\u044c' }, // Cyrillic Small Letter Soft Sign
        { L'\xd9', L'\u044b' }, // Cyrillic Small Letter Yeru
        { L'\xda', L'\u0437' }, // Cyrillic Small Letter Ze
        { L'\xdb', L'\u0448' }, // Cyrillic Small Letter Sha
        { L'\xdc', L'\u044d' }, // Cyrillic Small Letter E
        { L'\xdd', L'\u0449' }, // Cyrillic Small Letter Shcha
        { L'\xde', L'\u0447' }, // Cyrillic Small Letter Che
        { L'\xdf', L'\u044a' }, // Cyrillic Small Letter Hard Sign
        { L'\xe0', L'\u042e' }, // Cyrillic Capital Letter Yu
        { L'\xe1', L'\u0410' }, // Cyrillic Capital Letter A
        { L'\xe2', L'\u0411' }, // Cyrillic Capital Letter Be
        { L'\xe3', L'\u0426' }, // Cyrillic Capital Letter Tse
        { L'\xe4', L'\u0414' }, // Cyrillic Capital Letter De
        { L'\xe5', L'\u0415' }, // Cyrillic Capital Letter Ie
        { L'\xe6', L'\u0424' }, // Cyrillic Capital Letter Ef
        { L'\xe7', L'\u0413' }, // Cyrillic Capital Letter Ghe
        { L'\xe8', L'\u0425' }, // Cyrillic Capital Letter Ha
        { L'\xe9', L'\u0418' }, // Cyrillic Capital Letter I
        { L'\xea', L'\u0419' }, // Cyrillic Capital Letter Short I
        { L'\xeb', L'\u041a' }, // Cyrillic Capital Letter Ka
        { L'\xec', L'\u041b' }, // Cyrillic Capital Letter El
        { L'\xed', L'\u041c' }, // Cyrillic Capital Letter Em
        { L'\xee', L'\u041d' }, // Cyrillic Capital Letter En
        { L'\xef', L'\u041e' }, // Cyrillic Capital Letter O
        { L'\xf0', L'\u041f' }, // Cyrillic Capital Letter Pe
        { L'\xf1', L'\u042f' }, // Cyrillic Capital Letter Ya
        { L'\xf2', L'\u0420' }, // Cyrillic Capital Letter Er
        { L'\xf3', L'\u0421' }, // Cyrillic Capital Letter Es
        { L'\xf4', L'\u0422' }, // Cyrillic Capital Letter Te
        { L'\xf5', L'\u0423' }, // Cyrillic Capital Letter U
        { L'\xf6', L'\u0416' }, // Cyrillic Capital Letter Zhe
        { L'\xf7', L'\u0412' }, // Cyrillic Capital Letter Ve
        { L'\xf8', L'\u042c' }, // Cyrillic Capital Letter Soft Sign
        { L'\xf9', L'\u042b' }, // Cyrillic Capital Letter Yeru
        { L'\xfa', L'\u0417' }, // Cyrillic Capital Letter Ze
        { L'\xfb', L'\u0428' }, // Cyrillic Capital Letter Sha
        { L'\xfc', L'\u042d' }, // Cyrillic Capital Letter E
        { L'\xfd', L'\u0429' }, // Cyrillic Capital Letter Shcha
        { L'\xfe', L'\u0427' }, // Cyrillic Capital Letter Che
    };

    // See Figure 5-1 in Installing and Using The VT420 Video Terminal
    // With PC Terminal Mode Update (EK-VT42A-UP.A01)
    static constexpr auto DecGreek = Latin1BasedCharSet94{
        { L'\xa4', L'\u2426' }, // Undefined
        { L'\xa6', L'\u2426' }, // Undefined
        { L'\xa8', L'\u00a4' }, // Currency Sign
        { L'\xac', L'\u2426' }, // Undefined
        { L'\xad', L'\u2426' }, // Undefined
        { L'\xae', L'\u2426' }, // Undefined
        { L'\xaf', L'\u2426' }, // Undefined
        { L'\xb4', L'\u2426' }, // Undefined
        { L'\xb8', L'\u2426' }, // Undefined
        { L'\xbe', L'\u2426' }, // Undefined
        { L'\xc0', L'\u03ca' }, // Greek Small Letter Iota With Dialytika
        { L'\xc1', L'\u0391' }, // Greek Capital Letter Alpha
        { L'\xc2', L'\u0392' }, // Greek Capital Letter Beta
        { L'\xc3', L'\u0393' }, // Greek Capital Letter Gamma
        { L'\xc4', L'\u0394' }, // Greek Capital Letter Delta
        { L'\xc5', L'\u0395' }, // Greek Capital Letter Epsilon
        { L'\xc6', L'\u0396' }, // Greek Capital Letter Zeta
        { L'\xc7', L'\u0397' }, // Greek Capital Letter Eta
        { L'\xc8', L'\u0398' }, // Greek Capital Letter Theta
        { L'\xc9', L'\u0399' }, // Greek Capital Letter Iota
        { L'\xca', L'\u039a' }, // Greek Capital Letter Kappa
        { L'\xcb', L'\u039b' }, // Greek Capital Letter Lamda
        { L'\xcc', L'\u039c' }, // Greek Capital Letter Mu
        { L'\xcd', L'\u039d' }, // Greek Capital Letter Nu
        { L'\xce', L'\u039e' }, // Greek Capital Letter Xi
        { L'\xcf', L'\u039f' }, // Greek Capital Letter Omicron
        { L'\xd0', L'\u2426' }, // Undefined
        { L'\xd1', L'\u03a0' }, // Greek Capital Letter Pi
        { L'\xd2', L'\u03a1' }, // Greek Capital Letter Rho
        { L'\xd3', L'\u03a3' }, // Greek Capital Letter Sigma
        { L'\xd4', L'\u03a4' }, // Greek Capital Letter Tau
        { L'\xd5', L'\u03a5' }, // Greek Capital Letter Upsilon
        { L'\xd6', L'\u03a6' }, // Greek Capital Letter Phi
        { L'\xd7', L'\u03a7' }, // Greek Capital Letter Chi
        { L'\xd8', L'\u03a8' }, // Greek Capital Letter Psi
        { L'\xd9', L'\u03a9' }, // Greek Capital Letter Omega
        { L'\xda', L'\u03ac' }, // Greek Small Letter Alpha With Tonos
        { L'\xdb', L'\u03ad' }, // Greek Small Letter Epsilon With Tonos
        { L'\xdc', L'\u03ae' }, // Greek Small Letter Eta With Tonos
        { L'\xdd', L'\u03af' }, // Greek Small Letter Iota With Tonos
        { L'\xde', L'\u2426' }, // Undefined
        { L'\xdf', L'\u03cc' }, // Greek Small Letter Omicron With Tonos
        { L'\xe0', L'\u03cb' }, // Greek Small Letter Upsilon With Dialytika
        { L'\xe1', L'\u03b1' }, // Greek Small Letter Alpha
        { L'\xe2', L'\u03b2' }, // Greek Small Letter Beta
        { L'\xe3', L'\u03b3' }, // Greek Small Letter Gamma
        { L'\xe4', L'\u03b4' }, // Greek Small Letter Delta
        { L'\xe5', L'\u03b5' }, // Greek Small Letter Epsilon
        { L'\xe6', L'\u03b6' }, // Greek Small Letter Zeta
        { L'\xe7', L'\u03b7' }, // Greek Small Letter Eta
        { L'\xe8', L'\u03b8' }, // Greek Small Letter Theta
        { L'\xe9', L'\u03b9' }, // Greek Small Letter Iota
        { L'\xea', L'\u03ba' }, // Greek Small Letter Kappa
        { L'\xeb', L'\u03bb' }, // Greek Small Letter Lamda
        { L'\xec', L'\u03bc' }, // Greek Small Letter Mu
        { L'\xed', L'\u03bd' }, // Greek Small Letter Nu
        { L'\xee', L'\u03be' }, // Greek Small Letter Xi
        { L'\xef', L'\u03bf' }, // Greek Small Letter Omicron
        { L'\xf0', L'\u2426' }, // Undefined
        { L'\xf1', L'\u03c0' }, // Greek Small Letter Pi
        { L'\xf2', L'\u03c1' }, // Greek Small Letter Rho
        { L'\xf3', L'\u03c3' }, // Greek Small Letter Sigma
        { L'\xf4', L'\u03c4' }, // Greek Small Letter Tau
        { L'\xf5', L'\u03c5' }, // Greek Small Letter Upsilon
        { L'\xf6', L'\u03c6' }, // Greek Small Letter Phi
        { L'\xf7', L'\u03c7' }, // Greek Small Letter Chi
        { L'\xf8', L'\u03c8' }, // Greek Small Letter Psi
        { L'\xf9', L'\u03c9' }, // Greek Small Letter Omega
        { L'\xfa', L'\u03c2' }, // Greek Small Letter Final Sigma
        { L'\xfb', L'\u03cd' }, // Greek Small Letter Upsilon With Tonos
        { L'\xfc', L'\u03ce' }, // Greek Small Letter Omega With Tonos
        { L'\xfd', L'\u0384' }, // Greek Tonos
        { L'\xfe', L'\u2426' }, // Undefined
    };

    // See Figure 5-6 in Installing and Using The VT420 Video Terminal
    // With PC Terminal Mode Update (EK-VT42A-UP.A01)
    static constexpr auto DecHebrew = Latin1BasedCharSet94{
        { L'\xa4', L'\u2426' }, // Undefined
        { L'\xa6', L'\u2426' }, // Undefined
        { L'\xa8', L'\u00a4' }, // Currency Sign
        { L'\xac', L'\u2426' }, // Undefined
        { L'\xad', L'\u2426' }, // Undefined
        { L'\xae', L'\u2426' }, // Undefined
        { L'\xaf', L'\u2426' }, // Undefined
        { L'\xb4', L'\u2426' }, // Undefined
        { L'\xb8', L'\u2426' }, // Undefined
        { L'\xbe', L'\u2426' }, // Undefined
        { L'\xc0', L'\u2426' }, // Undefined
        { L'\xc1', L'\u2426' }, // Undefined
        { L'\xc2', L'\u2426' }, // Undefined
        { L'\xc3', L'\u2426' }, // Undefined
        { L'\xc4', L'\u2426' }, // Undefined
        { L'\xc5', L'\u2426' }, // Undefined
        { L'\xc6', L'\u2426' }, // Undefined
        { L'\xc7', L'\u2426' }, // Undefined
        { L'\xc8', L'\u2426' }, // Undefined
        { L'\xc9', L'\u2426' }, // Undefined
        { L'\xca', L'\u2426' }, // Undefined
        { L'\xcb', L'\u2426' }, // Undefined
        { L'\xcc', L'\u2426' }, // Undefined
        { L'\xcd', L'\u2426' }, // Undefined
        { L'\xce', L'\u2426' }, // Undefined
        { L'\xcf', L'\u2426' }, // Undefined
        { L'\xd0', L'\u2426' }, // Undefined
        { L'\xd1', L'\u2426' }, // Undefined
        { L'\xd2', L'\u2426' }, // Undefined
        { L'\xd3', L'\u2426' }, // Undefined
        { L'\xd4', L'\u2426' }, // Undefined
        { L'\xd5', L'\u2426' }, // Undefined
        { L'\xd6', L'\u2426' }, // Undefined
        { L'\xd7', L'\u2426' }, // Undefined
        { L'\xd8', L'\u2426' }, // Undefined
        { L'\xd9', L'\u2426' }, // Undefined
        { L'\xda', L'\u2426' }, // Undefined
        { L'\xdb', L'\u2426' }, // Undefined
        { L'\xdc', L'\u2426' }, // Undefined
        { L'\xdd', L'\u2426' }, // Undefined
        { L'\xde', L'\u2426' }, // Undefined
        { L'\xdf', L'\u2426' }, // Undefined
        { L'\xe0', L'\u05d0' }, // Hebrew Letter Alef
        { L'\xe1', L'\u05d1' }, // Hebrew Letter Bet
        { L'\xe2', L'\u05d2' }, // Hebrew Letter Gimel
        { L'\xe3', L'\u05d3' }, // Hebrew Letter Dalet
        { L'\xe4', L'\u05d4' }, // Hebrew Letter He
        { L'\xe5', L'\u05d5' }, // Hebrew Letter Vav
        { L'\xe6', L'\u05d6' }, // Hebrew Letter Zayin
        { L'\xe7', L'\u05d7' }, // Hebrew Letter Het
        { L'\xe8', L'\u05d8' }, // Hebrew Letter Tet
        { L'\xe9', L'\u05d9' }, // Hebrew Letter Yod
        { L'\xea', L'\u05da' }, // Hebrew Letter Final Kaf
        { L'\xeb', L'\u05db' }, // Hebrew Letter Kaf
        { L'\xec', L'\u05dc' }, // Hebrew Letter Lamed
        { L'\xed', L'\u05dd' }, // Hebrew Letter Final Mem
        { L'\xee', L'\u05de' }, // Hebrew Letter Mem
        { L'\xef', L'\u05df' }, // Hebrew Letter Final Nun
        { L'\xf0', L'\u05e0' }, // Hebrew Letter Nun
        { L'\xf1', L'\u05e1' }, // Hebrew Letter Samekh
        { L'\xf2', L'\u05e2' }, // Hebrew Letter Ayin
        { L'\xf3', L'\u05e3' }, // Hebrew Letter Final Pe
        { L'\xf4', L'\u05e4' }, // Hebrew Letter Pe
        { L'\xf5', L'\u05e5' }, // Hebrew Letter Final Tsadi
        { L'\xf6', L'\u05e6' }, // Hebrew Letter Tsadi
        { L'\xf7', L'\u05e7' }, // Hebrew Letter Qof
        { L'\xf8', L'\u05e8' }, // Hebrew Letter Resh
        { L'\xf9', L'\u05e9' }, // Hebrew Letter Shin
        { L'\xfa', L'\u05ea' }, // Hebrew Letter Tav
        { L'\xfb', L'\u2426' }, // Undefined
        { L'\xfc', L'\u2426' }, // Undefined
        { L'\xfd', L'\u2426' }, // Undefined
        { L'\xfe', L'\u2426' }, // Undefined
    };

    // See Figure 5-11 in Installing and Using The VT420 Video Terminal
    // With PC Terminal Mode Update (EK-VT42A-UP.A01)
    static constexpr auto DecTurkish = Latin1BasedCharSet94{
        { L'\xa4', L'\u2426' }, // Undefined
        { L'\xa6', L'\u2426' }, // Undefined
        { L'\xa8', L'\u00a4' }, // Currency Sign
        { L'\xac', L'\u2426' }, // Undefined
        { L'\xad', L'\u2426' }, // Undefined
        { L'\xae', L'\u0130' }, // Latin Capital Letter I With Dot Above
        { L'\xaf', L'\u2426' }, // Undefined
        { L'\xb4', L'\u2426' }, // Undefined
        { L'\xb8', L'\u2426' }, // Undefined
        { L'\xbe', L'\u0131' }, // Latin Small Letter Dotless I
        { L'\xd0', L'\u011e' }, // Latin Capital Letter G With Breve
        { L'\xd7', L'\u0152' }, // Latin Capital Ligature Oe
        { L'\xdd', L'\u0178' }, // Latin Capital Letter Y With Diaeresis
        { L'\xde', L'\u015e' }, // Latin Capital Letter S With Cedilla
        { L'\xf0', L'\u011f' }, // Latin Small Letter G With Breve
        { L'\xf7', L'\u0153' }, // Latin Small Ligature Oe
        { L'\xfd', L'\u00ff' }, // Latin Small Letter Y With Diaeresis
        { L'\xfe', L'\u015f' }, // Latin Small Letter S With Cedilla
    };

    // https://www.vt100.net/docs/vt220-rm/table2-5.html
    static constexpr auto BritishNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
    };

    // https://www.vt100.net/docs/vt220-rm/table2-6.html
    static constexpr auto DutchNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00be' }, // Vulgar Fraction Three Quarters
        { L'\x5b', L'\u0133' }, // Latin Small Ligature Ij (sometimes approximated as y with diaeresis)
        { L'\x5c', L'\u00bd' }, // Vulgar Fraction One Half
        { L'\x5d', L'\u007c' }, // Vertical Line
        { L'\x7b', L'\u00a8' }, // Diaeresis
        { L'\x7c', L'\u0192' }, // Latin Small Letter F With Hook (sometimes approximated as f)
        { L'\x7d', L'\u00bc' }, // Vulgar Fraction One Quarter
        { L'\x7e', L'\u00b4' }, // Acute Accent
    };

    // https://www.vt100.net/docs/vt220-rm/table2-7.html
    static constexpr auto FinnishNrcs = AsciiBasedCharSet{
        { L'\x5b', L'\u00c4' }, // Latin Capital Letter A With Diaeresis
        { L'\x5c', L'\u00d6' }, // Latin Capital Letter O With Diaeresis
        { L'\x5d', L'\u00c5' }, // Latin Capital Letter A With Ring Above
        { L'\x5e', L'\u00dc' }, // Latin Capital Letter U With Diaeresis
        { L'\x60', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7b', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7c', L'\u00f6' }, // Latin Small Letter O With Diaeresis
        { L'\x7d', L'\u00e5' }, // Latin Small Letter A With Ring Above
        { L'\x7e', L'\u00fc' }, // Latin Small Letter U With Diaeresis
    };

    // https://www.vt100.net/docs/vt220-rm/table2-8.html
    static constexpr auto FrenchNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x5b', L'\u00b0' }, // Degree Sign
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00a7' }, // Section Sign
        { L'\x7b', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7c', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x7d', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x7e', L'\u00a8' }, // Diaeresis
    };

    // https://www.itscj.ipsj.or.jp/iso-ir/069.pdf
    // Some terminal emulators consider all the French character sets as equivalent,
    // but the 6/6 designator is actually an updated ISO standard, which adds the
    // Micro Sign character, which is not included in the DEC version.
    static constexpr auto FrenchNrcsIso = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x5b', L'\u00b0' }, // Degree Sign
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00a7' }, // Section Sign
        { L'\x60', L'\u00b5' }, // Micro Sign
        { L'\x7b', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7c', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x7d', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x7e', L'\u00a8' }, // Diaeresis
    };

    // https://www.vt100.net/docs/vt220-rm/table2-9.html
    static constexpr auto FrenchCanadianNrcs = AsciiBasedCharSet{
        { L'\x40', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x5b', L'\u00e2' }, // Latin Small Letter A With Circumflex
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00ea' }, // Latin Small Letter E With Circumflex
        { L'\x5e', L'\u00ee' }, // Latin Small Letter I With Circumflex
        { L'\x60', L'\u00f4' }, // Latin Small Letter O With Circumflex
        { L'\x7b', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7c', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x7d', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x7e', L'\u00fb' }, // Latin Small Letter U With Circumflex
    };

    // https://www.vt100.net/docs/vt220-rm/table2-10.html
    static constexpr auto GermanNrcs = AsciiBasedCharSet{
        { L'\x40', L'\u00a7' }, // Section Sign
        { L'\x5b', L'\u00c4' }, // Latin Capital Letter A With Diaeresis
        { L'\x5c', L'\u00d6' }, // Latin Capital Letter O With Diaeresis
        { L'\x5d', L'\u00dc' }, // Latin Capital Letter U With Diaeresis
        { L'\x7b', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7c', L'\u00f6' }, // Latin Small Letter O With Diaeresis
        { L'\x7d', L'\u00fc' }, // Latin Small Letter U With Diaeresis (VT320 manual incorrectly has this as U+00A8)
        { L'\x7e', L'\u00df' }, // Latin Small Letter Sharp S
    };

    // See Figure 5-4 in Installing and Using The VT420 Video Terminal
    // With PC Terminal Mode Update (EK-VT42A-UP.A01)
    static constexpr auto GreekNrcs = AsciiBasedCharSet{
        { L'\x40', L'\u03ca' }, // Greek Small Letter Iota With Dialytika
        { L'\x41', L'\u0391' }, // Greek Capital Letter Alpha
        { L'\x42', L'\u0392' }, // Greek Capital Letter Beta
        { L'\x43', L'\u0393' }, // Greek Capital Letter Gamma
        { L'\x44', L'\u0394' }, // Greek Capital Letter Delta
        { L'\x45', L'\u0395' }, // Greek Capital Letter Epsilon
        { L'\x46', L'\u0396' }, // Greek Capital Letter Zeta
        { L'\x47', L'\u0397' }, // Greek Capital Letter Eta
        { L'\x48', L'\u0398' }, // Greek Capital Letter Theta
        { L'\x49', L'\u0399' }, // Greek Capital Letter Iota
        { L'\x4a', L'\u039a' }, // Greek Capital Letter Kappa
        { L'\x4b', L'\u039b' }, // Greek Capital Letter Lamda
        { L'\x4c', L'\u039c' }, // Greek Capital Letter Mu
        { L'\x4d', L'\u039d' }, // Greek Capital Letter Nu
        { L'\x4e', L'\u039e' }, // Greek Capital Letter Xi
        { L'\x4f', L'\u039f' }, // Greek Capital Letter Omicron
        { L'\x50', L'\u2426' }, // Undefined
        { L'\x51', L'\u03a0' }, // Greek Capital Letter Pi
        { L'\x52', L'\u03a1' }, // Greek Capital Letter Rho
        { L'\x53', L'\u03a3' }, // Greek Capital Letter Sigma
        { L'\x54', L'\u03a4' }, // Greek Capital Letter Tau
        { L'\x55', L'\u03a5' }, // Greek Capital Letter Upsilon
        { L'\x56', L'\u03a6' }, // Greek Capital Letter Phi
        { L'\x57', L'\u03a7' }, // Greek Capital Letter Chi
        { L'\x58', L'\u03a8' }, // Greek Capital Letter Psi
        { L'\x59', L'\u03a9' }, // Greek Capital Letter Omega
        { L'\x5a', L'\u03ac' }, // Greek Small Letter Alpha With Tonos
        { L'\x5b', L'\u03ad' }, // Greek Small Letter Epsilon With Tonos
        { L'\x5c', L'\u03ae' }, // Greek Small Letter Eta With Tonos
        { L'\x5d', L'\u03af' }, // Greek Small Letter Iota With Tonos
        { L'\x5e', L'\u2426' }, // Undefined
        { L'\x5f', L'\u03cc' }, // Greek Small Letter Omicron With Tonos
        { L'\x60', L'\u03cb' }, // Greek Small Letter Upsilon With Dialytika
        { L'\x61', L'\u03b1' }, // Greek Small Letter Alpha
        { L'\x62', L'\u03b2' }, // Greek Small Letter Beta
        { L'\x63', L'\u03b3' }, // Greek Small Letter Gamma
        { L'\x64', L'\u03b4' }, // Greek Small Letter Delta
        { L'\x65', L'\u03b5' }, // Greek Small Letter Epsilon
        { L'\x66', L'\u03b6' }, // Greek Small Letter Zeta
        { L'\x67', L'\u03b7' }, // Greek Small Letter Eta
        { L'\x68', L'\u03b8' }, // Greek Small Letter Theta
        { L'\x69', L'\u03b9' }, // Greek Small Letter Iota
        { L'\x6a', L'\u03ba' }, // Greek Small Letter Kappa
        { L'\x6b', L'\u03bb' }, // Greek Small Letter Lamda
        { L'\x6c', L'\u03bc' }, // Greek Small Letter Mu
        { L'\x6d', L'\u03bd' }, // Greek Small Letter Nu
        { L'\x6e', L'\u03be' }, // Greek Small Letter Xi
        { L'\x6f', L'\u03bf' }, // Greek Small Letter Omicron
        { L'\x70', L'\u2426' }, // Undefined
        { L'\x71', L'\u03c0' }, // Greek Small Letter Pi
        { L'\x72', L'\u03c1' }, // Greek Small Letter Rho
        { L'\x73', L'\u03c3' }, // Greek Small Letter Sigma
        { L'\x74', L'\u03c4' }, // Greek Small Letter Tau
        { L'\x75', L'\u03c5' }, // Greek Small Letter Upsilon
        { L'\x76', L'\u03c6' }, // Greek Small Letter Phi
        { L'\x77', L'\u03c7' }, // Greek Small Letter Chi
        { L'\x78', L'\u03c8' }, // Greek Small Letter Psi
        { L'\x79', L'\u03c9' }, // Greek Small Letter Omega
        { L'\x7a', L'\u03c2' }, // Greek Small Letter Final Sigma
        { L'\x7b', L'\u03cd' }, // Greek Small Letter Upsilon With Tonos
        { L'\x7c', L'\u03ce' }, // Greek Small Letter Omega With Tonos
        { L'\x7d', L'\u0384' }, // Greek Tonos
        { L'\x7e', L'\u2426' }, // Undefined
    };

    // See Figure 5-9 in Installing and Using The VT420 Video Terminal
    // With PC Terminal Mode Update (EK-VT42A-UP.A01)
    static constexpr auto HebrewNrcs = AsciiBasedCharSet{
        { L'\x60', L'\u05d0' }, // Hebrew Letter Alef
        { L'\x61', L'\u05d1' }, // Hebrew Letter Bet
        { L'\x62', L'\u05d2' }, // Hebrew Letter Gimel
        { L'\x63', L'\u05d3' }, // Hebrew Letter Dalet
        { L'\x64', L'\u05d4' }, // Hebrew Letter He
        { L'\x65', L'\u05d5' }, // Hebrew Letter Vav
        { L'\x66', L'\u05d6' }, // Hebrew Letter Zayin
        { L'\x67', L'\u05d7' }, // Hebrew Letter Het
        { L'\x68', L'\u05d8' }, // Hebrew Letter Tet
        { L'\x69', L'\u05d9' }, // Hebrew Letter Yod
        { L'\x6a', L'\u05da' }, // Hebrew Letter Final Kaf
        { L'\x6b', L'\u05db' }, // Hebrew Letter Kaf
        { L'\x6c', L'\u05dc' }, // Hebrew Letter Lamed
        { L'\x6d', L'\u05dd' }, // Hebrew Letter Final Mem
        { L'\x6e', L'\u05de' }, // Hebrew Letter Mem
        { L'\x6f', L'\u05df' }, // Hebrew Letter Final Nun
        { L'\x70', L'\u05e0' }, // Hebrew Letter Nun
        { L'\x71', L'\u05e1' }, // Hebrew Letter Samekh
        { L'\x72', L'\u05e2' }, // Hebrew Letter Ayin
        { L'\x73', L'\u05e3' }, // Hebrew Letter Final Pe
        { L'\x74', L'\u05e4' }, // Hebrew Letter Pe
        { L'\x75', L'\u05e5' }, // Hebrew Letter Final Tsadi
        { L'\x76', L'\u05e6' }, // Hebrew Letter Tsadi
        { L'\x77', L'\u05e7' }, // Hebrew Letter Qof
        { L'\x78', L'\u05e8' }, // Hebrew Letter Resh
        { L'\x79', L'\u05e9' }, // Hebrew Letter Shin
        { L'\x7a', L'\u05ea' }, // Hebrew Letter Tav
    };

    // https://www.vt100.net/docs/vt220-rm/table2-11.html
    static constexpr auto ItalianNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00a7' }, // Section Sign
        { L'\x5b', L'\u00b0' }, // Degree Sign
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x60', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x7b', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x7c', L'\u00f2' }, // Latin Small Letter O With Grave
        { L'\x7d', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x7e', L'\u00ec' }, // Latin Small Letter I With Grave
    };

    // https://www.vt100.net/docs/vt220-rm/table2-12.html
    static constexpr auto NorwegianDanishNrcs = AsciiBasedCharSet{
        { L'\x40', L'\u00c4' }, // Latin Capital Letter A With Diaeresis
        { L'\x5b', L'\u00c6' }, // Latin Capital Letter Ae
        { L'\x5c', L'\u00d8' }, // Latin Capital Letter O With Stroke
        { L'\x5d', L'\u00c5' }, // Latin Capital Letter A With Ring Above
        { L'\x5e', L'\u00dc' }, // Latin Capital Letter U With Diaeresis
        { L'\x60', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7b', L'\u00e6' }, // Latin Small Letter Ae
        { L'\x7c', L'\u00f8' }, // Latin Small Letter O With Stroke
        { L'\x7d', L'\u00e5' }, // Latin Small Letter A With Ring Above
        { L'\x7e', L'\u00fc' }, // Latin Small Letter U With Diaeresis
    };

    // https://www.itscj.ipsj.or.jp/iso-ir/060.pdf
    // Some terminal emulators consider all the Nordic character sets as equivalent,
    // but the 6/0 designator is a separate ISO-registered standard, which only maps
    // a subset of the characters included in the DEC version.
    static constexpr auto NorwegianDanishNrcsIso = AsciiBasedCharSet{
        { L'\x5b', L'\u00c6' }, // Latin Capital Letter Ae
        { L'\x5c', L'\u00d8' }, // Latin Capital Letter O With Stroke
        { L'\x5d', L'\u00c5' }, // Latin Capital Letter A With Ring Above
        { L'\x7b', L'\u00e6' }, // Latin Small Letter Ae
        { L'\x7c', L'\u00f8' }, // Latin Small Letter O With Stroke
        { L'\x7d', L'\u00e5' }, // Latin Small Letter A With Ring Above
    };

    // https://www.vt100.net/docs/vt320-uu/appendixe.html#SE.2.3
    static constexpr auto PortugueseNrcs = AsciiBasedCharSet{
        { L'\x5b', L'\u00c3' }, // Latin Capital Letter A With Tilde
        { L'\x5c', L'\u00c7' }, // Latin Capital Letter C With Cedilla
        { L'\x5d', L'\u00d5' }, // Latin Capital Letter O With Tilde
        { L'\x7b', L'\u00e3' }, // Latin Small Letter A With Tilde
        { L'\x7c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x7d', L'\u00f5' }, // Latin Small Letter O With Tilde
    };

    // https://en.wikipedia.org/wiki/KOI-7#KOI-7_N2
    // This is referred to as Russian 7-bit (KOI-7) in the VT520/VT525 Video
    // Terminal Programmer Information manual (EK-VT520-RM.A01)
    static constexpr auto RussianNrcs = AsciiBasedCharSet{
        { L'\x60', L'\u042e' }, // Cyrillic Capital Letter Yu
        { L'\x61', L'\u0410' }, // Cyrillic Capital Letter A
        { L'\x62', L'\u0411' }, // Cyrillic Capital Letter Be
        { L'\x63', L'\u0426' }, // Cyrillic Capital Letter Tse
        { L'\x64', L'\u0414' }, // Cyrillic Capital Letter De
        { L'\x65', L'\u0415' }, // Cyrillic Capital Letter Ie
        { L'\x66', L'\u0424' }, // Cyrillic Capital Letter Ef
        { L'\x67', L'\u0413' }, // Cyrillic Capital Letter Ghe
        { L'\x68', L'\u0425' }, // Cyrillic Capital Letter Ha
        { L'\x69', L'\u0418' }, // Cyrillic Capital Letter I
        { L'\x6a', L'\u0419' }, // Cyrillic Capital Letter Short I
        { L'\x6b', L'\u041a' }, // Cyrillic Capital Letter Ka
        { L'\x6c', L'\u041b' }, // Cyrillic Capital Letter El
        { L'\x6d', L'\u041c' }, // Cyrillic Capital Letter Em
        { L'\x6e', L'\u041d' }, // Cyrillic Capital Letter En
        { L'\x6f', L'\u041e' }, // Cyrillic Capital Letter O
        { L'\x70', L'\u041f' }, // Cyrillic Capital Letter Pe
        { L'\x71', L'\u042f' }, // Cyrillic Capital Letter Ya
        { L'\x72', L'\u0420' }, // Cyrillic Capital Letter Er
        { L'\x73', L'\u0421' }, // Cyrillic Capital Letter Es
        { L'\x74', L'\u0422' }, // Cyrillic Capital Letter Te
        { L'\x75', L'\u0423' }, // Cyrillic Capital Letter U
        { L'\x76', L'\u0416' }, // Cyrillic Capital Letter Zhe
        { L'\x77', L'\u0412' }, // Cyrillic Capital Letter Ve
        { L'\x78', L'\u042c' }, // Cyrillic Capital Letter Soft Sign
        { L'\x79', L'\u042b' }, // Cyrillic Capital Letter Yeru
        { L'\x7a', L'\u0417' }, // Cyrillic Capital Letter Ze
        { L'\x7b', L'\u0428' }, // Cyrillic Capital Letter Sha
        { L'\x7c', L'\u042d' }, // Cyrillic Capital Letter E
        { L'\x7d', L'\u0429' }, // Cyrillic Capital Letter Shcha
        { L'\x7e', L'\u0427' }, // Cyrillic Capital Letter Che
    };

    // https://www.vt100.net/docs/vt220-rm/table2-13.html
    static constexpr auto SpanishNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00a3' }, // Pound Sign
        { L'\x40', L'\u00a7' }, // Section Sign
        { L'\x5b', L'\u00a1' }, // Inverted Exclamation Mark
        { L'\x5c', L'\u00d1' }, // Latin Capital Letter N With Tilde
        { L'\x5d', L'\u00bf' }, // Inverted Question Mark
        { L'\x7b', L'\u00b0' }, // Degree Sign (VT320 manual has these last 3 off by 1)
        { L'\x7c', L'\u00f1' }, // Latin Small Letter N With Tilde
        { L'\x7d', L'\u00e7' }, // Latin Small Letter C With Cedilla
    };

    // https://www.vt100.net/docs/vt220-rm/table2-14.html
    static constexpr auto SwedishNrcs = AsciiBasedCharSet{
        { L'\x40', L'\u00c9' }, // Latin Capital Letter E With Acute
        { L'\x5b', L'\u00c4' }, // Latin Capital Letter A With Diaeresis
        { L'\x5c', L'\u00d6' }, // Latin Capital Letter O With Diaeresis
        { L'\x5d', L'\u00c5' }, // Latin Capital Letter A With Ring Above
        { L'\x5e', L'\u00dc' }, // Latin Capital Letter U With Diaeresis
        { L'\x60', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x7b', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7c', L'\u00f6' }, // Latin Small Letter O With Diaeresis
        { L'\x7d', L'\u00e5' }, // Latin Small Letter A With Ring Above
        { L'\x7e', L'\u00fc' }, // Latin Small Letter U With Diaeresis
    };

    // https://www.vt100.net/docs/vt220-rm/table2-15.html
    static constexpr auto SwissNrcs = AsciiBasedCharSet{
        { L'\x23', L'\u00f9' }, // Latin Small Letter U With Grave
        { L'\x40', L'\u00e0' }, // Latin Small Letter A With Grave
        { L'\x5b', L'\u00e9' }, // Latin Small Letter E With Acute
        { L'\x5c', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x5d', L'\u00ea' }, // Latin Small Letter E With Circumflex
        { L'\x5e', L'\u00ee' }, // Latin Small Letter I With Circumflex
        { L'\x5f', L'\u00e8' }, // Latin Small Letter E With Grave
        { L'\x60', L'\u00f4' }, // Latin Small Letter O With Circumflex
        { L'\x7b', L'\u00e4' }, // Latin Small Letter A With Diaeresis
        { L'\x7c', L'\u00f6' }, // Latin Small Letter O With Diaeresis
        { L'\x7d', L'\u00fc' }, // Latin Small Letter U With Diaeresis
        { L'\x7e', L'\u00fb' }, // Latin Small Letter U With Circumflex
    };

    // See Figure 5-14 in Installing and Using The VT420 Video Terminal
    // With PC Terminal Mode Update (EK-VT42A-UP.A01)
    static constexpr auto TurkishNrcs = AsciiBasedCharSet{
        { L'\x21', L'\u0131' }, // Latin Small Letter Dotless I
        { L'\x26', L'\u011f' }, // Latin Small Letter G With Breve
        { L'\x40', L'\u0130' }, // Latin Capital Letter I With Dot Above
        { L'\x5b', L'\u015e' }, // Latin Capital Letter S With Cedilla
        { L'\x5c', L'\u00d6' }, // Latin Capital Letter O With Diaeresis
        { L'\x5d', L'\u00c7' }, // Latin Capital Letter C With Cedilla
        { L'\x5e', L'\u00dc' }, // Latin Capital Letter U With Diaeresis
        { L'\x60', L'\u011e' }, // Latin Capital Letter G With Breve
        { L'\x7b', L'\u015f' }, // Latin Small Letter S With Cedilla
        { L'\x7c', L'\u00f6' }, // Latin Small Letter O With Diaeresis
        { L'\x7d', L'\u00e7' }, // Latin Small Letter C With Cedilla
        { L'\x7e', L'\u00fc' }, // Latin Small Letter U With Diaeresis
    };

    // We're reserving 96 characters (U+EF20 to U+EF7F) from the Unicode
    // Private Use Area for our dynamically redefinable characters sets.
    static constexpr auto DRCS_BASE_CHAR = L'\uEF20';
    static constexpr auto Drcs94 = CharSet<DRCS_BASE_CHAR, 95>{ { DRCS_BASE_CHAR, '\x20' } };
    static constexpr auto Drcs96 = CharSet<DRCS_BASE_CHAR, 96>{};

#pragma warning(pop)
}
