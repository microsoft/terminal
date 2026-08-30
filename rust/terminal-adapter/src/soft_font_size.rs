//! Portable DECDLD/DRCS cell-size inference from Microsoft's `FontBuffer`.
//!
//! The adapter's `SoftFontSizeDetection` contract is a pure sizing algorithm:
//! it validates the declared matrix/font-set/usage tuple and resolves a target
//! terminal cell size from explicit dimensions or the observed sixel extent.
//! Renderer upload and glyph storage are intentionally outside this owner.

const MAX_WIDTH: u16 = 16;
const MAX_HEIGHT: u16 = 32;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CellMatrix {
    Default,
    Invalid,
    Size5x10,
    Size6x10,
    Size7x10,
    Explicit(u16),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FontSet {
    Default,
    Size80x24,
    Size80x36,
    Size80x48,
    Size132x24,
    Size132x36,
    Size132x48,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FontUsage {
    Default,
    Text,
    FullCell,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CellSize {
    pub width: u16,
    pub height: u16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct Attributes {
    columns_per_page: u16,
    lines_per_page: u16,
    text_font: bool,
    size_declared_as_matrix: bool,
    declared_width: u16,
    declared_height: u16,
}

/// Resolves the final cell size for one Microsoft DECDLD sizing vector.
///
/// `sixel_data` is the DRCS bitmap payload after the charset identifier. For
/// size detection only its maximum sixel-row width and six-pixel row count are
/// relevant, matching `FontBuffer::_usedWidth/_usedHeight`.
#[must_use]
pub fn detect_cell_size(
    matrix: CellMatrix,
    cell_height: u16,
    font_set: FontSet,
    usage: FontUsage,
    sixel_data: &str,
) -> Option<CellSize> {
    let attributes = validate_attributes(matrix, cell_height, font_set, usage)?;
    let (used_width, used_height) = sixel_extent(sixel_data);
    let (width, height, _) = calculate_dimensions(attributes, used_width, used_height);
    Some(CellSize { width, height })
}

fn validate_attributes(
    matrix: CellMatrix,
    cell_height: u16,
    font_set: FontSet,
    usage: FontUsage,
) -> Option<Attributes> {
    let (columns_per_page, lines_per_page) = match font_set {
        FontSet::Default | FontSet::Size80x24 => (80, 24),
        FontSet::Size80x36 => (80, 36),
        FontSet::Size80x48 => (80, 48),
        FontSet::Size132x24 => (132, 24),
        FontSet::Size132x36 => (132, 36),
        FontSet::Size132x48 => (132, 48),
    };
    let text_font = matches!(usage, FontUsage::Default | FontUsage::Text);

    let (size_declared_as_matrix, declared_width, declared_height) = match matrix {
        CellMatrix::Invalid => return None,
        CellMatrix::Size5x10 => {
            if !text_font {
                return None;
            }
            (true, 5, 10)
        }
        CellMatrix::Size6x10 => {
            if !text_font && columns_per_page != 132 {
                return None;
            }
            (true, 6, 10)
        }
        CellMatrix::Size7x10 => {
            if !text_font {
                return None;
            }
            (true, 7, 10)
        }
        CellMatrix::Default => (false, 0, cell_height),
        CellMatrix::Explicit(width) => {
            if width > MAX_WIDTH || cell_height > MAX_HEIGHT {
                return None;
            }
            (false, width, cell_height)
        }
    };

    if declared_width > MAX_WIDTH || declared_height > MAX_HEIGHT {
        return None;
    }

    Some(Attributes {
        columns_per_page,
        lines_per_page,
        text_font,
        size_declared_as_matrix,
        declared_width,
        declared_height,
    })
}

fn sixel_extent(data: &str) -> (u16, u16) {
    if data.is_empty() {
        return (0, 6);
    }

    let mut used_width = 0_u16;
    let mut rows = 1_u16;
    let mut current_width = 0_u16;
    for ch in data.chars() {
        match ch {
            '?'..='~' => current_width = current_width.saturating_add(1),
            '/' => {
                used_width = used_width.max(current_width);
                current_width = 0;
                rows = rows.saturating_add(1);
            }
            ';' => {
                used_width = used_width.max(current_width);
                current_width = 0;
                rows = rows.max(1);
            }
            _ => {}
        }
    }
    used_width = used_width.max(current_width);
    (used_width, rows.saturating_mul(6))
}

fn calculate_dimensions(
    attributes: Attributes,
    used_width: u16,
    used_height: u16,
) -> (u16, u16, u16) {
    if attributes.size_declared_as_matrix {
        return if attributes.columns_per_page == 132 && attributes.declared_width <= 6 {
            (6, 10, 0)
        } else {
            (10, 10, 8)
        };
    }

    if attributes.declared_width != 0 && attributes.declared_height != 0 && !attributes.text_font {
        return (attributes.declared_width, attributes.declared_height, 0);
    }

    let text_width = if attributes.text_font {
        attributes.declared_width
    } else {
        0
    };

    if attributes.lines_per_page != 24 {
        let cell_width = if attributes.columns_per_page == 132 {
            6
        } else {
            10
        };
        let cell_height = if attributes.lines_per_page == 48 {
            8
        } else {
            10
        };
        return (cell_width, cell_height, text_width);
    }

    let in_range = |cell_width: u16, cell_height: u16| {
        let sixel_height = cell_height.div_ceil(6) * 6;
        let height_in_range = if attributes.declared_height != 0 {
            attributes.declared_height <= cell_height
        } else {
            used_height <= sixel_height
        };
        let width_in_range = if attributes.declared_width != 0 {
            attributes.declared_width <= cell_width
        } else {
            used_width <= cell_width
        };
        height_in_range && width_in_range
    };
    let no_declared_size = attributes.declared_width == 0 && attributes.declared_height == 0;

    if attributes.columns_per_page == 80 {
        if in_range(8, 10) && no_declared_size {
            (10, 10, 8)
        } else if in_range(15, 12) {
            (15, 12, text_width)
        } else if in_range(10, 16) {
            (10, 16, text_width)
        } else if in_range(10, 20) {
            (10, 20, text_width)
        } else if in_range(12, 30) {
            (12, 30, text_width)
        } else {
            (MAX_WIDTH, MAX_HEIGHT, text_width)
        }
    } else if in_range(6, 10) && no_declared_size {
        (6, 10, 0)
    } else if in_range(9, 12) {
        (9, 12, text_width)
    } else if in_range(6, 16) {
        (6, 16, text_width)
    } else if in_range(6, 20) {
        (6, 20, text_width)
    } else if in_range(7, 30) {
        (7, 30, text_width)
    } else {
        (MAX_WIDTH, MAX_HEIGHT, text_width)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cell(
        matrix: CellMatrix,
        height: u16,
        set: FontSet,
        usage: FontUsage,
        data: &str,
    ) -> Option<(u16, u16)> {
        detect_cell_size(matrix, height, set, usage, data).map(|size| (size.width, size.height))
    }

    #[test]
    fn microsoft_soft_font_size_detection_matches_every_source_vector() {
        use CellMatrix::{Default, Explicit, Invalid, Size5x10, Size6x10, Size7x10};
        use FontSet::{Size80x24, Size80x36, Size80x48, Size132x24, Size132x36, Size132x48};
        use FontUsage::{FullCell, Text};

        let matrix_cases = [
            (Size5x10, 0, Size80x24, Text, Some((10, 10))),
            (Size6x10, 0, Size80x24, Text, Some((10, 10))),
            (Size7x10, 0, Size80x24, Text, Some((10, 10))),
            (Size5x10, 0, Size132x24, Text, Some((6, 10))),
            (Size6x10, 0, Size132x24, Text, Some((6, 10))),
            (Size7x10, 0, Size132x24, Text, Some((10, 10))),
            (Size5x10, 0, Size80x24, FullCell, None),
            (Size6x10, 0, Size80x24, FullCell, None),
            (Size7x10, 0, Size80x24, FullCell, None),
            (Size5x10, 0, Size132x24, FullCell, None),
            (Size6x10, 0, Size132x24, FullCell, Some((6, 10))),
            (Size7x10, 0, Size132x24, FullCell, None),
            (Invalid, 0, Size80x24, Text, None),
            (Invalid, 0, Size132x24, Text, None),
            (Invalid, 0, Size80x24, FullCell, None),
            (Invalid, 0, Size132x24, FullCell, None),
            (Size7x10, 20, Size80x24, Text, Some((10, 10))),
        ];
        for (matrix, height, set, usage, expected) in matrix_cases {
            assert_eq!(cell(matrix, height, set, usage, ""), expected);
        }

        let explicit_cases = [
            (13, 17, Size80x24, FullCell, Some((13, 17))),
            (9, 25, Size132x24, FullCell, Some((9, 25))),
            (18, 38, Size80x24, FullCell, None),
            (12, 12, Size80x24, Text, Some((15, 12))),
            (9, 20, Size80x24, Text, Some((10, 20))),
            (10, 30, Size80x24, Text, Some((12, 30))),
            (8, 16, Size80x24, Text, Some((10, 16))),
            (7, 12, Size132x24, Text, Some((9, 12))),
            (5, 20, Size132x24, Text, Some((6, 20))),
            (6, 30, Size132x24, Text, Some((7, 30))),
            (5, 16, Size132x24, Text, Some((6, 16))),
        ];
        for (width, height, set, usage, expected) in explicit_cases {
            assert_eq!(cell(Explicit(width), height, set, usage, ""), expected);
        }

        let tall_set_cases = [
            (Size80x36, Text, (10, 10)),
            (Size80x48, Text, (10, 8)),
            (Size132x36, Text, (6, 10)),
            (Size132x48, Text, (6, 8)),
            (Size80x36, FullCell, (10, 10)),
            (Size80x48, FullCell, (10, 8)),
            (Size132x36, FullCell, (6, 10)),
            (Size132x48, FullCell, (6, 8)),
        ];
        for (set, usage, expected) in tall_set_cases {
            assert_eq!(cell(Default, 0, set, usage, ""), Some(expected));
        }

        let bitmap_cases = [
            (Size80x24, Text, "????????/????????", (10, 10)),
            (Size80x24, Text, "????????????/????????????", (15, 12)),
            (
                Size80x24,
                Text,
                "?????????/?????????/?????????/?????????",
                (10, 20),
            ),
            (
                Size80x24,
                Text,
                "??????????/??????????/??????????/??????????/??????????",
                (12, 30),
            ),
            (Size80x24, Text, "????????/????????/????????", (10, 16)),
            (Size132x24, Text, "?????/?????", (6, 10)),
            (Size132x24, Text, "???????/???????", (9, 12)),
            (Size132x24, Text, "?????/?????/?????/?????", (6, 20)),
            (
                Size132x24,
                Text,
                "??????/??????/??????/??????/??????",
                (7, 30),
            ),
            (Size132x24, Text, "?????/?????/?????", (6, 16)),
            (
                Size80x24,
                FullCell,
                "???????????????/???????????????",
                (15, 12),
            ),
            (
                Size80x24,
                FullCell,
                "??????????/??????????/??????????/??????????",
                (10, 20),
            ),
            (
                Size80x24,
                FullCell,
                "????????????/????????????/????????????/????????????/????????????",
                (12, 30),
            ),
            (
                Size80x24,
                FullCell,
                "??????????/??????????/??????????",
                (10, 16),
            ),
            (Size132x24, FullCell, "??????/??????", (6, 10)),
            (Size132x24, FullCell, "?????????/?????????", (9, 12)),
            (Size132x24, FullCell, "??????/??????/??????/??????", (6, 20)),
            (
                Size132x24,
                FullCell,
                "???????/???????/???????/???????/???????",
                (7, 30),
            ),
            (Size132x24, FullCell, "??????/??????/??????", (6, 16)),
        ];
        for (set, usage, data, expected) in bitmap_cases {
            assert_eq!(
                cell(Default, 0, set, usage, data),
                Some(expected),
                "data={data}"
            );
        }
    }
}
