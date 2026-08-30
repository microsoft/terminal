use terminal_settings::new_tab_menu::{NewTabMenuEntryType, NewTabMenuSettings};

#[test]
fn microsoft_new_tab_menu_defaults_to_remaining_profiles() {
    let settings = NewTabMenuSettings::from_user_settings_json("{}")
        .expect("empty settings object should use Microsoft's default menu");

    assert!(settings.warnings().is_empty());
    assert_eq!(settings.entries().len(), 1);
    assert_eq!(
        settings.entries()[0].entry_type(),
        NewTabMenuEntryType::RemainingProfiles
    );
}

#[test]
fn microsoft_new_tab_menu_parse_empty_folder() {
    let settings = NewTabMenuSettings::from_user_settings_json(
        r#"{
            "newTabMenu": [
                { "type": "folder" }
            ]
        }"#,
    )
    .expect("an empty folder entry is valid settings data");

    assert!(settings.warnings().is_empty());
    assert_eq!(settings.entries().len(), 1);
    assert_eq!(
        settings.entries()[0].entry_type(),
        NewTabMenuEntryType::Folder
    );
}
