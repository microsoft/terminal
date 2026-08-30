//! Safe, platform-neutral core for DEC Sixel parsing and rasterization.
//!
//! The C++ `SixelParser` mixes protocol parsing with `TextBuffer`, renderer,
//! scrolling, and image-slice integration. R03a isolates the deterministic core:
//! command parsing, conformance rules, palette mapping, raster attributes, and
//! indexed-pixel generation. Buffer flushing and terminal integration remain at
//! the adapter boundary for later R03 increments.

use terminal_parser::state_machine::MAX_PARAMETER_VALUE;

pub const DEFAULT_CONFORMANCE: i32 = 9;
const MAX_COLORS: usize = 256;
const SIXEL_ROWS: usize = 6;
const ESC: u16 = 0x1b;

const VT340_COLORS: [Rgb; 16] = [
    Rgb::new(0x00, 0x00, 0x00),
    Rgb::new(0x33, 0x33, 0xcc),
    Rgb::new(0xcc, 0x24, 0x24),
    Rgb::new(0x33, 0xcc, 0x33),
    Rgb::new(0xcc, 0x33, 0xcc),
    Rgb::new(0x33, 0xcc, 0xcc),
    Rgb::new(0xcc, 0xcc, 0x33),
    Rgb::new(0x78, 0x78, 0x78),
    Rgb::new(0x45, 0x45, 0x45),
    Rgb::new(0x57, 0x57, 0x99),
    Rgb::new(0x99, 0x45, 0x45),
    Rgb::new(0x57, 0x99, 0x57),
    Rgb::new(0x99, 0x57, 0x99),
    Rgb::new(0x57, 0x99, 0x99),
    Rgb::new(0x99, 0x99, 0x57),
    Rgb::new(0xcc, 0xcc, 0xcc),
];

const XTERM_LEVELS: [u8; 6] = [0, 95, 135, 175, 215, 255];

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Size {
    pub width: usize,
    pub height: usize,
}

impl Size {
    #[must_use]
    pub const fn new(width: usize, height: usize) -> Self {
        Self { width, height }
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Point {
    pub x: usize,
    pub y: usize,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Rgb {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl Rgb {
    #[must_use]
    pub const fn new(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Pixel {
    pub transparent: bool,
    pub color_index: u8,
}

impl Pixel {
    const TRANSPARENT: Self = Self {
        transparent: true,
        color_index: 0,
    };

    const BACKGROUND: Self = Self {
        transparent: false,
        color_index: 0,
    };

    const fn foreground(color_index: u8) -> Self {
        Self {
            transparent: false,
            color_index,
        }
    }
}

impl Default for Pixel {
    fn default() -> Self {
        Self::BACKGROUND
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum Background {
    #[default]
    Default,
    Transparent,
    Opaque,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum CommandState {
    Normal,
    Attributes,
    Color,
    Repeat,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Config {
    pub conformance_level: i32,
    pub macro_parameter: i32,
    pub background: Background,
    pub background_color: Option<i32>,
    pub canvas: Size,
}

impl Config {
    #[must_use]
    pub const fn new(canvas: Size) -> Self {
        Self {
            conformance_level: DEFAULT_CONFORMANCE,
            macro_parameter: 0,
            background: Background::Default,
            background_color: None,
            canvas,
        }
    }
}

#[derive(Debug, Clone)]
pub struct Parser {
    conformance_level: i32,
    cell_size: Size,
    max_colors: usize,
    display_mode: bool,
    state: CommandState,
    parameters: Vec<Option<i32>>,
    canvas: Size,
    max_pixel_aspect_ratio: usize,
    pixel_aspect_ratio: usize,
    sixel_height: usize,
    segment_height: usize,
    available_height: usize,
    background_size: Size,
    background_fill_required: bool,
    filled_background_height: Option<usize>,
    color_map: [u8; MAX_COLORS],
    color_map_used: [bool; MAX_COLORS],
    color_table: [Rgb; MAX_COLORS],
    colors_used: usize,
    colors_available: usize,
    foreground_pixel: Pixel,
    image_buffer: Vec<Pixel>,
    image_cursor: Point,
    image_width: usize,
    image_line_count: usize,
}

impl Parser {
    #[must_use]
    pub fn new(config: Config) -> Self {
        let cell_size = cell_size_for_level(config.conformance_level);
        let max_colors = max_colors_for_level(config.conformance_level);
        let max_pixel_aspect_ratio = (config.canvas.height / SIXEL_ROWS).max(1);
        let mut parser = Self {
            conformance_level: config.conformance_level,
            cell_size,
            max_colors,
            display_mode: true,
            state: CommandState::Normal,
            parameters: Vec::new(),
            canvas: config.canvas,
            max_pixel_aspect_ratio,
            pixel_aspect_ratio: 1,
            sixel_height: SIXEL_ROWS,
            segment_height: SIXEL_ROWS,
            available_height: config.canvas.height,
            background_size: Size::new(usize::MAX, usize::MAX),
            background_fill_required: false,
            filled_background_height: None,
            color_map: [0; MAX_COLORS],
            color_map_used: [false; MAX_COLORS],
            color_table: initial_color_table(),
            colors_used: 0,
            colors_available: max_colors,
            foreground_pixel: Pixel::BACKGROUND,
            image_buffer: Vec::new(),
            image_cursor: Point::default(),
            image_width: 0,
            image_line_count: 0,
        };
        parser.init_raster_attributes(config.macro_parameter, config.background);
        parser.init_color_map(config.background_color);
        parser.init_image_buffer();
        parser
    }

    /// Starts a new image while preserving terminal-scoped Sixel state.
    ///
    /// Windows Terminal keeps one `SixelParser` alive across DCS image
    /// definitions, so palette changes survive into later images. The
    /// per-image color-number map and raster state are rebuilt here.
    pub fn restart_image(&mut self, config: Config) {
        if self.conformance_level != config.conformance_level {
            self.conformance_level = config.conformance_level;
            self.cell_size = cell_size_for_level(config.conformance_level);
            self.max_colors = max_colors_for_level(config.conformance_level);
            self.color_table = initial_color_table();
            if self.conformance_level < 3 {
                self.display_mode = true;
            }
        }

        self.state = CommandState::Normal;
        self.parameters.clear();
        self.canvas = config.canvas;
        self.max_pixel_aspect_ratio = (config.canvas.height / SIXEL_ROWS).max(1);
        self.available_height = config.canvas.height;
        self.init_raster_attributes(config.macro_parameter, config.background);
        self.init_color_map(config.background_color);
        self.init_image_buffer();
    }

    pub fn set_display_mode(&mut self, enabled: bool) {
        if self.conformance_level >= 3 {
            self.display_mode = enabled;
        }
    }

    #[must_use]
    pub const fn display_mode(&self) -> bool {
        self.display_mode
    }

    #[must_use]
    pub const fn pixel_aspect_ratio(&self) -> usize {
        self.pixel_aspect_ratio
    }

    #[must_use]
    pub const fn sixel_height(&self) -> usize {
        self.sixel_height
    }

    #[must_use]
    pub const fn cell_size(&self) -> Size {
        self.cell_size
    }

    #[must_use]
    pub const fn image_width(&self) -> usize {
        self.image_width
    }

    #[must_use]
    pub fn image_height(&self) -> usize {
        self.image_buffer
            .len()
            .checked_div(self.canvas.width)
            .unwrap_or(0)
    }

    #[must_use]
    pub const fn image_cursor(&self) -> Point {
        self.image_cursor
    }

    #[must_use]
    pub const fn image_line_count(&self) -> usize {
        self.image_line_count
    }

    #[must_use]
    pub fn pixel(&self, x: usize, y: usize) -> Option<Pixel> {
        if x >= self.canvas.width {
            return None;
        }
        y.checked_mul(self.canvas.width)
            .and_then(|row| row.checked_add(x))
            .and_then(|index| self.image_buffer.get(index))
            .copied()
    }

    #[must_use]
    pub fn palette_color(&self, index: usize) -> Option<Rgb> {
        self.color_table.get(index).copied()
    }

    #[must_use]
    pub const fn foreground_pixel(&self) -> Pixel {
        self.foreground_pixel
    }

    pub fn put(&mut self, code_unit: u16) {
        if (u16::from(b'?')..=u16::from(b'~')).contains(&code_unit) {
            let repeat_count = self.apply_pending_command();
            let sixel_value = code_unit - u16::from(b'?');
            self.write_to_image_buffer(sixel_value, repeat_count);
        } else if (u16::from(b'0')..=u16::from(b'9')).contains(&code_unit)
            || code_unit == u16::from(b';')
        {
            self.parse_parameter_char(code_unit);
        } else {
            self.parse_command(code_unit);
        }
    }

    pub fn put_str(&mut self, input: &str) {
        for byte in input.bytes() {
            self.put(u16::from(byte));
        }
    }

    pub fn finish(&mut self) {
        self.put(ESC);
    }

    fn parse_command(&mut self, code_unit: u16) {
        match code_unit {
            value if value == u16::from(b'#') => {
                self.apply_pending_command();
                self.state = CommandState::Color;
                self.parameters.clear();
            }
            value if value == u16::from(b'!') => {
                self.apply_pending_command();
                self.state = CommandState::Repeat;
                self.parameters.clear();
            }
            value if value == u16::from(b'$') => {
                self.apply_pending_command();
                self.execute_carriage_return();
            }
            value if value == u16::from(b'-') => {
                self.apply_pending_command();
                self.execute_next_line();
            }
            value if value == u16::from(b'+') && self.conformance_level == 2 => {
                self.apply_pending_command();
                self.execute_move_to_home();
            }
            value if value == u16::from(b'"') && self.conformance_level >= 3 => {
                self.apply_pending_command();
                self.state = CommandState::Attributes;
                self.parameters.clear();
            }
            ESC => {
                if self.state == CommandState::Color {
                    self.apply_pending_command();
                }
                self.fill_image_background();
                self.execute_carriage_return();
            }
            _ => {}
        }
    }

    fn parse_parameter_char(&mut self, code_unit: u16) {
        if self.parameters.len() > 5 {
            return;
        }
        if self.parameters.is_empty() {
            self.parameters.push(None);
        }
        if code_unit == u16::from(b';') {
            self.parameters.push(None);
            return;
        }
        let digit = i32::from(code_unit - u16::from(b'0'));
        let current = self.parameters.last().copied().flatten().unwrap_or(0);
        let value = current
            .saturating_mul(10)
            .saturating_add(digit)
            .min(MAX_PARAMETER_VALUE);
        if let Some(parameter) = self.parameters.last_mut() {
            *parameter = Some(value);
        }
    }

    fn apply_pending_command(&mut self) -> usize {
        if self.state == CommandState::Normal {
            return 1;
        }
        let previous = self.state;
        self.state = CommandState::Normal;
        match previous {
            CommandState::Color => {
                self.define_color();
                1
            }
            CommandState::Repeat => numeric_parameter(self.parameters.first().copied().flatten()),
            CommandState::Attributes => {
                self.update_raster_attributes();
                1
            }
            CommandState::Normal => 1,
        }
    }

    fn execute_carriage_return(&mut self) {
        self.image_width = self.image_width.max(self.image_cursor.x);
        self.image_cursor.x = 0;
    }

    fn execute_next_line(&mut self) {
        self.execute_carriage_return();
        self.image_line_count += 1;
        if self.available_height > 0 {
            self.image_cursor.y = self.image_cursor.y.saturating_add(self.sixel_height);
            self.available_height = self.available_height.saturating_sub(self.sixel_height);
            self.resize_image_buffer(self.sixel_height);
            self.fill_image_background_when_extended();
        }
    }

    fn execute_move_to_home(&mut self) {
        self.execute_carriage_return();
        self.image_cursor.y = 0;
        self.available_height = self.canvas.height;
    }

    fn init_raster_attributes(&mut self, macro_parameter: i32, background: Background) {
        self.pixel_aspect_ratio = if self.conformance_level < 3 {
            2
        } else {
            match macro_parameter {
                0 | 1 | 5 | 6 => 2,
                2 => 5,
                3 | 4 => 3,
                _ => 1,
            }
        };
        self.sixel_height = SIXEL_ROWS * self.pixel_aspect_ratio;
        self.segment_height = self.sixel_height;
        self.background_fill_required =
            self.conformance_level == 1 || background != Background::Transparent;
        self.background_size = Size::new(usize::MAX, usize::MAX);
        self.filled_background_height = None;
    }

    fn update_raster_attributes(&mut self) {
        let y_aspect = parameter_or(&self.parameters, 0, 0);
        let x_aspect = parameter_or(&self.parameters, 1, 0);
        if x_aspect > 0 {
            let numerator = y_aspect.saturating_add(x_aspect - 1);
            let requested = usize::try_from(numerator / x_aspect).unwrap_or(1);
            self.pixel_aspect_ratio = requested.clamp(1, self.max_pixel_aspect_ratio);
            self.sixel_height = SIXEL_ROWS * self.pixel_aspect_ratio;
            self.segment_height = self.segment_height.max(self.sixel_height);
            self.resize_image_buffer(self.sixel_height);
        }

        let width = parameter_or(&self.parameters, 2, 0);
        let height = parameter_or(&self.parameters, 3, 0);
        if let Ok(width) = usize::try_from(width)
            && width > 0
        {
            self.background_size.width = width;
        }
        if let Ok(height) = usize::try_from(height)
            && height > 0
        {
            self.background_size.height = height;
        }
        self.fill_image_background_when_extended();
        self.execute_carriage_return();
    }

    fn init_color_map(&mut self, background_color: Option<i32>) {
        self.colors_used = 0;
        self.colors_available = self.max_colors;
        for (color_number, slot) in self.color_map.iter_mut().enumerate() {
            *slot = u8::try_from(color_number % self.max_colors).unwrap_or(0);
        }
        self.color_map_used.fill(false);

        if self.conformance_level == 2
            && let Some(color_number) =
                background_color.and_then(|value| usize::try_from(value).ok())
        {
            if color_number < self.max_colors {
                self.color_map[color_number] = 0;
                self.color_map_used[color_number] = true;
            } else {
                self.colors_available = self.max_colors - 1;
            }
        }

        let default_color_index = (self.max_colors - 1).min(15);
        self.foreground_pixel = Pixel::foreground(u8::try_from(default_color_index).unwrap_or(0));
    }

    fn define_color(&mut self) {
        let raw_number = parameter_or(&self.parameters, 0, 0);
        let color_number = usize::try_from(raw_number).unwrap_or(0) % MAX_COLORS;

        if self.parameters.len() > 1 && self.conformance_level > 1 {
            let model = parameter_or(&self.parameters, 1, 0);
            let x = parameter_or(&self.parameters, 2, 0);
            let y = parameter_or(&self.parameters, 3, 0);
            let z = parameter_or(&self.parameters, 4, 0);
            let color = match model {
                1 => Some(color_from_hls(x, y, z)),
                2 => Some(color_from_rgb100(x, y, z)),
                _ => None,
            };
            if let Some(color) = color {
                self.define_palette_color(color_number, color);
            }
        }

        self.foreground_pixel = Pixel::foreground(self.color_map[color_number]);
    }

    fn define_palette_color(&mut self, color_number: usize, color: Rgb) {
        if self.color_map_used[color_number] {
            let table_index = usize::from(self.color_map[color_number]);
            self.color_table[table_index] = color;
        } else if self.colors_used < self.colors_available {
            self.colors_used += 1;
            let table_index = self.colors_used % self.max_colors;
            self.color_map[color_number] = u8::try_from(table_index).unwrap_or(0);
            self.color_table[table_index] = color;
        } else if self.conformance_level == 2 {
            let mut table_index = 0;
            let mut best_difference = i32::MAX;
            for index in 0..self.max_colors {
                let difference = color_distance_squared(self.color_table[index], color);
                if difference <= best_difference {
                    best_difference = difference;
                    table_index = index;
                }
            }
            self.color_map[color_number] = u8::try_from(table_index).unwrap_or(0);
        }
        self.color_map_used[color_number] = true;
    }

    fn init_image_buffer(&mut self) {
        self.image_buffer.clear();
        self.image_cursor = Point::default();
        self.image_width = 0;
        self.image_line_count = 0;
        self.resize_image_buffer(self.sixel_height);
        if self.conformance_level < 3 {
            self.fill_image_background();
        }
    }

    fn resize_image_buffer(&mut self, required_height: usize) {
        let rows = self.image_cursor.y.saturating_add(required_height);
        let required_size = rows.saturating_mul(self.canvas.width);
        if required_size > self.image_buffer.len() {
            self.image_buffer.resize(required_size, Pixel::TRANSPARENT);
        }
    }

    fn fill_image_background(&mut self) {
        if !self.background_fill_required {
            return;
        }
        self.background_fill_required = false;
        let height = self.background_size.height.min(self.available_height);
        self.resize_image_buffer(height);
        self.fill_background_rows(height);
        self.filled_background_height =
            Some(self.image_cursor.y.saturating_add(self.available_height));
        self.fill_image_background_when_extended();
    }

    fn fill_background_rows(&mut self, height: usize) {
        let width = self.background_size.width.min(self.canvas.width);
        for y in self.image_cursor.y..self.image_cursor.y.saturating_add(height) {
            let Some(row_start) = y.checked_mul(self.canvas.width) else {
                break;
            };
            let Some(row_end) = row_start.checked_add(width) else {
                break;
            };
            let Some(row) = self.image_buffer.get_mut(row_start..row_end) else {
                break;
            };
            row.fill(Pixel::BACKGROUND);
        }
        self.image_width = self.image_width.max(width);
    }

    fn fill_image_background_when_extended(&mut self) {
        let Some(filled_height) = self.filled_background_height else {
            return;
        };
        let image_height = self.image_cursor.y.saturating_add(self.sixel_height);
        if image_height <= filled_height {
            return;
        }
        let new_height = round_up(image_height, self.cell_size.height);
        self.filled_background_height = Some(new_height);
        let additional_height = new_height.saturating_sub(self.image_cursor.y);
        self.resize_image_buffer(additional_height);
        self.fill_background_rows(additional_height);
    }

    fn write_to_image_buffer(&mut self, sixel_value: u16, repeat_count: usize) {
        self.fill_image_background();
        let remaining_width = self.canvas.width.saturating_sub(self.image_cursor.x);
        let repeat_count = repeat_count.min(remaining_width);
        if repeat_count == 0 {
            return;
        }
        if sixel_value == 0 {
            self.image_cursor.x += repeat_count;
            return;
        }

        for bit in 0..SIXEL_ROWS {
            if sixel_value & (1_u16 << bit) == 0 {
                continue;
            }
            let first_row = self.image_cursor.y + bit * self.pixel_aspect_ratio;
            for row in first_row..first_row + self.pixel_aspect_ratio {
                let Some(row_start) = row.checked_mul(self.canvas.width) else {
                    continue;
                };
                let first_column = row_start.saturating_add(self.image_cursor.x);
                let end_column = first_column.saturating_add(repeat_count);
                if let Some(pixels) = self.image_buffer.get_mut(first_column..end_column) {
                    pixels.fill(self.foreground_pixel);
                }
            }
        }
        self.image_cursor.x += repeat_count;
    }
}

#[must_use]
pub const fn cell_size_for_level(conformance_level: i32) -> Size {
    if conformance_level == 1 {
        Size::new(9, 20)
    } else {
        Size::new(10, 20)
    }
}

#[must_use]
pub const fn max_colors_for_level(conformance_level: i32) -> usize {
    match conformance_level {
        1 | 2 => 4,
        3 => 16,
        _ => MAX_COLORS,
    }
}

#[must_use]
pub fn color_from_rgb100(r: i32, g: i32, b: i32) -> Rgb {
    Rgb::new(
        scale_100_to_255(r),
        scale_100_to_255(g),
        scale_100_to_255(b),
    )
}

#[must_use]
pub fn color_from_hls(h: i32, l: i32, s: i32) -> Rgb {
    let hue = h.rem_euclid(360);
    let luminosity = f32::from(u8::try_from(l.clamp(0, 100)).unwrap_or(0));
    let saturation = f32::from(u8::try_from(s.clamp(0, 100)).unwrap_or(0));
    let chroma = (50.0_f32 - (luminosity - 50.0_f32).abs()) * saturation / 50.0_f32;
    let x_factor = f32::from(u8::try_from(60 - (hue % 120 - 60).abs()).unwrap_or(0));
    let x = chroma * x_factor / 60.0_f32;
    let lightness = luminosity - chroma / 2.0_f32;

    let bright = hls_component(chroma + lightness);
    let middle = hls_component(x + lightness);
    let dark = hls_component(lightness);

    match hue {
        0..=59 => Rgb::new(middle, dark, bright),
        60..=119 => Rgb::new(bright, dark, middle),
        120..=179 => Rgb::new(bright, middle, dark),
        180..=239 => Rgb::new(middle, bright, dark),
        240..=299 => Rgb::new(dark, bright, middle),
        _ => Rgb::new(dark, middle, bright),
    }
}

fn parameter_or(parameters: &[Option<i32>], index: usize, default: i32) -> i32 {
    parameters.get(index).copied().flatten().unwrap_or(default)
}

fn numeric_parameter(parameter: Option<i32>) -> usize {
    match parameter {
        Some(value) if value > 0 => usize::try_from(value).unwrap_or(1),
        _ => 1,
    }
}

fn scale_100_to_255(value: i32) -> u8 {
    let clamped = value.clamp(0, 100);
    let scaled = (clamped * 255 + 50) / 100;
    u8::try_from(scaled).unwrap_or(0)
}

#[expect(
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    reason = "DEC HLS conversion intentionally matches the C++ f32-to-byte rounding path"
)]
fn hls_component(value: f32) -> u8 {
    (value * (255.0_f32 / 100.0_f32) + 0.5_f32).clamp(0.0_f32, 255.0_f32) as u8
}

fn color_distance_squared(left: Rgb, right: Rgb) -> i32 {
    let red = i32::from(left.r) - i32::from(right.r);
    let green = i32::from(left.g) - i32::from(right.g);
    let blue = i32::from(left.b) - i32::from(right.b);
    red * red + green * green + blue * blue
}

fn round_up(value: usize, multiple: usize) -> usize {
    if multiple == 0 {
        return value;
    }
    value
        .saturating_add(multiple - 1)
        .checked_div(multiple)
        .unwrap_or(0)
        .saturating_mul(multiple)
}

fn initial_color_table() -> [Rgb; MAX_COLORS] {
    let mut table = [Rgb::default(); MAX_COLORS];
    table[..VT340_COLORS.len()].copy_from_slice(&VT340_COLORS);

    let mut index = 16;
    for red in XTERM_LEVELS {
        for green in XTERM_LEVELS {
            for blue in XTERM_LEVELS {
                table[index] = Rgb::new(red, green, blue);
                index += 1;
            }
        }
    }
    for gray in 0_u8..24 {
        let component = 8 + gray * 10;
        table[232 + usize::from(gray)] = Rgb::new(component, component, component);
    }
    table
}

#[cfg(test)]
mod tests {
    use super::*;

    fn transparent_config(width: usize, height: usize) -> Config {
        let mut config = Config::new(Size::new(width, height));
        config.background = Background::Transparent;
        config.macro_parameter = 7;
        config
    }

    #[test]
    fn conformance_helpers_match_dec_levels() {
        assert_eq!(cell_size_for_level(1), Size::new(9, 20));
        assert_eq!(cell_size_for_level(2), Size::new(10, 20));
        assert_eq!(max_colors_for_level(1), 4);
        assert_eq!(max_colors_for_level(2), 4);
        assert_eq!(max_colors_for_level(3), 16);
        assert_eq!(max_colors_for_level(DEFAULT_CONFORMANCE), 256);
    }

    #[test]
    fn display_mode_can_only_be_reset_from_level_three() {
        let mut level_two = Config::new(Size::new(4, 20));
        level_two.conformance_level = 2;
        let mut parser = Parser::new(level_two);
        parser.set_display_mode(false);
        assert!(parser.display_mode());

        let mut level_three = Config::new(Size::new(4, 20));
        level_three.conformance_level = 3;
        let mut parser = Parser::new(level_three);
        parser.set_display_mode(false);
        assert!(!parser.display_mode());
    }

    #[test]
    fn macro_parameter_controls_initial_aspect_ratio() {
        for (macro_parameter, expected) in [(0, 2), (2, 5), (3, 3), (7, 1), (99, 1)] {
            let mut config = transparent_config(4, 60);
            config.macro_parameter = macro_parameter;
            let parser = Parser::new(config);
            assert_eq!(parser.pixel_aspect_ratio(), expected);
        }

        let mut config = transparent_config(4, 60);
        config.conformance_level = 2;
        config.macro_parameter = 7;
        assert_eq!(Parser::new(config).pixel_aspect_ratio(), 2);
    }

    #[test]
    fn sixel_bits_render_vertical_pixels_without_unsafe_memory_access() {
        let mut parser = Parser::new(transparent_config(4, 20));
        parser.put_str("@A");
        parser.finish();

        let foreground = parser.foreground_pixel();
        assert_eq!(parser.image_width(), 2);
        assert_eq!(parser.pixel(0, 0), Some(foreground));
        assert_eq!(parser.pixel(0, 1), Some(Pixel::TRANSPARENT));
        assert_eq!(parser.pixel(1, 0), Some(Pixel::TRANSPARENT));
        assert_eq!(parser.pixel(1, 1), Some(foreground));
    }

    #[test]
    fn repeat_defaults_to_one_and_clamps_to_canvas_width() {
        let mut parser = Parser::new(transparent_config(4, 20));
        parser.put_str("!0@!99A");
        parser.finish();

        assert_eq!(parser.image_width(), 4);
        assert!(!parser.pixel(0, 0).unwrap_or(Pixel::TRANSPARENT).transparent);
        for x in 1..4 {
            assert!(!parser.pixel(x, 1).unwrap_or(Pixel::TRANSPARENT).transparent);
        }
    }

    #[test]
    fn carriage_return_allows_sixels_to_overprint() {
        let mut parser = Parser::new(transparent_config(2, 20));
        parser.put_str("@$A");
        parser.finish();

        assert_eq!(parser.image_width(), 1);
        assert!(!parser.pixel(0, 0).unwrap_or(Pixel::TRANSPARENT).transparent);
        assert!(!parser.pixel(0, 1).unwrap_or(Pixel::TRANSPARENT).transparent);
    }

    #[test]
    fn graphics_next_line_advances_by_current_sixel_height() {
        let mut parser = Parser::new(transparent_config(2, 20));
        parser.put_str("@-@");
        parser.finish();

        assert_eq!(parser.sixel_height(), 6);
        assert_eq!(parser.image_line_count(), 1);
        assert_eq!(parser.pixel(0, 0), Some(parser.foreground_pixel()));
        assert_eq!(parser.pixel(0, 6), Some(parser.foreground_pixel()));
    }

    #[test]
    fn raster_attributes_round_ratio_up_and_trigger_carriage_return() {
        let mut parser = Parser::new(transparent_config(4, 60));
        parser.put_str("@@\"5;2;3;18@");
        parser.finish();

        assert_eq!(parser.pixel_aspect_ratio(), 3);
        assert_eq!(parser.sixel_height(), 18);
        // Raster attributes perform a carriage return after the first two sixels,
        // so the final sixel overprints from column zero instead of extending the width.
        assert_eq!(parser.image_width(), 2);
        assert_eq!(parser.image_cursor().x, 0);
    }

    #[test]
    fn invalid_zero_x_aspect_keeps_previous_ratio() {
        let mut parser = Parser::new(transparent_config(4, 60));
        parser.put_str("\"5;0@");
        parser.finish();
        assert_eq!(parser.pixel_aspect_ratio(), 1);
    }

    #[test]
    fn color_definition_allocates_and_reuses_palette_entry() {
        let mut parser = Parser::new(transparent_config(2, 20));
        parser.put_str("#2;2;100;0;0@");
        let first = parser.foreground_pixel();
        assert_eq!(first.color_index, 1);
        assert_eq!(parser.palette_color(1), Some(Rgb::new(255, 0, 0)));

        parser.put_str("$#2;2;0;100;0A");
        assert_eq!(parser.foreground_pixel().color_index, 1);
        assert_eq!(parser.palette_color(1), Some(Rgb::new(0, 255, 0)));
    }

    #[test]
    fn color_number_parameter_saturates_like_vt_parameters() {
        let mut parser = Parser::new(transparent_config(1, 20));
        parser.put_str("#999999@");
        parser.finish();
        assert_eq!(parser.foreground_pixel().color_index, 255);
    }

    #[test]
    fn rgb_percentage_conversion_matches_windows_terminal_rounding() {
        assert_eq!(color_from_rgb100(0, 50, 100), Rgb::new(0, 128, 255));
        assert_eq!(color_from_rgb100(101, 0, 25), Rgb::new(255, 0, 64));
    }

    #[test]
    fn hls_uses_dec_hue_orientation() {
        assert_eq!(color_from_hls(0, 50, 100), Rgb::new(0, 0, 255));
        assert_eq!(color_from_hls(120, 50, 100), Rgb::new(255, 0, 0));
        assert_eq!(color_from_hls(240, 50, 100), Rgb::new(0, 255, 0));
    }

    #[test]
    fn transparent_background_preserves_unset_pixels() {
        let mut parser = Parser::new(transparent_config(3, 12));
        parser.put(u16::from(b'@'));
        parser.finish();
        assert_eq!(parser.pixel(1, 0), Some(Pixel::TRANSPARENT));
    }

    #[test]
    fn opaque_background_uses_color_zero_and_raster_dimensions() {
        let mut config = Config::new(Size::new(4, 20));
        config.macro_parameter = 7;
        let mut parser = Parser::new(config);
        parser.put_str("\"1;1;2;3@");
        parser.finish();

        assert_eq!(parser.image_width(), 2);
        assert_eq!(parser.pixel(1, 2), Some(Pixel::BACKGROUND));
        assert_eq!(parser.pixel(2, 2), Some(Pixel::TRANSPARENT));
    }

    #[test]
    fn level_one_always_fills_background() {
        let mut config = transparent_config(3, 20);
        config.conformance_level = 1;
        let parser = Parser::new(config);
        assert_eq!(parser.image_width(), 3);
        assert_eq!(parser.pixel(2, 5), Some(Pixel::BACKGROUND));
    }

    #[test]
    fn level_two_home_command_returns_to_first_raster_row() {
        let mut config = transparent_config(2, 40);
        config.conformance_level = 2;
        let mut parser = Parser::new(config);
        parser.put_str("@-@+A");
        parser.finish();

        assert!(!parser.pixel(0, 0).unwrap_or(Pixel::TRANSPARENT).transparent);
        assert!(!parser.pixel(0, 1).unwrap_or(Pixel::TRANSPARENT).transparent);
        assert!(
            !parser
                .pixel(0, 12)
                .unwrap_or(Pixel::TRANSPARENT)
                .transparent
        );
    }

    #[test]
    fn xterm_extended_palette_is_initialized_after_vt340_entries() {
        let parser = Parser::new(transparent_config(1, 20));
        assert_eq!(parser.palette_color(0), Some(VT340_COLORS[0]));
        assert_eq!(parser.palette_color(15), Some(VT340_COLORS[15]));
        assert_eq!(parser.palette_color(16), Some(Rgb::new(0, 0, 0)));
        assert_eq!(parser.palette_color(21), Some(Rgb::new(0, 0, 255)));
        assert_eq!(parser.palette_color(232), Some(Rgb::new(8, 8, 8)));
        assert_eq!(parser.palette_color(255), Some(Rgb::new(238, 238, 238)));
    }
}
