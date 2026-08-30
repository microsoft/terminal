use terminal_settings::{
    command_expansion::{
        ExpandedCommandSettings, PaletteAction, PaletteCommand, PaletteSplitDirection,
    },
    elevate::ElevateSettingsModel,
};

fn command_named<'a>(commands: &'a [PaletteCommand], name: &str) -> &'a PaletteCommand {
    commands
        .iter()
        .find(|command| command.name() == name)
        .unwrap()
}

fn assert_split(command: &PaletteCommand, profile: &str, direction: PaletteSplitDirection) {
    assert_eq!(command.action(), PaletteAction::SplitPane);
    assert_eq!(command.profile(), profile);
    assert_eq!(command.split_direction(), Some(direction));
    assert!(command.nested().is_empty());
}

fn profile_fixture(second_name: &str) -> String {
    format!(
        r#""profiles":[
            {{"name":"profile0","guid":"{{6239a42c-0000-49a3-80bd-e8fdd045185c}}","commandline":"cmd.exe"}},
            {{"name":"{second_name}","guid":"{{6239a42c-1111-49a3-80bd-e8fdd045185c}}","commandline":"pwsh.exe"}},
            {{"name":"profile2","commandline":"wsl.exe"}}
        ]"#
    )
}

#[test]
fn microsoft_local_terminal_app_settings_test_iterate_commands_contract() {
    let json = format!(
        r#"{{{},"actions":[{{
            "name":"iterable command ${{profile.name}}",
            "iterateOn":"profiles",
            "command":{{"action":"splitPane","profile":"${{profile.name}}"}}
        }}]}}"#,
        profile_fixture("profile1")
    );
    let settings = ExpandedCommandSettings::from_json(&json).unwrap();
    assert_eq!(settings.active_profile_count(), 3);
    assert_eq!(settings.warning_count(), 0);
    let template = &settings.source_commands()[0];
    assert_eq!(template.name(), "iterable command ${profile.name}");
    assert_split(
        template,
        "${profile.name}",
        PaletteSplitDirection::Automatic,
    );

    let expanded = settings.expanded_commands();
    assert_eq!(expanded.len(), 3);
    for name in ["profile0", "profile1", "profile2"] {
        assert_split(
            command_named(expanded, &format!("iterable command {name}")),
            name,
            PaletteSplitDirection::Automatic,
        );
    }
}

#[test]
fn microsoft_local_terminal_app_settings_test_iterate_on_generated_named_commands_contract() {
    let json = format!(
        r#"{{{},"actions":[{{
            "iterateOn":"profiles",
            "command":{{"action":"splitPane","profile":"${{profile.name}}"}}
        }}]}}"#,
        profile_fixture("profile1")
    );
    let settings = ExpandedCommandSettings::from_json(&json).unwrap();
    assert_eq!(
        settings.source_commands()[0].name(),
        "Split pane, profile: ${profile.name}"
    );
    assert_eq!(settings.expanded_commands().len(), 3);
    for name in ["profile0", "profile1", "profile2"] {
        assert_split(
            command_named(
                settings.expanded_commands(),
                &format!("Split pane, profile: {name}"),
            ),
            name,
            PaletteSplitDirection::Automatic,
        );
    }
}

#[test]
fn microsoft_local_terminal_app_settings_test_iterate_on_bad_json_contract() {
    let json = format!(
        r#"{{{},"actions":[{{
            "name":"iterable command ${{profile.name}}",
            "iterateOn":"profiles",
            "command":{{"action":"splitPane","profile":"${{profile.name}}"}}
        }}]}}"#,
        profile_fixture(r#"profile1\""#)
    );
    let settings = ExpandedCommandSettings::from_json(&json).unwrap();
    let command = command_named(settings.expanded_commands(), "iterable command profile1\"");
    assert_split(command, "profile1\"", PaletteSplitDirection::Automatic);
    assert_eq!(settings.expanded_commands().len(), 3);
}

#[test]
fn microsoft_local_terminal_app_settings_test_nested_commands_contract() {
    let json = format!(
        r#"{{{},"actions":[{{"name":"Connect to ssh...","commands":[
            {{"name":"first.com","command":{{"action":"newTab","commandline":"ssh me@first.com"}}}},
            {{"name":"second.com","command":{{"action":"newTab","commandline":"ssh me@second.com"}}}}
        ]}}]}}"#,
        profile_fixture("profile1")
    );
    let settings = ExpandedCommandSettings::from_json(&json).unwrap();
    let root = &settings.expanded_commands()[0];
    assert_eq!(root.name(), "Connect to ssh...");
    assert_eq!(root.action(), PaletteAction::Invalid);
    assert_eq!(root.nested().len(), 2);
    for (name, commandline) in [
        ("first.com", "ssh me@first.com"),
        ("second.com", "ssh me@second.com"),
    ] {
        let child = root.nested_named(name).unwrap();
        assert_eq!(child.action(), PaletteAction::NewTab);
        assert_eq!(child.commandline(), commandline);
        assert!(child.nested().is_empty());
    }
}

#[test]
fn microsoft_local_terminal_app_settings_test_nested_in_nested_command_contract() {
    let json = format!(
        r#"{{{},"actions":[{{"name":"grandparent","commands":[
            {{"name":"parent","commands":[
                {{"name":"child1","command":{{"action":"newTab","commandline":"ssh me@first.com"}}}},
                {{"name":"child2","command":{{"action":"newTab","commandline":"ssh me@second.com"}}}}
            ]}}
        ]}}]}}"#,
        profile_fixture("profile1")
    );
    let settings = ExpandedCommandSettings::from_json(&json).unwrap();
    let grandparent = &settings.expanded_commands()[0];
    let parent = grandparent.nested_named("parent").unwrap();
    assert_eq!(grandparent.action(), PaletteAction::Invalid);
    assert_eq!(parent.action(), PaletteAction::Invalid);
    assert_eq!(parent.nested().len(), 2);
    assert_eq!(
        parent.nested_named("child1").unwrap().commandline(),
        "ssh me@first.com"
    );
    assert_eq!(
        parent.nested_named("child2").unwrap().commandline(),
        "ssh me@second.com"
    );
}

#[test]
fn microsoft_local_terminal_app_settings_test_nested_in_iterable_command_contract() {
    let json = format!(
        r#"{{{},"actions":[{{
            "iterateOn":"profiles","name":"${{profile.name}}...","commands":[
                {{"command":{{"action":"splitPane","profile":"${{profile.name}}","split":"auto"}}}},
                {{"command":{{"action":"splitPane","profile":"${{profile.name}}","split":"right"}}}},
                {{"command":{{"action":"splitPane","profile":"${{profile.name}}","split":"down"}}}}
            ]
        }}]}}"#,
        profile_fixture("profile1")
    );
    let settings = ExpandedCommandSettings::from_json(&json).unwrap();
    assert_eq!(settings.expanded_commands().len(), 3);
    for profile in ["profile0", "profile1", "profile2"] {
        let parent = command_named(settings.expanded_commands(), &format!("{profile}..."));
        assert_eq!(parent.action(), PaletteAction::Invalid);
        assert_eq!(parent.nested().len(), 3);
        assert_split(
            parent
                .nested_named(&format!("Split pane, profile: {profile}"))
                .unwrap(),
            profile,
            PaletteSplitDirection::Automatic,
        );
        assert_split(
            parent
                .nested_named(&format!("Split pane, split: right, profile: {profile}"))
                .unwrap(),
            profile,
            PaletteSplitDirection::Right,
        );
        assert_split(
            parent
                .nested_named(&format!("Split pane, split: down, profile: {profile}"))
                .unwrap(),
            profile,
            PaletteSplitDirection::Down,
        );
    }
}

#[test]
fn microsoft_local_terminal_app_settings_test_iterable_in_nested_command_contract() {
    let json = format!(
        r#"{{{},"actions":[{{"name":"New Tab With Profile...","commands":[{{
            "iterateOn":"profiles",
            "command":{{"action":"newTab","profile":"${{profile.name}}"}}
        }}]}}]}}"#,
        profile_fixture("profile1")
    );
    let settings = ExpandedCommandSettings::from_json(&json).unwrap();
    let root = &settings.expanded_commands()[0];
    assert_eq!(root.name(), "New Tab With Profile...");
    assert_eq!(root.nested().len(), 3);
    for profile in ["profile0", "profile1", "profile2"] {
        let child = root
            .nested_named(&format!("New tab, profile: {profile}"))
            .unwrap();
        assert_eq!(child.action(), PaletteAction::NewTab);
        assert_eq!(child.profile(), profile);
        assert!(child.nested().is_empty());
    }
}

#[test]
fn microsoft_local_terminal_app_settings_test_mixed_nested_and_iterable_command_contract() {
    let json = format!(
        r#"{{{},"actions":[{{"name":"New Pane...","commands":[{{
            "iterateOn":"profiles","name":"${{profile.name}}...","commands":[
                {{"command":{{"action":"splitPane","profile":"${{profile.name}}","split":"auto"}}}},
                {{"command":{{"action":"splitPane","profile":"${{profile.name}}","split":"right"}}}},
                {{"command":{{"action":"splitPane","profile":"${{profile.name}}","split":"down"}}}}
            ]
        }}]}}]}}"#,
        profile_fixture("profile1")
    );
    let settings = ExpandedCommandSettings::from_json(&json).unwrap();
    let root = &settings.expanded_commands()[0];
    assert_eq!(root.name(), "New Pane...");
    assert_eq!(root.nested().len(), 3);
    for profile in ["profile0", "profile1", "profile2"] {
        let parent = root.nested_named(&format!("{profile}...")).unwrap();
        assert_eq!(parent.nested().len(), 3);
        assert_split(
            parent
                .nested_named(&format!("Split pane, profile: {profile}"))
                .unwrap(),
            profile,
            PaletteSplitDirection::Automatic,
        );
        assert_split(
            parent
                .nested_named(&format!("Split pane, split: right, profile: {profile}"))
                .unwrap(),
            profile,
            PaletteSplitDirection::Right,
        );
        assert_split(
            parent
                .nested_named(&format!("Split pane, split: down, profile: {profile}"))
                .unwrap(),
            profile,
            PaletteSplitDirection::Down,
        );
    }
}

#[test]
fn microsoft_local_terminal_app_settings_test_iterable_color_scheme_commands_contract() {
    let json = format!(
        r#"{{{},"schemes":[
            {{"name":"Campbell"}},{{"name":"Campbell PowerShell"}},{{"name":"Vintage"}}
        ],"actions":[{{
            "name":"iterable command ${{scheme.name}}","iterateOn":"schemes",
            "command":{{"action":"splitPane","profile":"${{scheme.name}}"}}
        }}]}}"#,
        profile_fixture("profile1")
    );
    let settings = ExpandedCommandSettings::from_json(&json).unwrap();
    assert_eq!(settings.active_profile_count(), 3);
    assert_eq!(settings.warning_count(), 0);
    assert_eq!(settings.source_commands()[0].profile(), "${scheme.name}");
    for scheme in ["Campbell", "Campbell PowerShell", "Vintage"] {
        assert_split(
            command_named(
                settings.expanded_commands(),
                &format!("iterable command {scheme}"),
            ),
            scheme,
            PaletteSplitDirection::Automatic,
        );
    }
}

#[test]
fn microsoft_local_terminal_app_settings_test_elevate_arg_contract() {
    let json = r#"{
        "profiles":[
            {"name":"profile0","commandline":"cmd.exe"},
            {"name":"profile1","elevate":true,"commandline":"pwsh.exe"},
            {"name":"profile2","elevate":false,"commandline":"wsl.exe"}
        ],
        "keybindings":[
            {"keys":["ctrl+a"],"command":{"action":"newTab","profile":"profile0"}},
            {"keys":["ctrl+b"],"command":{"action":"newTab","profile":"profile1"}},
            {"keys":["ctrl+c"],"command":{"action":"newTab","profile":"profile2"}},
            {"keys":["ctrl+d"],"command":{"action":"newTab","profile":"profile0","elevate":false}},
            {"keys":["ctrl+e"],"command":{"action":"newTab","profile":"profile1","elevate":false}},
            {"keys":["ctrl+f"],"command":{"action":"newTab","profile":"profile2","elevate":false}},
            {"keys":["ctrl+g"],"command":{"action":"newTab","profile":"profile0","elevate":true}},
            {"keys":["ctrl+h"],"command":{"action":"newTab","profile":"profile1","elevate":true}},
            {"keys":["ctrl+i"],"command":{"action":"newTab","profile":"profile2","elevate":true}}
        ]
    }"#;
    let settings = ElevateSettingsModel::from_json(json).unwrap();
    assert_eq!(settings.active_profile_count(), 3);
    assert_eq!(settings.warning_count(), 0);
    assert_eq!(settings.binding_count(), 9);

    for (key, profile, action, effective, commandline) in [
        ("ctrl+a", "profile0", None, false, "cmd.exe"),
        ("ctrl+b", "profile1", None, true, "pwsh.exe"),
        ("ctrl+c", "profile2", None, false, "wsl.exe"),
        ("ctrl+d", "profile0", Some(false), false, "cmd.exe"),
        ("ctrl+e", "profile1", Some(false), false, "pwsh.exe"),
        ("ctrl+f", "profile2", Some(false), false, "wsl.exe"),
        ("ctrl+g", "profile0", Some(true), true, "cmd.exe"),
        ("ctrl+h", "profile1", Some(true), true, "pwsh.exe"),
        ("ctrl+i", "profile2", Some(true), true, "wsl.exe"),
    ] {
        let snapshot = settings.binding_snapshot(key).unwrap();
        assert_eq!(snapshot.profile, profile);
        assert_eq!(snapshot.action_elevate, action);
        assert_eq!(snapshot.effective_elevate, effective);
        assert_eq!(snapshot.commandline, commandline);
    }
}
