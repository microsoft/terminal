use terminal_settings::{
    color_scheme::Color,
    terminal_settings::{
        BindingSnapshot, CommandLinePlatform, NewTerminalArgs, ShortcutKind, SplitDirection,
        TerminalSettingsModel, command_line_to_argv, launch_position_from_string,
        normalize_command_line,
    },
};

#[derive(Default)]
struct FakeWindowsPlatform {
    system_root: Option<String>,
    executables: Vec<(String, String)>,
}

impl FakeWindowsPlatform {
    fn terminal_host() -> Self {
        Self {
            system_root: Some(r"C:\Windows".to_owned()),
            executables: vec![
                (
                    "cmd.exe".to_owned(),
                    r"C:\Windows\System32\cmd.exe".to_owned(),
                ),
                (
                    r"C:\Windows\System32\cmd.exe".to_owned(),
                    r"C:\Windows\System32\cmd.exe".to_owned(),
                ),
            ],
        }
    }

    fn with_executable(mut self, candidate: &str, resolved: &str) -> Self {
        self.executables
            .push((candidate.to_owned(), resolved.to_owned()));
        self
    }
}

impl CommandLinePlatform for FakeWindowsPlatform {
    fn expand_environment(&self, command_line: &str) -> String {
        let Some(system_root) = &self.system_root else {
            return command_line.to_owned();
        };
        command_line.replace("%SystemRoot%", system_root)
    }

    fn resolve_executable(&self, candidate: &str) -> Option<String> {
        self.executables.iter().find_map(|(known, resolved)| {
            known
                .eq_ignore_ascii_case(candidate)
                .then(|| resolved.clone())
        })
    }
}

fn binding(model: &TerminalSettingsModel, key: &str) -> BindingSnapshot {
    model
        .terminal_args_for_binding(key)
        .unwrap_or_else(|| panic!("missing binding {key}"))
}

#[test]
fn microsoft_terminal_settings_test_terminal_args_for_binding_contract() {
    let json = r#"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "profiles": { "list": [
            {
                "name": "profile0",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                "historySize": 1,
                "commandline": "cmd.exe"
            },
            {
                "name": "profile1",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                "historySize": 2,
                "commandline": "pwsh.exe"
            },
            {
                "name": "profile2",
                "historySize": 3,
                "commandline": "wsl.exe"
            }
        ],
        "defaults": {
            "historySize": 29
        } },
        "keybindings": [
            { "keys": ["ctrl+a"], "command": { "action": "splitPane", "split": "vertical" } },
            { "keys": ["ctrl+b"], "command": { "action": "splitPane", "split": "vertical", "profile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" } },
            { "keys": ["ctrl+c"], "command": { "action": "splitPane", "split": "vertical", "profile": "profile1" } },
            { "keys": ["ctrl+d"], "command": { "action": "splitPane", "split": "vertical", "profile": "profile2" } },
            { "keys": ["ctrl+e"], "command": { "action": "splitPane", "split": "horizontal", "commandline": "foo.exe" } },
            { "keys": ["ctrl+f"], "command": { "action": "splitPane", "split": "horizontal", "profile": "profile1", "commandline": "foo.exe" } },
            { "keys": ["ctrl+g"], "command": { "action": "newTab" } },
            { "keys": ["ctrl+h"], "command": { "action": "newTab", "startingDirectory": "c:\\foo" } },
            { "keys": ["ctrl+i"], "command": { "action": "newTab", "profile": "profile2", "startingDirectory": "c:\\foo" } },
            { "keys": ["ctrl+j"], "command": { "action": "newTab", "tabTitle": "bar" } },
            { "keys": ["ctrl+k"], "command": { "action": "newTab", "profile": "profile2", "tabTitle": "bar" } },
            { "keys": ["ctrl+l"], "command": { "action": "newTab", "profile": "profile1", "tabTitle": "bar", "startingDirectory": "c:\\foo", "commandline":"foo.exe" } }
        ]
    }"#;
    let model = TerminalSettingsModel::from_json(json).unwrap();
    let platform = FakeWindowsPlatform::terminal_host();
    let guid0 = "{6239a42c-0000-49a3-80bd-e8fdd045185c}";
    let guid1 = "{6239a42c-1111-49a3-80bd-e8fdd045185c}";
    let profile2_guid = model.profile_guid_by_name("profile2").unwrap().to_owned();

    assert_eq!(model.active_profile_count(), 3);
    assert_eq!(model.binding_count(), 12);
    assert!(!profile2_guid.is_empty());

    let a = binding(&model, "ctrl+a");
    assert_eq!(a.shortcut, ShortcutKind::SplitPane);
    assert_eq!(a.split_direction, Some(SplitDirection::Right));
    assert_eq!(a.terminal_args, NewTerminalArgs::default());
    let selected = model.profile_for_args(&a.terminal_args, &platform);
    let settings = model
        .create_with_new_terminal_args(Some(&a.terminal_args), &platform)
        .unwrap();
    assert_eq!(selected.profile_guid.as_deref(), Some(guid0));
    assert_eq!(settings.commandline, "cmd.exe");
    assert_eq!(settings.history_size, 1);

    for key in ["ctrl+b", "ctrl+c"] {
        let item = binding(&model, key);
        assert_eq!(item.shortcut, ShortcutKind::SplitPane);
        assert_eq!(item.split_direction, Some(SplitDirection::Right));
        let selected = model.profile_for_args(&item.terminal_args, &platform);
        let settings = model
            .create_with_new_terminal_args(Some(&item.terminal_args), &platform)
            .unwrap();
        assert_eq!(selected.profile_guid.as_deref(), Some(guid1));
        assert_eq!(settings.commandline, "pwsh.exe");
        assert_eq!(settings.history_size, 2);
    }

    let d = binding(&model, "ctrl+d");
    assert_eq!(d.split_direction, Some(SplitDirection::Right));
    assert_eq!(d.terminal_args.profile, "profile2");
    let selected = model.profile_for_args(&d.terminal_args, &platform);
    let settings = model
        .create_with_new_terminal_args(Some(&d.terminal_args), &platform)
        .unwrap();
    assert_eq!(
        selected.profile_guid.as_deref(),
        Some(profile2_guid.as_str())
    );
    assert_eq!(settings.commandline, "wsl.exe");
    assert_eq!(settings.history_size, 3);

    let e = binding(&model, "ctrl+e");
    assert_eq!(e.shortcut, ShortcutKind::SplitPane);
    assert_eq!(e.split_direction, Some(SplitDirection::Down));
    assert_eq!(e.terminal_args.commandline, "foo.exe");
    assert!(e.terminal_args.profile.is_empty());
    let selected = model.profile_for_args(&e.terminal_args, &platform);
    let settings = model
        .create_with_new_terminal_args(Some(&e.terminal_args), &platform)
        .unwrap();
    assert_eq!(selected.profile_guid, None);
    assert_eq!(selected.history_size, 29);
    assert_eq!(settings.commandline, "foo.exe");
    assert_eq!(settings.history_size, 29);

    let f = binding(&model, "ctrl+f");
    assert_eq!(f.split_direction, Some(SplitDirection::Down));
    assert_eq!(f.terminal_args.profile, "profile1");
    assert_eq!(f.terminal_args.commandline, "foo.exe");
    let selected = model.profile_for_args(&f.terminal_args, &platform);
    let settings = model
        .create_with_new_terminal_args(Some(&f.terminal_args), &platform)
        .unwrap();
    assert_eq!(selected.profile_guid.as_deref(), Some(guid1));
    assert_eq!(settings.commandline, "foo.exe");
    assert_eq!(settings.history_size, 2);

    let g = binding(&model, "ctrl+g");
    assert_eq!(g.shortcut, ShortcutKind::NewTab);
    let settings = model
        .create_with_new_terminal_args(Some(&g.terminal_args), &platform)
        .unwrap();
    assert_eq!(settings.profile_guid.as_deref(), Some(guid0));
    assert_eq!(settings.commandline, "cmd.exe");
    assert_eq!(settings.history_size, 1);

    let h = binding(&model, "ctrl+h");
    assert_eq!(h.terminal_args.starting_directory, r"c:\foo");
    let settings = model
        .create_with_new_terminal_args(Some(&h.terminal_args), &platform)
        .unwrap();
    assert_eq!(settings.profile_guid.as_deref(), Some(guid0));
    assert_eq!(settings.starting_directory, r"c:\foo");
    assert_eq!(settings.history_size, 1);

    let i = binding(&model, "ctrl+i");
    assert_eq!(i.terminal_args.profile, "profile2");
    assert_eq!(i.terminal_args.starting_directory, r"c:\foo");
    let settings = model
        .create_with_new_terminal_args(Some(&i.terminal_args), &platform)
        .unwrap();
    assert_eq!(
        settings.profile_guid.as_deref(),
        Some(profile2_guid.as_str())
    );
    assert_eq!(settings.commandline, "wsl.exe");
    assert_eq!(settings.starting_directory, r"c:\foo");
    assert_eq!(settings.history_size, 3);

    let j = binding(&model, "ctrl+j");
    assert_eq!(j.terminal_args.tab_title, "bar");
    let settings = model
        .create_with_new_terminal_args(Some(&j.terminal_args), &platform)
        .unwrap();
    assert_eq!(settings.profile_guid.as_deref(), Some(guid0));
    assert_eq!(settings.starting_title, "bar");
    assert_eq!(settings.history_size, 1);

    let k = binding(&model, "ctrl+k");
    assert_eq!(k.terminal_args.profile, "profile2");
    assert_eq!(k.terminal_args.tab_title, "bar");
    let settings = model
        .create_with_new_terminal_args(Some(&k.terminal_args), &platform)
        .unwrap();
    assert_eq!(
        settings.profile_guid.as_deref(),
        Some(profile2_guid.as_str())
    );
    assert_eq!(settings.commandline, "wsl.exe");
    assert_eq!(settings.starting_title, "bar");
    assert_eq!(settings.history_size, 3);

    let l = binding(&model, "ctrl+l");
    assert_eq!(l.terminal_args.profile, "profile1");
    assert_eq!(l.terminal_args.commandline, "foo.exe");
    assert_eq!(l.terminal_args.starting_directory, r"c:\foo");
    assert_eq!(l.terminal_args.tab_title, "bar");
    let settings = model
        .create_with_new_terminal_args(Some(&l.terminal_args), &platform)
        .unwrap();
    assert_eq!(settings.profile_guid.as_deref(), Some(guid1));
    assert_eq!(settings.commandline, "foo.exe");
    assert_eq!(settings.starting_title, "bar");
    assert_eq!(settings.starting_directory, r"c:\foo");
    assert_eq!(settings.history_size, 2);
}

#[test]
fn microsoft_terminal_settings_command_line_to_argv_w_contract() {
    // Microsoft also verifies that CommandLineToArgvW places the returned
    // strings back-to-back in one HLOCAL allocation. Rust intentionally owns
    // only the argument values/quoting semantics, not that API memory shape.
    for expected_argc in 1..=16 {
        let mut expected = Vec::new();
        let mut input = String::new();
        for index in 0..expected_argc {
            let count = (index * 7 % 64) + 1;
            let ch = char::from(b'a' + u8::try_from(index % 26).unwrap());
            let value = std::iter::repeat_n(ch, count).collect::<String>();
            let quoted = index % 2 == 0;
            if index != 0 {
                input.push(' ');
            }
            if quoted {
                input.push('"');
            }
            input.push_str(&value);
            if quoted {
                input.push('"');
            }
            expected.push(value);
        }
        assert_eq!(command_line_to_argv(&input), expected);
    }
}

#[test]
fn microsoft_terminal_settings_normalize_command_line_contract() {
    let file2 = r"C:\Temp\12345678-abcd-1234-abcd-123456789abc two\file 2.exe";
    let platform = FakeWindowsPlatform::default().with_executable(file2, file2);
    let input = format!(r#"{file2} -foo "bar1 bar2" -baz"#);
    let expected = format!("{file2}\0-foo\0bar1 bar2\0-baz");
    assert_eq!(normalize_command_line(&input, &platform), expected);
    assert_eq!(normalize_command_line(r"C:\", &platform), r"C:\");
}

#[test]
fn microsoft_terminal_settings_get_profile_for_args_with_commandline_contract() {
    let json = r#"{
        "profiles": {
            "defaults": { "historySize": 123 },
            "list": [
                { "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}", "commandline": "%SystemRoot%\\System32\\cmd.exe" },
                { "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}", "commandline": "cmd.exe /A" },
                { "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}", "commandline": "cmd.exe /A /B" },
                { "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}", "commandline": "cmd.exe /A /C", "connectionType": "{9a9977a7-1fe0-49c0-b6c0-13a0cd1c98a1}" },
                { "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}", "commandline": "C:\\invalid.exe" }
            ]
        }
    }"#;
    let model = TerminalSettingsModel::from_json(json).unwrap();
    let platform = FakeWindowsPlatform::terminal_host();
    let cases = [
        ("cmd.exe", Some(0usize)),
        ("cmd.exe /a", Some(1)),
        (r"%SystemRoot%\System32\cmd.exe /A", Some(1)),
        (r"C:\Windows\System32\cmd.exe /A /C", Some(1)),
        ("cmd.exe /A /B", Some(2)),
        ("cmd.exe /A /B /C", Some(2)),
        (r"C:\Windows\System32\cmd.exe /A /C", Some(1)),
        (r"C:\invalid.exe /A /B", Some(4)),
        (r"C:\Windows\regedit.exe", None),
    ];

    for (input, expected) in cases {
        let args = NewTerminalArgs {
            commandline: input.to_owned(),
            ..NewTerminalArgs::default()
        };
        let selected = model.profile_for_args(&args, &platform);
        if let Some(index) = expected {
            let discriminator = 0x1111usize * index;
            let expected_guid = format!("{{6239a42c-{discriminator:04x}-49a3-80bd-e8fdd045185c}}");
            assert_eq!(
                selected.profile_guid.as_deref(),
                Some(expected_guid.as_str())
            );
        } else {
            assert_eq!(selected.profile_guid, None);
            assert_eq!(selected.history_size, 123);
        }
    }
}

#[test]
fn microsoft_terminal_settings_make_settings_for_profile_contract() {
    let json = r#"
    {
        "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}", "historySize": 1 },
            { "name": "profile1", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}", "historySize": 2 }
        ]
    }"#;
    let model = TerminalSettingsModel::from_json(json).unwrap();
    let platform = FakeWindowsPlatform::default();
    assert_eq!(model.create_with_profile(0).unwrap().history_size, 1);
    assert_eq!(model.create_with_profile(1).unwrap().history_size, 2);
    assert_eq!(
        model
            .create_with_new_terminal_args(None, &platform)
            .unwrap()
            .history_size,
        1
    );
}

#[test]
fn microsoft_terminal_settings_make_settings_for_default_profile_that_doesnt_exist_contract() {
    let json = r#"
    {
        "defaultProfile": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}", "historySize": 1 },
            { "name": "profile1", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}", "historySize": 2 }
        ]
    }"#;
    let model = TerminalSettingsModel::from_json(json).unwrap();
    let platform = FakeWindowsPlatform::default();
    assert_eq!(model.warning_count(), 2);
    assert_eq!(model.active_profile_count(), 2);
    assert_eq!(
        model.default_profile_guid(),
        Some("{6239a42c-1111-49a3-80bd-e8fdd045185c}")
    );
    assert_eq!(
        model
            .create_with_new_terminal_args(None, &platform)
            .unwrap()
            .history_size,
        1
    );
}

#[test]
fn microsoft_terminal_settings_test_layer_profile_on_color_scheme_contract() {
    let json = r##"
    {
        "defaultProfile": "profile5",
        "profiles": [
            { "name": "profile0", "colorScheme": "schemeWithCursorColor" },
            { "name": "profile1", "colorScheme": "schemeWithoutCursorColor" },
            { "name": "profile2", "colorScheme": "schemeWithCursorColor", "cursorColor": "#234567" },
            { "name": "profile3", "colorScheme": "schemeWithoutCursorColor", "cursorColor": "#345678" },
            { "name": "profile4", "cursorColor": "#456789" },
            { "name": "profile5" }
        ],
        "schemes": [
            {
                "name": "schemeWithCursorColor", "cursorColor": "#123456",
                "black": "#121314", "red": "#121314", "green": "#121314", "yellow": "#121314",
                "blue": "#121314", "purple": "#121314", "cyan": "#121314", "white": "#121314",
                "brightBlack": "#121314", "brightRed": "#121314", "brightGreen": "#121314", "brightYellow": "#121314",
                "brightBlue": "#121314", "brightPurple": "#121314", "brightCyan": "#121314", "brightWhite": "#121314"
            },
            {
                "name": "schemeWithoutCursorColor",
                "black": "#121314", "red": "#121314", "green": "#121314", "yellow": "#121314",
                "blue": "#121314", "purple": "#121314", "cyan": "#121314", "white": "#121314",
                "brightBlack": "#121314", "brightRed": "#121314", "brightGreen": "#121314", "brightYellow": "#121314",
                "brightBlue": "#121314", "brightPurple": "#121314", "brightCyan": "#121314", "brightWhite": "#121314"
            }
        ]
    }"##;
    let model = TerminalSettingsModel::from_json(json).unwrap();
    assert_eq!(model.active_profile_count(), 6);
    assert_eq!(
        model.cursor_color_for_profile(0),
        Some(Color::rgb(0x12, 0x34, 0x56))
    );
    assert_eq!(
        model.cursor_color_for_profile(1),
        Some(Color::rgb(0xff, 0xff, 0xff))
    );
    assert_eq!(
        model.cursor_color_for_profile(2),
        Some(Color::rgb(0x23, 0x45, 0x67))
    );
    assert_eq!(
        model.cursor_color_for_profile(3),
        Some(Color::rgb(0x34, 0x56, 0x78))
    );
    assert_eq!(
        model.cursor_color_for_profile(4),
        Some(Color::rgb(0x45, 0x67, 0x89))
    );
    assert_eq!(
        model.cursor_color_for_profile(5),
        Some(Color::rgb(0xff, 0xff, 0xff))
    );
}

#[test]
fn microsoft_terminal_settings_test_commandline_to_title_promotion_contract() {
    let json = r#"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "profiles": { "list": [
            {
                "name": "profile0",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                "historySize": 1,
                "commandline": "cmd.exe"
            }
        ], "defaults": { "historySize": 29 } }
    }"#;
    let model = TerminalSettingsModel::from_json(json).unwrap();
    let platform = FakeWindowsPlatform::default();
    let cases = [
        ("profile0", "", "", "profile0"),
        ("profile0", "foo.exe", "", "profile0"),
        ("", "", "Analog Kid", "Analog Kid"),
        ("", "foo.exe", "Digital Man", "Digital Man"),
        ("", "foo.exe", "", "foo.exe"),
        ("", "foo.exe bar", "", "foo.exe"),
        ("", r#""foo exe.exe" bar"#, "", "foo exe.exe"),
        ("", r#""" grand designs"#, "", ""),
        ("", " imagine a man", "", ""),
    ];
    for (profile, commandline, tab_title, expected) in cases {
        let args = NewTerminalArgs {
            profile: profile.to_owned(),
            commandline: commandline.to_owned(),
            tab_title: tab_title.to_owned(),
            ..NewTerminalArgs::default()
        };
        let settings = model
            .create_with_new_terminal_args(Some(&args), &platform)
            .unwrap();
        assert_eq!(settings.starting_title, expected);
    }
}

#[test]
fn microsoft_terminal_settings_test_initial_position_parsing_contract() {
    let cases = [
        ("50", Some(50), Some(50)),
        ("100,", Some(100), None),
        (",100", None, Some(100)),
        ("50,50", Some(50), Some(50)),
        ("abc,100", None, Some(100)),
        ("abc", None, None),
    ];
    for (input, x, y) in cases {
        let position = launch_position_from_string(input);
        assert_eq!(position.x, x);
        assert_eq!(position.y, y);
    }
}
