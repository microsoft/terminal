#include "pch.h"
#include "Profiles.h"
#include "Profiles.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::AccessCache;
using namespace winrt::Windows::Storage::Pickers;

namespace winrt::SettingsControl::implementation
{
    Profiles::Profiles()
    {
        m_profileModel = winrt::make<ObjectModel::implementation::ProfileModel>();
        InitializeComponent();
    }

    Profiles::Profiles(ObjectModel::ProfileModel profile) :
        m_profileModel{ profile }
    {
        InitializeComponent();
    }

    ObjectModel::ProfileModel Profiles::ProfileModel()
    {
        return m_profileModel;
    }

    void Profiles::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
    }

    void Profiles::cursorColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //cursorColorPickerButton.Flyout.Hide();
    }

    void Profiles::cursorColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //cursorColorPickerButton.Flyout.Hide();
    }

    void Profiles::foregroundColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //foregroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::foregroundColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //foregroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::backgroundColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //backgroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::backgroundColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //backgroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::selectionBackgroundColorPickerConfirmColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //selectionBackgroundColorPickerButton.Flyout.Hide();
    }

    void Profiles::selectionBackgroundColorPickerCancelColor_Click(IInspectable const&, RoutedEventArgs const&)
    {
        //selectionBackgroundColorPickerButton.Flyout.Hide();
    }

    fire_and_forget Profiles::BackgroundImage_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        FileOpenPicker picker;

        picker.ViewMode(PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(PickerLocationId::PicturesLibrary);
        picker.FileTypeFilter().ReplaceAll({ L".jpg", L".jpeg", L".png", L".gif" });

        StorageFile file = co_await picker.PickSingleFileAsync();
        if (file != nullptr)
        {
            BackgroundImage().Text(file.Path());
        }
    }

    fire_and_forget Profiles::Commandline_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        FileOpenPicker picker;

        picker.ViewMode(PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(PickerLocationId::ComputerFolder);
        picker.FileTypeFilter().ReplaceAll({ L".bat" });

        StorageFile file = co_await picker.PickSingleFileAsync();
        if (file != nullptr)
        {
            Commandline().Text(file.Path());
        }
    }

    /*
    fire_and_forget Profiles::StartingDirectory_Click(IInspectable const&, RoutedEventArgs const&)
    {
        // This crashes on click, for some reason
        auto lifetime = get_strong();

        FolderPicker picker;
        picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);

        StorageFolder folder = co_await picker.PickSingleFolderAsync();
        if (folder != nullptr)
        {
            StorageApplicationPermissions::FutureAccessList().AddOrReplace(L"PickedFolderToken", folder);
            StartingDirectory().Text(folder.Path());
        }
    }
    */
}
