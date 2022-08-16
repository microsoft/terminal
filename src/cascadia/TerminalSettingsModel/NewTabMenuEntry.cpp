#include "pch.h"
#include "NewTabMenuEntry.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"
#include "SeparatorEntry.h"
#include "FolderEntry.h"

#include "NewTabMenuEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static constexpr std::string_view TypeKey{ "type" };

// This is a map of NewTabMenuEntryType->function<Json::Value, NewTabMenuEntry>
static const std::unordered_map<NewTabMenuEntryType, std::function<winrt::com_ptr<implementation::NewTabMenuEntry>(const Json::Value&)>> typeDeserializerMap{
    { NewTabMenuEntryType::Separator, implementation::SeparatorEntry::FromJson },
    { NewTabMenuEntryType::Folder, implementation::FolderEntry::FromJson }
};

implementation::NewTabMenuEntry::NewTabMenuEntry(const NewTabMenuEntryType type) noexcept :
    _Type{ type }
{
}

Json::Value implementation::NewTabMenuEntry::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, TypeKey, _Type);

    return json;
}

winrt::com_ptr<implementation::NewTabMenuEntry> implementation::NewTabMenuEntry::FromJson(const Json::Value& json)
{
    auto type = JsonUtils::GetValueForKey<NewTabMenuEntryType>(json, TypeKey);
    const auto deserializer = typeDeserializerMap.find(type);
    if (deserializer == typeDeserializerMap.end() || !deserializer->second)
    {
        return nullptr;
    }

    return deserializer->second(json);
}
