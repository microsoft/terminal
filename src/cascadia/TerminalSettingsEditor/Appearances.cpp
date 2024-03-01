// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Appearances.h"
#include "Appearances.g.cpp"
#include "AxisKeyValuePair.g.cpp"
#include "FeatureKeyValuePair.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static constexpr std::array<std::wstring_view, 11> DefaultFeatures{
    L"rlig",
    L"locl",
    L"ccmp",
    L"calt",
    L"liga",
    L"clig",
    L"rnrn",
    L"kern",
    L"mark",
    L"mkmk",
    L"dist"
};

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    bool Font::HasPowerlineCharacters()
    {
        if (!_hasPowerlineCharacters.has_value())
        {
            try
            {
                winrt::com_ptr<IDWriteFont> font;
                THROW_IF_FAILED(_family->GetFont(0, font.put()));
                BOOL exists{};
                // We're actually checking for the "Extended" PowerLine glyph set.
                // They're more fun.
                THROW_IF_FAILED(font->HasCharacter(0xE0B6, &exists));
                _hasPowerlineCharacters = (exists == TRUE);
            }
            catch (...)
            {
                _hasPowerlineCharacters = false;
            }
        }
        return _hasPowerlineCharacters.value_or(false);
    }

    Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> Font::FontAxesTagsAndNames()
    {
        if (!_fontAxesTagsAndNames)
        {
            wil::com_ptr<IDWriteFont> font;
            THROW_IF_FAILED(_family->GetFont(0, font.put()));
            wil::com_ptr<IDWriteFontFace> fontFace;
            THROW_IF_FAILED(font->CreateFontFace(fontFace.put()));
            wil::com_ptr<IDWriteFontFace5> fontFace5;
            if (fontFace5 = fontFace.try_query<IDWriteFontFace5>())
            {
                wil::com_ptr<IDWriteFontResource> fontResource;
                THROW_IF_FAILED(fontFace5->GetFontResource(fontResource.put()));

                const auto axesCount = fontFace5->GetFontAxisValueCount();
                if (axesCount > 0)
                {
                    std::vector<DWRITE_FONT_AXIS_VALUE> axesVector(axesCount);
                    fontFace5->GetFontAxisValues(axesVector.data(), axesCount);

                    uint32_t localeIndex;
                    BOOL localeExists;
                    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
                    const auto localeToTry = GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) ? localeName : L"en-US";

                    std::unordered_map<winrt::hstring, winrt::hstring> fontAxesTagsAndNames;
                    for (uint32_t i = 0; i < axesCount; ++i)
                    {
                        wil::com_ptr<IDWriteLocalizedStrings> names;
                        THROW_IF_FAILED(fontResource->GetAxisNames(i, names.put()));

                        if (!SUCCEEDED(names->FindLocaleName(localeToTry, &localeIndex, &localeExists)) || !localeExists)
                        {
                            // default to the first locale in the list
                            localeIndex = 0;
                        }

                        UINT32 length = 0;
                        if (SUCCEEDED(names->GetStringLength(localeIndex, &length)))
                        {
                            winrt::impl::hstring_builder builder{ length };
                            if (SUCCEEDED(names->GetString(localeIndex, builder.data(), length + 1)))
                            {
                                fontAxesTagsAndNames.insert(std::pair<winrt::hstring, winrt::hstring>(_tagToString(axesVector[i].axisTag), builder.to_hstring()));
                                continue;
                            }
                        }
                        // if there was no name found, it means the font does not actually support this axis
                        // don't insert anything into the vector in this case
                    }
                    _fontAxesTagsAndNames = winrt::single_threaded_map(std::move(fontAxesTagsAndNames));
                }
            }
        }
        return _fontAxesTagsAndNames;
    }

    IMap<hstring, hstring> Font::FontFeaturesTagsAndNames()
    {
        if (!_fontFeaturesTagsAndNames)
        {
            wil::com_ptr<IDWriteFont> font;
            THROW_IF_FAILED(_family->GetFont(0, font.put()));
            wil::com_ptr<IDWriteFontFace> fontFace;
            THROW_IF_FAILED(font->CreateFontFace(fontFace.put()));

            wil::com_ptr<IDWriteFactory2> factory;
            THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(factory), reinterpret_cast<::IUnknown**>(factory.addressof())));
            wil::com_ptr<IDWriteTextAnalyzer> textAnalyzer;
            factory->CreateTextAnalyzer(textAnalyzer.addressof());
            wil::com_ptr<IDWriteTextAnalyzer2> textAnalyzer2 = textAnalyzer.query<IDWriteTextAnalyzer2>();

            DWRITE_SCRIPT_ANALYSIS scriptAnalysis{};
            UINT32 tagCount;
            // we have to call GetTypographicFeatures twice, first to get the actual count then to get the features
            std::ignore = textAnalyzer2->GetTypographicFeatures(fontFace.get(), scriptAnalysis, L"en-us", 0, &tagCount, nullptr);
            std::vector<DWRITE_FONT_FEATURE_TAG> tags{ tagCount };
            textAnalyzer2->GetTypographicFeatures(fontFace.get(), scriptAnalysis, L"en-us", tagCount, &tagCount, tags.data());

            std::unordered_map<winrt::hstring, winrt::hstring> fontFeaturesTagsAndNames;
            for (auto tag : tags)
            {
                const auto tagString = _tagToString(tag);
                hstring formattedResourceString{ fmt::format(L"Profile_FontFeature_{}", tagString) };
                hstring localizedName{ tagString };
                // we have resource strings for common font features, see if one for this feature exists
                if (HasLibraryResourceWithName(formattedResourceString))
                {
                    localizedName = GetLibraryResourceString(formattedResourceString);
                }
                fontFeaturesTagsAndNames.insert(std::pair<winrt::hstring, winrt::hstring>(tagString, localizedName));
            }
            _fontFeaturesTagsAndNames = winrt::single_threaded_map<winrt::hstring, winrt::hstring>(std::move(fontFeaturesTagsAndNames));
        }
        return _fontFeaturesTagsAndNames;
    }

    winrt::hstring Font::_tagToString(DWRITE_FONT_AXIS_TAG tag)
    {
        std::wstring result;
        for (int i = 0; i < 4; ++i)
        {
            result.push_back((tag >> (i * 8)) & 0xFF);
        }
        return winrt::hstring{ result };
    }

    hstring Font::_tagToString(DWRITE_FONT_FEATURE_TAG tag)
    {
        std::wstring result;
        for (int i = 0; i < 4; ++i)
        {
            result.push_back((tag >> (i * 8)) & 0xFF);
        }
        return hstring{ result };
    }

    AxisKeyValuePair::AxisKeyValuePair(winrt::hstring axisKey, float axisValue, const Windows::Foundation::Collections::IMap<winrt::hstring, float>& baseMap, const Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>& tagToNameMap) :
        _AxisKey{ axisKey },
        _AxisValue{ axisValue },
        _baseMap{ baseMap },
        _tagToNameMap{ tagToNameMap }
    {
        if (_tagToNameMap.HasKey(_AxisKey))
        {
            int32_t i{ 0 };
            // IMap guarantees that the iteration order is the same every time
            // so this conversion of key to index is safe
            for (const auto tagAndName : _tagToNameMap)
            {
                if (tagAndName.Key() == _AxisKey)
                {
                    _AxisIndex = i;
                    break;
                }
                ++i;
            }
        }
    }

    winrt::hstring AxisKeyValuePair::AxisKey()
    {
        return _AxisKey;
    }

    float AxisKeyValuePair::AxisValue()
    {
        return _AxisValue;
    }

    int32_t AxisKeyValuePair::AxisIndex()
    {
        return _AxisIndex;
    }

    void AxisKeyValuePair::AxisValue(float axisValue)
    {
        if (axisValue != _AxisValue)
        {
            _baseMap.Remove(_AxisKey);
            _AxisValue = axisValue;
            _baseMap.Insert(_AxisKey, _AxisValue);
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"AxisValue" });
        }
    }

    void AxisKeyValuePair::AxisKey(winrt::hstring axisKey)
    {
        if (axisKey != _AxisKey)
        {
            _baseMap.Remove(_AxisKey);
            _AxisKey = axisKey;
            _baseMap.Insert(_AxisKey, _AxisValue);
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"AxisKey" });
        }
    }

    void AxisKeyValuePair::AxisIndex(int32_t axisIndex)
    {
        if (axisIndex != _AxisIndex)
        {
            _AxisIndex = axisIndex;

            int32_t i{ 0 };
            // same as in the constructor, iterating through IMap
            // gives us the same order every time
            for (const auto tagAndName : _tagToNameMap)
            {
                if (i == _AxisIndex)
                {
                    AxisKey(tagAndName.Key());
                    break;
                }
                ++i;
            }
        }
    }

    FeatureKeyValuePair::FeatureKeyValuePair(hstring featureKey, uint32_t featureValue, const IMap<hstring, uint32_t>& baseMap, const IMap<hstring, hstring>& tagToNameMap) :
        _FeatureKey{ featureKey },
        _FeatureValue{ featureValue },
        _baseMap{ baseMap },
        _tagToNameMap{ tagToNameMap }
    {
        if (_tagToNameMap.HasKey(_FeatureKey))
        {
            int32_t i{ 0 };
            // this loop assumes that every time we iterate through the map
            // we get the same ordering
            for (const auto tagAndName : _tagToNameMap)
            {
                if (tagAndName.Key() == _FeatureKey)
                {
                    _FeatureIndex = i;
                    break;
                }
                ++i;
            }
        }
    }

    hstring FeatureKeyValuePair::FeatureKey()
    {
        return _FeatureKey;
    }

    uint32_t FeatureKeyValuePair::FeatureValue()
    {
        return _FeatureValue;
    }

    int32_t FeatureKeyValuePair::FeatureIndex()
    {
        return _FeatureIndex;
    }

    void FeatureKeyValuePair::FeatureValue(uint32_t featureValue)
    {
        if (featureValue != _FeatureValue)
        {
            _baseMap.Remove(_FeatureKey);
            _FeatureValue = featureValue;
            _baseMap.Insert(_FeatureKey, _FeatureValue);
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"FeatureValue" });
        }
    }

    void FeatureKeyValuePair::FeatureKey(hstring featureKey)
    {
        if (featureKey != _FeatureKey)
        {
            _baseMap.Remove(_FeatureKey);
            _FeatureKey = featureKey;
            _baseMap.Insert(_FeatureKey, _FeatureValue);
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"FeatureKey" });
        }
    }

    void FeatureKeyValuePair::FeatureIndex(int32_t featureIndex)
    {
        if (featureIndex != _FeatureIndex)
        {
            _FeatureIndex = featureIndex;

            int32_t i{ 0 };
            // same as in the constructor, this assumes that iterating through the map
            // gives us the same order every time
            for (const auto tagAndName : _tagToNameMap)
            {
                if (i == _FeatureIndex)
                {
                    FeatureKey(tagAndName.Key());
                    break;
                }
                ++i;
            }
        }
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
                _NotifyChanges(L"UseDesktopBGImage", L"BackgroundImageSettingsVisible");
            }
            else if (viewModelProperty == L"FontAxes")
            {
                // this is a weird one
                // we manually make the observable vector based on the map in the settings model
                // (this is due to xaml being unable to bind a list view to a map)
                // so when the FontAxes change (say from the reset button), reinitialize the observable vector
                InitializeFontAxesVector();
            }
            else if (viewModelProperty == L"FontFeatures")
            {
                // same as the FontAxes one
                InitializeFontFeaturesVector();
            }
        });

        InitializeFontAxesVector();
        InitializeFontFeaturesVector();

        // Cache the original BG image path. If the user clicks "Use desktop
        // wallpaper", then un-checks it, this is the string we'll restore to
        // them.
        if (BackgroundImagePath() != L"desktopWallpaper")
        {
            _lastBgImagePath = BackgroundImagePath();
        }
    }

    double AppearanceViewModel::LineHeight() const noexcept
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        const auto cellHeight = fontInfo.CellHeight();
        const auto str = cellHeight.c_str();

        auto& errnoRef = errno; // Nonzero cost, pay it once.
        errnoRef = 0;

        wchar_t* end;
        const auto value = std::wcstod(str, &end);

        return str == end || errnoRef == ERANGE ? NAN : value;
    }

    void AppearanceViewModel::LineHeight(const double value)
    {
        std::wstring str;

        if (value >= 0.1 && value <= 10.0)
        {
            str = fmt::format(FMT_STRING(L"{:.6g}"), value);
        }

        const auto fontInfo = _appearance.SourceProfile().FontInfo();

        if (fontInfo.CellHeight() != str)
        {
            if (str.empty())
            {
                fontInfo.ClearCellHeight();
            }
            else
            {
                fontInfo.CellHeight(str);
            }
            _NotifyChanges(L"HasLineHeight", L"LineHeight");
        }
    }

    bool AppearanceViewModel::HasLineHeight() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.HasCellHeight();
    }

    void AppearanceViewModel::ClearLineHeight()
    {
        LineHeight(NAN);
    }

    Model::FontConfig AppearanceViewModel::LineHeightOverrideSource() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.CellHeightOverrideSource();
    }

    void AppearanceViewModel::SetFontWeightFromDouble(double fontWeight)
    {
        FontWeight(winrt::Microsoft::Terminal::UI::Converters::DoubleToFontWeight(fontWeight));
    }

    void AppearanceViewModel::SetBackgroundImageOpacityFromPercentageValue(double percentageValue)
    {
        BackgroundImageOpacity(winrt::Microsoft::Terminal::UI::Converters::PercentageValueToPercentage(percentageValue));
    }

    void AppearanceViewModel::SetBackgroundImagePath(winrt::hstring path)
    {
        BackgroundImagePath(path);
    }

    bool AppearanceViewModel::UseDesktopBGImage()
    {
        return BackgroundImagePath() == L"desktopWallpaper";
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
            if (BackgroundImagePath() != L"desktopWallpaper")
            {
                _lastBgImagePath = BackgroundImagePath();
            }
            BackgroundImagePath(L"desktopWallpaper");
        }
        else
        {
            // Restore the path we had previously cached. This might be the
            // empty string.
            BackgroundImagePath(_lastBgImagePath);
        }
    }

    bool AppearanceViewModel::BackgroundImageSettingsVisible()
    {
        return BackgroundImagePath() != L"";
    }

    void AppearanceViewModel::ClearColorScheme()
    {
        ClearDarkColorSchemeName();
        _NotifyChanges(L"CurrentColorScheme");
    }

    Editor::ColorSchemeViewModel AppearanceViewModel::CurrentColorScheme()
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

    void AppearanceViewModel::AddNewAxisKeyValuePair()
    {
        if (!_appearance.SourceProfile().FontInfo().FontAxes())
        {
            _appearance.SourceProfile().FontInfo().FontAxes(winrt::single_threaded_map<winrt::hstring, float>());
        }
        auto fontAxesMap = _appearance.SourceProfile().FontInfo().FontAxes();

        // find one axis that does not already exist, and add that
        // if there are no more possible axes to add, the button is disabled so there shouldn't be a way to get here
        const auto possibleAxesTagsAndNames = ProfileViewModel::FindFontWithLocalizedName(FontFace()).FontAxesTagsAndNames();
        for (const auto tagAndName : possibleAxesTagsAndNames)
        {
            if (!fontAxesMap.HasKey(tagAndName.Key()))
            {
                fontAxesMap.Insert(tagAndName.Key(), gsl::narrow<float>(0));
                FontAxesVector().Append(_CreateAxisKeyValuePairHelper(tagAndName.Key(), gsl::narrow<float>(0), fontAxesMap, possibleAxesTagsAndNames));
                break;
            }
        }
        _NotifyChanges(L"CanFontAxesBeAdded");
    }

    void AppearanceViewModel::DeleteAxisKeyValuePair(winrt::hstring key)
    {
        for (uint32_t i = 0; i < _FontAxesVector.Size(); i++)
        {
            if (_FontAxesVector.GetAt(i).AxisKey() == key)
            {
                FontAxesVector().RemoveAt(i);
                _appearance.SourceProfile().FontInfo().FontAxes().Remove(key);
                if (_FontAxesVector.Size() == 0)
                {
                    _appearance.SourceProfile().FontInfo().ClearFontAxes();
                }
                break;
            }
        }
        _NotifyChanges(L"CanFontAxesBeAdded");
    }

    void AppearanceViewModel::InitializeFontAxesVector()
    {
        if (!_FontAxesVector)
        {
            _FontAxesVector = winrt::single_threaded_observable_vector<Editor::AxisKeyValuePair>();
        }

        _FontAxesVector.Clear();
        if (const auto fontAxesMap = _appearance.SourceProfile().FontInfo().FontAxes())
        {
            const auto fontAxesTagToNameMap = ProfileViewModel::FindFontWithLocalizedName(FontFace()).FontAxesTagsAndNames();
            for (const auto axis : fontAxesMap)
            {
                // only show the axes that the font supports
                // any axes that the font doesn't support continue to be stored in the json, we just don't show them in the UI
                if (fontAxesTagToNameMap.HasKey(axis.Key()))
                {
                    _FontAxesVector.Append(_CreateAxisKeyValuePairHelper(axis.Key(), axis.Value(), fontAxesMap, fontAxesTagToNameMap));
                }
            }
        }
        _NotifyChanges(L"AreFontAxesAvailable", L"CanFontAxesBeAdded");
    }

    // Method Description:
    // - Determines whether the currently selected font has any variable font axes
    bool AppearanceViewModel::AreFontAxesAvailable()
    {
        return ProfileViewModel::FindFontWithLocalizedName(FontFace()).FontAxesTagsAndNames().Size() > 0;
    }

    // Method Description:
    // - Determines whether the currently selected font has any variable font axes that have not already been set
    bool AppearanceViewModel::CanFontAxesBeAdded()
    {
        if (const auto fontAxesTagToNameMap = ProfileViewModel::FindFontWithLocalizedName(FontFace()).FontAxesTagsAndNames(); fontAxesTagToNameMap.Size() > 0)
        {
            if (const auto fontAxesMap = _appearance.SourceProfile().FontInfo().FontAxes())
            {
                for (const auto tagAndName : fontAxesTagToNameMap)
                {
                    if (!fontAxesMap.HasKey(tagAndName.Key()))
                    {
                        // we found an axis that has not been set
                        return true;
                    }
                }
                // all possible axes have been set already
                return false;
            }
            // the font supports font axes but the profile has none set
            return true;
        }
        // the font does not support any font axes
        return false;
    }

    // Method Description:
    // - Creates an AxisKeyValuePair and sets up an event handler for it
    Editor::AxisKeyValuePair AppearanceViewModel::_CreateAxisKeyValuePairHelper(winrt::hstring axisKey, float axisValue, const Windows::Foundation::Collections::IMap<winrt::hstring, float>& baseMap, const Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>& tagToNameMap)
    {
        const auto axisKeyValuePair = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::AxisKeyValuePair>(axisKey, axisValue, baseMap, tagToNameMap);
        // when either the key or the value changes, send an event for the preview control to catch
        axisKeyValuePair.PropertyChanged([weakThis = get_weak()](auto& /*sender*/, auto& /*e*/) {
            if (auto appVM{ weakThis.get() })
            {
                appVM->_NotifyChanges(L"AxisKeyValuePair");
            }
        });
        return axisKeyValuePair;
    }

    void AppearanceViewModel::AddNewFeatureKeyValuePair()
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        auto fontFeaturesMap = fontInfo.FontFeatures();
        if (!fontFeaturesMap)
        {
            fontFeaturesMap = winrt::single_threaded_map<hstring, uint32_t>();
            fontInfo.FontFeatures(fontFeaturesMap);
        }

        // find one feature that does not already exist, and add that
        // if there are no more possible features to add, the button is disabled so there shouldn't be a way to get here
        const auto possibleFeaturesTagsAndNames = ProfileViewModel::FindFontWithLocalizedName(FontFace()).FontFeaturesTagsAndNames();
        for (const auto tagAndName : possibleFeaturesTagsAndNames)
        {
            const auto featureKey = tagAndName.Key();
            if (!fontFeaturesMap.HasKey(featureKey))
            {
                const auto featureDefaultValue = _IsDefaultFeature(featureKey) ? 1 : 0;
                fontFeaturesMap.Insert(featureKey, featureDefaultValue);
                FontFeaturesVector().Append(_CreateFeatureKeyValuePairHelper(featureKey, featureDefaultValue, fontFeaturesMap, possibleFeaturesTagsAndNames));
                break;
            }
        }
        _NotifyChanges(L"CanFontFeaturesBeAdded");
    }

    void AppearanceViewModel::DeleteFeatureKeyValuePair(hstring key)
    {
        for (uint32_t i = 0; i < _FontFeaturesVector.Size(); i++)
        {
            if (_FontFeaturesVector.GetAt(i).FeatureKey() == key)
            {
                FontFeaturesVector().RemoveAt(i);
                _appearance.SourceProfile().FontInfo().FontFeatures().Remove(key);
                if (_FontFeaturesVector.Size() == 0)
                {
                    _appearance.SourceProfile().FontInfo().ClearFontFeatures();
                }
                break;
            }
        }
        _NotifyChanges(L"CanFontAxesBeAdded");
    }

    void AppearanceViewModel::InitializeFontFeaturesVector()
    {
        if (!_FontFeaturesVector)
        {
            _FontFeaturesVector = single_threaded_observable_vector<Editor::FeatureKeyValuePair>();
        }

        _FontFeaturesVector.Clear();
        if (const auto fontFeaturesMap = _appearance.SourceProfile().FontInfo().FontFeatures())
        {
            const auto fontFeaturesTagToNameMap = ProfileViewModel::FindFontWithLocalizedName(FontFace()).FontFeaturesTagsAndNames();
            for (const auto feature : fontFeaturesMap)
            {
                const auto featureKey = feature.Key();
                // only show the features that the font supports
                // any features that the font doesn't support continue to be stored in the json, we just don't show them in the UI
                if (fontFeaturesTagToNameMap.HasKey(featureKey))
                {
                    _FontFeaturesVector.Append(_CreateFeatureKeyValuePairHelper(featureKey, feature.Value(), fontFeaturesMap, fontFeaturesTagToNameMap));
                }
            }
        }
        _NotifyChanges(L"AreFontFeaturesAvailable", L"CanFontFeaturesBeAdded");
    }

    // Method Description:
    // - Determines whether the currently selected font has any font features
    bool AppearanceViewModel::AreFontFeaturesAvailable()
    {
        return ProfileViewModel::FindFontWithLocalizedName(FontFace()).FontFeaturesTagsAndNames().Size() > 0;
    }

    // Method Description:
    // - Determines whether the currently selected font has any font features that have not already been set
    bool AppearanceViewModel::CanFontFeaturesBeAdded()
    {
        if (const auto fontFeaturesTagToNameMap = ProfileViewModel::FindFontWithLocalizedName(FontFace()).FontFeaturesTagsAndNames(); fontFeaturesTagToNameMap.Size() > 0)
        {
            if (const auto fontFeaturesMap = _appearance.SourceProfile().FontInfo().FontFeatures())
            {
                for (const auto tagAndName : fontFeaturesTagToNameMap)
                {
                    if (!fontFeaturesMap.HasKey(tagAndName.Key()))
                    {
                        // we found a feature that has not been set
                        return true;
                    }
                }
                // all possible features have been set already
                return false;
            }
            // the font supports font features but the profile has none set
            return true;
        }
        // the font does not support any font features
        return false;
    }

    // Method Description:
    // - Creates a FeatureKeyValuePair and sets up an event handler for it
    Editor::FeatureKeyValuePair AppearanceViewModel::_CreateFeatureKeyValuePairHelper(hstring featureKey, uint32_t featureValue, const IMap<hstring, uint32_t>& baseMap, const IMap<hstring, hstring>& tagToNameMap)
    {
        const auto featureKeyValuePair = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::FeatureKeyValuePair>(featureKey, featureValue, baseMap, tagToNameMap);
        // when either the key or the value changes, send an event for the preview control to catch
        featureKeyValuePair.PropertyChanged([weakThis = get_weak()](auto& sender, const PropertyChangedEventArgs& args) {
            if (auto appVM{ weakThis.get() })
            {
                appVM->_NotifyChanges(L"FeatureKeyValuePair");
                const auto settingName{ args.PropertyName() };
                if (settingName == L"FeatureKey")
                {
                    const auto senderPair = sender.as<FeatureKeyValuePair>();
                    const auto senderKey = senderPair->FeatureKey();
                    if (appVM->_IsDefaultFeature(senderKey))
                    {
                        senderPair->FeatureValue(1);
                    }
                    else
                    {
                        senderPair->FeatureValue(0);
                    }
                }
            }
        });
        return featureKeyValuePair;
    }

    bool AppearanceViewModel::_IsDefaultFeature(winrt::hstring featureKey)
    {
        for (const auto defaultFeature : DefaultFeatures)
        {
            if (defaultFeature == featureKey)
            {
                return true;
            }
        }
        return false;
    }

    DependencyProperty Appearances::_AppearanceProperty{ nullptr };

    Appearances::Appearances() :
        _ShowAllFonts{ false },
        _ShowProportionalFontWarning{ false }
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

        _FontAxesNames = winrt::single_threaded_observable_vector<winrt::hstring>();
        FontAxesNamesCVS().Source(_FontAxesNames);

        _FontFeaturesNames = winrt::single_threaded_observable_vector<hstring>();
        FontFeaturesNamesCVS().Source(_FontFeaturesNames);

        INITIALIZE_BINDABLE_ENUM_SETTING(IntenseTextStyle, IntenseTextStyle, winrt::Microsoft::Terminal::Settings::Model::IntenseStyle, L"Appearance_IntenseTextStyle", L"Content");
    }

    // Method Description:
    // - Searches through our list of monospace fonts to determine if the settings model's current font face is a monospace font
    bool Appearances::UsingMonospaceFont() const noexcept
    {
        auto result{ false };
        const auto currentFont{ Appearance().FontFace() };
        for (const auto& font : ProfileViewModel::MonospaceFontList())
        {
            if (font.LocalizedName() == currentFont)
            {
                result = true;
            }
        }
        return result;
    }

    // Method Description:
    // - Determines whether we should show the list of all the fonts, or we should just show monospace fonts
    bool Appearances::ShowAllFonts() const noexcept
    {
        // - _ShowAllFonts is directly bound to the checkbox. So this is the user set value.
        // - If we are not using a monospace font, show all of the fonts so that the ComboBox is still properly bound
        return _ShowAllFonts || !UsingMonospaceFont();
    }

    void Appearances::ShowAllFonts(const bool& value)
    {
        if (_ShowAllFonts != value)
        {
            _ShowAllFonts = value;
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
        }
    }

    IInspectable Appearances::CurrentFontFace() const
    {
        const auto& appearanceVM{ Appearance() };
        const auto appearanceFontFace{ appearanceVM.FontFace() };
        return box_value(ProfileViewModel::FindFontWithLocalizedName(appearanceFontFace));
    }

    void Appearances::FontFace_SelectionChanged(const IInspectable& /*sender*/, const SelectionChangedEventArgs& e)
    {
        // NOTE: We need to hook up a selection changed event handler here instead of directly binding to the appearance view model.
        //       A two way binding to the view model causes an infinite loop because both combo boxes keep fighting over which one's right.
        const auto selectedItem{ e.AddedItems().GetAt(0) };
        const auto newFontFace{ unbox_value<Editor::Font>(selectedItem) };
        Appearance().FontFace(newFontFace.LocalizedName());
        if (!UsingMonospaceFont())
        {
            ShowProportionalFontWarning(true);
        }
        else
        {
            ShowProportionalFontWarning(false);
        }

        _FontAxesNames.Clear();
        const auto axesTagsAndNames = newFontFace.FontAxesTagsAndNames();
        for (const auto tagAndName : axesTagsAndNames)
        {
            _FontAxesNames.Append(tagAndName.Value());
        }

        _FontFeaturesNames.Clear();
        const auto featuresTagsAndNames = newFontFace.FontFeaturesTagsAndNames();
        for (const auto tagAndName : featuresTagsAndNames)
        {
            _FontFeaturesNames.Append(tagAndName.Value());
        }

        // when the font face changes, we have to tell the view model to update the font axes/features vectors
        // since the new font may not have the same possible axes as the previous one
        Appearance().InitializeFontAxesVector();
        if (!Appearance().AreFontAxesAvailable())
        {
            // if the previous font had available font axes and the expander was expanded,
            // at this point the expander would be set to disabled so manually collapse it
            FontAxesContainer().SetExpanded(false);
            FontAxesContainer().HelpText(RS_(L"Profile_FontAxesUnavailable/Text"));
        }
        else
        {
            FontAxesContainer().HelpText(RS_(L"Profile_FontAxesAvailable/Text"));
        }

        Appearance().InitializeFontFeaturesVector();
        if (!Appearance().AreFontFeaturesAvailable())
        {
            // if the previous font had available font features and the expander was expanded,
            // at this point the expander would be set to disabled so manually collapse it
            FontFeaturesContainer().SetExpanded(false);
            FontFeaturesContainer().HelpText(RS_(L"Profile_FontFeaturesUnavailable/Text"));
        }
        else
        {
            FontFeaturesContainer().HelpText(RS_(L"Profile_FontFeaturesAvailable/Text"));
        }
    }

    void Appearances::_ViewModelChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*args*/)
    {
        const auto& obj{ d.as<Editor::Appearances>() };
        get_self<Appearances>(obj)->_UpdateWithNewViewModel();
    }

    void Appearances::_UpdateWithNewViewModel()
    {
        if (Appearance())
        {
            const auto& biAlignmentVal{ static_cast<int32_t>(Appearance().BackgroundImageAlignment()) };
            for (const auto& biButton : _BIAlignmentButtons)
            {
                biButton.IsChecked(biButton.Tag().as<int32_t>() == biAlignmentVal);
            }

            FontAxesCVS().Source(Appearance().FontAxesVector());
            Appearance().AreFontAxesAvailable() ? FontAxesContainer().HelpText(RS_(L"Profile_FontAxesAvailable/Text")) : FontAxesContainer().HelpText(RS_(L"Profile_FontAxesUnavailable/Text"));

            FontFeaturesCVS().Source(Appearance().FontFeaturesVector());
            Appearance().AreFontFeaturesAvailable() ? FontFeaturesContainer().HelpText(RS_(L"Profile_FontFeaturesAvailable/Text")) : FontFeaturesContainer().HelpText(RS_(L"Profile_FontFeaturesUnavailable/Text"));

            _ViewModelChangedRevoker = Appearance().PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
                const auto settingName{ args.PropertyName() };
                if (settingName == L"CursorShape")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
                }
                else if (settingName == L"DarkColorSchemeName" || settingName == L"LightColorSchemeName")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
                }
                else if (settingName == L"BackgroundImageStretchMode")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentBackgroundImageStretchMode" });
                }
                else if (settingName == L"BackgroundImageAlignment")
                {
                    _UpdateBIAlignmentControl(static_cast<int32_t>(Appearance().BackgroundImageAlignment()));
                }
                else if (settingName == L"FontWeight")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontWeight" });
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
                }
                else if (settingName == L"FontFace" || settingName == L"CurrentFontList")
                {
                    // notify listener that all font face related values might have changed
                    if (!UsingMonospaceFont())
                    {
                        _ShowAllFonts = true;
                    }
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontFace" });
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"UsingMonospaceFont" });
                }
                else if (settingName == L"IntenseTextStyle")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentIntenseTextStyle" });
                }
                else if (settingName == L"AdjustIndistinguishableColors")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAdjustIndistinguishableColors" });
                }
                else if (settingName == L"ShowProportionalFontWarning")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowProportionalFontWarning" });
                }
                // YOU THERE ADDING A NEW APPEARANCE SETTING
                // Make sure you add a block like
                //
                //   else if (settingName == L"MyNewSetting")
                //   {
                //       _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentMyNewSetting" });
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
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentBackgroundImageStretchMode" });
            _UpdateBIAlignmentControl(static_cast<int32_t>(Appearance().BackgroundImageAlignment()));
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontWeight" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentFontFace" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"UsingMonospaceFont" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentIntenseTextStyle" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAdjustIndistinguishableColors" });
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"ShowProportionalFontWarning" });
        }
    }

    fire_and_forget Appearances::BackgroundImage_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            Appearance().BackgroundImagePath(file);
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

    void Appearances::DeleteAxisKeyValuePair_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        if (const auto& button{ sender.try_as<Controls::Button>() })
        {
            if (const auto& tag{ button.Tag().try_as<winrt::hstring>() })
            {
                Appearance().DeleteAxisKeyValuePair(tag.value());
            }
        }
    }

    void Appearances::AddNewAxisKeyValuePair_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        Appearance().AddNewAxisKeyValuePair();
    }

    void Appearances::DeleteFeatureKeyValuePair_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        if (const auto& button{ sender.try_as<Controls::Button>() })
        {
            if (const auto& tag{ button.Tag().try_as<hstring>() })
            {
                Appearance().DeleteFeatureKeyValuePair(tag.value());
            }
        }
    }

    void Appearances::AddNewFeatureKeyValuePair_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        Appearance().AddNewFeatureKeyValuePair();
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
            _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
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
