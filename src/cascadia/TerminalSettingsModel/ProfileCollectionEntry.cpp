// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ProfileCollectionEntry.h"
#include "JsonUtils.h"

#include "ProfileCollectionEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

ProfileCollectionEntry::ProfileCollectionEntry(const NewTabMenuEntryType type) noexcept :
    ProfileCollectionEntryT<ProfileCollectionEntry, NewTabMenuEntry>(type)
{
}
