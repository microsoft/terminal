use terminal_settings::profile::{Profile, ProfileSettings};

#[test]
fn microsoft_profile_setting_inheritance_fallback() {
    let settings = ProfileSettings::from_json(
        r#"{
            "profiles": {
                "defaults": { "historySize": 5000 },
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                    },
                    {
                        "name": "profile1",
                        "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                        "snapOnInput": false
                    }
                ]
            }
        }"#,
    )
    .unwrap();

    let profiles = settings.profiles();
    assert_eq!(profiles.len(), 2);
    assert_eq!(profiles[0].history_size(), 5000);
    assert!(profiles[0].snap_on_input());
    assert_eq!(profiles[1].history_size(), 5000);
    assert!(!profiles[1].snap_on_input());
}

#[test]
fn microsoft_profile_clear_setting_restores_inheritance() {
    let parent = Profile::from_json(
        r#"{
            "name": "parent",
            "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "historySize": 1000,
            "tabTitle": "ParentTitle"
        }"#,
    )
    .unwrap();
    let mut child = parent.create_child();
    child
        .layer_json(
            r#"{
                "name": "child",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                "historySize": 2000,
                "tabTitle": "ChildTitle"
            }"#,
        )
        .unwrap();

    assert_eq!(child.history_size(), 2000);
    assert_eq!(child.tab_title(), "ChildTitle");
    assert!(child.has_history_size());
    assert!(child.has_tab_title());

    child.clear_history_size();
    assert!(!child.has_history_size());
    assert_eq!(child.history_size(), 1000);

    child.clear_tab_title();
    assert!(!child.has_tab_title());
    assert_eq!(child.tab_title(), "ParentTitle");
}

#[test]
fn microsoft_profile_has_setting_at_specific_layer() {
    let settings = ProfileSettings::from_json(
        r#"{
            "profiles": {
                "defaults": {
                    "historySize": 5000,
                    "tabTitle": "DefaultTitle"
                },
                "list": [
                    {
                        "name": "profile0",
                        "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                        "historySize": 9001
                    },
                    {
                        "name": "profile1",
                        "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
                    }
                ]
            }
        }"#,
    )
    .unwrap();

    let profiles = settings.profiles();
    assert_eq!(profiles.len(), 2);

    assert!(profiles[0].has_history_size());
    assert_eq!(profiles[0].history_size(), 9001);
    assert!(!profiles[0].has_tab_title());
    assert_eq!(profiles[0].tab_title(), "DefaultTitle");

    assert!(!profiles[1].has_history_size());
    assert_eq!(profiles[1].history_size(), 5000);

    assert!(settings.defaults().has_history_size());
    assert_eq!(settings.defaults().history_size(), 5000);
}
