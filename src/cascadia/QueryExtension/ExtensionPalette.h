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

        // We don't use the winrt_property macro here because we just need the setter
        void AIKeyAndEndpoint(const winrt::hstring& endpoint, const winrt::hstring& key);
        void IconPath(const winrt::hstring& iconPath);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ControlName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, QueryBoxPlaceholderText, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, IsProgressRingActive, _PropertyChangedHandlers, false);

        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ActiveCommandline, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ProfileName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Controls::IconElement, ResolvedIcon, _PropertyChangedHandlers, nullptr);

        TYPED_EVENT(ActiveControlInfoRequested, winrt::Microsoft::Terminal::Query::Extension::ExtensionPalette, Windows::Foundation::IInspectable);
        TYPED_EVENT(AIKeyAndEndpointRequested, winrt::Microsoft::Terminal::Query::Extension::ExtensionPalette, Windows::Foundation::IInspectable);
        TYPED_EVENT(InputSuggestionRequested, winrt::Microsoft::Terminal::Query::Extension::ExtensionPalette, winrt::hstring);

    private:
        friend struct ExtensionPaletteT<ExtensionPalette>; // for Xaml to bind events

        winrt::Windows::UI::Xaml::FrameworkElement::Loaded_revoker _loadedRevoker;

        // info/methods for the http requests
        winrt::hstring _AIEndpoint;
        winrt::hstring _AIKey;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };

        // chat history storage
        Windows::Foundation::Collections::IObservableVector<GroupedChatMessages> _messages{ nullptr };
        winrt::Windows::Data::Json::JsonArray _jsonMessages;

        winrt::fire_and_forget _getSuggestions(const winrt::hstring& prompt, const winrt::hstring& currentLocalTime);

        winrt::hstring _getCurrentLocalTimeHelper();
        void _splitResponseAndAddToChatHelper(const winrt::hstring& response);
        void _setFocusAndPlaceholderTextHelper();
        bool _verifyModelIsValidHelper(const Windows::Data::Json::JsonObject jsonResponse);

        void _clearAndInitializeMessages(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _listItemClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Controls::ItemClickEventArgs& e);
        void _rootPointerPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void _backdropPointerPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void _lostFocusHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _previewKeyDownHandler(const Windows::Foundation::IInspectable& sender,
                                    const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);

        void _close();
    };

    struct ChatMessage : ChatMessageT<ChatMessage>
    {
        ChatMessage(winrt::hstring content, bool isQuery, bool isCode) :
            _messageContent{ content },
            _isQuery{ isQuery },
            _isCode{ isCode } {}

        bool IsQuery() const { return _isQuery; };
        bool IsCode() const { return _isCode; };
        winrt::hstring MessageContent() const { return _messageContent; };

    private:
        bool _isQuery;
        bool _isCode;
        winrt::hstring _messageContent;
    };

    struct GroupedChatMessages : GroupedChatMessagesT<GroupedChatMessages>
    {
        GroupedChatMessages(winrt::hstring key, bool isQuery, winrt::hstring profileName, const Windows::Foundation::Collections::IVector<Windows::Foundation::IInspectable>& messages)
        {
            _Key = key;
            _isQuery = isQuery;
            _ProfileName = profileName;
            _messages = messages;
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

    private:
        bool _isQuery;
        Windows::Foundation::Collections::IVector<Windows::Foundation::IInspectable> _messages;
    };
}

namespace winrt::Microsoft::Terminal::Query::Extension::factory_implementation
{
    BASIC_FACTORY(ExtensionPalette);
    BASIC_FACTORY(ChatMessage);
    BASIC_FACTORY(GroupedChatMessages);
}
