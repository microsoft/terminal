// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FuzzySearchPane.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    FuzzySearchPane::FuzzySearchPane(const winrt::Microsoft::Terminal::Control::TermControl& control)
    {
        _control = control;
        _root = winrt::Windows::UI::Xaml::Controls::Grid{};
        // Vertical and HorizontalAlignment are Stretch by default

        auto res = Windows::UI::Xaml::Application::Current().Resources();
        auto bg = res.Lookup(winrt::box_value(L"UnfocusedBorderBrush"));
        _root.Background(bg.try_as<Media::Brush>());

        const Controls::RowDefinition row1;
        row1.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        _root.RowDefinitions().Append(row1);

        const Controls::RowDefinition row2;
        row2.Height(GridLengthHelper::Auto());
        _root.RowDefinitions().Append(row2);

        _listBox = Controls::ListBox{};
        _listBox.Margin({ 10, 10, 10, 10 });
        _root.Children().Append(_listBox);

        _searchBox = Controls::TextBox{};
        _root.Children().Append(_searchBox);

        _searchBox.TextChanged({ this, &FuzzySearchPane::OnTextChanged });
        _searchBox.KeyDown({ this, &FuzzySearchPane::OnKeyUp });

        Controls::Grid::SetRow(_listBox, 0);
        Controls::Grid::SetRow(_searchBox, 1);
    }

    void FuzzySearchPane::OnKeyUp(Windows::Foundation::IInspectable const&, Input::KeyRoutedEventArgs const& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Down || e.OriginalKey() == winrt::Windows::System::VirtualKey::Up)
        {
            auto selectedIndex = _listBox.SelectedIndex();

            if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Down)
            {
                selectedIndex++;
            }
            else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Up)
            {
                selectedIndex--;
            }

            if (selectedIndex >= 0 && selectedIndex < static_cast<int32_t>(_listBox.Items().Size()))
            {
                _listBox.SelectedIndex(selectedIndex);
                _listBox.ScrollIntoView(Controls::ListBox().SelectedItem());
            }

            e.Handled(true);
        }
        else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            if (const auto selectedItem = _listBox.SelectedItem())
            {
                if (const auto listBoxItem = selectedItem.try_as<Controls::ListBoxItem>())
                {
                    if (const auto fuzzyMatch = listBoxItem.DataContext().try_as<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>())
                    {
                        _control.SelectChar(fuzzyMatch.FirstPosition());
                        _control.Focus(FocusState::Programmatic);
                        e.Handled(true);
                    }
                }
            }
        }
        else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Escape)
        {
            Close();
            e.Handled(true);
        }
    }

    void FuzzySearchPane::OnTextChanged(Windows::Foundation::IInspectable const&, Controls::TextChangedEventArgs const&) const
    {
        _listBox.Items().Clear();
        const auto needle = _searchBox.Text();
        const auto fuzzySearchResult = _control.FuzzySearch(needle);
        if (fuzzySearchResult.NumberOfResults() > 0)
        {
            for (auto fuzzyMatch : fuzzySearchResult.Results())
            {
                auto control = Controls::TextBlock{};
                const auto inlinesCollection = control.Inlines();
                inlinesCollection.Clear();

                for (const auto& match : fuzzyMatch.Segments())
                {
                    const auto matchText = match.TextSegment();
                    const auto fontWeight = match.IsHighlighted() ? Windows::UI::Text::FontWeights::Bold() : Windows::UI::Text::FontWeights::Normal();

                    Media::SolidColorBrush foregroundBrush;

                    if (match.IsHighlighted())
                    {
                        foregroundBrush.Color(Windows::UI::Colors::OrangeRed());
                    }
                    else
                    {
                        foregroundBrush.Color(Windows::UI::Colors::White());
                    }

                    Documents::Run run;
                    run.Text(matchText);
                    run.FontWeight(fontWeight);
                    run.Foreground(foregroundBrush);
                    inlinesCollection.Append(run);
                }
                auto lbi = Controls::ListBoxItem();
                lbi.DataContext(fuzzyMatch);
                lbi.Content(control);
                _listBox.Items().Append(lbi);
            }
            _listBox.SelectedIndex(0);
        }
    }

    void FuzzySearchPane::UpdateSettings(const CascadiaSettings& /*settings*/)
    {
        // Nothing to do.
    }

    winrt::Windows::UI::Xaml::FrameworkElement FuzzySearchPane::GetRoot()
    {
        return _root;
    }
    winrt::Windows::Foundation::Size FuzzySearchPane::MinimumSize()
    {
        return { 1, 1 };
    }
    void FuzzySearchPane::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        _searchBox.Focus(reason);
    }
    void FuzzySearchPane::Close()
    {
        CloseRequested.raise(*this, nullptr);
    }

    INewContentArgs FuzzySearchPane::GetNewTerminalArgs(const BuildStartupKind /* kind */) const
    {
        return BaseContentArgs(L"scratchpad");
    }

    winrt::hstring FuzzySearchPane::Icon() const
    {
        static constexpr std::wstring_view glyph{ L"\xe70b" }; // QuickNote
        return winrt::hstring{ glyph };
    }

    winrt::Windows::UI::Xaml::Media::Brush FuzzySearchPane::BackgroundBrush()
    {
        return _root.Background();
    }
}
