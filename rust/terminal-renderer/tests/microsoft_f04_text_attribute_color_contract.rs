use terminal_buffer::{
    text_attribute::TextAttribute,
    text_color::{DEFAULT_BACKGROUND, DEFAULT_FOREGROUND, Rgb, TABLE_SIZE, TextColor},
};
use terminal_renderer::{
    RenderMode, RenderSettingsPolicy, ResolvedAttributeColors, resolve_text_attribute_colors,
};

fn color_table() -> [Rgb; TABLE_SIZE] {
    let mut table = [Rgb::default(); TABLE_SIZE];
    table[0] = Rgb::new(12, 12, 12);
    table[2] = Rgb::new(19, 161, 14);
    table[8] = Rgb::new(118, 118, 118);
    table[DEFAULT_FOREGROUND] = Rgb::new(1, 2, 3);
    table[DEFAULT_BACKGROUND] = Rgb::new(4, 5, 6);
    table
}

fn colors(foreground: Rgb, background: Rgb) -> ResolvedAttributeColors {
    ResolvedAttributeColors {
        foreground,
        background,
    }
}

fn resolve_with_settings(
    attribute: TextAttribute,
    table: &[Rgb; TABLE_SIZE],
    settings: RenderSettingsPolicy,
) -> ResolvedAttributeColors {
    resolve_text_attribute_colors(
        attribute,
        table,
        DEFAULT_FOREGROUND,
        DEFAULT_BACKGROUND,
        settings,
    )
}

fn resolve(attribute: TextAttribute, table: &[Rgb; TABLE_SIZE]) -> ResolvedAttributeColors {
    resolve_with_settings(attribute, table, RenderSettingsPolicy::default())
}

#[test]
fn microsoft_f04_text_attribute_color_getters_match_source_contract() {
    let table = color_table();
    let red = Rgb::new(255, 0, 0);
    let faint_red = Rgb::new(127, 0, 0);
    let green = Rgb::new(0, 255, 0);
    let mut attribute = TextAttribute::from_rgb(red, green);

    assert!(!attribute.is_reverse_video());
    assert_eq!(
        attribute
            .foreground()
            .resolve(&table, DEFAULT_FOREGROUND, false),
        red
    );
    assert_eq!(
        attribute
            .background()
            .resolve(&table, DEFAULT_BACKGROUND, false),
        green
    );
    assert_eq!(resolve(attribute, &table), colors(red, green));

    attribute.set_reverse_video(true);
    assert_eq!(resolve(attribute, &table), colors(green, red));

    attribute.set_reverse_video(false);
    attribute.set_faint(true);
    assert_eq!(resolve(attribute, &table), colors(faint_red, green));

    attribute.set_reverse_video(true);
    assert_eq!(resolve(attribute, &table), colors(green, faint_red));

    attribute.set_reverse_video(false);
    attribute.set_faint(false);
    attribute.set_invisible(true);
    assert_eq!(resolve(attribute, &table), colors(green, green));

    attribute.set_reverse_video(true);
    assert_eq!(resolve(attribute, &table), colors(red, red));
}

#[test]
fn microsoft_f04_reverse_default_colors_match_source_contract() {
    let table = color_table();
    let default_foreground = Rgb::new(1, 2, 3);
    let default_background = Rgb::new(4, 5, 6);
    let red = Rgb::new(255, 0, 0);
    let green = Rgb::new(0, 255, 0);
    let mut attribute = TextAttribute::default();

    assert!(!attribute.is_reverse_video());
    assert_eq!(
        attribute
            .foreground()
            .resolve(&table, DEFAULT_FOREGROUND, false),
        default_foreground
    );
    assert_eq!(
        attribute
            .background()
            .resolve(&table, DEFAULT_BACKGROUND, false),
        default_background
    );
    assert_eq!(
        resolve(attribute, &table),
        colors(default_foreground, default_background)
    );

    attribute.set_reverse_video(true);
    assert!(attribute.is_reverse_video());
    assert_eq!(
        resolve(attribute, &table),
        colors(default_background, default_foreground)
    );

    attribute.set_foreground(TextColor::rgb(red.r, red.g, red.b));
    assert!(attribute.is_reverse_video());
    assert_eq!(resolve(attribute, &table), colors(default_background, red));

    attribute.invert();
    assert!(!attribute.is_reverse_video());
    attribute.set_default_foreground();
    attribute.set_background(TextColor::rgb(green.r, green.g, green.b));
    assert_eq!(
        resolve(attribute, &table),
        colors(default_foreground, green)
    );
}

#[test]
fn microsoft_f04_intense_as_bright_matches_source_contract() {
    let table = color_table();
    let dark_black = table[0];
    let bright_black = table[8];
    let dark_green = table[2];
    let default_foreground = table[DEFAULT_FOREGROUND];
    let default_background = table[DEFAULT_BACKGROUND];
    let mut settings = RenderSettingsPolicy::default();
    let mut attribute = TextAttribute::default();

    assert!(!attribute.is_intense());
    assert_eq!(
        attribute
            .foreground()
            .resolve(&table, DEFAULT_FOREGROUND, false),
        default_foreground
    );
    assert_eq!(
        attribute
            .background()
            .resolve(&table, DEFAULT_BACKGROUND, false),
        default_background
    );

    settings.set_mode(RenderMode::IntenseIsBright, true);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(default_foreground, default_background)
    );
    settings.set_mode(RenderMode::IntenseIsBright, false);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(default_foreground, default_background)
    );

    attribute.set_intense(true);
    assert!(attribute.is_intense());
    settings.set_mode(RenderMode::IntenseIsBright, true);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(default_foreground, default_background)
    );
    settings.set_mode(RenderMode::IntenseIsBright, false);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(default_foreground, default_background)
    );

    attribute.set_foreground(TextColor::index16(TextColor::DARK_BLACK));
    assert!(attribute.is_intense());
    settings.set_mode(RenderMode::IntenseIsBright, true);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(bright_black, default_background)
    );
    settings.set_mode(RenderMode::IntenseIsBright, false);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(dark_black, default_background)
    );

    attribute.set_background(TextColor::index16(TextColor::DARK_GREEN));
    assert!(attribute.is_intense());
    settings.set_mode(RenderMode::IntenseIsBright, true);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(bright_black, dark_green)
    );
    settings.set_mode(RenderMode::IntenseIsBright, false);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(dark_black, dark_green)
    );

    attribute.set_intense(false);
    assert!(!attribute.is_intense());
    settings.set_mode(RenderMode::IntenseIsBright, true);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(dark_black, dark_green)
    );
    settings.set_mode(RenderMode::IntenseIsBright, false);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(dark_black, dark_green)
    );

    attribute.set_intense(true);
    attribute.set_foreground(TextColor::index16(TextColor::BRIGHT_BLACK));
    assert!(attribute.is_intense());
    settings.set_mode(RenderMode::IntenseIsBright, true);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(bright_black, dark_green)
    );
    settings.set_mode(RenderMode::IntenseIsBright, false);
    assert_eq!(
        resolve_with_settings(attribute, &table, settings),
        colors(bright_black, dark_green)
    );

    settings.set_mode(RenderMode::IntenseIsBright, true);
    assert!(settings.mode(RenderMode::IntenseIsBright));
}
