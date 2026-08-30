use std::collections::BTreeMap;

use terminal_settings::media_resource::{
    MediaFragment, MediaOrigin, MediaPathResolution, MediaPlatform, MediaResourceSettings,
    MediaResourceState, resolve_media_path,
};

const USER_BASE: &str = r"C:\Windows";
const FRAGMENT_BASE: &str = r"C:\Windows\Media";
const CMD: &str = r"C:\Windows\System32\cmd.exe";
const CSCRIPT: &str = r"C:\Windows\System32\cscript.exe";
const BASE_GUID: &str = "{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}";

const INBOX: &str = r#"{
  "actions":[{"command":"closeWindow","icon":"fakeCommandIconPath","id":"Terminal.CloseWindow"}],
  "profiles":{"list":[
    {"backgroundImage":"imagePathFromBase","guid":"{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}","icon":"iconFromBase","name":"Base","bellSound":["C:\\Windows\\Media\\Alarm01.wav","C:\\Windows\\Media\\Alarm02.wav"]},
    {"backgroundImage":"focusedImagePathFromBase","experimental.pixelShaderPath":"focusedPixelShaderPathFromBase","experimental.pixelShaderImagePath":"focusedPixelShaderImagePathFromBase","unfocusedAppearance":{"backgroundImage":"unfocusedImagePathFromBase","experimental.pixelShaderPath":"unfocusedPixelShaderPathFromBase","experimental.pixelShaderImagePath":"unfocusedPixelShaderImagePathFromBase"},"guid":"{84f3d5cc-ecd9-49a9-96be-8bced39d4290}","name":"BaseFullyLoaded"}
  ]}
}"#;

fn settings(user: &str, fragments: &[MediaFragment<'_>]) -> MediaResourceSettings {
    MediaResourceSettings::from_layers(INBOX, user, fragments, USER_BASE).unwrap()
}

fn reject_all(settings: &mut MediaResourceSettings) {
    settings.resolve_media_resources(|_, _, resource| resource.reject());
}

fn resolve_all(settings: &mut MediaResourceSettings, value: &str) {
    settings.resolve_media_resources(|_, _, resource| resource.resolve(value));
}

#[test]
fn microsoft_media_resource_validate_resolver_called_for_inbox_contract() {
    let mut settings = settings("{}", &[]);
    let mut calls = 0;
    settings.resolve_media_resources(|origin, _, resource| {
        assert_eq!(origin, MediaOrigin::InBox);
        calls += 1;
        resource.resolve("resolved");
    });
    assert_eq!(calls, 11);
    assert_eq!(settings.profile_icon("Base").unwrap().resolved, "resolved");
    assert_eq!(
        settings.profile_background("Base", false).unwrap().resolved,
        "resolved"
    );
}

#[test]
fn microsoft_media_resource_validate_resolver_called_for_inbox_and_user_contract() {
    let mut settings = settings(
        r#"{"profiles":{"defaults":{"icon":"iconFromDefaults"},"list":[{"guid":"{2cdb0be2-f601-4f70-9a6c-3472b3257883}","icon":"iconFromUser","name":"UserProfile1"}]},"actions":[{"command":{"action":"sendInput","input":"IT CAME FROM BEYOND THE STARS"},"icon":null,"id":"Terminal.CloseWindow"}]}"#,
        &[],
    );
    let mut origins = BTreeMap::new();
    settings.resolve_media_resources(|origin, base_path, resource| {
        if matches!(origin, MediaOrigin::User | MediaOrigin::ProfilesDefaults) {
            assert!(!base_path.is_empty());
        }
        *origins.entry(origin).or_insert(0usize) += 1;
        resource.resolve("resolved");
    });
    assert_eq!(origins.get(&MediaOrigin::InBox), Some(&10));
    assert_eq!(origins.get(&MediaOrigin::ProfilesDefaults), Some(&1));
    assert_eq!(origins.get(&MediaOrigin::User), Some(&1));
    let base = settings.profile_icon("Base").unwrap();
    assert_eq!(base.path, "iconFromDefaults");
    assert_eq!(base.resolved, "resolved");
    let user = settings.profile_icon("UserProfile1").unwrap();
    assert_eq!(user.path, "iconFromUser");
    assert_eq!(user.resolved, "resolved");
}

#[test]
fn microsoft_media_resource_validate_resolver_called_for_fragments_contract() {
    let fragment = MediaFragment {
        source: "fragment",
        base_path: FRAGMENT_BASE,
        content: r#"{"profiles":[{"guid":"{4e7c2b36-642f-4694-83f8-8a5052038a23}","name":"FragmentProfile","commandline":"not_a_real_path","icon":"DoesNotMatterIgnoredByMockResolver"}],"actions":[{"command":{"action":"sendInput","input":"SOME DAY SOMETHING'S COMING"},"icon":"foo.ico","id":"Dustin.SendInput"}]}"#,
    };
    let mut settings = settings("{}", &[fragment]);
    let mut origins = BTreeMap::new();
    settings.resolve_media_resources(|origin, base_path, resource| {
        if origin == MediaOrigin::Fragment {
            assert_eq!(base_path, FRAGMENT_BASE);
        }
        *origins.entry(origin).or_insert(0usize) += 1;
        resource.resolve("resolved");
    });
    assert_eq!(origins.get(&MediaOrigin::Fragment), Some(&2));
    assert_eq!(
        settings.profile_icon("FragmentProfile").unwrap().resolved,
        "resolved"
    );
}

#[test]
fn microsoft_media_resource_validate_resolver_called_for_new_tab_menu_entries_contract() {
    let mut settings = settings(
        r#"{"newTabMenu":[{"icon":"menuItemIcon1","id":"Terminal.CloseWindow","type":"action"},{"icon":"menuItemIcon2","profile":"{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}","type":"profile"},{"allowEmpty":true,"entries":[{"icon":"menuItemIcon4","profile":"{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}","type":"profile"},{"allowEmpty":true,"entries":[{"icon":"menuItemIcon6","profile":"{862d46aa-cc9c-4e6c-b872-9cadaafcdbbe}","type":"profile"}],"icon":"menuItemIcon5","inline":"never","name":"Or was it...?","type":"folder"}],"icon":"menuItemIcon3","inline":"never","name":"Lovecraft in Brooklyn","type":"folder"}]}"#,
        &[],
    );
    let mut origins = BTreeMap::new();
    settings.resolve_media_resources(|origin, _, resource| {
        *origins.entry(origin).or_insert(0usize) += 1;
        resource.resolve("resolved");
    });
    assert_eq!(origins.get(&MediaOrigin::InBox), Some(&11));
    assert_eq!(origins.get(&MediaOrigin::User), Some(&6));
}

#[test]
fn microsoft_media_resource_validate_resolver_called_incrementally_on_change_contract() {
    let mut settings = settings(
        r#"{"profiles":{"defaults":{"icon":"iconFromDefaults"},"list":[{"guid":"{2cdb0be2-f601-4f70-9a6c-3472b3257883}","icon":"iconFromUser","name":"UserProfile1"}]}}"#,
        &[],
    );
    resolve_all(&mut settings, "resolved");
    assert!(settings.set_profile_icon("Base", "NewIconFromRuntime"));
    let pending = settings.profile_icon("Base").unwrap();
    assert!(!pending.ok);
    assert_eq!(pending.state, MediaResourceState::Pending);
    let mut calls = 0;
    settings.resolve_media_resources(|origin, _, resource| {
        assert_eq!(origin, MediaOrigin::User);
        calls += 1;
        resource.resolve("newResolvedValue");
    });
    assert_eq!(calls, 1);
    let icon = settings.profile_icon("Base").unwrap();
    assert_eq!(icon.path, "NewIconFromRuntime");
    assert_eq!(icon.resolved, "newResolvedValue");
}

#[test]
fn microsoft_media_resource_validate_resolver_not_called_for_emoji_icons_contract() {
    let mut settings = settings(
        r##"{"profiles":{"list":[{"icon":"♥","name":"Basic"},{"icon":"","name":"MDL2"},{"icon":"👨‍👩‍👧‍👦","name":"GraphemeCluster"},{"icon":"🕴️","name":"SurrogatePair"},{"icon":"#️⃣","name":"VariantWithEnclosingCombiner"}]}}"##,
        &[],
    );
    let mut user_calls = 0;
    settings.resolve_media_resources(|origin, _, resource| {
        if origin == MediaOrigin::User {
            user_calls += 1;
        }
        resource.reject();
    });
    assert_eq!(user_calls, 0);
    for (name, expected) in [
        ("Basic", "♥"),
        ("MDL2", ""),
        ("GraphemeCluster", "👨‍👩‍👧‍👦"),
        ("SurrogatePair", "🕴️"),
        ("VariantWithEnclosingCombiner", "#️⃣"),
    ] {
        let icon = settings.profile_icon(name).unwrap();
        assert!(icon.ok);
        assert_eq!(icon.path, expected);
        assert_eq!(icon.resolved, expected);
    }
}

#[test]
fn microsoft_media_resource_profile_defaults_contains_invalid_icon_contract() {
    let mut settings = settings(r#"{"profiles":{"defaults":{"icon":"DoesNotMatter"}}}"#, &[]);
    reject_all(&mut settings);
    assert_eq!(settings.profile_icon("Base").unwrap().resolved, CMD);
}

#[test]
fn microsoft_media_resource_profile_specifies_invalid_icon_and_commandline_contract() {
    let mut settings = settings(
        r#"{"profiles":{"defaults":{"icon":"DoesNotMatter","commandline":"C:\\Windows\\System32\\ping.exe"},"list":[{"guid":"{2cdb0be2-f601-4f70-9a6c-3472b3257883}","icon":"DoesNotMatter","commandline":"C:\\Windows\\System32\\cscript.exe","name":"ProfileSpecifiesInvalidIconAndCommandline"}]}}"#,
        &[],
    );
    reject_all(&mut settings);
    assert_eq!(
        settings
            .profile_icon("ProfileSpecifiesInvalidIconAndCommandline")
            .unwrap()
            .resolved,
        CSCRIPT
    );
}

#[test]
fn microsoft_media_resource_profile_specifies_invalid_icon_and_no_commandline_contract() {
    let mut settings = settings(
        r#"{"profiles":{"defaults":{"icon":"DoesNotMatter"},"list":[{"guid":"{af9dec6c-1337-4278-897d-69ca04920b27}","icon":"DoesNotMatter","name":"ProfileSpecifiesInvalidIconAndNoCommandline"}]}}"#,
        &[],
    );
    reject_all(&mut settings);
    assert_eq!(
        settings
            .profile_icon("ProfileSpecifiesInvalidIconAndNoCommandline")
            .unwrap()
            .resolved,
        CMD
    );
}

#[test]
fn microsoft_media_resource_profile_inherits_invalid_icon_and_has_commandline_contract() {
    let mut settings = settings(
        r#"{"profiles":{"defaults":{"icon":"DoesNotMatter"},"list":[{"guid":"{b0f32281-7173-46ef-aa2f-ddcf36670cf0}","commandline":"C:\\Windows\\System32\\cscript.exe","name":"ProfileInheritsInvalidIconAndHasCommandline"}]}}"#,
        &[],
    );
    reject_all(&mut settings);
    assert_eq!(
        settings
            .profile_icon("ProfileInheritsInvalidIconAndHasCommandline")
            .unwrap()
            .resolved,
        CMD
    );
}

#[test]
fn microsoft_media_resource_profile_inherits_invalid_icon_and_has_no_commandline_contract() {
    let mut settings = settings(
        r#"{"profiles":{"defaults":{"icon":"DoesNotMatter"},"list":[{"guid":"{21c4122a-b094-4436-9e9c-a06f05f35ad2}","name":"ProfileInheritsInvalidIconAndHasNoCommandline"}]}}"#,
        &[],
    );
    reject_all(&mut settings);
    assert_eq!(
        settings
            .profile_icon("ProfileInheritsInvalidIconAndHasNoCommandline")
            .unwrap()
            .resolved,
        CMD
    );
}

#[test]
fn microsoft_media_resource_profile_specifies_null_icon_contract() {
    let mut settings = settings(
        r#"{"profiles":{"defaults":{"icon":"DoesNotMatter","commandline":"C:\\Windows\\System32\\ping.exe"},"list":[{"guid":"{e1332dad-232c-4019-b3ff-05a4386c8c46}","icon":null,"commandline":"C:\\Windows\\System32\\cscript.exe","name":"ProfileSpecifiesNullIcon"}]}}"#,
        &[],
    );
    reject_all(&mut settings);
    assert_eq!(
        settings
            .profile_icon("ProfileSpecifiesNullIcon")
            .unwrap()
            .resolved,
        CSCRIPT
    );
}

#[test]
fn microsoft_media_resource_profile_specifies_null_icon_and_has_no_commandline_contract() {
    let mut settings = settings(
        r#"{"profiles":{"defaults":{"icon":"DoesNotMatter"},"list":[{"guid":"{b4053177-ae5c-4600-8b77-5f81a5d313e1}","icon":null,"name":"ProfileSpecifiesNullIconAndHasNoCommandline"}]}}"#,
        &[],
    );
    reject_all(&mut settings);
    assert_eq!(
        settings
            .profile_icon("ProfileSpecifiesNullIconAndHasNoCommandline")
            .unwrap()
            .resolved,
        CMD
    );
}

#[test]
fn microsoft_media_resource_profile_overwrites_bell_sound_contract() {
    let mut settings = settings(
        &format!(
            r#"{{"profiles":{{"list":[{{"guid":"{BASE_GUID}","bellSound":["does not matter; resolved rejected"]}}]}}}}"#
        ),
        &[],
    );
    reject_all(&mut settings);
    let sounds = settings.profile_bell_sounds("Base").unwrap();
    assert_eq!(sounds.len(), 1);
    assert_eq!(sounds[0].state, MediaResourceState::Rejected);
}

#[test]
fn microsoft_media_resource_fragment_updates_base_profile_contract() {
    let content =
        format!(r#"{{"profiles":[{{"updates":"{BASE_GUID}","icon":"IconFromFragment"}}]}}"#);
    let fragment = MediaFragment {
        source: "fragment",
        base_path: FRAGMENT_BASE,
        content: &content,
    };
    let mut settings = settings("{}", &[fragment]);
    settings.resolve_media_resources(|_, base_path, resource| resource.resolve(base_path));
    let icon = settings.profile_icon("Base").unwrap();
    assert_eq!(icon.path, "IconFromFragment");
    assert_eq!(icon.resolved, FRAGMENT_BASE);
}

#[test]
fn microsoft_media_resource_fragment_action_resources_get_resolved_contract() {
    let content = format!(
        r#"{{"profiles":[{{"updates":"{BASE_GUID}","icon":"IconFromFragment"}}],"actions":[{{"command":{{"action":"sendInput","input":"FROM WAY OUT BEYOND THE STARS"}},"icon":"foo.ico","id":"Dustin.SendInput"}}]}}"#
    );
    let fragment = MediaFragment {
        source: "fragment",
        base_path: FRAGMENT_BASE,
        content: &content,
    };
    let mut settings = settings("{}", &[fragment]);
    settings.resolve_media_resources(|_, base_path, resource| resource.resolve(base_path));
    let icon = settings.action_icon("Dustin.SendInput").unwrap();
    assert_eq!(icon.path, "foo.ico");
    assert_eq!(icon.resolved, FRAGMENT_BASE);
}

#[test]
fn microsoft_media_resource_disabled_fragment_not_resolved_contract() {
    let fragment = MediaFragment {
        source: "fragment",
        base_path: FRAGMENT_BASE,
        content: r#"{"profiles":[{"guid":"{4e7c2b36-642f-4694-83f8-8a5052038a23}","name":"FragmentProfile","commandline":"not_a_real_path","icon":"DoesNotMatterIgnoredByMockResolver"}]}"#,
    };
    let mut settings = settings(r#"{"disabledProfileSources":["fragment"]}"#, &[fragment]);
    let mut calls = 0;
    settings.resolve_media_resources(|origin, _, resource| {
        assert_ne!(origin, MediaOrigin::Fragment);
        calls += 1;
        resource.resolve("resolved");
    });
    assert_eq!(calls, 11);
    let icon = settings.profile_icon("Base").unwrap();
    assert_eq!(icon.path, "iconFromBase");
    assert_eq!(icon.resolved, "resolved");
}

#[test]
fn microsoft_media_resource_fragment_appearance_and_user_appearance_interaction_contract() {
    let fragment = MediaFragment {
        source: "fragment",
        base_path: "FRAGMENT",
        content: r#"{"profiles":[{"guid":"{4e7c2b36-642f-4694-83f8-8a5052038a23}","name":"FragmentProfileWithUnfocusedBackgroundImage","commandline":"not_a_real_path","backgroundImage":"focusedBackgroundImage1","unfocusedAppearance":{"backgroundImage":"unfocusedBackgroundImage1"}},{"guid":"{94df2990-d645-4675-8d9d-f8c89f842e6b}","name":"FragmentProfileWithNoUnfocusedBackgroundImage","commandline":"not_a_real_path","backgroundImage":"focusedBackgroundImage2"}]}"#,
    };
    let mut settings = settings(
        r#"{"profiles":[{"guid":"{4e7c2b36-642f-4694-83f8-8a5052038a23}","unfocusedAppearance":{"experimental.pixelShaderPath":"unfocusedPixelShaderPath1"}},{"guid":"{94df2990-d645-4675-8d9d-f8c89f842e6b}","unfocusedAppearance":{"backgroundImage":"userSpecifiedUnfocusedBackgroundImage"}}]}"#,
        &[fragment],
    );
    settings.resolve_media_resources(|_, base_path, resource| {
        resource.resolve(&format!("{}-{}", base_path, resource.path()));
    });
    let focused = settings
        .profile_background("FragmentProfileWithUnfocusedBackgroundImage", false)
        .unwrap();
    let unfocused = settings
        .profile_background("FragmentProfileWithUnfocusedBackgroundImage", true)
        .unwrap();
    let shader = settings
        .profile_pixel_shader("FragmentProfileWithUnfocusedBackgroundImage", true)
        .unwrap();
    assert_eq!(focused.resolved, "FRAGMENT-focusedBackgroundImage1");
    assert_eq!(unfocused.resolved, focused.resolved);
    assert_eq!(unfocused.identity, focused.identity);
    assert_eq!(shader.resolved, r"C:\Windows-unfocusedPixelShaderPath1");
    let focused = settings
        .profile_background("FragmentProfileWithNoUnfocusedBackgroundImage", false)
        .unwrap();
    let unfocused = settings
        .profile_background("FragmentProfileWithNoUnfocusedBackgroundImage", true)
        .unwrap();
    assert_eq!(focused.resolved, "FRAGMENT-focusedBackgroundImage2");
    assert_eq!(
        unfocused.resolved,
        r"C:\Windows-userSpecifiedUnfocusedBackgroundImage"
    );
    assert_ne!(unfocused.identity, focused.identity);
}

#[derive(Debug)]
struct TestPlatform;

impl MediaPlatform for TestPlatform {
    fn file_exists(&self, path: &str) -> bool {
        matches!(
            path.to_ascii_lowercase().as_str(),
            r"c:\windows\system32\cmd.exe"
                | r"c:\windows\explorer.exe"
                | r"\\?\c:\windows\system32\cmd.exe"
        )
    }

    fn environment(&self, name: &str) -> Option<String> {
        name.eq_ignore_ascii_case("ComSpec").then(|| CMD.to_owned())
    }

    fn desktop_wallpaper(&self) -> Option<String> {
        Some(r"C:\Users\Test\wallpaper.jpg".to_owned())
    }
}

fn assert_resolution(actual: MediaPathResolution, ok: bool, resolved: &str) {
    assert_eq!(actual.ok, ok);
    assert_eq!(actual.resolved, resolved);
}

#[test]
fn microsoft_media_resource_real_resolver_file_paths_contract() {
    let platform = TestPlatform;
    for input in [
        r"C:\Windows\System32\cmd.exe",
        "C:/Windows/System32/cmd.exe",
    ] {
        assert_resolution(resolve_media_path(input, USER_BASE, &platform), true, CMD);
    }
    for input in ["explorer.exe", r"..\Windows\explorer.exe"] {
        assert_resolution(
            resolve_media_path(input, USER_BASE, &platform),
            true,
            r"C:\Windows\explorer.exe",
        );
    }
    assert_resolution(
        resolve_media_path("%ComSpec%", USER_BASE, &platform),
        true,
        CMD,
    );
    assert_resolution(
        resolve_media_path(r"X:\foobar.ico", USER_BASE, &platform),
        false,
        "",
    );
}

#[test]
fn microsoft_media_resource_real_resolver_special_keywords_contract() {
    let platform = TestPlatform;
    assert_resolution(resolve_media_path("none", USER_BASE, &platform), true, "");
    assert_resolution(
        resolve_media_path("desktopWallpaper", USER_BASE, &platform),
        true,
        r"C:\Users\Test\wallpaper.jpg",
    );
}

#[test]
fn microsoft_media_resource_real_resolver_url_cases_contract() {
    let platform = TestPlatform;
    assert_resolution(
        resolve_media_path("https://contoso.com/explorer.exe", USER_BASE, &platform),
        true,
        r"C:\Windows\explorer.exe",
    );
    assert_resolution(
        resolve_media_path(
            "https://contoso.com/it_would_be_a_real_surprise_if_windows_added_a_file_named_this.ico",
            USER_BASE,
            &platform,
        ),
        false,
        "",
    );
    assert_resolution(
        resolve_media_path("file:///C:/Windows/System32/cmd.exe", USER_BASE, &platform),
        true,
        CMD,
    );
    for uri in [
        "ms-resource:///ProfileIcons/foo.png",
        "ms-appx:///ProfileIcons/foo.png",
    ] {
        assert_resolution(resolve_media_path(uri, USER_BASE, &platform), true, uri);
    }
    assert_resolution(
        resolve_media_path(
            "ms-appx://Microsoft.Burrito/Resources/explorer.exe",
            USER_BASE,
            &platform,
        ),
        true,
        r"C:\Windows\explorer.exe",
    );
    for uri in [
        "ftp://0.0.0.0/share/file.png",
        "x://is_this_a_file_or_a_path",
        "fake-scheme://foo",
        "http:/e/x",
    ] {
        assert_resolution(resolve_media_path(uri, USER_BASE, &platform), false, "");
    }
}

#[test]
fn microsoft_media_resource_real_resolver_unc_cases_contract() {
    let platform = TestPlatform;
    for path in [
        r"\\server",
        r"\\server\share",
        r"\\server\share\file",
        r"\\?\UNC\server",
        r"\\?\UNC\server\share",
        r"\\?\UNC\server\share\file",
    ] {
        assert!(!resolve_media_path(path, FRAGMENT_BASE, &platform).ok);
    }
    assert!(resolve_media_path(r"\\?\C:\Windows\System32\cmd.exe", FRAGMENT_BASE, &platform).ok);
}
