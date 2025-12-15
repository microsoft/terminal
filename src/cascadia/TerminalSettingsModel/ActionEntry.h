/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ActionEntry.h

Abstract:
- An action entry in the "new tab" dropdown menu

Author(s):
- Pankaj Bhojwani - May 2024

--*/
#pragma once

#include "NewTabMenuEntry.h"
#include "ActionEntry.g.h"
#include "MediaResourceSupport.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ActionEntry : ActionEntryT<ActionEntry, NewTabMenuEntry, IPathlessMediaResourceContainer>
    {
    public:
        ActionEntry() noexcept;

        Model::NewTabMenuEntry Copy() const;

        Json::Value ToJson() const override;
        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);

        void ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const Model::MediaResourceResolver& resolver) override;

        IMediaResource Icon() const noexcept
        {
            return _icon ? _icon : MediaResource::Empty();
        }

        void Icon(const IMediaResource& val)
        {
            _icon = val;
        }

        WINRT_PROPERTY(winrt::hstring, ActionId);

    private:
        IMediaResource _icon;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ActionEntry);
}
