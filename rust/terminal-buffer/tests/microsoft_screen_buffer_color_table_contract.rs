use terminal_buffer::color_table::{ColorAlias, ColorTableState};
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_color::{DEFAULT_BACKGROUND, DEFAULT_FOREGROUND, Rgb, TextColor};

#[test]
fn microsoft_screen_buffer_vt_set_color_table_contract() {
    let mut state = ColorTableState::default();

    let valid = [
        ("0;rgb:1/1/1", 0, Rgb::new(0x11, 0x11, 0x11)),
        ("1;rgb:1/23/1", 1, Rgb::new(0x11, 0x23, 0x11)),
        ("2;rgb:1/23/12", 2, Rgb::new(0x11, 0x23, 0x12)),
        ("3;rgb:12/23/12", 3, Rgb::new(0x12, 0x23, 0x12)),
        ("4;rgb:ff/a1/1b", 4, Rgb::new(0xff, 0xa1, 0x1b)),
        ("5;rgb:ff/a1/1b", 5, Rgb::new(0xff, 0xa1, 0x1b)),
    ];
    for (payload, index, expected) in valid {
        assert!(state.apply_osc(4, payload));
        assert_eq!(state.color(index), Some(expected));
    }

    assert!(state.apply_osc(4, "5;rgb:09/09/09"));
    let unchanged = Rgb::new(9, 9, 9);
    for invalid in [
        "5;rgb:/1/1",
        "5;rgb:1/1/1/1",
        "5;rgb:1//1",
        "5;rgb://",
        "5;rgb:1/11/",
        "5;rgbi:1/1/1",
        "5;cmyk:1/1/1",
        ";rgb:1/1/1",
        "5;1/1/1",
    ] {
        assert!(!state.apply_osc(4, invalid));
        assert_eq!(state.color(5), Some(unchanged), "payload={invalid}");
    }
}

#[test]
fn microsoft_screen_buffer_set_global_color_table_contract() {
    let mut state = ColorTableState::default();
    let original_red = state.color(usize::from(TextColor::DARK_RED)).unwrap();
    let replacement = Rgb::new(0x11, 0x22, 0x33);
    assert_ne!(original_red, replacement);

    let mut indexed_red_background = TextAttribute::default();
    indexed_red_background.set_background(TextColor::index16(TextColor::DARK_RED));
    let main_written_before_change = indexed_red_background;
    let alternate_written_before_change = indexed_red_background;

    assert_eq!(
        state.attribute_colors(main_written_before_change).1,
        original_red
    );
    assert_eq!(
        state.attribute_colors(alternate_written_before_change).1,
        original_red
    );

    assert!(state.apply_osc(4, "1;rgb:11/22/33"));

    // Palette state is global: existing indexed cells in both logical buffers
    // resolve through the replacement without rewriting their TextAttribute.
    assert_eq!(
        state.attribute_colors(main_written_before_change).1,
        replacement
    );
    assert_eq!(
        state.attribute_colors(alternate_written_before_change).1,
        replacement
    );
}

#[test]
fn microsoft_screen_buffer_set_color_table_three_digits_contract() {
    let mut state = ColorTableState::default();
    let original = state.color(123).unwrap();
    let replacement = Rgb::new(0x11, 0x22, 0x33);
    assert_ne!(original, replacement);

    let mut indexed = TextAttribute::default();
    indexed.set_background(TextColor::index256(123));
    assert_eq!(state.attribute_colors(indexed).1, original);

    assert!(state.apply_osc(4, "123;rgb:11/22/33"));
    assert_eq!(state.attribute_colors(indexed).1, replacement);
}

#[test]
fn microsoft_screen_buffer_set_default_foreground_color_contract() {
    let mut state = ColorTableState::default();

    assert!(state.apply_osc(10, "rgb:33/66/99"));
    assert_eq!(
        state.color(DEFAULT_FOREGROUND),
        Some(Rgb::new(0x33, 0x66, 0x99))
    );

    assert!(state.apply_osc(10, "rgb:ff/ff/ff"));
    let white = Rgb::new(0xff, 0xff, 0xff);
    assert_eq!(state.color(DEFAULT_FOREGROUND), Some(white));

    assert!(!state.apply_osc(10, "99/66/33"));
    assert_eq!(state.color(DEFAULT_FOREGROUND), Some(white));
}

#[test]
fn microsoft_screen_buffer_set_default_background_color_contract() {
    let mut state = ColorTableState::default();

    assert!(state.apply_osc(11, "rgb:33/66/99"));
    assert_eq!(
        state.color(DEFAULT_BACKGROUND),
        Some(Rgb::new(0x33, 0x66, 0x99))
    );

    assert!(state.apply_osc(11, "rgb:ff/ff/ff"));
    let white = Rgb::new(0xff, 0xff, 0xff);
    assert_eq!(state.color(DEFAULT_BACKGROUND), Some(white));

    assert!(!state.apply_osc(11, "99/66/33"));
    assert_eq!(state.color(DEFAULT_BACKGROUND), Some(white));
}

#[test]
fn microsoft_screen_buffer_assign_color_aliases_contract() {
    let mut state = ColorTableState::default();
    let original = [
        state.alias_index(ColorAlias::DefaultForeground),
        state.alias_index(ColorAlias::DefaultBackground),
        state.alias_index(ColorAlias::FrameForeground),
        state.alias_index(ColorAlias::FrameBackground),
    ];

    assert!(!state.assign_color_aliases(0, 12, 34));
    assert_eq!(
        state.alias_index(ColorAlias::DefaultForeground),
        original[0]
    );
    assert_eq!(
        state.alias_index(ColorAlias::DefaultBackground),
        original[1]
    );
    assert_eq!(state.alias_index(ColorAlias::FrameForeground), original[2]);
    assert_eq!(state.alias_index(ColorAlias::FrameBackground), original[3]);

    assert!(state.assign_color_aliases(1, 23, 45));
    assert_eq!(state.alias_index(ColorAlias::DefaultForeground), 23);
    assert_eq!(state.alias_index(ColorAlias::DefaultBackground), 45);

    assert!(state.assign_color_aliases(2, 34, 56));
    assert_eq!(state.alias_index(ColorAlias::FrameForeground), 34);
    assert_eq!(state.alias_index(ColorAlias::FrameBackground), 56);

    state.reset_to_initial();
    assert_eq!(
        state.alias_index(ColorAlias::DefaultForeground),
        original[0]
    );
    assert_eq!(
        state.alias_index(ColorAlias::DefaultBackground),
        original[1]
    );
    assert_eq!(state.alias_index(ColorAlias::FrameForeground), original[2]);
    assert_eq!(state.alias_index(ColorAlias::FrameBackground), original[3]);
}

#[test]
fn microsoft_screen_buffer_set_defaults_individually_both_default_contract() {
    let mut state = ColorTableState::default();
    assert!(state.apply_osc(10, "rgb:ff/ff/00"));
    assert!(state.apply_osc(11, "rgb:ff/00/ff"));

    let yellow = Rgb::new(255, 255, 0);
    let magenta = Rgb::new(255, 0, 255);
    let bright_green = state.color(usize::from(TextColor::BRIGHT_GREEN)).unwrap();
    let dark_blue = state.color(usize::from(TextColor::DARK_BLUE)).unwrap();

    let defaults = TextAttribute::default();
    let mut indexed = TextAttribute::default();
    indexed.set_foreground(TextColor::index16(TextColor::BRIGHT_GREEN));
    indexed.set_background(TextColor::index16(TextColor::DARK_BLUE));
    let mut default_foreground = indexed;
    default_foreground.set_default_foreground();
    let mut default_background = indexed;
    default_background.set_default_background();

    assert!(!defaults.is_legacy());
    assert!(indexed.is_legacy());
    assert!(!default_foreground.is_legacy());
    assert!(!default_background.is_legacy());

    assert_eq!(state.attribute_colors(defaults), (yellow, magenta));
    assert_eq!(state.attribute_colors(indexed), (bright_green, dark_blue));
    assert_eq!(
        state.attribute_colors(default_foreground),
        (yellow, dark_blue)
    );
    assert_eq!(
        state.attribute_colors(default_background),
        (bright_green, magenta)
    );
}

#[test]
fn microsoft_screen_buffer_set_defaults_together_contract() {
    let mut state = ColorTableState::default();
    assert!(state.apply_osc(10, "rgb:ff/ff/00"));
    assert!(state.apply_osc(11, "rgb:ff/00/ff"));

    let defaults = TextAttribute::default();
    let mut indexed_250 = TextAttribute::default();
    indexed_250.set_background(TextColor::index256(250));

    assert_eq!(
        state.attribute_colors(defaults),
        (Rgb::new(255, 255, 0), Rgb::new(255, 0, 255))
    );
    assert_eq!(
        state.attribute_colors(indexed_250),
        (Rgb::new(255, 255, 0), state.color(250).unwrap())
    );

    indexed_250.set_default_foreground();
    indexed_250.set_default_background();
    assert_eq!(indexed_250, defaults);
}

#[test]
fn microsoft_screen_buffer_reverse_reset_with_default_background_contract() {
    let mut state = ColorTableState::default();
    assert!(state.apply_osc(11, "rgb:ff/00/ff"));
    let magenta = Rgb::new(255, 0, 255);

    let defaults = TextAttribute::default();
    let normal = state.attribute_colors(defaults);
    assert_eq!(normal.1, magenta);

    let mut reversed = defaults;
    reversed.invert();
    assert!(reversed.is_reverse_video());
    assert_eq!(state.attribute_colors(reversed).0, magenta);

    reversed.invert();
    assert!(!reversed.is_reverse_video());
    assert_eq!(state.attribute_colors(reversed).1, magenta);
    assert_eq!(reversed, defaults);
}
