/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingsWriteNotifier.h

Abstract:
- The late-bound "request auto-save" sink shared by every settings-model object
  in the live tree.

Author(s):
- Carlos Zamora - June 2026

--*/
#pragma once

#include <memory>
#include <functional>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct SettingsWriteNotifier
    {
        void Notify() const
        {
            if (_handler)
            {
                _handler();
            }
        }

        void SetHandler(std::function<void()> handler)
        {
            _handler = std::move(handler);
        }

    private:
        std::function<void()> _handler;
    };

    struct WriteNotifiable
    {
    public:
        using WriteSettingsSink = std::shared_ptr<SettingsWriteNotifier>;
        void SetWriteSettingsSink(const WriteSettingsSink& sink)
        {
            _writeSettingsSink = sink;
        }

        void NotifyWriteSettings() const
        {
            if (_writeSettingsSink)
            {
                _writeSettingsSink->Notify();
            }
        }

    protected:
        WriteSettingsSink _writeSettingsSink{};
    };
}
