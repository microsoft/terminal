/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- KeysMap.h

Abstract:
- A mapping of key chords to actions. Includes (de)serialization logic.

Author(s):
- Carlos Zamora - September 2020

--*/

#pragma once

#include "KeysMap.g.h"
#include "IInheritable.h"
#include "Command.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct KeyChordHash
    {
        inline std::size_t operator()(const Control::KeyChord& key) const
        {
            return static_cast<size_t>(key.Hash());
        }
    };

    struct KeyChordEquality
    {
        inline bool operator()(const Control::KeyChord& lhs, const Control::KeyChord& rhs) const
        {
            return lhs.Equals(rhs);
        }
    };

    struct KeysMap : KeysMapT<KeysMap>, IInheritable<KeysMap>
    {
        void Thing();
        void Blah();
        void Ayy();

        std::vector<SettingsLoadWarnings> LayerJson(const Json::Value& json, const OriginTag origin, const bool withKeybindings = true);

    private:
        std::unordered_map<Control::KeyChord, winrt::hstring, KeyChordHash, KeyChordEquality> _KeyMap;
    };
}
