#include "pch.h"
#include "SeparatorEntry.h"
#include "JsonUtils.h"

#include "SeparatorEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

implementation::SeparatorEntry::SeparatorEntry() noexcept :
    SeparatorEntryT<implementation::SeparatorEntry, implementation::NewTabMenuEntry>(NewTabMenuEntryType::Separator)
{
}

Json::Value implementation::SeparatorEntry::ToJson() const
{
    auto json = implementation::NewTabMenuEntry::ToJson();
    return json;
}

winrt::com_ptr<implementation::NewTabMenuEntry> implementation::SeparatorEntry::FromJson(const Json::Value&)
{
    return winrt::make_self<implementation::SeparatorEntry>();
}
