use terminal_buffer::{
    text_attribute::{LegacyColorDefaults, TextAttribute},
    text_color::TextColor,
};

#[test]
fn microsoft_f04_text_attribute_legacy_roundtrip_matches_source_contract() {
    let defaults = LegacyColorDefaults::default();
    let legacy = 0x0041; // FOREGROUND_BLUE | BACKGROUND_RED
    let attribute = TextAttribute::from_legacy(legacy, defaults);

    assert!(attribute.is_legacy());
    assert_eq!(attribute.legacy_attributes(defaults), legacy);
}

#[test]
fn microsoft_f04_text_attribute_meta_bits_roundtrip_matches_source_contract() {
    const META_FLAGS: [u16; 5] = [0x0400, 0x0800, 0x1000, 0x4000, 0x8000];
    let defaults = LegacyColorDefaults::default();

    for flag in META_FLAGS {
        let legacy = 0x0041 | flag;
        let attribute = TextAttribute::from_legacy(legacy, defaults);

        assert!(attribute.is_legacy());
        assert_eq!(attribute.legacy_attributes(defaults), legacy);
        assert_eq!(attribute.character_attributes(), flag);
    }
}

#[test]
fn microsoft_f04_text_attribute_exhaustive_legacy_roundtrip_matches_source_contract() {
    const ALL_ATTRS: u16 = 0xdfff;
    const COMMON_LVB_LEADING_BYTE: u16 = 0x0100;
    const COMMON_LVB_TRAILING_BYTE: u16 = 0x0200;
    const NON_META_RESERVED_BIT: u16 = 0x2000;

    let defaults = LegacyColorDefaults::default();

    for legacy in 0..ALL_ATTRS {
        if legacy & (NON_META_RESERVED_BIT | COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE)
            != 0
        {
            continue;
        }

        let attribute = TextAttribute::from_legacy(legacy, defaults);
        assert_eq!(
            attribute.legacy_attributes(defaults),
            legacy,
            "legacy attribute 0x{legacy:04x} must round-trip exactly"
        );
    }
}

#[test]
fn microsoft_f04_text_attribute_default_colors_roundtrip_matches_source_contract() {
    const FOREGROUND_RED: u16 = 0x0004;
    const FOREGROUND_GREEN: u16 = 0x0002;
    const BACKGROUND_BLUE: u16 = 0x0010;
    const BACKGROUND_GREEN: u16 = 0x0020;

    let defaults = LegacyColorDefaults::from_legacy_attribute(FOREGROUND_RED | BACKGROUND_BLUE);

    let foreground_default_legacy = FOREGROUND_RED | BACKGROUND_GREEN;
    let mut foreground_default = TextAttribute::default();
    foreground_default.set_default_foreground();
    foreground_default.set_background(TextColor::index256(TextColor::DARK_GREEN));
    assert_eq!(
        foreground_default,
        TextAttribute::from_legacy(foreground_default_legacy, defaults)
    );
    assert_eq!(
        foreground_default.legacy_attributes(defaults),
        foreground_default_legacy
    );

    let background_default_legacy = FOREGROUND_GREEN | BACKGROUND_BLUE;
    let mut background_default = TextAttribute::default();
    background_default.set_foreground(TextColor::index256(TextColor::DARK_GREEN));
    background_default.set_default_background();
    assert_eq!(
        background_default,
        TextAttribute::from_legacy(background_default_legacy, defaults)
    );
    assert_eq!(
        background_default.legacy_attributes(defaults),
        background_default_legacy
    );

    let both_default_legacy = FOREGROUND_RED | BACKGROUND_BLUE;
    let mut both_default = TextAttribute::default();
    both_default.set_default_foreground();
    both_default.set_default_background();
    assert_eq!(
        both_default,
        TextAttribute::from_legacy(both_default_legacy, defaults)
    );
    assert_eq!(
        both_default.legacy_attributes(defaults),
        both_default_legacy
    );
}
