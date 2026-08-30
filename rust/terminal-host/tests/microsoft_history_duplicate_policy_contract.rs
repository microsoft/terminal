use terminal_host::command_history::CommandHistoryStore;

fn allocated_history() -> CommandHistoryStore {
    let mut store = CommandHistoryStore::new(4, 10);
    assert!(store.allocate("foo.exe", 4));
    store
}

#[test]
fn microsoft_history_add_sequential_duplicates_contract() {
    let mut store = allocated_history();
    let history = store.find_by_handle_mut(4).expect("allocated history");

    assert!(history.add("dir", false));
    assert!(history.add("dir", false));

    assert_eq!(history.command_count(), 1);
    assert_eq!(history.get_nth(0), Some("dir"));
}

#[test]
fn microsoft_history_add_sequential_no_duplicates_contract() {
    let mut store = allocated_history();
    let history = store.find_by_handle_mut(4).expect("allocated history");

    assert!(history.add("dir", true));
    assert!(history.add("dir", true));

    assert_eq!(history.command_count(), 1);
    assert_eq!(history.get_nth(0), Some("dir"));
}

#[test]
fn microsoft_history_add_nonsequential_duplicates_contract() {
    let mut store = allocated_history();
    let history = store.find_by_handle_mut(4).expect("allocated history");

    assert!(history.add("dir", false));
    assert!(history.add("cd", false));
    assert!(history.add("dir", false));

    assert_eq!(history.command_count(), 3);
    let commands = history
        .commands()
        .iter()
        .map(String::as_str)
        .collect::<Vec<_>>();
    assert_eq!(commands, ["dir", "cd", "dir"]);
}

#[test]
fn microsoft_history_add_nonsequential_no_duplicates_contract() {
    let mut store = allocated_history();
    let history = store.find_by_handle_mut(4).expect("allocated history");

    assert!(history.add("dir", true));
    assert!(history.add("cd", false));
    assert!(history.add("dir", true));

    assert_eq!(history.command_count(), 2);
    let commands = history
        .commands()
        .iter()
        .map(String::as_str)
        .collect::<Vec<_>>();
    assert_eq!(commands, ["cd", "dir"]);
}
