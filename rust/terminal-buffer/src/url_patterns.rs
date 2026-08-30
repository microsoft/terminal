//! URL pattern detection over logical terminal-buffer rows.
//!
//! `TerminalCore` detects plain-text URLs independently from OSC 8 hyperlinks.
//! Wrapped rows form one logical line, while unwrapped row boundaries terminate
//! a candidate. Results remain in stable buffer coordinates and can be projected
//! back into viewport-relative coordinates after scrollback movement.

use crate::row::DbcsAttribute;
use crate::text_buffer::{TextBuffer, TextBufferPoint};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UrlPattern {
    uri: String,
    pub start: TextBufferPoint,
    pub end: TextBufferPoint,
}

impl UrlPattern {
    #[must_use]
    pub fn uri(&self) -> &str {
        &self.uri
    }

    #[must_use]
    pub fn contains(&self, point: TextBufferPoint) -> bool {
        point_at_or_after(point, self.start) && point_at_or_before(point, self.end)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ViewportUrlPattern {
    pub uri: String,
    pub start: TextBufferPoint,
    pub end: TextBufferPoint,
}

#[derive(Debug, Clone, Copy)]
struct LogicalChar {
    scalar: char,
    point: Option<TextBufferPoint>,
}

#[must_use]
pub fn detect_url_patterns(buffer: &TextBuffer) -> Vec<UrlPattern> {
    let logical = logical_chars(buffer);
    let mut patterns = Vec::new();
    let mut index = 0usize;

    while index < logical.len() {
        let prefix_len = if matches_prefix(&logical, index, "https://") {
            8
        } else if matches_prefix(&logical, index, "http://") {
            7
        } else {
            index += 1;
            continue;
        };

        let begin = index;
        let mut end = index + prefix_len;
        while let Some(item) = logical.get(end) {
            if item.point.is_none() || is_url_terminator(item.scalar) {
                break;
            }
            end += 1;
        }

        if end > begin + prefix_len {
            let start = logical[begin]
                .point
                .expect("URL prefix comes from a buffer cell");
            let finish = logical[end - 1]
                .point
                .expect("URL suffix comes from a buffer cell");
            let uri = logical[begin..end].iter().map(|item| item.scalar).collect();
            patterns.push(UrlPattern {
                uri,
                start,
                end: finish,
            });
            index = end;
        } else {
            index += 1;
        }
    }

    patterns
}

#[must_use]
pub fn url_at_buffer_position(patterns: &[UrlPattern], point: TextBufferPoint) -> Option<&str> {
    patterns
        .iter()
        .find(|pattern| pattern.contains(point))
        .map(UrlPattern::uri)
}

#[must_use]
pub fn url_interval_from_viewport_position(
    patterns: &[UrlPattern],
    viewport_point: TextBufferPoint,
    visible_start: u16,
) -> Option<ViewportUrlPattern> {
    let buffer_point = TextBufferPoint::new(
        viewport_point.x,
        viewport_point.y.checked_add(visible_start)?,
    );
    let pattern = patterns
        .iter()
        .find(|pattern| pattern.contains(buffer_point))?;
    Some(ViewportUrlPattern {
        uri: pattern.uri.clone(),
        start: TextBufferPoint::new(pattern.start.x, pattern.start.y.checked_sub(visible_start)?),
        end: TextBufferPoint::new(pattern.end.x, pattern.end.y.checked_sub(visible_start)?),
    })
}

fn logical_chars(buffer: &TextBuffer) -> Vec<LogicalChar> {
    let mut logical = Vec::new();
    for y in 0..buffer.height() {
        let row = buffer.row(i32::from(y));
        let limit = row.measure_right();
        let mut x = 0u16;
        while x < limit {
            if matches!(row.dbcs_attribute_at(i32::from(x)), DbcsAttribute::Trailing) {
                x = x.saturating_add(1);
                continue;
            }

            for decoded in char::decode_utf16(row.glyph_at(i32::from(x)).iter().copied()) {
                logical.push(LogicalChar {
                    scalar: decoded.unwrap_or(char::REPLACEMENT_CHARACTER),
                    point: Some(TextBufferPoint::new(x, y)),
                });
            }
            x = x.saturating_add(match row.dbcs_attribute_at(i32::from(x)) {
                DbcsAttribute::Leading => 2,
                DbcsAttribute::Single | DbcsAttribute::Trailing => 1,
            });
        }

        if !row.was_wrap_forced() {
            logical.push(LogicalChar {
                scalar: '\n',
                point: None,
            });
        }
    }
    logical
}

fn matches_prefix(logical: &[LogicalChar], begin: usize, prefix: &str) -> bool {
    prefix.chars().enumerate().all(|(offset, expected)| {
        logical
            .get(begin + offset)
            .is_some_and(|item| item.point.is_some() && item.scalar == expected)
    })
}

fn is_url_terminator(ch: char) -> bool {
    ch.is_whitespace() || matches!(ch, '<' | '>' | '"' | '\'' | '`')
}

fn point_at_or_after(point: TextBufferPoint, boundary: TextBufferPoint) -> bool {
    point.y > boundary.y || (point.y == boundary.y && point.x >= boundary.x)
}

fn point_at_or_before(point: TextBufferPoint, boundary: TextBufferPoint) -> bool {
    point.y < boundary.y || (point.y == boundary.y && point.x <= boundary.x)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::row_writer::replace_text;
    use crate::text_attribute::TextAttribute;

    fn utf16(text: &str) -> Vec<u16> {
        text.encode_utf16().collect()
    }

    fn write(buffer: &mut TextBuffer, y: i32, x: i32, text: &str) {
        replace_text(buffer.row_mut(y), x, &utf16(text)).expect("Microsoft URL fixture fits");
    }

    #[test]
    fn microsoft_terminal_buffer_url_pattern_detection_contract() {
        let mut single = TextBuffer::new(80, 8, TextAttribute::default()).expect("valid buffer");
        let before = "<Before>";
        let url = "https://www.contoso.com";
        let after = "<After>";
        write(&mut single, 0, 0, &format!("{before}{url}{after}"));
        let patterns = detect_url_patterns(&single);
        let start_x = u16::try_from(before.len()).expect("fixture column");
        let end_x = u16::try_from(before.len() + url.len() - 1).expect("fixture column");
        assert_eq!(
            url_at_buffer_position(&patterns, TextBufferPoint::new(start_x - 1, 0)),
            None
        );
        assert_eq!(
            url_at_buffer_position(&patterns, TextBufferPoint::new(start_x, 0)),
            Some(url)
        );
        assert_eq!(
            url_at_buffer_position(&patterns, TextBufferPoint::new(end_x, 0)),
            Some(url)
        );
        assert_eq!(
            url_at_buffer_position(&patterns, TextBufferPoint::new(end_x + 1, 0)),
            None
        );

        let long_url = "https://www.contoso.com/this-is-a-very-long-path/that-will-wrap-across-multiple-rows-in-the-terminal-buffer";
        let prefix = "WRAP>";
        let combined = format!("{prefix}{long_url}");
        assert!(combined.len() > 80);
        let mut wrapped = TextBuffer::new(80, 8, TextAttribute::default()).expect("valid buffer");
        write(&mut wrapped, 2, 0, &combined[..80]);
        wrapped.row_mut(2).set_wrap_forced(true);
        write(&mut wrapped, 3, 0, &combined[80..]);
        let patterns = detect_url_patterns(&wrapped);
        let wrapped_start = u16::try_from(prefix.len()).expect("fixture column");
        assert_eq!(
            url_at_buffer_position(&patterns, TextBufferPoint::new(wrapped_start, 2)),
            Some(long_url)
        );
        assert_eq!(
            url_at_buffer_position(&patterns, TextBufferPoint::new(0, 3)),
            Some(long_url)
        );
        assert_eq!(
            url_at_buffer_position(&patterns, TextBufferPoint::new(wrapped_start - 1, 2)),
            None
        );

        let mut history = TextBuffer::new(80, 80, TextAttribute::default()).expect("valid buffer");
        let scroll_url = "https://www.example.com/scrolled";
        write(&mut history, 40, 0, scroll_url);
        let patterns = detect_url_patterns(&history);
        assert_eq!(
            url_at_buffer_position(&patterns, TextBufferPoint::new(0, 40)),
            Some(scroll_url)
        );

        let viewport_url = "https://www.example.com/viewport";
        write(&mut history, 50, 0, viewport_url);
        let patterns = detect_url_patterns(&history);
        let interval =
            url_interval_from_viewport_position(&patterns, TextBufferPoint::new(0, 5), 45)
                .expect("viewport-relative URL interval");
        assert_eq!(interval.uri, viewport_url);
        assert_eq!(interval.start, TextBufferPoint::new(0, 5));
    }
}
