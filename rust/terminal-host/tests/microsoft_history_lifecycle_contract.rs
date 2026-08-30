use terminal_host::command_history::CommandHistoryStore;

const APPS: [&str; 5] = ["foo.exe", "bar.exe", "baz.exe", "apple.exe", "banana.exe"];
const ITEMS: [&str; 12] = [
    "dir",
    "dir /w",
    "dir /p /w",
    "telnet 127.0.0.1",
    "ipconfig",
    "ipconfig /all",
    "net",
    "ping 127.0.0.1",
    "cd ..",
    "bcz",
    "notepad sources",
    "git push",
];

fn handle(index: usize) -> u64 {
    ((index + 1) * 4) as u64
}

#[test]
fn microsoft_history_allocate_and_free_one_app_contract() {
    let mut store = CommandHistoryStore::new(4, 10);
    let process = handle(0);

    assert!(store.allocate("testapp1.exe", process));
    assert_eq!(store.history_count(), 1);
    assert!(
        store
            .find_by_handle(process)
            .is_some_and(terminal_host::command_history::CommandHistory::is_allocated)
    );

    store.free(process);
    assert_eq!(store.history_count(), 1);
    assert!(store.find_by_handle(process).is_none());
    let retained = store
        .find_stored_by_exe("testapp1.exe")
        .expect("free retains the session history");
    assert!(!retained.is_allocated());
}

#[test]
fn microsoft_history_allocate_too_many_apps_contract() {
    let mut store = CommandHistoryStore::new(4, 10);

    for (index, app) in APPS.iter().enumerate() {
        let allocated = store.allocate(app, handle(index));
        assert_eq!(allocated, index < 4, "app={app}");
    }

    assert_eq!(store.history_count(), 4);
    for app in &APPS[..4] {
        assert!(store.find_by_exe(app).is_some(), "app={app}");
    }
    assert!(store.find_by_exe(APPS[4]).is_none());
}

#[test]
fn microsoft_history_ensure_history_restored_after_client_leaves_and_rejoins_contract() {
    let mut store = CommandHistoryStore::new(4, 10);
    let original_handle = handle(0);
    assert!(store.allocate(APPS[0], original_handle));

    {
        let history = store
            .find_by_handle_mut(original_handle)
            .expect("allocated history");
        for command in &ITEMS[..10] {
            assert!(history.add(command, false));
        }
        assert_eq!(history.command_count(), 10);
    }

    store.free(original_handle);
    let reattached_handle = handle(14);
    assert!(store.allocate(APPS[0], reattached_handle));
    let history = store
        .find_by_handle(reattached_handle)
        .expect("same app reattaches to retained history");
    assert_eq!(history.command_count(), 10);
    assert_eq!(
        history.commands(),
        ITEMS[..10]
            .iter()
            .map(|value| (*value).to_owned())
            .collect::<Vec<_>>()
    );
}

#[test]
fn microsoft_history_too_many_apps_does_not_take_list_contract() {
    let mut store = CommandHistoryStore::new(4, 10);

    for (index, app) in APPS[..4].iter().enumerate() {
        let process = handle(index);
        assert!(store.allocate(app, process));
        let history = store
            .find_by_handle_mut(process)
            .expect("allocated history");
        for command in &ITEMS[..10] {
            assert!(history.add(command, false));
        }
        assert_eq!(history.command_count(), 10);
    }

    assert_eq!(store.history_count(), 4);
    assert!(!store.allocate(APPS[4], 444));
    assert_eq!(store.history_count(), 4);
}

#[test]
fn microsoft_history_app_names_match_insensitive_contract() {
    let mut store = CommandHistoryStore::new(4, 10);
    assert!(store.allocate("testApp", 777));
    let history = store.find_by_handle(777).expect("allocated history");
    assert!(history.is_app_name_match("TEsTaPP"));
}

#[test]
fn microsoft_history_realloc_up_contract() {
    let mut store = CommandHistoryStore::new(4, 10);
    let process = handle(0);
    assert!(store.allocate(APPS[0], process));

    let before = {
        let history = store
            .find_by_handle_mut(process)
            .expect("allocated history");
        for command in ITEMS {
            assert!(history.add(command, false));
        }
        assert_eq!(history.command_count(), 10);
        history.commands().to_vec()
    };

    {
        let history = store
            .find_by_handle_mut(process)
            .expect("allocated history");
        history.realloc(ITEMS.len());
        assert_eq!(history.command_count(), 10);
        assert_eq!(history.commands(), before);

        for command in ITEMS {
            assert!(history.add(command, false));
        }
        assert_eq!(history.command_count(), ITEMS.len());
    }
}

#[test]
fn microsoft_history_realloc_down_contract() {
    let mut store = CommandHistoryStore::new(4, 10);
    let process = handle(0);
    assert!(store.allocate(APPS[0], process));

    let before = {
        let history = store
            .find_by_handle_mut(process)
            .expect("allocated history");
        for command in &ITEMS[..10] {
            assert!(history.add(command, false));
        }
        assert_eq!(history.command_count(), 10);
        history.commands().to_vec()
    };

    let history = store
        .find_by_handle_mut(process)
        .expect("allocated history");
    history.realloc(5);
    assert_eq!(history.command_count(), 5);
    assert_eq!(history.commands(), &before[..5]);
}
