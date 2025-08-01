// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Appearances.h"

#include <LibraryResources.h>
#include "../WinRTUtils/inc/Utils.h"

#include "EnumEntry.h"
#include "ProfileViewModel.h"

#include "Appearances.g.cpp"

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

// These features are enabled by default by DWrite, so if a user adds them,
// we initialize the setting to a value of 1 instead of 0.
static constexpr std::array s_defaultFeatures{
    DWRITE_MAKE_FONT_FEATURE_TAG('c', 'a', 'l', 't'),
    DWRITE_MAKE_FONT_FEATURE_TAG('c', 'c', 'm', 'p'),
    DWRITE_MAKE_FONT_FEATURE_TAG('c', 'l', 'i', 'g'),
    DWRITE_MAKE_FONT_FEATURE_TAG('d', 'i', 's', 't'),
    DWRITE_MAKE_FONT_FEATURE_TAG('k', 'e', 'r', 'n'),
    DWRITE_MAKE_FONT_FEATURE_TAG('l', 'i', 'g', 'a'),
    DWRITE_MAKE_FONT_FEATURE_TAG('l', 'o', 'c', 'l'),
    DWRITE_MAKE_FONT_FEATURE_TAG('m', 'a', 'r', 'k'),
    DWRITE_MAKE_FONT_FEATURE_TAG('m', 'k', 'm', 'k'),
    DWRITE_MAKE_FONT_FEATURE_TAG('r', 'l', 'i', 'g'),
    DWRITE_MAKE_FONT_FEATURE_TAG('r', 'n', 'r', 'n'),
};

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct TagToStringImpl
    {
        explicit TagToStringImpl(uint32_t tag) noexcept
        {
            _buffer[0] = static_cast<wchar_t>((tag >> 0) & 0xFF);
            _buffer[1] = static_cast<wchar_t>((tag >> 8) & 0xFF);
            _buffer[2] = static_cast<wchar_t>((tag >> 16) & 0xFF);
            _buffer[3] = static_cast<wchar_t>((tag >> 24) & 0xFF);
            _buffer[4] = 0;
        }

        operator std::wstring_view() const noexcept
        {
            return { &_buffer[0], 4 };
        }

    private:
        wchar_t _buffer[5];
    };

    // Turns a DWRITE_MAKE_OPENTYPE_TAG into a string_view...
    // (...buffer holder because someone needs to hold onto the data the view refers to.)
    static TagToStringImpl tagToString(uint32_t tag) noexcept
    {
        return TagToStringImpl{ tag };
    }

    // Turns a string to a DWRITE_MAKE_OPENTYPE_TAG. Returns 0 on failure.
    static uint32_t tagFromString(std::wstring_view str) noexcept
    {
        if (str.size() != 4)
        {
            return 0;
        }

        // Check if all 4 characters are printable ASCII.
        for (int i = 0; i < 4; ++i)
        {
            const auto ch = str[i];
            if (ch < 0x20 || ch > 0x7E)
            {
                return 0;
            }
        }

        return DWRITE_MAKE_OPENTYPE_TAG(str[0], str[1], str[2], str[3]);
    }

    static winrt::hstring getLocalizedStringByIndex(IDWriteLocalizedStrings* strings, UINT32 index)
    {
        UINT32 length = 0;
        THROW_IF_FAILED(strings->GetStringLength(index, &length));

        winrt::impl::hstring_builder builder{ length };
        THROW_IF_FAILED(strings->GetString(index, builder.data(), length + 1));

        return builder.to_hstring();
    }

    static UINT32 getLocalizedStringIndex(IDWriteLocalizedStrings* strings, const wchar_t* locale, UINT32 fallback)
    {
        UINT32 index;
        BOOL exists;
        if (FAILED(strings->FindLocaleName(locale, &index, &exists)) || !exists)
        {
            index = fallback;
        }
        return index;
    }

    Font::Font(winrt::hstring name, winrt::hstring localizedName) :
        _Name{ std::move(name) },
        _LocalizedName{ std::move(localizedName) }
    {
    }

    bool FontKeyValuePair::SortAscending(const Editor::FontKeyValuePair& lhs, const Editor::FontKeyValuePair& rhs)
    {
        const auto& a = winrt::get_self<FontKeyValuePair>(lhs)->KeyDisplayStringRef();
        const auto& b = winrt::get_self<FontKeyValuePair>(rhs)->KeyDisplayStringRef();
        return til::compare_linguistic_insensitive(a, b) < 0;
    }

    FontKeyValuePair::FontKeyValuePair(winrt::weak_ref<AppearanceViewModel> vm, winrt::hstring keyDisplayString, uint32_t key, float value, bool isFontFeature) :
        _vm{ std::move(vm) },
        _keyDisplayString{ std::move(keyDisplayString) },
        _key{ key },
        _value{ value },
        _isFontFeature{ isFontFeature }
    {
    }

    uint32_t FontKeyValuePair::Key() const noexcept
    {
        return _key;
    }

    winrt::hstring FontKeyValuePair::KeyDisplayString()
    {
        return KeyDisplayStringRef();
    }

    // You can't return a const-ref from a WinRT function, because the cppwinrt generated wrapper chokes on it.
    // So, now we got two KeyDisplayString() functions, because I refuse to AddRef/Release this for no reason.
    // I mean, really it makes no perf. difference, but I'm not kneeling down for an incompetent code generator.
    const winrt::hstring& FontKeyValuePair::KeyDisplayStringRef()
    {
        if (!_keyDisplayString.empty())
        {
            return _keyDisplayString;
        }

        const auto tagString = tagToString(_key);
        hstring displayString;

        if (_isFontFeature)
        {
            const auto key = fmt::format(FMT_COMPILE(L"Profile_FontFeature_{}"), std::wstring_view{ tagString });
            if (HasLibraryResourceWithName(key))
            {
                displayString = GetLibraryResourceString(key);
                displayString = hstring{ fmt::format(FMT_COMPILE(L"{} ({})"), displayString, std::wstring_view{ tagString }) };
            }
        }

        if (displayString.empty())
        {
            displayString = hstring{ tagString };
        }

        _keyDisplayString = displayString;
        return _keyDisplayString;
    }

    float FontKeyValuePair::Value() const noexcept
    {
        return _value;
    }

    void FontKeyValuePair::Value(float v)
    {
        if (_value == v)
        {
            return;
        }

        _value = v;

        if (const auto vm = _vm.get())
        {
            vm->UpdateFontSetting(this);
        }
    }

    void FontKeyValuePair::SetValueDirect(float v)
    {
        _value = v;
    }

    bool FontKeyValuePair::IsFontFeature() const noexcept
    {
        return _isFontFeature;
    }

    AppearanceViewModel::AppearanceViewModel(const Model::AppearanceConfig& appearance) :
        _appearance{ appearance }
    {
        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        //  unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"BackgroundImagePath")
            {
                // notify listener that all background image related values might have changed
                //
                // We need to do this so if someone manually types "desktopWallpaper"
                // into the path TextBox, we properly update the checkbox and stored
                // _lastBgImagePath. Without this, then we'll permanently hide the text
                // box, prevent it from ever being changed again.
                _NotifyChanges(L"UseDesktopBGImage", L"BackgroundImageSettingsVisible", L"CurrentBackgroundImagePath");
            }
            else if (viewModelProperty == L"BackgroundImageAlignment")
            {
                _NotifyChanges(L"BackgroundImageAlignmentCurrentValue");
            }
            else if (viewModelProperty == L"Foreground")
            {
                _NotifyChanges(L"ForegroundPreview");
            }
            else if (viewModelProperty == L"Background")
            {
                _NotifyChanges(L"BackgroundPreview");
            }
            else if (viewModelProperty == L"SelectionBackground")
            {
                _NotifyChanges(L"SelectionBackgroundPreview");
            }
            else if (viewModelProperty == L"CursorColor")
            {
                _NotifyChanges(L"CursorColorPreview");
            }
            else if (viewModelProperty == L"DarkColorSchemeName" || viewModelProperty == L"LightColorSchemeName")
            {
                _NotifyChanges(L"CurrentColorScheme");
            }
            else if (viewModelProperty == L"CurrentColorScheme")
            {
                _NotifyChanges(L"ForegroundPreview", L"BackgroundPreview", L"SelectionBackgroundPreview", L"CursorColorPreview");
            }
        });

        // Cache the original BG image path. If the user clicks "Use desktop
        // wallpaper", then un-checks it, this is the string we'll restore to
        // them.
        if (BackgroundImagePath().Path() != L"desktopWallpaper")
        {
            _lastBgImagePath = BackgroundImagePath().Path();
        }
    }

    winrt::hstring AppearanceViewModel::FontFace() const
    {
        return _appearance.SourceProfile().FontInfo().FontFace();
    }

    void AppearanceViewModel::FontFace(const winrt::hstring& value)
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        if (fontInfo.FontFace() == value)
        {
            return;
        }

        fontInfo.FontFace(value);
        _invalidateFontFaceDependents();

        _NotifyChanges(L"HasFontFace", L"FontFace");
    }

    bool AppearanceViewModel::HasFontFace() const
    {
        return _appearance.SourceProfile().FontInfo().HasFontFace();
    }

    void AppearanceViewModel::ClearFontFace()
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();

        fontInfo.ClearFontFace();
        _invalidateFontFaceDependents();

        _NotifyChanges(L"HasFontFace", L"FontFace");
    }

    Model::FontConfig AppearanceViewModel::FontFaceOverrideSource() const
    {
        return _appearance.SourceProfile().FontInfo().FontFaceOverrideSource();
    }

    void AppearanceViewModel::_refreshFontFaceDependents()
    {
        wil::com_ptr<IDWriteFactory> factory;
        THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(factory), reinterpret_cast<::IUnknown**>(factory.addressof())));

        wil::com_ptr<IDWriteFontCollection> fontCollection;
        THROW_IF_FAILED(factory->GetSystemFontCollection(fontCollection.addressof(), FALSE));

        const auto fontFaceSpec = FontFace();
        std::wstring missingFonts;
        std::wstring proportionalFonts;
        std::array<std::vector<Editor::FontKeyValuePair>, 2> fontSettingsRemaining;
        BOOL hasPowerlineCharacters = FALSE;

        wchar_t localeNameBuffer[LOCALE_NAME_MAX_LENGTH];
        const auto localeName = GetUserDefaultLocaleName(localeNameBuffer, LOCALE_NAME_MAX_LENGTH) ? localeNameBuffer : L"en-US";

        til::iterate_font_families(fontFaceSpec, [&](wil::zwstring_view name) {
            std::wstring* accumulator = nullptr;

            try
            {
                UINT32 index = 0;
                BOOL exists = FALSE;
                THROW_IF_FAILED(fontCollection->FindFamilyName(name.c_str(), &index, &exists));

                // Look ma, no goto!
                do
                {
                    if (!exists)
                    {
                        accumulator = &missingFonts;
                        break;
                    }

                    wil::com_ptr<IDWriteFontFamily> fontFamily;
                    THROW_IF_FAILED(fontCollection->GetFontFamily(index, fontFamily.addressof()));

                    wil::com_ptr<IDWriteFont> font;
                    THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.addressof()));

                    if (!font.query<IDWriteFont1>()->IsMonospacedFont())
                    {
                        accumulator = &proportionalFonts;
                    }

                    // We're actually checking for the "Extended" PowerLine glyph set.
                    // They're more fun.
                    BOOL hasE0B6 = FALSE;
                    std::ignore = font->HasCharacter(0xE0B6, &hasE0B6);
                    hasPowerlineCharacters |= hasE0B6;

                    wil::com_ptr<IDWriteFontFace> fontFace;
                    THROW_IF_FAILED(font->CreateFontFace(fontFace.addressof()));

                    _generateFontAxes(fontFace.get(), localeName, fontSettingsRemaining[FontAxesIndex]);
                    _generateFontFeatures(fontFace.get(), fontSettingsRemaining[FontFeaturesIndex]);
                } while (false);
            }
            catch (...)
            {
                accumulator = &missingFonts;
                LOG_CAUGHT_EXCEPTION();
            }

            if (accumulator)
            {
                if (!accumulator->empty())
                {
                    accumulator->append(L", ");
                }
                accumulator->append(name);
            }
        });

        // Up to this point, our two vectors are sorted by tag value. We want to sort them by display string now,
        // because this will result in sorted fontSettingsUsed/Unused lists below.
        for (auto& v : fontSettingsRemaining)
        {
            std::sort(v.begin(), v.end(), FontKeyValuePair::SortAscending);
        }

        std::array<std::vector<Editor::FontKeyValuePair>, 2> fontSettingsUsed;
        const std::array fontSettingsUser{
            _appearance.SourceProfile().FontInfo().FontAxes(),
            _appearance.SourceProfile().FontInfo().FontFeatures(),
        };

        // Find all axes and features that are in the user settings, and move them to the used list.
        // They'll be displayed as a list in the UI.
        for (int i = FontAxesIndex; i <= FontFeaturesIndex; i++)
        {
            const auto& map = fontSettingsUser[i];
            if (!map)
            {
                continue;
            }

            for (const auto& [tagString, value] : fontSettingsUser[i])
            {
                const auto tag = tagFromString(tagString);
                if (!tag)
                {
                    continue;
                }

                auto& remaining = fontSettingsRemaining[i];
                const auto it = std::ranges::find_if(remaining, [&](const Editor::FontKeyValuePair& kv) {
                    return winrt::get_self<FontKeyValuePair>(kv)->Key() == tag;
                });

                Editor::FontKeyValuePair kv{ nullptr };
                if (it != remaining.end())
                {
                    kv = std::move(*it);
                    remaining.erase(it);

                    const auto kvImpl = winrt::get_self<FontKeyValuePair>(kv);
                    kvImpl->SetValueDirect(value);
                }
                else
                {
                    kv = winrt::make<FontKeyValuePair>(get_weak(), hstring{}, tag, value, i == FontFeaturesIndex);
                }

                fontSettingsUsed[i].emplace_back(std::move(kv));
            }
        }

        std::array<std::vector<MenuFlyoutItemBase>, 2> fontSettingsUnused;

        // All remaining (= unused) axes and features are turned into menu items.
        // They'll be displayed as a flyout when clicking the "add item" button.
        for (int i = FontAxesIndex; i <= FontFeaturesIndex; i++)
        {
            for (const auto& kv : fontSettingsRemaining[i])
            {
                fontSettingsUnused[i].emplace_back(_createFontSettingMenuItem(kv));
            }
        }

        auto& d = _fontFaceDependents.emplace();
        d.missingFontFaces = winrt::hstring{ missingFonts };
        d.proportionalFontFaces = winrt::hstring{ proportionalFonts };
        d.hasPowerlineCharacters = hasPowerlineCharacters;

        d.fontSettingsUsed[FontAxesIndex] = winrt::single_threaded_observable_vector(std::move(fontSettingsUsed[FontAxesIndex]));
        d.fontSettingsUsed[FontFeaturesIndex] = winrt::single_threaded_observable_vector(std::move(fontSettingsUsed[FontFeaturesIndex]));
        d.fontSettingsUnused = std::move(fontSettingsUnused);

        _notifyChangesForFontSettings();
    }

    std::pair<std::vector<Editor::FontKeyValuePair>::const_iterator, bool> AppearanceViewModel::_fontSettingSortedByKeyInsertPosition(const std::vector<Editor::FontKeyValuePair>& vec, uint32_t key)
    {
        const auto it = std::lower_bound(vec.begin(), vec.end(), key, [](const Editor::FontKeyValuePair& lhs, uint32_t rhs) {
            return winrt::get_self<FontKeyValuePair>(lhs)->Key() < rhs;
        });
        const auto exists = it != vec.end() && winrt::get_self<FontKeyValuePair>(*it)->Key() == key;
        return { it, exists };
    }

    void AppearanceViewModel::_generateFontAxes(IDWriteFontFace* fontFace, const wchar_t* localeName, std::vector<Editor::FontKeyValuePair>& list)
    {
        const auto fontFace5 = wil::try_com_query<IDWriteFontFace5>(fontFace);
        if (!fontFace5)
        {
            return;
        }

        const auto axesCount = fontFace5->GetFontAxisValueCount();
        if (axesCount == 0)
        {
            return;
        }

        std::vector<DWRITE_FONT_AXIS_VALUE> axesVector(axesCount);
        THROW_IF_FAILED(fontFace5->GetFontAxisValues(axesVector.data(), axesCount));

        wil::com_ptr<IDWriteFontResource> fontResource;
        THROW_IF_FAILED(fontFace5->GetFontResource(fontResource.addressof()));

        for (UINT32 i = 0; i < axesCount; ++i)
        {
            wil::com_ptr<IDWriteLocalizedStrings> names;
            THROW_IF_FAILED(fontResource->GetAxisNames(i, names.addressof()));

            // As per MSDN:
            // > The font author may not have supplied names for some font axes.
            // > The localized strings will be empty in that case.
            if (names->GetCount() == 0)
            {
                continue;
            }

            const auto tag = axesVector[i].axisTag;
            const auto [it, tagExists] = _fontSettingSortedByKeyInsertPosition(list, tag);
            if (tagExists)
            {
                continue;
            }

            UINT32 index;
            BOOL exists;
            if (FAILED(names->FindLocaleName(localeName, &index, &exists)) || !exists)
            {
                index = 0;
            }

            const auto idx = getLocalizedStringIndex(names.get(), localeName, 0);
            const auto localizedName = getLocalizedStringByIndex(names.get(), idx);
            const auto tagString = tagToString(tag);
            hstring displayString{ fmt::format(FMT_COMPILE(L"{} ({})"), localizedName, std::wstring_view{ tagString }) };

            const auto value = axesVector[i].value;

            list.emplace(it, winrt::make<FontKeyValuePair>(get_weak(), std::move(displayString), tag, value, false));
        }
    }

    void AppearanceViewModel::_generateFontFeatures(IDWriteFontFace* fontFace, std::vector<Editor::FontKeyValuePair>& list)
    {
        wil::com_ptr<IDWriteFactory> factory;
        THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(factory), reinterpret_cast<::IUnknown**>(factory.addressof())));

        wil::com_ptr<IDWriteTextAnalyzer> textAnalyzer;
        THROW_IF_FAILED(factory->CreateTextAnalyzer(textAnalyzer.addressof()));
        const auto textAnalyzer2 = textAnalyzer.query<IDWriteTextAnalyzer2>();

        static constexpr DWRITE_SCRIPT_ANALYSIS scriptAnalysis{};
        UINT32 tagCount;
        if (textAnalyzer2->GetTypographicFeatures(fontFace, scriptAnalysis, L"en-US", 0, &tagCount, nullptr) != E_NOT_SUFFICIENT_BUFFER)
        {
            return;
        }
        std::vector<DWRITE_FONT_FEATURE_TAG> tags{ tagCount };
        if (FAILED(textAnalyzer2->GetTypographicFeatures(fontFace, scriptAnalysis, L"en-US", tagCount, &tagCount, tags.data())))
        {
            return;
        }

        for (const auto& tag : tags)
        {
            const auto [it, tagExists] = _fontSettingSortedByKeyInsertPosition(list, tag);
            if (tagExists)
            {
                continue;
            }

            const auto dfBeg = s_defaultFeatures.begin();
            const auto dfEnd = s_defaultFeatures.end();
            const auto isDefaultFeature = std::find(dfBeg, dfEnd, tag) != dfEnd;
            const auto value = isDefaultFeature ? 1.0f : 0.0f;

            list.emplace(it, winrt::make<FontKeyValuePair>(get_weak(), hstring{}, tag, value, true));
        }
    }

    MenuFlyoutItemBase AppearanceViewModel::_createFontSettingMenuItem(const Editor::FontKeyValuePair& kv)
    {
        const auto kvImpl = winrt::get_self<FontKeyValuePair>(kv);

        MenuFlyoutItem item;
        item.Text(kvImpl->KeyDisplayStringRef());
        item.Click([weakSelf = get_weak(), kv](const IInspectable& sender, const RoutedEventArgs&) {
            if (const auto self = weakSelf.get())
            {
                self->AddFontKeyValuePair(sender, kv);
            }
        });
        return item;
    }

    // Call this when all the _fontFaceDependents members have changed.
    void AppearanceViewModel::_notifyChangesForFontSettings()
    {
        _NotifyChanges(L"FontFaceDependents");
        _NotifyChanges(L"FontAxes");
        _NotifyChanges(L"FontFeatures");
        _NotifyChanges(L"HasFontAxes");
        _NotifyChanges(L"HasFontFeatures");
    }

    // Call this when used items moved into unused and vice versa.
    // Because this doesn't recreate the IObservableVector instances,
    // we don't need to notify the UI about changes to the "FontAxes" property.
    void AppearanceViewModel::_notifyChangesForFontSettingsReactive(FontSettingIndex fontSettingsIndex)
    {
        _NotifyChanges(L"FontFaceDependents");
        switch (fontSettingsIndex)
        {
        case FontAxesIndex:
            _NotifyChanges(L"HasFontAxes");
            break;
        case FontFeaturesIndex:
            _NotifyChanges(L"HasFontFeatures");
            break;
        default:
            break;
        }
    }

    double AppearanceViewModel::_parseCellSizeValue(const hstring& val) const
    {
        const auto str = val.c_str();

        auto& errnoRef = errno; // Nonzero cost, pay it once.
        errnoRef = 0;

        wchar_t* end;
        const auto value = std::wcstod(str, &end);

        return str == end || errnoRef == ERANGE ? NAN : value;
    }

    double AppearanceViewModel::LineHeight() const
    {
        const auto cellHeight = _appearance.SourceProfile().FontInfo().CellHeight();
        return _parseCellSizeValue(cellHeight);
    }

    double AppearanceViewModel::CellWidth() const
    {
        const auto cellWidth = _appearance.SourceProfile().FontInfo().CellWidth();
        return _parseCellSizeValue(cellWidth);
    }

#define CELL_SIZE_SETTER(modelName, viewModelName)                 \
    std::wstring str;                                              \
                                                                   \
    if (value >= 0.1 && value <= 10.0)                             \
    {                                                              \
        str = fmt::format(FMT_COMPILE(L"{:.6g}"), value);          \
    }                                                              \
                                                                   \
    const auto fontInfo = _appearance.SourceProfile().FontInfo();  \
                                                                   \
    if (fontInfo.modelName() != str)                               \
    {                                                              \
        if (str.empty())                                           \
        {                                                          \
            fontInfo.Clear##modelName();                           \
        }                                                          \
        else                                                       \
        {                                                          \
            fontInfo.modelName(str);                               \
        }                                                          \
        _NotifyChanges(L"Has" #viewModelName, L## #viewModelName); \
    }

    void AppearanceViewModel::LineHeight(const double value)
    {
        CELL_SIZE_SETTER(CellHeight, LineHeight);
    }

    void AppearanceViewModel::CellWidth(const double value)
    {
        CELL_SIZE_SETTER(CellWidth, CellWidth);
    }

    bool AppearanceViewModel::HasLineHeight() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.HasCellHeight();
    }

    bool AppearanceViewModel::HasCellWidth() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.HasCellWidth();
    }

    void AppearanceViewModel::ClearLineHeight()
    {
        LineHeight(NAN);
    }

    void AppearanceViewModel::ClearCellWidth()
    {
        CellWidth(NAN);
    }

    Model::FontConfig AppearanceViewModel::LineHeightOverrideSource() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.CellHeightOverrideSource();
    }

    Model::FontConfig AppearanceViewModel::CellWidthOverrideSource() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.CellWidthOverrideSource();
    }

    void AppearanceViewModel::SetFontWeightFromDouble(double fontWeight)
    {
        FontWeight(winrt::Microsoft::Terminal::UI::Converters::DoubleToFontWeight(fontWeight));
    }

    const AppearanceViewModel::FontFaceDependentsData& AppearanceViewModel::FontFaceDependents()
    {
        if (!_fontFaceDependents)
        {
            _refreshFontFaceDependents();
        }
        return *_fontFaceDependents;
    }

    winrt::hstring AppearanceViewModel::MissingFontFaces()
    {
        return FontFaceDependents().missingFontFaces;
    }

    winrt::hstring AppearanceViewModel::ProportionalFontFaces()
    {
        return FontFaceDependents().proportionalFontFaces;
    }

    bool AppearanceViewModel::HasPowerlineCharacters()
    {
        return FontFaceDependents().hasPowerlineCharacters;
    }

    IObservableVector<Editor::FontKeyValuePair> AppearanceViewModel::FontAxes()
    {
        return FontFaceDependents().fontSettingsUsed[FontAxesIndex];
    }

    bool AppearanceViewModel::HasFontAxes() const
    {
        return _appearance.SourceProfile().FontInfo().HasFontAxes();
    }

    void AppearanceViewModel::ClearFontAxes()
    {
        _deleteAllFontKeyValuePairs(FontAxesIndex);
    }

    Model::FontConfig AppearanceViewModel::FontAxesOverrideSource() const
    {
        return _appearance.SourceProfile().FontInfo().FontAxesOverrideSource();
    }

    IObservableVector<Editor::FontKeyValuePair> AppearanceViewModel::FontFeatures()
    {
        return FontFaceDependents().fontSettingsUsed[FontFeaturesIndex];
    }

    bool AppearanceViewModel::HasFontFeatures() const
    {
        return _appearance.SourceProfile().FontInfo().HasFontFeatures();
    }

    void AppearanceViewModel::ClearFontFeatures()
    {
        _deleteAllFontKeyValuePairs(FontFeaturesIndex);
    }

    Model::FontConfig AppearanceViewModel::FontFeaturesOverrideSource() const
    {
        return _appearance.SourceProfile().FontInfo().FontFeaturesOverrideSource();
    }

    void AppearanceViewModel::AddFontKeyValuePair(const IInspectable& sender, const Editor::FontKeyValuePair& kv)
    {
        if (!_fontFaceDependents)
        {
            return;
        }

        const auto kvImpl = winrt::get_self<FontKeyValuePair>(kv);
        const auto fontSettingsIndex = kvImpl->IsFontFeature() ? FontFeaturesIndex : FontAxesIndex;
        auto& d = *_fontFaceDependents;
        auto& used = d.fontSettingsUsed[fontSettingsIndex];
        auto& unused = d.fontSettingsUnused[fontSettingsIndex];

        const auto it = std::ranges::find(unused, sender);
        if (it == unused.end())
        {
            return;
        }

        // Sync the added value into the user settings model.
        UpdateFontSetting(kvImpl);

        // Insert the item into the used list, keeping it sorted by the display text.
        {
            const auto it = std::lower_bound(used.begin(), used.end(), kv, FontKeyValuePair::SortAscending);
            used.InsertAt(gsl::narrow<uint32_t>(it - used.begin()), kv);
        }

        unused.erase(it);

        _notifyChangesForFontSettingsReactive(fontSettingsIndex);
    }

    void AppearanceViewModel::DeleteFontKeyValuePair(const Editor::FontKeyValuePair& kv)
    {
        if (!_fontFaceDependents)
        {
            return;
        }

        const auto kvImpl = winrt::get_self<FontKeyValuePair>(kv);
        const auto tag = kvImpl->Key();
        const auto tagString = tagToString(tag);
        const auto fontSettingsIndex = kvImpl->IsFontFeature() ? FontFeaturesIndex : FontAxesIndex;
        auto& d = *_fontFaceDependents;
        auto& used = d.fontSettingsUsed[fontSettingsIndex];

        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        auto fontSettingsUser = kvImpl->IsFontFeature() ? fontInfo.FontFeatures() : fontInfo.FontAxes();
        if (!fontSettingsUser)
        {
            return;
        }

        const auto it = std::ranges::find(used, kv);
        if (it == used.end())
        {
            return;
        }

        fontSettingsUser.Remove(std::wstring_view{ tagString });

        _addMenuFlyoutItemToUnused(fontSettingsIndex, _createFontSettingMenuItem(*it));
        used.RemoveAt(gsl::narrow<uint32_t>(it - used.begin()));

        _notifyChangesForFontSettingsReactive(fontSettingsIndex);
    }

    void AppearanceViewModel::_deleteAllFontKeyValuePairs(FontSettingIndex fontSettingsIndex)
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        if (fontSettingsIndex == FontFeaturesIndex)
        {
            fontInfo.ClearFontFeatures();
        }
        else
        {
            fontInfo.ClearFontAxes();
        }

        if (!_fontFaceDependents)
        {
            return;
        }

        auto& d = *_fontFaceDependents;
        auto& used = d.fontSettingsUsed[fontSettingsIndex];

        for (const auto& kv : used)
        {
            _addMenuFlyoutItemToUnused(fontSettingsIndex, _createFontSettingMenuItem(kv));
        }

        used.Clear();

        _notifyChangesForFontSettingsReactive(fontSettingsIndex);
    }

    // Inserts the given menu item into the unused list, while keeping it sorted by the display text.
    void AppearanceViewModel::_addMenuFlyoutItemToUnused(FontSettingIndex index, MenuFlyoutItemBase item)
    {
        if (!_fontFaceDependents)
        {
            return;
        }

        auto& d = *_fontFaceDependents;
        auto& unused = d.fontSettingsUnused[index];

        const auto it = std::lower_bound(unused.begin(), unused.end(), item, [](const MenuFlyoutItemBase& lhs, const MenuFlyoutItemBase& rhs) {
            const auto& a = lhs.as<MenuFlyoutItem>().Text();
            const auto& b = rhs.as<MenuFlyoutItem>().Text();
            return til::compare_linguistic_insensitive(a, b) < 0;
        });
        unused.insert(it, std::move(item));
    }

    void AppearanceViewModel::UpdateFontSetting(const FontKeyValuePair* kvImpl)
    {
        const auto tag = kvImpl->Key();
        const auto value = kvImpl->Value();
        const auto tagString = tagToString(tag);
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        auto fontSettingsUser = kvImpl->IsFontFeature() ? fontInfo.FontFeatures() : fontInfo.FontAxes();

        if (!fontSettingsUser)
        {
            fontSettingsUser = winrt::single_threaded_map<hstring, float>();
            if (kvImpl->IsFontFeature())
            {
                fontInfo.FontFeatures(fontSettingsUser);
            }
            else
            {
                fontInfo.FontAxes(fontSettingsUser);
            }
        }

        std::ignore = fontSettingsUser.Insert(std::wstring_view{ tagString }, value);
        // Pwease call Profiles_Appearance::_onProfilePropertyChanged to make the pweview connyection wewoad. Thanks!! uwu
        // ...I hate this.
        _NotifyChanges(L"uwu");
    }

    void AppearanceViewModel::SetBackgroundImageOpacityFromPercentageValue(double percentageValue)
    {
        BackgroundImageOpacity(static_cast<float>(percentageValue) / 100.0f);
    }

    void AppearanceViewModel::SetBackgroundImagePath(winrt::hstring path)
    {
        _appearance.BackgroundImagePath(Model::MediaResourceHelper::FromString(path));
        _NotifyChanges(L"BackgroundImagePath");
    }

    hstring AppearanceViewModel::BackgroundImageAlignmentCurrentValue() const
    {
        const auto alignment = BackgroundImageAlignment();
        hstring alignmentResourceKey = L"Profile_BackgroundImageAlignment";
        if (alignment == (ConvergedAlignment::Vertical_Center | ConvergedAlignment::Horizontal_Center))
        {
            alignmentResourceKey = alignmentResourceKey + L"Center";
        }
        else
        {
            // Append vertical alignment to the resource key
            switch (alignment & static_cast<ConvergedAlignment>(0xF0))
            {
            case ConvergedAlignment::Vertical_Bottom:
                alignmentResourceKey = alignmentResourceKey + L"Bottom";
                break;
            case ConvergedAlignment::Vertical_Top:
                alignmentResourceKey = alignmentResourceKey + L"Top";
                break;
            }

            // Append horizontal alignment to the resource key
            switch (alignment & static_cast<ConvergedAlignment>(0x0F))
            {
            case ConvergedAlignment::Horizontal_Left:
                alignmentResourceKey = alignmentResourceKey + L"Left";
                break;
            case ConvergedAlignment::Horizontal_Right:
                alignmentResourceKey = alignmentResourceKey + L"Right";
                break;
            }
        }
        alignmentResourceKey = alignmentResourceKey + L"/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip";

        // We can't use the RS_ macro here because the resource key is dynamic
        return GetLibraryResourceString(alignmentResourceKey);
    }

    hstring AppearanceViewModel::CurrentBackgroundImagePath() const
    {
        const auto bgImagePath = BackgroundImagePath().Path();
        if (bgImagePath.empty())
        {
            return RS_(L"Appearance_BackgroundImageNone");
        }
        else if (bgImagePath == L"desktopWallpaper")
        {
            return RS_(L"Profile_UseDesktopImage/Content");
        }
        return bgImagePath;
    }

    bool AppearanceViewModel::UseDesktopBGImage() const
    {
        return BackgroundImagePath().Path() == L"desktopWallpaper";
    }

    void AppearanceViewModel::UseDesktopBGImage(const bool useDesktop)
    {
        if (useDesktop)
        {
            // Stash the current value of BackgroundImagePath. If the user
            // checks and un-checks the "Use desktop wallpaper" button, we want
            // the path that we display in the text box to remain unchanged.
            //
            // Only stash this value if it's not the special "desktopWallpaper"
            // value.
            if (BackgroundImagePath().Path() != L"desktopWallpaper")
            {
                _lastBgImagePath = BackgroundImagePath().Path();
            }
            SetBackgroundImagePath(L"desktopWallpaper");
        }
        else
        {
            // Restore the path we had previously cached. This might be the
            // empty string.
            SetBackgroundImagePath(_lastBgImagePath);
        }
    }

    bool AppearanceViewModel::BackgroundImageSettingsVisible() const
    {
        return !BackgroundImagePath().Path().empty();
    }

    void AppearanceViewModel::ClearColorScheme()
    {
        ClearDarkColorSchemeName();
        _NotifyChanges(L"CurrentColorScheme");
    }

    Editor::ColorSchemeViewModel AppearanceViewModel::CurrentColorScheme() const
    {
        const auto schemeName{ DarkColorSchemeName() };
        const auto allSchemes{ SchemesList() };
        for (const auto& scheme : allSchemes)
        {
            if (scheme.Name() == schemeName)
            {
                return scheme;
            }
        }
        // This Appearance points to a color scheme that was renamed or deleted.
        // Fallback to the first one in the list.
        return allSchemes.GetAt(0);
    }

    void AppearanceViewModel::CurrentColorScheme(const ColorSchemeViewModel& val)
    {
        DarkColorSchemeName(val.Name());
        LightColorSchemeName(val.Name());
    }

    static inline Windows::UI::Color _getColorPreview(const IReference<Microsoft::Terminal::Core::Color>& modelVal, Windows::UI::Color deducedVal)
    {
        if (modelVal)
        {
            // user defined an override value
            return Windows::UI::Color{
                .A = 255,
                .R = modelVal.Value().R,
                .G = modelVal.Value().G,
                .B = modelVal.Value().B
            };
        }
        // set to null --> deduce value from color scheme
        return deducedVal;
    }

    Windows::UI::Color AppearanceViewModel::ForegroundPreview() const
    {
        return _getColorPreview(_appearance.Foreground(), CurrentColorScheme().ForegroundColor().Color());
    }

    Windows::UI::Color AppearanceViewModel::BackgroundPreview() const
    {
        return _getColorPreview(_appearance.Background(), CurrentColorScheme().BackgroundColor().Color());
    }

    Windows::UI::Color AppearanceViewModel::SelectionBackgroundPreview() const
    {
        return _getColorPreview(_appearance.SelectionBackground(), CurrentColorScheme().SelectionBackgroundColor().Color());
    }

    Windows::UI::Color AppearanceViewModel::CursorColorPreview() const
    {
        return _getColorPreview(_appearance.CursorColor(), CurrentColorScheme().CursorColor().Color());
    }

    DependencyProperty Appearances::_AppearanceProperty{ nullptr };

    Appearances::Appearances()
    {
        InitializeComponent();

        {
            using namespace winrt::Windows::Globalization::NumberFormatting;
            // > .NET rounds to 12 significant digits when displaying doubles, so we will [...]
            // ...obviously not do that, because this is an UI element for humans. This prevents
            // issues when displaying 32-bit floats, because WinUI is unaware about their existence.
            IncrementNumberRounder rounder;
            rounder.Increment(1e-6);

            for (const auto& box : { _fontSizeBox(), _lineHeightBox() })
            {
                // BODGY: Depends on WinUI internals.
                box.NumberFormatter().as<DecimalFormatter>().NumberRounder(rounder);
            }
        }

        INITIALIZE_BINDABLE_ENUM_SETTING(CursorShape, CursorStyle, winrt::Microsoft::Terminal::Core::CursorStyle, L"Profile_CursorShape", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(AdjustIndistinguishableColors, AdjustIndistinguishableColors, winrt::Microsoft::Terminal::Core::AdjustTextMode, L"Profile_AdjustIndistinguishableColors", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(BackgroundImageStretchMode, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, L"Profile_BackgroundImageStretchMode", L"Content");

        // manually add Custom FontWeight option. Don't add it to the Map
        INITIALIZE_BINDABLE_ENUM_SETTING(FontWeight, FontWeight, uint16_t, L"Profile_FontWeight", L"Content");
        _CustomFontWeight = winrt::make<EnumEntry>(RS_(L"Profile_FontWeightCustom/Content"), winrt::box_value<uint16_t>(0u));
        _FontWeightList.Append(_CustomFontWeight);

        if (!_AppearanceProperty)
        {
            _AppearanceProperty =
                DependencyProperty::Register(
                    L"Appearance",
                    xaml_typename<Editor::AppearanceViewModel>(),
                    xaml_typename<Editor::Appearances>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ &Appearances::_ViewModelChanged } });
        }

        // manually keep track of all the Background Image Alignment buttons
        _BIAlignmentButtons.at(0) = BIAlign_TopLeft();
        _BIAlignmentButtons.at(1) = BIAlign_Top();
        _BIAlignmentButtons.at(2) = BIAlign_TopRight();
        _BIAlignmentButtons.at(3) = BIAlign_Left();
        _BIAlignmentButtons.at(4) = BIAlign_Center();
        _BIAlignmentButtons.at(5) = BIAlign_Right();
        _BIAlignmentButtons.at(6) = BIAlign_BottomLeft();
        _BIAlignmentButtons.at(7) = BIAlign_Bottom();
        _BIAlignmentButtons.at(8) = BIAlign_BottomRight();

        // apply automation properties to more complex setting controls
        for (const auto& biButton : _BIAlignmentButtons)
        {
            const auto tooltip{ ToolTipService::GetToolTip(biButton) };
            Automation::AutomationProperties::SetName(biButton, unbox_value<hstring>(tooltip));
        }

        const auto showAllFontsCheckboxTooltip{ ToolTipService::GetToolTip(ShowAllFontsCheckbox()) };
        Automation::AutomationProperties::SetFullDescription(ShowAllFontsCheckbox(), unbox_value<hstring>(showAllFontsCheckboxTooltip));

        const auto backgroundImgCheckboxTooltip{ ToolTipService::GetToolTip(UseDesktopImageCheckBox()) };
        Automation::AutomationProperties::SetFullDescription(UseDesktopImageCheckBox(), unbox_value<hstring>(backgroundImgCheckboxTooltip));

        INITIALIZE_BINDABLE_ENUM_SETTING(IntenseTextStyle, IntenseTextStyle, winrt::Microsoft::Terminal::Settings::Model::IntenseStyle, L"Appearance_IntenseTextStyle", L"Content");
    }

    IObservableVector<Editor::Font> Appearances::FilteredFontList()
    {
        if (!_filteredFonts)
        {
            _updateFilteredFontList();
        }
        return _filteredFonts;
    }

    // Method Description:
    // - Determines whether we should show the list of all the fonts, or we should just show monospace fonts
    bool Appearances::ShowAllFonts() const noexcept
    {
        return _ShowAllFonts;
    }

    void Appearances::ShowAllFonts(const bool value)
    {
        if (_ShowAllFonts != value)
        {
            _ShowAllFonts = value;
            _filteredFonts = nullptr;
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"FilteredFontList" });
        }
    }

    void Appearances::FontFaceBox_GotFocus(const Windows::Foundation::IInspectable& sender, const RoutedEventArgs&)
    {
        _updateFontNameFilter({});
        sender.as<AutoSuggestBox>().IsSuggestionListOpen(true);
    }

    void Appearances::FontFaceBox_LostFocus(const IInspectable& sender, const RoutedEventArgs&)
    {
        _updateFontName(sender.as<AutoSuggestBox>().Text());
    }

    void Appearances::FontFaceBox_QuerySubmitted(const AutoSuggestBox& sender, const AutoSuggestBoxQuerySubmittedEventArgs& args)
    {
        // When pressing Enter within the input line, this callback will be invoked with no suggestion.
        const auto font = unbox_value_or<Editor::Font>(args.ChosenSuggestion(), nullptr);
        if (!font)
        {
            return;
        }

        const auto fontName = font.Name();
        auto fontSpec = sender.Text();

        const std::wstring_view fontSpecView{ fontSpec };
        if (const auto idx = fontSpecView.rfind(L','); idx != std::wstring_view::npos)
        {
            const auto prefix = fontSpecView.substr(0, idx);
            const auto suffix = std::wstring_view{ fontName };
            fontSpec = winrt::hstring{ fmt::format(FMT_COMPILE(L"{}, {}"), prefix, suffix) };
        }
        else
        {
            fontSpec = fontName;
        }

        sender.Text(fontSpec);

        // Normally we'd just update the model property in LostFocus above, but because WinUI is the Ralph Wiggum
        // among the UI frameworks, it raises the LostFocus event _before_ the QuerySubmitted event.
        // So, when you press Save, the model will have the wrong font face string, because LostFocus was raised too early.
        // Also, this causes the first tab in the application to be focused, so when you press Enter it'll switch tabs.
        //
        // You can't just assign focus back to the AutoSuggestBox, because the FocusState() within the GotFocus event handler
        // contains random values. This prevents us from avoiding the IsSuggestionListOpen(true) in our GotFocus event handler.
        // You can't just do IsSuggestionListOpen(false) either, because you can show the list with that property but not hide it.
        // So, we update the model manually and assign focus to the parent container.
        //
        // BUT you can't just focus the parent container, because of a weird interaction with AutoSuggestBox where it'll refuse to lose
        // focus if you picked a suggestion that matches the current fontSpec. So, we unfocus it first and then focus the parent container.
        _updateFontName(fontSpec);
        sender.Focus(FocusState::Unfocused);
        FontFaceContainer().Focus(FocusState::Programmatic);
    }

    void Appearances::FontFaceBox_TextChanged(const AutoSuggestBox& sender, const AutoSuggestBoxTextChangedEventArgs& args)
    {
        if (args.Reason() != AutoSuggestionBoxTextChangeReason::UserInput)
        {
            return;
        }

        const auto fontSpec = sender.Text();
        std::wstring_view filter{ fontSpec };

        // Find the last font name in the font, spec, list.
        if (const auto idx = filter.rfind(L','); idx != std::wstring_view::npos)
        {
            filter = filter.substr(idx + 1);
        }

        filter = til::trim(filter, L' ');
        _updateFontNameFilter(filter);
    }

    void Appearances::_updateFontName(hstring fontSpec)
    {
        const auto appearance = Appearance();
        if (fontSpec.empty())
        {
            appearance.ClearFontFace();
        }
        else
        {
            appearance.FontFace(std::move(fontSpec));
        }
    }

    void Appearances::_updateFontNameFilter(std::wstring_view filter)
    {
        if (_fontNameFilter != filter)
        {
            _filteredFonts = nullptr;
            _fontNameFilter = filter;
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"FilteredFontList" });
        }
    }

    void Appearances::_updateFilteredFontList()
    {
        _filteredFonts = _ShowAllFonts ? ProfileViewModel::CompleteFontList() : ProfileViewModel::MonospaceFontList();

        if (_fontNameFilter.empty())
        {
            return;
        }

        std::vector<Editor::Font> filtered;
        filtered.reserve(_filteredFonts.Size());

        for (const auto& font : _filteredFonts)
        {
            const auto name = font.Name();
            bool match = til::contains_linguistic_insensitive(name, _fontNameFilter);

            if (!match)
            {
                const auto localizedName = font.LocalizedName();
                match = localizedName != name && til::contains_linguistic_insensitive(localizedName, _fontNameFilter);
            }

            if (match)
            {
                filtered.emplace_back(font);
            }
        }

        _filteredFonts = winrt::single_threaded_observable_vector(std::move(filtered));
    }

    void Appearances::_ViewModelChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*args*/)
    {
        const auto& obj{ d.as<Editor::Appearances>() };
        get_self<Appearances>(obj)->_UpdateWithNewViewModel();
    }

    void Appearances::_UpdateWithNewViewModel()
    {
        if (const auto appearance = Appearance())
        {
            const auto appearanceImpl = winrt::get_self<AppearanceViewModel>(appearance);
            const auto& biAlignmentVal{ static_cast<int32_t>(appearanceImpl->BackgroundImageAlignment()) };
            for (const auto& biButton : _BIAlignmentButtons)
            {
                biButton.IsChecked(biButton.Tag().as<int32_t>() == biAlignmentVal);
            }

            {
                const auto& d = appearanceImpl->FontFaceDependents();

                const std::array buttons{
                    AddFontAxisButton(),
                    AddFontFeatureButton(),
                };

                for (int i = 0; i < 2; ++i)
                {
                    const auto& button = buttons[i];
                    const auto& data = d.fontSettingsUnused[i];
                    const auto items = button.Flyout().as<MenuFlyout>().Items();
                    items.ReplaceAll(data);
                    button.IsEnabled(!data.empty());
                }
            }

            _ViewModelChangedRevoker = appearance.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
                const auto settingName{ args.PropertyName() };
                if (settingName == L"CursorShape")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
                }
                else if (settingName == L"DarkColorSchemeName" || settingName == L"LightColorSchemeName")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
                }
                else if (settingName == L"BackgroundImageStretchMode")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentBackgroundImageStretchMode" });
                }
                else if (settingName == L"BackgroundImageAlignment")
                {
                    _UpdateBIAlignmentControl(static_cast<int32_t>(appearanceImpl->BackgroundImageAlignment()));
                }
                else if (settingName == L"FontWeight")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentFontWeight" });
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
                }
                else if (settingName == L"FontFaceDependents")
                {
                    const auto& d = appearanceImpl->FontFaceDependents();

                    const std::array buttons{
                        AddFontAxisButton(),
                        AddFontFeatureButton(),
                    };

                    for (int i = 0; i < 2; ++i)
                    {
                        const auto& button = buttons[i];
                        const auto& data = d.fontSettingsUnused[i];
                        const auto flyout = button.Flyout().as<MenuFlyout>();
                        const auto items = flyout.Items();
                        items.ReplaceAll(data);
                        button.IsEnabled(!data.empty());
                        // WinUI doesn't hide the flyout when it's currently open and the items are now empty.
                        // In fact it doesn't close it, period. You click an item? Flyout stays open!
                        // This gets called whenever an item is selected, so it's the "perfect" time to close it.
                        flyout.Hide();
                    }
                }
                else if (settingName == L"IntenseTextStyle")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentIntenseTextStyle" });
                }
                else if (settingName == L"AdjustIndistinguishableColors")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentAdjustIndistinguishableColors" });
                }
                else if (settingName == L"ShowProportionalFontWarning")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"ShowProportionalFontWarning" });
                }
                // YOU THERE ADDING A NEW APPEARANCE SETTING
                // Make sure you add a block like
                //
                //   else if (settingName == L"MyNewSetting")
                //   {
                //       PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentMyNewSetting" });
                //   }
                //
                // To make sure that changes to the AppearanceViewModel will
                // propagate back up to the actual UI (in Appearances). The
                // CurrentMyNewSetting properties are the ones that are bound in
                // XAML. If you don't do this right (or only raise a property
                // changed for "MyNewSetting"), then things like the reset
                // button won't work right.
            });

            // make sure to send all the property changed events once here
            // we do this in the case an old appearance was deleted and then a new one is created,
            // the old settings need to be updated in xaml
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentBackgroundImageStretchMode" });
            _UpdateBIAlignmentControl(static_cast<int32_t>(appearance.BackgroundImageAlignment()));
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentFontWeight" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentIntenseTextStyle" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentAdjustIndistinguishableColors" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"ShowProportionalFontWarning" });
        }
    }

    safe_void_coroutine Appearances::BackgroundImage_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            Appearance().SetBackgroundImagePath(file);
        }
    }

    void Appearances::BIAlignment_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        if (const auto& button{ sender.try_as<Primitives::ToggleButton>() })
        {
            if (const auto& tag{ button.Tag().try_as<int32_t>() })
            {
                // Update the Appearance's value and the control
                Appearance().BackgroundImageAlignment(static_cast<ConvergedAlignment>(*tag));
                _UpdateBIAlignmentControl(*tag);
            }
        }
    }

    // Method Description:
    // - Resets all of the buttons to unchecked, and checks the one with the provided tag
    // Arguments:
    // - val - the background image alignment (ConvergedAlignment) that we want to represent in the control
    void Appearances::_UpdateBIAlignmentControl(const int32_t val)
    {
        for (const auto& biButton : _BIAlignmentButtons)
        {
            if (const auto& biButtonAlignment{ biButton.Tag().try_as<int32_t>() })
            {
                biButton.IsChecked(biButtonAlignment == val);
            }
        }
    }

    void Appearances::DeleteFontKeyValuePair_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        const auto element = sender.as<FrameworkElement>();
        const auto tag = element.Tag();
        const auto kv = tag.as<Editor::FontKeyValuePair>();
        winrt::get_self<AppearanceViewModel>(Appearance())->DeleteFontKeyValuePair(kv);
    }

    bool Appearances::IsVintageCursor() const
    {
        return Appearance().CursorShape() == Core::CursorStyle::Vintage;
    }

    IInspectable Appearances::CurrentFontWeight() const
    {
        // if no value was found, we have a custom value
        const auto maybeEnumEntry{ _FontWeightMap.TryLookup(Appearance().FontWeight().Weight) };
        return maybeEnumEntry ? maybeEnumEntry : _CustomFontWeight;
    }

    void Appearances::CurrentFontWeight(const IInspectable& enumEntry)
    {
        if (auto ee = enumEntry.try_as<Editor::EnumEntry>())
        {
            if (ee != _CustomFontWeight)
            {
                const auto weight{ winrt::unbox_value<uint16_t>(ee.EnumValue()) };
                const Windows::UI::Text::FontWeight setting{ weight };
                Appearance().FontWeight(setting);

                // Appearance does not have observable properties
                // So the TwoWay binding doesn't update on the State --> Slider direction
                FontWeightSlider().Value(weight);
            }
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
        }
    }

    bool Appearances::IsCustomFontWeight()
    {
        // Use SelectedItem instead of CurrentFontWeight.
        // CurrentFontWeight converts the Appearance's value to the appropriate enum entry,
        // whereas SelectedItem identifies which one was selected by the user.
        return FontWeightComboBox().SelectedItem() == _CustomFontWeight;
    }

}
