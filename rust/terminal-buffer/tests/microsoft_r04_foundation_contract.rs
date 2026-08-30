use std::collections::{BTreeMap, BTreeSet, hash_map::DefaultHasher};
use std::hash::{Hash, Hasher};
use std::sync::{Arc, Mutex, mpsc};

use terminal_buffer::geometry::{InclusiveRect, Point, Rect};
use terminal_buffer::output_cell::GlyphWidthDetector;
use terminal_buffer::rle::Rle;
use terminal_buffer::text_attribute::{LegacyColorDefaults, TextAttribute};
use terminal_buffer::text_buffer::TextBuffer;
use terminal_buffer::text_color::{DEFAULT_FOREGROUND, Rgb, TABLE_SIZE, TextColor};
use terminal_buffer::width_detector::CodepointWidthDetector;

#[test]
fn microsoft_r04_text_color_contract_covers_default_index_rgb_and_mutation() {
    let mut table = [Rgb::default(); TABLE_SIZE];
    for (index, color) in table.iter_mut().take(16).enumerate() {
        let value = u8::try_from(index).expect("legacy palette index fits in u8");
        *color = Rgb::new(value, value, value);
    }
    table[DEFAULT_FOREGROUND] = Rgb::new(21, 22, 23);

    let mut color = TextColor::default();
    assert!(color.is_default());
    assert_eq!(
        color.resolve(&table, DEFAULT_FOREGROUND, false),
        Rgb::new(21, 22, 23)
    );

    color.set_index(7, false);
    assert!(color.is_index16());
    assert_eq!(color.resolve(&table, DEFAULT_FOREGROUND, false), table[7]);
    assert_eq!(color.resolve(&table, DEFAULT_FOREGROUND, true), table[15]);

    color.set_index(7, true);
    assert!(color.is_index256());
    assert_eq!(color.resolve(&table, DEFAULT_FOREGROUND, true), table[7]);

    color.set_rgb(Rgb::new(1, 2, 3));
    assert!(color.is_rgb());
    assert_eq!(color.rgb_value(), Rgb::new(1, 2, 3));

    color.set_default();
    assert!(color.is_default());
}

#[test]
fn microsoft_r04_text_attribute_legacy_roundtrip_covers_legacy_and_meta_bits() {
    let defaults = LegacyColorDefaults::default();
    for foreground in 0u16..=15 {
        for background in 0u16..=15 {
            for meta in [0u16, 0x0400, 0x0800, 0x1000, 0x4000, 0x8000] {
                let legacy = foreground | (background << 4) | meta;
                let attribute = TextAttribute::from_legacy(legacy, defaults);
                assert_eq!(attribute.legacy_attributes(defaults), legacy);
            }
        }
    }
}

#[test]
fn microsoft_r04_reflow_contract_preserves_wrap_chain_and_wide_glyph_boundary() {
    let fill = TextAttribute::default();
    let mut buffer = TextBuffer::new(5, 4, fill).expect("valid buffer");
    buffer
        .row_mut(0)
        .replace_glyph(0, 1, &[u16::from(b'A')])
        .expect("glyph fits");
    buffer
        .row_mut(0)
        .replace_glyph(1, 2, &[0x4e00])
        .expect("wide glyph fits");
    buffer
        .row_mut(0)
        .replace_glyph(3, 1, &[u16::from(b'B')])
        .expect("glyph fits");
    buffer.row_mut(0).set_wrap_forced(true);
    buffer
        .row_mut(1)
        .replace_glyph(0, 1, &[u16::from(b'C')])
        .expect("glyph fits");

    buffer
        .resize_width_reflow(3, fill)
        .expect("reflow succeeds");

    assert_eq!(buffer.width(), 3);
    assert_eq!(buffer.row(0).glyph_at(0), &[u16::from(b'A')]);
    assert_eq!(buffer.row(0).glyph_at(1), &[0x4e00]);
    assert!(buffer.row(0).was_wrap_forced());
    assert_eq!(buffer.row(1).glyph_at(0), &[u16::from(b'B')]);
    assert_eq!(buffer.row(1).glyph_at(1), &[u16::from(b' ')]);
    assert_eq!(buffer.row(1).glyph_at(2), &[u16::from(b'C')]);
}

#[test]
fn microsoft_r04_unicode_width_contract_keeps_ambiguous_narrow_and_known_wide() {
    fn utf16(ch: char) -> Vec<u16> {
        let mut storage = [0; 2];
        ch.encode_utf16(&mut storage).to_vec()
    }

    let detector = CodepointWidthDetector;
    assert!(!detector.is_full_width(&utf16('A')));
    assert!(!detector.is_full_width(&utf16('·')));
    assert!(detector.is_full_width(&utf16('界')));
    assert!(detector.is_full_width(&utf16('Ａ')));
    assert!(detector.is_full_width(&utf16('🚀')));
    assert!(!detector.is_full_width(&[0xd83d]));
}

#[test]
fn microsoft_r04_geometry_contract_keeps_til_point_and_rectangle_core_semantics() {
    assert_eq!(Point::default(), Point::new(0, 0));
    assert_eq!(Point::new(3, -4), Point { x: 3, y: -4 });

    let rect = Rect::new(3, 4, 13, 9);
    assert_eq!(rect.width(), 10);
    assert_eq!(rect.height(), 5);
    assert_eq!(Rect::default(), Rect::new(0, 0, 0, 0));

    let inclusive = InclusiveRect::new(3, 4, 12, 8);
    assert_eq!(inclusive.width(), 10);
    assert_eq!(inclusive.height(), 5);
}

#[test]
fn microsoft_r04_rle_contract_covers_construction_lookup_replace_and_canonicalization() {
    let empty = Rle::new(0, 7u8);
    assert!(empty.is_empty());

    let mut values = Rle::new(8, 1u8);
    assert_eq!(values.at(0), Some(&1));
    assert_eq!(values.at(8), None);
    values.replace(2, 6, 2);
    assert_eq!(values.expanded(), vec![1, 1, 2, 2, 2, 2, 1, 1]);
    values.replace(1, 7, 1);
    assert_eq!(values.expanded(), vec![1; 8]);
    assert_eq!(values.runs().len(), 1);

    let clone = values.clone();
    assert_eq!(clone, values);
}

#[test]
fn microsoft_r04_rle_construct_with_length_and_value_matches_source_contract() {
    let values = Rle::new(5, 1u16);
    assert_eq!(values.len(), 5);
    assert!(!values.is_empty());
    assert_eq!(values.expanded(), vec![1; 5]);
}

#[test]
fn microsoft_r04_rle_copy_swap_and_move_observables_match_source_contract() {
    let mut full = Rle::new(8, 1u16);
    full.replace(3, 5, 2);
    assert_eq!(full.expanded(), vec![1, 1, 1, 2, 2, 1, 1, 1]);

    let mut rle1 = full.clone();
    let mut rle2 = Rle::<u16>::default();
    std::mem::swap(&mut rle1, &mut rle2);
    assert!(rle1.is_empty());
    assert_eq!(rle2, full);

    rle1 = rle2.clone();
    assert_eq!(rle1, full);
    assert_eq!(rle2, full);

    rle1 = Rle::new(1, 1);
    assert_eq!(rle1.expanded(), vec![1]);
    rle1 = rle2;
    assert_eq!(rle1, full);
}

#[test]
fn microsoft_r04_rle_comparison_matches_source_contract() {
    let mut rle1 = Rle::new(4, 1u16);
    rle1.replace(1, 3, 3);
    rle1.replace(3, 4, 2);
    let mut rle2 = rle1.clone();

    assert_eq!(rle1, rle2);
    rle2.replace(0, 1, 2);
    assert_ne!(rle1, rle2);
}

#[test]
fn microsoft_r04_option_and_collection_replacements_preserve_foundation_observables() {
    let first = Some(11).or(Some(22));
    let defaulted = None::<i32>.or(Some(33));
    assert_eq!(first, Some(11));
    assert_eq!(defaulted, Some(33));

    let mut vector = vec![1, 2, 3];
    vector.insert(1, 9);
    assert_eq!(vector, [1, 9, 2, 3]);
    let copied = vector.clone();
    assert_eq!(copied, vector);

    let set = [3, 1, 2, 2].into_iter().collect::<BTreeSet<_>>();
    assert_eq!(set.into_iter().collect::<Vec<_>>(), [1, 2, 3]);

    let map = [("b", 2), ("a", 1)].into_iter().collect::<BTreeMap<_, _>>();
    assert_eq!(map.get("a"), Some(&1));
    assert_eq!(map["b"], 2);
}

#[test]
fn microsoft_r04_string_replacement_contract_covers_split_replace_case_and_parse() {
    let parts = "123;456;789".split(';').collect::<Vec<_>>();
    assert_eq!(parts, ["123", "456", "789"]);
    assert_eq!("abcabc".replace("ab", "xy"), "xycxyc");
    assert!("Terminal".starts_with("Term"));
    assert!("Terminal".ends_with("inal"));
    assert!("Terminal".eq_ignore_ascii_case("terminal"));
    assert_eq!("AbC".to_ascii_lowercase(), "abc");
    assert_eq!("AbC".to_ascii_uppercase(), "ABC");
    assert_eq!("18446744073709551615".parse::<u64>(), Ok(u64::MAX));
    assert!("18446744073709551616".parse::<u64>().is_err());
    assert_eq!("-42".parse::<i64>(), Ok(-42));
}

#[test]
fn microsoft_r04_utf16_replacement_contract_matches_surrogates_and_replacement_behavior() {
    fn decode(input: &[u16]) -> String {
        char::decode_utf16(input.iter().copied())
            .map(|item| item.unwrap_or(char::REPLACEMENT_CHARACTER))
            .collect()
    }

    assert_eq!(decode(&[]), "");
    assert_eq!(decode(&[u16::from(b'a')]), "a");
    assert_eq!(decode(&[0xd801, 0xdc01]), "𐐁");
    assert_eq!(decode(&[0xd801, u16::from(b'a')]), "�a");
    assert_eq!(decode(&[0xdc01, u16::from(b'a')]), "�a");

    let source = "a𐐁ネコ🚀";
    let encoded = source.encode_utf16().collect::<Vec<_>>();
    let decoded = String::from_utf16(&encoded).expect("valid UTF-16 must decode");
    assert_eq!(decoded, source);
}

#[test]
fn microsoft_r04_math_replacement_contract_covers_floor_ceiling_rounding_and_integer_division() {
    assert_eq!(3.25_f64.floor().to_bits(), 3.0_f64.to_bits());
    assert_eq!((-3.25_f64).floor().to_bits(), (-4.0_f64).to_bits());
    assert_eq!(3.25_f64.ceil().to_bits(), 4.0_f64.to_bits());
    assert_eq!((-3.25_f64).ceil().to_bits(), (-3.0_f64).to_bits());
    assert_eq!(3.5_f64.round().to_bits(), 4.0_f64.to_bits());
    assert_eq!((-3.5_f64).round().to_bits(), (-4.0_f64).to_bits());
    assert_eq!(10_i32.div_euclid(3), 3);
    assert_eq!((10_i32 + 3 - 1) / 3, 4);
}

#[test]
fn microsoft_r04_sync_replacement_contract_preserves_mutex_and_spsc_ordering_basics() {
    let shared = Arc::new(Mutex::new(Vec::new()));
    {
        let mut guard = shared.lock().expect("mutex is not poisoned");
        guard.extend([1, 2, 3]);
    }
    assert_eq!(*shared.lock().expect("mutex is not poisoned"), [1, 2, 3]);

    let (sender, receiver) = mpsc::sync_channel(4);
    for value in [10, 20, 30] {
        sender.send(value).expect("channel remains connected");
    }
    drop(sender);
    assert_eq!(receiver.into_iter().collect::<Vec<_>>(), [10, 20, 30]);
}

#[test]
fn microsoft_r04_hash_replacement_contract_is_stable_within_one_rust_process() {
    fn hash(value: &str) -> u64 {
        let mut hasher = DefaultHasher::new();
        value.hash(&mut hasher);
        hasher.finish()
    }

    for value in ["", "a", "abc", "message digest"] {
        assert_eq!(hash(value), hash(value));
    }
    assert_ne!(hash("a"), hash("abc"));
}
