// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../NativeSerialization.h"
#include "Profile.h"
#include "AppearanceConfig.h"
#include "FontConfig.h"
#include "PropertyDefinitions.h"
#include "MediaResourceResolver.h"
#include "../../../types/inc/Utils.hpp"
#include <DefaultSettings.h>
#include <shellapi.h>

namespace Microsoft::Terminal::Settings::Model::Native
{
    static constexpr GUID RuntimeGeneratedProfileNamespace{ 0xf65ddb7e, 0x706b, 0x4499, { 0x8a, 0x50, 0x40, 0x31, 0x3c, 0xaf, 0x51, 0x0a } };
    static constexpr std::string_view UpdatesKey{ "updates" };
    static constexpr std::string_view NameKey{ "name" };
    static constexpr std::string_view GuidKey{ "guid" };
    static constexpr std::string_view SourceKey{ "source" };
    static constexpr std::string_view HiddenKey{ "hidden" };
    static constexpr std::string_view FontInfoKey{ "font" };
    static constexpr std::string_view PaddingKey{ "padding" };
    static constexpr std::string_view TabColorKey{ "tabColor" };
    static constexpr std::string_view UnfocusedAppearanceKey{ "unfocusedAppearance" };

    TSM_NATIVE_LIFETIME_DEFINITIONS(Profile)
    TSM_NATIVE_PROPERTY_DEFINITIONS(Profile, std::optional<Native::Color>, TabColor, false, "")
    TSM_NATIVE_PROPERTY_DEFINITIONS(Profile, winrt::com_ptr<Native::AppearanceConfig>, UnfocusedAppearance, false, "")
    TSM_NATIVE_PROPERTY_DEFINITIONS(Profile, winrt::hstring, Name, false, "", L"Default")
    TSM_NATIVE_PROPERTY_DEFINITIONS(Profile, winrt::hstring, Source, false, "")
    TSM_NATIVE_PROPERTY_DEFINITIONS(Profile, bool, Hidden, false, "", false)
    TSM_NATIVE_PROPERTY_DEFINITIONS(Profile, winrt::guid, Guid, false, "", _GenerateGuidForProfile(Name(), Source()))
    TSM_NATIVE_PROPERTY_DEFINITIONS(Profile, winrt::hstring, Padding, false, "", DEFAULT_PADDING)

#define DEFINE_NATIVE_PROFILE_PROPERTY(type, name, jsonKey, ...) \
    TSM_NATIVE_PROPERTY_DEFINITIONS(Profile, type, name, true, jsonKey, __VA_ARGS__)
    NATIVE_PROFILE_SETTINGS(DEFINE_NATIVE_PROFILE_PROPERTY)
#undef DEFINE_NATIVE_PROFILE_PROPERTY

    Profile::Profile(ConstructionToken) :
        _DefaultAppearance{ AppearanceConfig::Create(get_weak()) },
        _FontInfo{ FontConfig::Create(get_weak()) }
    {
    }

    Profile::~Profile() noexcept = default;

    winrt::com_ptr<Profile> Profile::Create()
    {
        return winrt::make_self<Profile>(ConstructionToken{});
    }

    winrt::com_ptr<Profile> Profile::Create(const winrt::guid guid)
    {
        auto profile = Create();
        profile->_Guid = guid;
        return profile;
    }

    winrt::com_ptr<Profile> Profile::CreateChild() const
    {
        auto child = Create();
        child->AddLeastImportantParent(const_cast<Profile*>(this)->get_strong());
        child->FinalizeInheritance();
        return child;
    }

    void Profile::ClearParents()
    {
        _parents.clear();
    }

    void Profile::AddLeastImportantParent(winrt::com_ptr<Profile> parent)
    {
        _parents.emplace_back(std::move(parent));
    }

    void Profile::AddMostImportantParent(winrt::com_ptr<Profile> parent)
    {
        _parents.emplace(_parents.begin(), std::move(parent));
    }

    std::span<const winrt::com_ptr<Profile>> Profile::Parents() const noexcept
    {
        return _parents;
    }

    void Profile::CreateUnfocusedAppearance()
    {
        if (!_UnfocusedAppearance)
        {
            auto appearance = AppearanceConfig::Create(get_weak());
            appearance->AddLeastImportantParent(_DefaultAppearance);
            _UnfocusedAppearance = std::move(appearance);
        }
    }

    void Profile::DeleteUnfocusedAppearance()
    {
        _UnfocusedAppearance = std::nullopt;
    }

    winrt::com_ptr<AppearanceConfig> Profile::DefaultAppearance() const
    {
        return _DefaultAppearance;
    }

    winrt::com_ptr<FontConfig> Profile::FontInfo() const
    {
        return _FontInfo;
    }

    void Profile::CopyInheritanceGraphs(std::unordered_map<const Profile*, winrt::com_ptr<Profile>>& visited, const std::vector<winrt::com_ptr<Profile>>& source, std::vector<winrt::com_ptr<Profile>>& target)
    {
        for (const auto& profile : source)
        {
            target.emplace_back(profile->CopyInheritanceGraph(visited));
        }
    }

    winrt::com_ptr<Profile>& Profile::CopyInheritanceGraph(std::unordered_map<const Profile*, winrt::com_ptr<Profile>>& visited) const
    {
        auto& clone = visited[this];
        if (!clone)
        {
            clone = CopySettings();
            CopyInheritanceGraphs(visited, _parents, clone->_parents);
            clone->FinalizeInheritance();
        }
        return clone;
    }

    winrt::com_ptr<Profile> Profile::CopySettings() const
    {
        const auto profile = Create();
        const auto weakProfile = profile->get_weak();
        const auto fontInfo = FontConfig::CopyFontInfo(_FontInfo.get(), weakProfile);
        const auto defaultAppearance = AppearanceConfig::CopyAppearance(_DefaultAppearance.get(), weakProfile);

        profile->_Deleted = _Deleted;
        profile->_Orphaned = _Orphaned;
        profile->_Updates = _Updates;
        profile->_Guid = _Guid;
        profile->_Name = _Name;
        profile->_Source = _Source;
        profile->_Hidden = _Hidden;
        profile->_TabColor = _TabColor;
        profile->_Padding = _Padding;
        profile->_Origin = _Origin;
        profile->_FontInfo = fontInfo;
        profile->_DefaultAppearance = defaultAppearance;

#define COPY_NATIVE_PROFILE_PROPERTY(type, name, ...) profile->_##name = _##name;
        NATIVE_PROFILE_SETTINGS(COPY_NATIVE_PROFILE_PROPERTY)
#undef COPY_NATIVE_PROFILE_PROPERTY

        if (_BellSound && *_BellSound)
        {
            std::vector<winrt::com_ptr<MediaResource>> resources((*_BellSound)->Size());
            (*_BellSound)->GetMany(0, resources);
            profile->_BellSound = MakeMediaResourceList(std::move(resources));
        }
        if (_UnfocusedAppearance)
        {
            winrt::com_ptr<AppearanceConfig> unfocused;
            if (*_UnfocusedAppearance)
            {
                unfocused = AppearanceConfig::CopyAppearance(_UnfocusedAppearance->get(), weakProfile);
                unfocused->AddLeastImportantParent(defaultAppearance);
            }
            profile->_UnfocusedAppearance = std::move(unfocused);
        }
        return profile;
    }

    winrt::com_ptr<Profile> Profile::FromJson(const Json::Value& json)
    {
        auto profile = Create();
        profile->LayerJson(json);
        return profile;
    }

    void Profile::LayerJson(const Json::Value& json)
    {
        _DefaultAppearance->LayerJson(json);
        _FontInfo->LayerJson(json);
        JsonUtils::GetValueForKey(json, NameKey, _Name);
        JsonUtils::GetValueForKey(json, UpdatesKey, _Updates);
        JsonUtils::GetValueForKey(json, GuidKey, _Guid);
        JsonUtils::GetValueForKey(json, SourceKey, _Source);
        JsonUtils::GetValueForKey(json, HiddenKey, _Hidden);
        _logSettingIfSet(HiddenKey, _Hidden.has_value());
        JsonUtils::GetValueForKey(json, PaddingKey, _Padding, JsonUtils::OptionalConverter<winrt::hstring, JsonUtils::PermissiveStringConverter<std::wstring>>{});
        _logSettingIfSet(PaddingKey, _Padding.has_value());
        JsonUtils::GetValueForKey(json, TabColorKey, _TabColor);
        _logSettingIfSet(TabColorKey, _TabColor.has_value());
        JsonUtils::GetValueForKey(json, "experimental.showMarks", _ShowMarks);
        JsonUtils::GetValueForKey(json, "experimental.autoMarkPrompts", _AutoMarkPrompts);
        JsonUtils::GetValueForKey(json, "experimental.rightClickContextMenu", _RightClickContextMenu);

#define PARSE_NATIVE_PROFILE_PROPERTY(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(json, jsonKey, _##name);          \
    _logSettingIfSet(jsonKey, _##name.has_value());
        NATIVE_PROFILE_SETTINGS(PARSE_NATIVE_PROFILE_PROPERTY)
#undef PARSE_NATIVE_PROFILE_PROPERTY

        if (json.isMember(JsonKey(UnfocusedAppearanceKey)))
        {
            auto appearance = AppearanceConfig::Create(get_weak());
            appearance->AddLeastImportantParent(_DefaultAppearance);
            appearance->LayerJson(json[JsonKey(UnfocusedAppearanceKey)]);
            _UnfocusedAppearance = std::move(appearance);
            _logSettingSet(UnfocusedAppearanceKey);
        }
    }

    void Profile::FinalizeInheritance()
    {
        _DefaultAppearance->ClearParents();
        _FontInfo->ClearParents();
        for (const auto& parent : _parents)
        {
            _DefaultAppearance->AddLeastImportantParent(parent->_DefaultAppearance);
            _FontInfo->AddLeastImportantParent(parent->_FontInfo);
        }
    }

    Json::Value Profile::ToJson() const
    {
        auto json = _DefaultAppearance->ToJson();
        const auto writeBasicSettings = !Source().empty();
        JsonUtils::SetValueForKey(json, NameKey, writeBasicSettings ? Name() : _Name);
        JsonUtils::SetValueForKey(json, GuidKey, writeBasicSettings ? Guid() : _Guid);
        JsonUtils::SetValueForKey(json, HiddenKey, writeBasicSettings ? Hidden() : _Hidden);
        JsonUtils::SetValueForKey(json, SourceKey, writeBasicSettings ? Source() : _Source);
        JsonUtils::SetValueForKey(json, PaddingKey, _Padding);
        JsonUtils::SetValueForKey(json, TabColorKey, _TabColor);

#define SERIALIZE_NATIVE_PROFILE_PROPERTY(type, name, jsonKey, ...) JsonUtils::SetValueForKey(json, jsonKey, _##name);
        NATIVE_PROFILE_SETTINGS(SERIALIZE_NATIVE_PROFILE_PROPERTY)
#undef SERIALIZE_NATIVE_PROFILE_PROPERTY

        if (auto font = _FontInfo->ToJson(); !font.empty())
        {
            json[JsonKey(FontInfoKey)] = std::move(font);
        }
        if (_UnfocusedAppearance)
        {
            json[JsonKey(UnfocusedAppearanceKey)] = *_UnfocusedAppearance ? (*_UnfocusedAppearance)->ToJson() : Json::Value{ Json::nullValue };
        }
        return json;
    }

    winrt::hstring Profile::EvaluatedStartingDirectory() const
    {
        const auto path = StartingDirectory();
        return path.empty() ? path : winrt::hstring{ EvaluateStartingDirectory(path.c_str()) };
    }

    std::wstring Profile::EvaluateStartingDirectory(const std::wstring& directory)
    {
        return wil::ExpandEnvironmentStringsW<std::wstring>(directory.c_str());
    }

    winrt::guid Profile::_GenerateGuidForProfile(const std::wstring_view& name, const std::wstring_view& source) noexcept
    {
        const auto namespaceGuid = !source.empty() ?
            ::Microsoft::Console::Utils::CreateV5Uuid(RuntimeGeneratedProfileNamespace, std::as_bytes(std::span{ source })) :
            RuntimeGeneratedProfileNamespace;
        return { ::Microsoft::Console::Utils::CreateV5Uuid(namespaceGuid, std::as_bytes(std::span{ name })) };
    }

    std::wstring Profile::NormalizeCommandLine(const wchar_t* commandLine)
    {
        std::wstring normalized;
        THROW_IF_FAILED(wil::ExpandEnvironmentStringsW(commandLine, normalized));
        auto argc = 0;
        wil::unique_hlocal_ptr<PWSTR[]> argv{ CommandLineToArgvW(normalized.c_str(), &argc) };
        THROW_LAST_ERROR_IF(!argc);
        auto startOfArguments = 1;
        for (;;)
        {
            const auto status = wil::SearchPathW(nullptr, argv[0], L".exe", normalized);
            if (status == S_OK)
            {
                const auto attributes = GetFileAttributesW(normalized.c_str());
                if (attributes != INVALID_FILE_ATTRIBUTES && WI_IsFlagClear(attributes, FILE_ATTRIBUTE_DIRECTORY))
                {
                    std::filesystem::path path{ std::move(normalized) };
                    std::error_code error;
                    auto canonicalPath = std::filesystem::canonical(path, error);
                    if (!error)
                    {
                        path = std::move(canonicalPath);
                    }
                    normalized = std::move(const_cast<std::wstring&>(path.native()));
                    break;
                }
            }
            else if (status != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                break;
            }
            if ((argc - startOfArguments) < 2)
            {
                break;
            }
            argv[startOfArguments][-1] = L' ';
            ++startOfArguments;
        }
        if (argc > startOfArguments)
        {
            const auto begin = argv[startOfArguments] - 1;
            const auto lastArg = argv[argc - 1];
            const auto end = lastArg + wcslen(lastArg);
            normalized.append(begin, end);
        }
        return normalized;
    }

    void Profile::_logSettingSet(const std::string_view& setting)
    {
        _changeLog.emplace(setting);
    }

    void Profile::_logSettingIfSet(const std::string_view& setting, const bool isSet)
    {
        if (isSet)
        {
            static constexpr winrt::guid WindowsPowerShellGuid{ 0x61c54bbd, 0xc2c6, 0x5271, { 0x96, 0xe7, 0x00, 0x9a, 0x87, 0xff, 0x44, 0xbf } };
            static constexpr winrt::guid CommandPromptGuid{ 0x0caa0dad, 0x35be, 0x5f56, { 0xa8, 0xff, 0xaf, 0xce, 0xee, 0xaa, 0x61, 0x01 } };
            const auto isWinPow = _Guid.has_value() && *_Guid == WindowsPowerShellGuid;
            const auto isCmd = _Guid.has_value() && *_Guid == CommandPromptGuid;
            const auto isACS = _Name.has_value() && til::equals_insensitive_ascii(*_Name, L"Azure Cloud Shell");
            const auto isDynamic = _Source.has_value() && til::starts_with(*_Source, L"Windows.Terminal");
            const auto hidingFalse = til::equals_insensitive_ascii(setting, HiddenKey) && _Hidden.has_value() && _Hidden == false;
            const auto winPowCommandline = til::equals_insensitive_ascii(setting, "commandline") && _Commandline.has_value() && til::equals_insensitive_ascii(*_Commandline, L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
            const auto cmdCommandline = til::equals_insensitive_ascii(setting, "commandline") && _Commandline.has_value() && til::equals_insensitive_ascii(*_Commandline, L"%SystemRoot%\\System32\\cmd.exe");
            if (!(isWinPow && (hidingFalse || winPowCommandline)) &&
                !(isCmd && (hidingFalse || cmdCommandline)) &&
                !(isACS && hidingFalse) &&
                !(isDynamic && hidingFalse))
            {
                _logSettingSet(setting);
            }
        }
    }

    void Profile::ResolveMediaResources(const MediaResourceResolver& resolver)
    {
        if (const auto icon = _getIconImpl(); icon && *icon)
        {
            const auto source = _getIconOverrideSourceImpl();
            ResolveIconMediaResource(source->_Origin, source->_SourceBasePath, *icon, resolver);
            if (!(*icon)->Ok() || ((*icon)->Resolved().empty() && !source->Commandline().empty()))
            {
                auto replacement = MediaResource::FromString((*icon)->Path());
                const auto commandline = NormalizeCommandLine(source->Commandline().c_str());
                replacement->Resolve(commandline.c_str());
                source->_Icon = std::move(replacement);
            }
        }
        _DefaultAppearance->ResolveMediaResources(resolver);
        if (const auto appearance = UnfocusedAppearance())
        {
            appearance->ResolveMediaResources(resolver);
        }
        if (const auto [source, resources] = _getBellSoundOverrideSourceAndValueImpl(); source && resources && *resources)
        {
            for (auto iterator = (*resources)->First(); iterator->HasCurrent(); iterator->MoveNext())
            {
                ResolveMediaResource(source->_Origin, source->_SourceBasePath, iterator->Current(), resolver);
            }
        }
    }

    void Profile::LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const
    {
        for (const auto& setting : _changeLog)
        {
            changes.emplace(fmt::format(FMT_COMPILE("{}.{}"), context, setting));
        }
        const auto fontContext = fmt::format(FMT_COMPILE("{}.{}"), context, FontInfoKey);
        _FontInfo->LogSettingChanges(changes, fontContext);
        const auto appearanceContext = fmt::format(FMT_COMPILE("{}.{}"), context, "appearance");
        _DefaultAppearance->LogSettingChanges(changes, appearanceContext);
        if (_UnfocusedAppearance && *_UnfocusedAppearance)
        {
            (*_UnfocusedAppearance)->LogSettingChanges(changes, appearanceContext);
        }
    }

    bool Profile::Deleted() const noexcept { return _Deleted; }
    void Profile::Deleted(const bool value) noexcept { _Deleted = value; }
    bool Profile::Orphaned() const noexcept { return _Orphaned; }
    void Profile::Orphaned(const bool value) noexcept { _Orphaned = value; }
    OriginTag Profile::Origin() const noexcept { return _Origin; }
    void Profile::Origin(const OriginTag value) noexcept { _Origin = value; }
    winrt::guid Profile::Updates() const noexcept { return _Updates; }
    void Profile::Updates(const winrt::guid value) noexcept { _Updates = value; }
    winrt::hstring Profile::SourceBasePath() const { return _SourceBasePath; }
    void Profile::SourceBasePath(const winrt::hstring& value) { _SourceBasePath = value; }
    void Profile::Icon(const winrt::hstring& path) { Icon(MediaResource::FromString(path)); }
}
