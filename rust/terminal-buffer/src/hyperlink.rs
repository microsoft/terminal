use std::collections::{HashMap, HashSet};

use crate::text_buffer::TextBuffer;

/// Safe, platform-neutral ownership of the hyperlink map semantics used by `TextBuffer`.
///
/// Hyperlinks without a custom id receive a fresh numeric id every time. Hyperlinks with
/// a custom id are stable for the same `(custom_id, uri)` pair, while the same custom id
/// may legitimately identify different URIs and therefore receives a distinct numeric id.
#[derive(Clone, Debug, Default)]
pub struct HyperlinkStore {
    next_id: u16,
    uri_by_id: HashMap<u16, String>,
    custom_pair_to_id: HashMap<(String, String), u16>,
}

impl HyperlinkStore {
    #[must_use]
    pub fn new() -> Self {
        Self {
            next_id: 1,
            uri_by_id: HashMap::new(),
            custom_pair_to_id: HashMap::new(),
        }
    }

    pub fn add(&mut self, uri: impl Into<String>, custom_id: Option<&str>) -> u16 {
        let uri = uri.into();

        if let Some(custom_id) = custom_id {
            let key = (custom_id.to_owned(), uri.clone());
            if let Some(existing) = self.custom_pair_to_id.get(&key) {
                return *existing;
            }

            let id = self.allocate_id();
            self.uri_by_id.insert(id, uri);
            self.custom_pair_to_id.insert(key, id);
            return id;
        }

        let id = self.allocate_id();
        self.uri_by_id.insert(id, uri);
        id
    }

    pub fn uri(&self, id: u16) -> Option<&str> {
        self.uri_by_id.get(&id).map(String::as_str)
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.uri_by_id.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.uri_by_id.is_empty()
    }

    /// Drops hyperlink-map entries that are no longer referenced by any live
    /// text-buffer cell. Custom-id aliases are pruned with the numeric map so the
    /// two registries cannot diverge after circular-buffer rotation.
    pub fn trim_to_buffer(&mut self, buffer: &TextBuffer) {
        let live_ids: HashSet<u16> = buffer
            .logical_rows()
            .flat_map(|row| row.attributes().iter().copied())
            .map(super::text_attribute::TextAttribute::hyperlink_id)
            .filter(|&id| id != 0)
            .collect();

        self.uri_by_id.retain(|id, _| live_ids.contains(id));
        self.custom_pair_to_id.retain(|_, id| live_ids.contains(id));
    }

    fn allocate_id(&mut self) -> u16 {
        let id = self.next_id;
        self.next_id = self
            .next_id
            .checked_add(1)
            .expect("hyperlink id space exhausted");
        id
    }
}

#[cfg(test)]
mod tests {
    use super::HyperlinkStore;
    use crate::text_attribute::TextAttribute;
    use crate::text_buffer::TextBuffer;

    #[test]
    fn anonymous_hyperlinks_are_independent() {
        let mut store = HyperlinkStore::new();
        let first = store.add("https://example.test/a", None);
        let second = store.add("https://example.test/a", None);
        assert_ne!(first, second);
        assert_eq!(store.uri(first), Some("https://example.test/a"));
        assert_eq!(store.uri(second), Some("https://example.test/a"));
    }

    #[test]
    fn custom_pair_is_stable() {
        let mut store = HyperlinkStore::new();
        let first = store.add("https://example.test/a", Some("same"));
        let second = store.add("https://example.test/a", Some("same"));
        assert_eq!(first, second);
        assert_eq!(store.len(), 1);
    }

    #[test]
    fn same_custom_id_different_uri_is_not_aliased() {
        let mut store = HyperlinkStore::new();
        let first = store.add("https://example.test/a", Some("same"));
        let second = store.add("https://example.test/b", Some("same"));
        assert_ne!(first, second);
        assert_eq!(store.uri(first), Some("https://example.test/a"));
        assert_eq!(store.uri(second), Some("https://example.test/b"));
    }

    #[test]
    fn microsoft_text_buffer_hyperlink_trim_contract() {
        let fill = TextAttribute::default();
        let mut buffer = TextBuffer::new(80, 10, fill).unwrap();
        let mut store = HyperlinkStore::new();
        let obsolete = store.add("test.url", Some("CustomId"));
        let live = store.add("other.url", Some("OtherCustomId"));

        let mut obsolete_attr = fill;
        obsolete_attr.set_hyperlink_id(obsolete);
        buffer.row_mut(0).set_attr_to_end(70, obsolete_attr);
        let mut live_attr = fill;
        live_attr.set_hyperlink_id(live);
        buffer.row_mut(5).set_attr_to_end(70, live_attr);

        buffer.rotate_up(1, fill);
        store.trim_to_buffer(&buffer);

        assert_eq!(store.uri(obsolete), None);
        assert_eq!(store.uri(live), Some("other.url"));
        assert_eq!(store.len(), 1);
    }

    #[test]
    fn microsoft_text_buffer_no_hyperlink_trim_contract() {
        let fill = TextAttribute::default();
        let mut buffer = TextBuffer::new(80, 10, fill).unwrap();
        let mut store = HyperlinkStore::new();
        let id = store.add("test.url", Some("CustomId"));

        let mut attribute = fill;
        attribute.set_hyperlink_id(id);
        buffer.row_mut(0).set_attr_to_end(70, attribute);
        buffer.row_mut(5).set_attr_to_end(70, attribute);

        buffer.rotate_up(1, fill);
        store.trim_to_buffer(&buffer);

        assert_eq!(store.uri(id), Some("test.url"));
        assert_eq!(store.len(), 1);
    }
}
