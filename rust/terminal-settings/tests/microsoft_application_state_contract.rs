use terminal_settings::application_state::{ApplicationState, WindowLayout};

fn layout(marker: &str) -> WindowLayout {
    WindowLayout::with_tab_layout(vec![marker.to_owned()])
}

#[test]
fn microsoft_application_state_save_and_lookup_workspace() {
    let mut state = ApplicationState::new();
    let expected = layout("tab-1");

    state.save_workspace("win1", expected.clone());

    assert_eq!(
        state.all_persisted_workspaces().get("win1"),
        Some(&expected)
    );
}

#[test]
fn microsoft_application_state_remove_workspace_returns_false_when_missing() {
    let mut state = ApplicationState::new();

    assert!(!state.remove_workspace("does-not-exist"));
    state.save_workspace("win1", layout("tab-1"));
    assert!(state.remove_workspace("win1"));
    assert!(!state.remove_workspace("win1"));
}

#[test]
fn microsoft_application_state_rename_workspace_migrates_entry() {
    let mut state = ApplicationState::new();
    let expected = layout("persisted");
    state.save_workspace("oldName", expected.clone());

    assert!(state.rename_workspace("oldName", "newName"));
    assert!(!state.all_persisted_workspaces().contains_key("oldName"));
    assert_eq!(
        state.all_persisted_workspaces().get("newName"),
        Some(&expected)
    );
}

#[test]
fn microsoft_application_state_rename_workspace_no_op_for_empty_or_equal_names() {
    let mut state = ApplicationState::new();
    state.save_workspace("win1", layout("tab-1"));

    assert!(!state.rename_workspace("win1", "win1"));
    assert!(!state.rename_workspace("", "win2"));
    assert!(state.rename_workspace("win1", ""));
    assert!(!state.all_persisted_workspaces().contains_key("win1"));
    assert!(!state.all_persisted_workspaces().contains_key(""));
    assert!(!state.rename_workspace("win1", ""));
}

#[test]
fn microsoft_application_state_rename_workspace_no_op_for_missing_entry() {
    let mut state = ApplicationState::new();

    assert!(!state.rename_workspace("missing", "newName"));
    assert!(state.all_persisted_workspaces().is_empty());
}

#[test]
fn microsoft_application_state_take_workspace_removes_and_returns() {
    let mut state = ApplicationState::new();
    let expected = layout("tab-1");
    state.save_workspace("win1", expected.clone());

    assert_eq!(state.take_workspace("win1"), Some(expected));
    assert_eq!(state.take_workspace("win1"), None);
}

#[test]
fn microsoft_application_state_take_workspace_returns_none_when_missing() {
    let mut state = ApplicationState::new();

    assert_eq!(state.take_workspace("missing"), None);
}
