use terminal_buffer::hyperlink::HyperlinkStore;

#[test]
fn microsoft_screen_buffer_test_add_hyperlink_contract() {
    let mut store = HyperlinkStore::new();

    let first = store.add("https://example.test/one", None);
    let second = store.add("https://example.test/two", None);

    assert_ne!(first, second);
    assert_eq!(store.uri(first), Some("https://example.test/one"));
    assert_eq!(store.uri(second), Some("https://example.test/two"));
}

#[test]
fn microsoft_screen_buffer_test_add_hyperlink_custom_id_contract() {
    let mut store = HyperlinkStore::new();

    let first = store.add("https://example.test/one", Some("custom"));
    let repeated = store.add("https://example.test/one", Some("custom"));
    let other_custom = store.add("https://example.test/one", Some("other"));

    assert_eq!(first, repeated);
    assert_ne!(first, other_custom);
    assert_eq!(store.uri(first), Some("https://example.test/one"));
}

#[test]
fn microsoft_screen_buffer_test_add_hyperlink_custom_id_different_uri_contract() {
    let mut store = HyperlinkStore::new();

    let first = store.add("https://example.test/one", Some("custom"));
    let second = store.add("https://example.test/two", Some("custom"));
    let first_again = store.add("https://example.test/one", Some("custom"));

    assert_ne!(first, second);
    assert_eq!(first, first_again);
    assert_eq!(store.uri(first), Some("https://example.test/one"));
    assert_eq!(store.uri(second), Some("https://example.test/two"));
}
