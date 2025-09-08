// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ExtensionPalette.g.h"
#include "ChatMessage.g.h"
#include "GroupedChatMessages.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct ExtensionPalette : ExtensionPaletteT<ExtensionPalette>
    {
        ExtensionPalette();
        void SetProvider(const Extension::ILMProvider lmProvider);
        bool ProviderExists() const noexcept;

        // We don't use the winrt_property macro here because we just need the setter
        void IconPath(const winrt::hstring& iconPath);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ControlName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, QueryBoxPlaceholderText, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, IsProgressRingActive, _PropertyChangedHandlers, false);

        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ActiveCommandline, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ProfileName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Controls::IconElement, ResolvedIcon, _PropertyChangedHandlers, nullptr);

        TYPED_EVENT(ActiveControlInfoRequested, winrt::Microsoft::Terminal::Query::Extension::ExtensionPalette, Windows::Foundation::IInspectable);
        TYPED_EVENT(InputSuggestionRequested, winrt::Microsoft::Terminal::Query::Extension::ExtensionPalette, winrt::hstring);
        TYPED_EVENT(ExportChatHistoryRequested, winrt::Microsoft::Terminal::Query::Extension::ExtensionPalette, winrt::hstring);
        TYPED_EVENT(SetUpProviderInSettingsRequested, winrt::Microsoft::Terminal::Query::Extension::ExtensionPalette, Windows::Foundation::IInspectable);

    private:
        friend struct ExtensionPaletteT<ExtensionPalette>; // for Xaml to bind events

        winrt::Windows::UI::Xaml::FrameworkElement::Loaded_revoker _loadedRevoker;

        ILMProvider _lmProvider{ nullptr };

        // chat history storage
        Windows::Foundation::Collections::IObservableVector<GroupedChatMessages> _messages{ nullptr };

        winrt::fire_and_forget _getSuggestions(const winrt::hstring& prompt, const winrt::hstring& currentLocalTime);

        winrt::hstring _getCurrentLocalTimeHelper();
        void _splitResponseAndAddToChatHelper(const winrt::Microsoft::Terminal::Query::Extension::IResponse response);
        void _setFocusAndPlaceholderTextHelper();

        void _clearAndInitializeMessages(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _exportMessagesToFile(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _closeChat(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _backdropPointerPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void _queryBoxGotFocusHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _previewKeyDownHandler(const Windows::Foundation::IInspectable& sender,
                                    const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _setUpAIProviderInSettings(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
    };

    struct ChatMessage : ChatMessageT<ChatMessage>
    {
        ChatMessage(winrt::hstring content, bool isQuery);

        bool IsQuery() const { return _isQuery; };
        winrt::hstring MessageContent() const { return _messageContent; };
        winrt::Windows::UI::Xaml::Controls::RichTextBlock RichBlock() const { return _richBlock; };

        TYPED_EVENT(RunCommandClicked, winrt::Microsoft::Terminal::Query::Extension::ChatMessage, winrt::hstring);

    private:
        bool _isQuery;
        winrt::hstring _messageContent;
        Windows::UI::Xaml::Controls::RichTextBlock _richBlock;
    };

    struct GroupedChatMessages : GroupedChatMessagesT<GroupedChatMessages>
    {
        GroupedChatMessages(winrt::hstring key,
                            bool isQuery,
                            const Windows::Foundation::Collections::IVector<Windows::Foundation::IInspectable>& messages,
                            winrt::hstring attribution = winrt::hstring{},
                            winrt::hstring badgeImagePath = winrt::hstring{})
        {
            _Key = key;
            _isQuery = isQuery;
            _messages = messages;
            _Attribution = attribution;

            if (!badgeImagePath.empty())
            {
                Windows::Foundation::Uri badgeImageSourceUri{ badgeImagePath };
                _BadgeBitmapImage = winrt::Windows::UI::Xaml::Media::Imaging::BitmapImage{ badgeImageSourceUri };
            }
        }
        winrt::Windows::Foundation::Collections::IIterator<winrt::Windows::Foundation::IInspectable> First()
        {
            return _messages.First();
        };
        winrt::Windows::Foundation::IInspectable GetAt(uint32_t index)
        {
            return _messages.GetAt(index);
        };
        uint32_t Size()
        {
            return _messages.Size();
        };
        winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable> GetView()
        {
            return _messages.GetView();
        };
        bool IndexOf(winrt::Windows::Foundation::IInspectable const& value, uint32_t& index)
        {
            return _messages.IndexOf(value, index);
        };
        void SetAt(uint32_t index, winrt::Windows::Foundation::IInspectable const& value)
        {
            _messages.SetAt(index, value);
        };
        void InsertAt(uint32_t index, winrt::Windows::Foundation::IInspectable const& value)
        {
            _messages.InsertAt(index, value);
        };
        void RemoveAt(uint32_t index)
        {
            _messages.RemoveAt(index);
        };
        void Append(winrt::Windows::Foundation::IInspectable const& value)
        {
            _messages.Append(value);
        };
        void RemoveAtEnd()
        {
            _messages.RemoveAtEnd();
        };
        void Clear()
        {
            _messages.Clear();
        };
        uint32_t GetMany(uint32_t startIndex, array_view<winrt::Windows::Foundation::IInspectable> items)
        {
            return _messages.GetMany(startIndex, items);
        };
        void ReplaceAll(array_view<winrt::Windows::Foundation::IInspectable const> items)
        {
            _messages.ReplaceAll(items);
        };

        bool IsQuery() const { return _isQuery; };
        WINRT_PROPERTY(winrt::hstring, Key);
        WINRT_PROPERTY(winrt::hstring, ProfileName);
        WINRT_PROPERTY(winrt::hstring, Attribution);
        WINRT_PROPERTY(winrt::Windows::UI::Xaml::Media::Imaging::BitmapImage, BadgeBitmapImage, nullptr);

    private:
        bool _isQuery;
        Windows::Foundation::Collections::IVector<Windows::Foundation::IInspectable> _messages;
    };

    struct TerminalContext : public winrt::implements<TerminalContext, winrt::Microsoft::Terminal::Query::Extension::IContext>
    {
        TerminalContext(const winrt::hstring& activeCommandline) :
            ActiveCommandline{ activeCommandline } {}

        til::property<winrt::hstring> ActiveCommandline;
    };

    struct SystemResponse : public winrt::implements<SystemResponse, winrt::Microsoft::Terminal::Query::Extension::IResponse>
    {
        SystemResponse(const winrt::hstring& message, const winrt::Microsoft::Terminal::Query::Extension::ErrorTypes errorType, const winrt::hstring& responseAttribution) :
            Message{ message },
            ErrorType{ errorType },
            ResponseAttribution{ responseAttribution } {}

        til::property<winrt::hstring> Message;
        til::property<winrt::Microsoft::Terminal::Query::Extension::ErrorTypes> ErrorType;
        til::property<winrt::hstring> ResponseAttribution;
    };
}

namespace winrt::Microsoft::Terminal::Query::Extension::factory_implementation
{
    BASIC_FACTORY(ExtensionPalette);
    BASIC_FACTORY(ChatMessage);
    BASIC_FACTORY(GroupedChatMessages);
}
